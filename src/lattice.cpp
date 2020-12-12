/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>

#include <iostream>

#include "jtag.hpp"
#include "lattice.hpp"
#include "latticeBitParser.hpp"
#include "mcsParser.hpp"
#include "progressBar.hpp"
#include "rawParser.hpp"
#include "display.hpp"
#include "part.hpp"
#include "spiFlash.hpp"

using namespace std;

#define ISC_ENABLE				0xc6
#  define ISC_ENABLE_FLASH_MODE	(1 << 3)
#  define ISC_ENABLE_SRAM_MODE	(0 << 3)
#define ISC_DISABLE				0x26
#define READ_DEVICE_ID_CODE		0xE0
#define FLASH_ERASE				0x0E
#  define FLASH_ERASE_UFM		(1<<3)
#  define FLASH_ERASE_CFG   	(1<<2)
#  define FLASH_ERASE_FEATURE	(1<<1)
#  define FLASH_ERASE_SRAM		(1<<0)
#  define FLASH_ERASE_ALL       0x0F
#define CHECK_BUSY_FLAG			0xF0
#  define CHECK_BUSY_FLAG_BUSY	(1 << 7)
#define RESET_CFG_ADDR			0x46
#define PROG_CFG_FLASH			0x70
#define REG_CFG_FLASH			0x73
#define PROG_FEATURE_ROW		0xE4
#define PROG_FEABITS			0xF8
#define PROG_DONE				0x5E
#define REFRESH					0x79

#define READ_FEATURE_ROW 0xE7
#define READ_FEABITS     0xFB
#define READ_STATUS_REGISTER 0x3C
#	define REG_STATUS_DONE		(1 << 8)
#	define REG_STATUS_ISC_EN	(1 << 9)
#	define REG_STATUS_BUSY		(1 << 12)
#	define REG_STATUS_FAIL		(1 << 13)
#	define REG_STATUS_CNF_CHK_MASK	(0x7 << 23)
#	define REG_STATUS_EXEC_ERR	(1 << 26)

Lattice::Lattice(Jtag *jtag, const string filename,
	bool flash_wr, bool sram_wr, bool verbose):
		Device(jtag, filename, verbose), _fpga_family(UNKNOWN_FAMILY)
{
	(void)sram_wr;
	if (_filename != "") {
		if (_file_extension == "jed" || _file_extension == "mcs") {
			_mode = Device::FLASH_MODE;
		} else if (_file_extension == "bit") {
			if (flash_wr)
				_mode = Device::FLASH_MODE;
			else
				_mode = Device::MEM_MODE;
		} else if (flash_wr) {  // for raw bin to flash at offset != 0
			_mode = Device::FLASH_MODE;
		} else {
			throw std::exception();
		}
	}
	/* check device family */
	uint32_t idcode = idCode();
	string family = fpga_list[idcode].family;
	if (family == "MachXO2")
		_fpga_family = MACHXO2_FAMILY;
	else if (family == "MachXO3LF")
		_fpga_family = MACHXO3_FAMILY;
	else if (family == "MachXO3D")
	        _fpga_family = MACHXO3D_FAMILY;
	else if (family == "ECP5")
		_fpga_family = ECP5_FAMILY;
	else if (family == "CrosslinkNX")
		_fpga_family = NEXUS_FAMILY;
	else if (family == "CertusNX")
		_fpga_family = NEXUS_FAMILY;
	else {
		printError("Unknown device family");
		throw std::exception();
	}
}

void displayFeabits(uint16_t _featbits)
{
	uint8_t boot_sequence = (_featbits >> 12) & 0x03;
	uint8_t m = (_featbits >> 11) & 0x01;
	printf("\tboot mode                                :");
	switch (boot_sequence) {
		case 0:
			if (m != 0x01)
				printf(" Single Boot from NVCM/Flash\n");
			else
				printf(" Dual Boot from NVCM/Flash then External if there is a failure\n");
			break;
		case 1:
			if (m == 0x01)
				printf(" Single Boot from External Flash\n");
			else
				printf(" Error!\n");
			break;
		default:
			printf(" Error!\n");
	}
	printf("\tMaster Mode SPI                          : %s\n",
	    (((_featbits>>11)&0x01)?"enable":"disable"));
	printf("\tI2c port                                 : %s\n",
	    (((_featbits>>10)&0x01)?"disable":"enable"));
	printf("\tSlave SPI port                           : %s\n",
	    (((_featbits>>9)&0x01)?"disable":"enable"));
	printf("\tJTAG port                                : %s\n",
	    (((_featbits>>8)&0x01)?"disable":"enable"));
	printf("\tDONE                                     : %s\n",
	    (((_featbits>>7)&0x01)?"enable":"disable"));
	printf("\tINITN                                    : %s\n",
	    (((_featbits>>6)&0x01)?"enable":"disable"));
	printf("\tPROGRAMN                                 : %s\n",
	    (((_featbits>>5)&0x01)?"disable":"enable"));
	printf("\tMy_ASSP                                  : %s\n",
	    (((_featbits>>4)&0x01)?"enable":"disable"));
	printf("\tPassword (Flash Protect Key) Protect All : %s\n",
	    (((_featbits>>3)&0x01)?"Enaabled" : "Disabled"));
	printf("\tPassword (Flash Protect Key) Protect     : %s\n",
	    (((_featbits>>2)&0x01)?"Enabled" : "Disabled"));
}

bool Lattice::checkStatus(uint32_t val, uint32_t mask)
{
	uint32_t reg = readStatusReg();

	return ((reg & mask) == val) ? true : false;
}

bool Lattice::program_mem()
{
	bool err;
	LatticeBitParser _bit(_filename, _verbose);

	printInfo("Open file " + _filename + " ", false);
	printSuccess("DONE");

	err = _bit.parse();

	printInfo("Parse file ", false);
	if (err == EXIT_FAILURE) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}

	if (_verbose)
		_bit.displayHeader();

	/* read ID Code 0xE0 */
	if (_verbose) {
		printf("IDCode : %x\n", idCode());
		displayReadReg(readStatusReg());
	}

	/* preload 0x1C */
	uint8_t tx_buf[26];
	memset(tx_buf, 0xff, 26);
	wr_rd(0x1C, tx_buf, 26, NULL, 0);

	wr_rd(0xFf, NULL, 0, NULL, 0);

	/* ISC Enable 0xC6 */
	printInfo("Enable configuration: ", false);
	if (!EnableISC(0x00)) {
		printError("FAIL");
		displayReadReg(readStatusReg());
		return false;
	} else {
		printSuccess("DONE");
	}

	/* ISC ERASE */
	printInfo("SRAM erase: ", false);
	if (flashErase(FLASH_ERASE_SRAM) == false) {
		printError("FAIL");
		displayReadReg(readStatusReg());
		return false;
	} else {
		printSuccess("DONE");
	}

	/* LSC_INIT_ADDRESS */
	wr_rd(0x46, NULL, 0, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);

	uint8_t *data = _bit.getData();
	int length = _bit.getLength()/8;
	wr_rd(0x7A, NULL, 0, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	uint8_t tmp[1024];
	int size = 1024;

	ProgressBar progress("Loading", length, 50);

	for (int i = 0; i < length; i += size) {
		progress.display(i);

		if (length < i + size)
			size = length-i;

		for (int ii = 0; ii < size; ii++)
			tmp[ii] = ConfigBitstreamParser::reverseByte(data[i+ii]);

		_jtag->shiftDR(tmp, NULL, size*8, Jtag::SHIFT_DR);
	}

	_jtag->set_state(Jtag::RUN_TEST_IDLE);

	if (checkStatus(0, REG_STATUS_CNF_CHK_MASK))
		progress.done();
	else {
		progress.fail();
		displayReadReg(readStatusReg());
		return false;
	}

	wr_rd(0xff, NULL, 0, NULL, 0);

	if (_verbose)
		printf("userCode: %08x\n", userCode());

	/* bypass */
	wr_rd(0xff, NULL, 0, NULL, 0);
	/* disable configuration mode */
	printInfo("Disable configuration: ", false);
	if (!DisableISC()) {
		printError("FAIL");
		displayReadReg(readStatusReg());
		return false;
	} else {
		printSuccess("DONE");
	}

	if (_verbose)
		displayReadReg(readStatusReg());

	/* bypass */
	wr_rd(0xff, NULL, 0, NULL, 0);
	_jtag->go_test_logic_reset();
	return true;
}

bool Lattice::program_intFlash()
{
	bool err;
	uint64_t featuresRow;
	uint16_t feabits;
	uint8_t eraseMode;
	vector<string> ufm_data, cfg_data, ebr_data;

	JedParser _jed(_filename, _verbose);

	printInfo("Open file " + _filename + " ", false);
	printSuccess("DONE");

	err = _jed.parse();

	printInfo("Parse file ", false);
	if (err == EXIT_FAILURE) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}

	/* bypass */
	wr_rd(0xff, NULL, 0, NULL, 0);
	/* ISC Enable 0xC6 followed by
	 * 0x08 (Enable nVCM/Flash Normal mode */
	printInfo("Enable configuration: ", false);
	if (!EnableISC(0x08)) {
		printError("FAIL");
		displayReadReg(readStatusReg());
		return false;
	} else {
		printSuccess("DONE");
	}

	for (size_t i = 0; i < _jed.nb_section(); i++) {
		string note = _jed.noteForSection(i);
		if (note == "TAG DATA") {
			eraseMode |= FLASH_ERASE_UFM;
			ufm_data = _jed.data_for_section(i);
		} else if (note == "END CONFIG DATA") {
			continue;
		} else if (note == "EBR_INIT DATA") {
			ebr_data = _jed.data_for_section(i);
		} else {
			cfg_data = _jed.data_for_section(i);
		}
	}

	/* check if feature area must be updated */
	featuresRow = _jed.featuresRow();
	feabits = _jed.feabits();
	eraseMode = FLASH_ERASE_CFG;
	if (featuresRow != readFeaturesRow() || feabits != readFeabits())
		eraseMode |= FLASH_ERASE_FEATURE;

	/* ISC ERASE */
	printInfo("Flash erase: ", false);
	if (flashErase(eraseMode) == false) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}

	/* LSC_INIT_ADDRESS */
	wr_rd(0x46, NULL, 0, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);

	/* flash CfgFlash */
	if (false == flashProg(0, "data", cfg_data))
		return false;

	/* flash EBR Init */
	if (ebr_data.size()) {
		if (false == flashProg(0, "EBR", ebr_data))
			return false;
	}
	/* verify write */
	if (Verify(cfg_data) == false)
		return false;

	/* missing usercode update */

	/* LSC_INIT_ADDRESS */
	wr_rd(0x46, NULL, 0, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);

	if ((eraseMode & FLASH_ERASE_FEATURE) != 0) {
		/* write feature row */
		printInfo("Program features Row: ", false);
		if (writeFeaturesRow(_jed.featuresRow(), true) == false) {
			printError("FAIL");
			return false;
		} else {
			printSuccess("DONE");
		}
		/* write feabits */
		printInfo("Program feabits: ", false);
		if (writeFeabits(_jed.feabits(), true) == false) {
			printError("FAIL");
			return false;
		} else {
			printSuccess("DONE");
		}
	}

	/* ISC program done 0x5E */
	printInfo("Write program Done: ", false);
	if (writeProgramDone() == false) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}

	/* bypass */
	wr_rd(0xff, NULL, 0, NULL, 0);
	/* disable configuration mode */
	printInfo("Disable configuration: ", false);
	if (!DisableISC()) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}
	return true;
}

bool Lattice::program_extFlash(unsigned int offset)
{
	ConfigBitstreamParser *_bit;
	if (_file_extension == "mcs")
		_bit = new McsParser(_filename, true, _verbose);
	else if (_file_extension == "bit")
		_bit = new LatticeBitParser(_filename, _verbose);
	else {
		if (offset == 0) {
			printError("Error: can't write raw data at the beginning of the flash");
			throw std::exception();
		}
		_bit = new RawParser(_filename, false);
	}

	printInfo("Open file " + _filename + " ", false);
	printSuccess("DONE");

	int err = _bit->parse();

	printInfo("Parse file ", false);
	if (err == EXIT_FAILURE) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}

	/*IR = 0h3A, DR=0hFE,0h68. Enter RUNTESTIDLE.
	 * thank @GregDavill
	 * https://twitter.com/GregDavill/status/1251786406441086977
	 */
	_jtag->shiftIR(0x3A, 8, Jtag::EXIT1_IR);
	uint8_t tmp[2] = {0xFE, 0x68};
	_jtag->shiftDR(tmp, NULL, 16);

	uint8_t *data = _bit->getData();
	int length = _bit->getLength()/8;

	/* test SPI */
	SPIFlash flash(this, _verbose);
	flash.reset();
	flash.read_id();
	flash.read_status_reg();
	flash.erase_and_prog(offset, data, length);

	delete _bit;
	return true;
}

bool Lattice::program_flash(unsigned int offset)
{
	/* read ID Code 0xE0 */
	if (_verbose) {
		printf("IDCode : %x\n", idCode());
		displayReadReg(readStatusReg());
	}

	/* preload 0x1C */
	uint8_t tx_buf[26];
	memset(tx_buf, 0xff, 26);
	wr_rd(0x1C, tx_buf, 26, NULL, 0);

	wr_rd(0xFf, NULL, 0, NULL, 0);

	/* ISC Enable 0xC6 */
	printInfo("Enable configuration: ", false);
	if (!EnableISC(0x00)) {
		printError("FAIL");
		displayReadReg(readStatusReg());
		return false;
	} else {
		printSuccess("DONE");
	}

	/* ISC ERASE */
	printInfo("SRAM erase: ", false);
	if (flashErase(FLASH_ERASE_SRAM) == false) {
		printError("FAIL");
		displayReadReg(readStatusReg());
		return false;
	} else {
		printSuccess("DONE");
	}

	DisableISC();

	if (_file_extension == "jed")
		program_intFlash();
	else
		program_extFlash(offset);

	/* *************************** */
	/* reload bitstream from flash */
	/* *************************** */

	/* ISC REFRESH 0x79 */
	printInfo("Refresh: ", false);
	if (loadConfiguration() == false) {
		printError("FAIL");
		displayReadReg(readStatusReg());
		return false;
	} else {
		printSuccess("DONE");
	}

	/* bypass */
	wr_rd(0xff, NULL, 0, NULL, 0);
	_jtag->go_test_logic_reset();
	return true;
}

void Lattice::program(unsigned int offset)
{
	if (_mode == FLASH_MODE)
		program_flash(offset);
	else if (_mode == MEM_MODE)
		program_mem();
}

/* flash mode :
 */
bool Lattice::EnableISC(uint8_t flash_mode)
{
	wr_rd(ISC_ENABLE, &flash_mode, 1, NULL, 0);

	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);
	if (!pollBusyFlag())
		return false;
	if (!checkStatus(REG_STATUS_ISC_EN, REG_STATUS_ISC_EN))
		return false;
	return true;
}

bool Lattice::DisableISC()
{
	wr_rd(ISC_DISABLE, NULL, 0, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);
	if (!pollBusyFlag())
		return false;
	if (!checkStatus(0, REG_STATUS_ISC_EN))
		return false;
	return true;
}

bool Lattice::EnableCfgIf()
{
	uint8_t tx_buf = 0x08;
	wr_rd(0x74, &tx_buf, 1, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);
	return pollBusyFlag();
}

bool Lattice::DisableCfg()
{
	uint8_t tx_buf, rx_buf;
	wr_rd(0x26, &tx_buf, 1, &rx_buf, 1);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);
	return true;
}

int Lattice::idCode()
{
	uint8_t device_id[4];
	wr_rd(READ_DEVICE_ID_CODE, NULL, 0, device_id, 4);
	return device_id[3] << 24 |
					device_id[2] << 16 |
					device_id[1] << 8  |
					device_id[0];
}

int Lattice::userCode()
{
	uint8_t usercode[4];
	wr_rd(0xC0, NULL, 0, usercode, 4);
	return usercode[3] << 24 |
					usercode[2] << 16 |
					usercode[1] << 8  |
					usercode[0];
}

bool Lattice::checkID()
{
	printf("\n");
	printf("check ID\n");
	uint8_t tx[4];
	wr_rd(0xE2, tx, 4, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);

	uint32_t reg = readStatusReg();
	displayReadReg(reg);

	tx[3] = 0x61;
	tx[2] = 0x2b;
	tx[1] = 0xd0;
	tx[0] = 0x43;
	wr_rd(0xE2, tx, 4, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);
	reg = readStatusReg();
	displayReadReg(reg);
	printf("%08x\n", reg);
	printf("\n");
	return true;
}

/* feabits is MSB first
 * maybe this register too
 * or not
 */
uint32_t Lattice::readStatusReg()
{
	uint32_t reg;
	uint8_t rx[4], tx[4];
	/* valgrind warn */
	memset(tx, 0, 4);
	wr_rd(0x3C, tx, 4, rx, 4);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);
	reg = rx[3] << 24 | rx[2] << 16 | rx[1] << 8 | rx[0];
	return reg;
}

bool Lattice::wr_rd(uint8_t cmd,
					uint8_t *tx, int tx_len,
					uint8_t *rx, int rx_len,
					bool verbose)
{
	int xfer_len = rx_len;
	if (tx_len > rx_len)
		xfer_len = tx_len;

	uint8_t xfer_tx[xfer_len];
	uint8_t xfer_rx[xfer_len];
	memset(xfer_tx, 0, xfer_len);
	int i;
	if (tx != NULL) {
		for (i = 0; i < tx_len; i++)
			xfer_tx[i] = tx[i];
	}

	_jtag->shiftIR(&cmd, NULL, 8, Jtag::PAUSE_IR);
	if (rx || tx) {
		_jtag->shiftDR(xfer_tx, (rx) ? xfer_rx : NULL, 8 * xfer_len,
			Jtag::PAUSE_DR);
	}
	if (rx) {
		if (verbose) {
			for (i=xfer_len-1; i >= 0; i--)
				printf("%02x ", xfer_rx[i]);
			printf("\n");
		}
	    for (i = 0; i < rx_len; i++)
			rx[i] = (xfer_rx[i]);
	}
	return true;
}

void Lattice::displayReadReg(uint32_t dev)
{
	uint8_t err;
	printf("displayReadReg\n");
	if (dev & 1<<0) printf("\tTRAN Mode\n");
	printf("\tConfig Target Selection : %x\n", (dev >> 1) & 0x07);
	if (dev & 1<<4) printf("\tJTAG Active\n");
	if (dev & 1<<5) printf("\tPWD Protect\n");
	if (dev & 1<<6) printf("\tOTP\n");
	if (dev & 1<<7) printf("\tDecrypt Enable\n");
	if (dev & REG_STATUS_DONE) printf("\tDone Flag\n");
	if (dev & REG_STATUS_ISC_EN) printf("\tISC Enable\n");
	if (dev & 1 << 10) printf("\tWrite Enable\n");
	if (dev & 1 << 11) printf("\tRead Enable\n");
	if (dev & REG_STATUS_BUSY) printf("\tBusy Flag\n");
	if (dev & REG_STATUS_FAIL) printf("\tFail Flag\n");
	if (dev & 1 << 14) printf("\tFFEA OTP\n");
	if (dev & 1 << 15) printf("\tDecrypt Only\n");
	if (dev & 1 << 16) printf("\tPWD Enable\n");
	if (_fpga_family == NEXUS_FAMILY) {
		if (dev & 1 << 17) printf("\tPWD All\n");
		if (dev & 1 << 18) printf("\tCID En\n");
		if (dev & 1 << 19) printf("\tinternal use\n");
		if (dev & 1 << 21) printf("\tEncryption PreAmble\n");
		if (dev & 1 << 22) printf("\tStd PreAmble\n");
		if (dev & 1 << 23) printf("\tSPIm Fail1\n");
		err = (dev >> 24)&0x0f;
	} else {
		if (dev & 1 << 17) printf("\tUFM OTP\n");
		if (dev & 1 << 18) printf("\tASSP\n");
		if (dev & 1 << 19) printf("\tSDM Enable\n");
		if (dev & 1 << 20) printf("\tEncryption PreAmble\n");
		if (dev & 1 << 21) printf("\tStd PreAmble\n");
		if (dev & 1 << 22) printf("\tSPIm Fail1\n");
		err = (dev >> 23)&0x07;
	}

	printf("\t");
	switch (err) {
		case 0:
			printf("No err\n");
			break;
		case 1:
			printf("ID ERR\n");
			break;
		case 2:
			printf("CMD ERR\n");
			break;
		case 3:
			printf("CRC ERR\n");
			break;
		case 4:
			printf("Preamble ERR\n");
			break;
		case 5:
			printf("Abort ERR\n");
			break;
		case 6:
			printf("Overflow ERR\n");
			break;
		case 7:
			printf("SDM EOF\n");
			break;
		default:
			printf("unknown %x\n", err);
	}

	if (_fpga_family == NEXUS_FAMILY) {
		if ((dev >> 28) & 0x01) printf("\tEXEC Error\n");
		if ((dev >> 29) & 0x01) printf("\tID Error\n");
		if ((dev >> 30) & 0x01) printf("\tInvalid Command\n");
		if ((dev >> 31) & 0x01) printf("\tWDT Busy\n");
	} else {
		if (dev & REG_STATUS_EXEC_ERR) printf("\tEXEC Error\n");
		if ((dev >> 27) & 0x01) printf("\tDevice failed to verify\n");
		if ((dev >> 28) & 0x01) printf("\tInvalid Command\n");
		if ((dev >> 29) & 0x01) printf("\tSED Error\n");
		if ((dev >> 30) & 0x01) printf("\tBypass Mode\n");
		if ((dev >> 31) & 0x01) printf("\tFT Mode\n");
	}

#if 0
	if (_fpga_family == NEXUS_FAMILY) {
		if ((dev >> 33) & 0x01) printf("\tDry Run Done\n");
		err = (dev >> 34)&0x0f;
		printf("\tBSE Error 1 Code for previous bitstream execution\n");
		printf("\t\t");
		switch (err) {
			case 0:
				printf("No err\n");
				break;
			case 1:
				printf("ID ERR\n");
				break;
			case 2:
				printf("CMD ERR\n");
				break;
			case 3:
				printf("CRC ERR\n");
				break;
			case 4:
				printf("Preamble ERR\n");
				break;
			case 5:
				printf("Abort ERR\n");
				break;
			case 6:
				printf("Overflow ERR\n");
				break;
			case 7:
				printf("SDM EOF\n");
				break;
			case 8:
				printf("Authentification ERR\n");
				break;
			case 9:
				printf("Authentification Setup ERR\n");
				break;
			case 10:
				printf("Bitstream Engine Timeout ERR\n");
				break;
			default:
				printf("unknown %x\n", err);
		}
		if ((dev >> 38) & 0x01) printf("\tBypass Mode\n");
		if ((dev >> 39) & 0x01) printf("\tFlow Through Mode\n");
		if ((dev >> 42) & 0x01) printf("\tSFDP Timeout\n");
		if ((dev >> 43) & 0x01) printf("\tKey Destroy pass\n");
		if ((dev >> 44) & 0x01) printf("\tINITN\n");
		if ((dev >> 45) & 0x01) printf("\tI3C Parity Error2\n");
		if ((dev >> 46) & 0x01) printf("\tINIT Bus ID Error\n");
		if ((dev >> 47) & 0x01) printf("\tI3C Parity Error1\n");
		err = (dev >> 48) & 0x03;
		printf("\tAuthentification mode:\n");
		printf("\t\t");
		switch (err) {
			case 3:
			case 0:
				printf("No Auth\n");
				break;
			case 1:
				printf("ECDSA\n");
				break;
			case 2:
				printf("HMAC\n");
				break;
		}
		if ((dev >> 50) & 0x01) printf("\tAuthentification Done\n");
		if ((dev >> 51) & 0x01) printf("\tDry Run Authentification Done\n");
#endif
}

bool Lattice::pollBusyFlag(bool verbose)
{
	uint8_t rx;
	int timeout = 0;
	do {
		wr_rd(CHECK_BUSY_FLAG, NULL, 0, &rx, 1);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(1000);
		if (verbose)
			printf("pollBusyFlag :%02x\n", rx);
		if (timeout == 100000000){
			cerr << "timeout" << endl;
			return false;
		} else {
			timeout++;
		}
	} while (rx != 0);

	return true;
}

bool Lattice::flashEraseAll()
{
	return flashErase(0xf);
}

bool Lattice::flashErase(uint8_t mask)
{
	uint8_t tx[1] = {mask};
	wr_rd(FLASH_ERASE, tx, 1, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);
	if (!pollBusyFlag())
		return false;
	if (!checkStatus(0, REG_STATUS_FAIL))
		return false;
	return true;
}

bool Lattice::flashProg(uint32_t start_addr, const string &name, vector<string> data)
{
	(void)start_addr;
	ProgressBar progress("Writing " + name, data.size(), 50);
	for (uint32_t line = 0; line < data.size(); line++) {
		wr_rd(PROG_CFG_FLASH, (uint8_t *)data[line].c_str(),
				16, NULL, 0);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(1000);
		progress.display(line);
		if (pollBusyFlag() == false)
			return false;
	}
	progress.done();
	return true;
}

bool Lattice::Verify(std::vector<std::string> data, bool unlock)
{
	uint8_t tx_buf[16], rx_buf[16];
	if (unlock)
		EnableISC(0x08);

	wr_rd(0x46, NULL, 0, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);

	tx_buf[0] = REG_CFG_FLASH;
	_jtag->shiftIR(tx_buf, NULL, 8, Jtag::PAUSE_IR);

	memset(tx_buf, 0, 16);
	bool failure = false;
	ProgressBar progress("Verifying", data.size(), 50);
	for (size_t line = 0;  line< data.size(); line++) {
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(2);
		_jtag->shiftDR(tx_buf, rx_buf, 16*8, Jtag::PAUSE_DR);
		for (size_t i = 0; i < data[line].size(); i++) {
			if (rx_buf[i] != (unsigned char)data[line][i]) {
				printf("%3zu %3zu %02x -> %02x\n", line, i,
						rx_buf[i], (unsigned char)data[line][i]);
				failure = true;
			}
		}
		if (failure) {
			printf("Verify Failure\n");
			break;
		}
		progress.display(line);
	}
	if (unlock)
		DisableISC();

	progress.done();

	return true;
}

uint64_t Lattice::readFeaturesRow()
{
	uint8_t tx_buf[8];
	uint8_t rx_buf[8];
	uint64_t reg = 0;
	memset(tx_buf, 0, 8);
	wr_rd(READ_FEATURE_ROW, tx_buf, 8, rx_buf, 8);
	for (int i = 0; i < 8; i++)
		reg |= ((uint64_t)rx_buf[i] << (i*8));
	return reg;
}

uint16_t Lattice::readFeabits()
{
	uint8_t rx_buf[2];
	wr_rd(READ_FEABITS, NULL, 0, rx_buf, 2);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);

	return rx_buf[0] | (((uint16_t)rx_buf[1]) << 8);
}

bool Lattice::writeFeaturesRow(uint64_t features, bool verify)
{
	uint8_t tx_buf[8];
	for (int i=0; i < 8; i++)
		tx_buf[i] = ((features >> (i*8)) & 0x00ff);
	wr_rd(PROG_FEATURE_ROW, tx_buf, 8, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);
	if (!pollBusyFlag())
		return false;
	if (verify)
		return (features == readFeaturesRow()) ? true : false;
	return true;
}

bool Lattice::writeFeabits(uint16_t feabits, bool verify)
{
	uint8_t tx_buf[2] = {(uint8_t)(feabits&0x00ff),
							(uint8_t)(0x00ff & (feabits>>8))};

	wr_rd(PROG_FEABITS, tx_buf, 2, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);
	if (!pollBusyFlag())
		return false;
	if (verify)
		return (feabits == readFeabits()) ? true : false;
	return true;
}

bool Lattice::writeProgramDone()
{
	wr_rd(PROG_DONE, NULL, 0, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);
	if (!pollBusyFlag())
		return false;
	if (!checkStatus(REG_STATUS_DONE, REG_STATUS_DONE))
		return false;
	return true;
}

bool Lattice::loadConfiguration()
{
	wr_rd(REFRESH, NULL, 0, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);
	if (!pollBusyFlag())
		return false;
	if (!checkStatus(REG_STATUS_DONE, REG_STATUS_DONE))
		return false;
	return true;
}

/* ------------------ */
/* SPI implementation */
/* ------------------ */

int Lattice::spi_put(uint8_t cmd, uint8_t *tx, uint8_t *rx, uint32_t len)
{
	int xfer_len = len + 1;
	uint8_t jtx[xfer_len];
	uint8_t jrx[xfer_len];

	jtx[0] = LatticeBitParser::reverseByte(cmd);

	if (tx != NULL) {
		for (uint32_t i=0; i < len; i++)
			jtx[i+1] = LatticeBitParser::reverseByte(tx[i]);
	}

	/* send first already stored cmd,
	 * in the same time store each byte
	 * to next
	 */
	_jtag->shiftDR(jtx, (rx == NULL)? NULL: jrx, 8*xfer_len);

	if (rx != NULL) {
		for (uint32_t i=0; i < len; i++)
			rx[i] = LatticeBitParser::reverseByte(jrx[i+1]);
	}
	return 0;
}

int Lattice::spi_put(uint8_t *tx, uint8_t *rx, uint32_t len)
{
	int xfer_len = len;
	uint8_t jtx[xfer_len];
	uint8_t jrx[xfer_len];

	if (tx != NULL) {
		for (uint32_t i=0; i < len; i++)
			jtx[i] = LatticeBitParser::reverseByte(tx[i]);
	}

	/* send first already stored cmd,
	 * in the same time store each byte
	 * to next
	 */
	_jtag->shiftDR(jtx, (rx == NULL)? NULL: jrx, 8*xfer_len);

	if (rx != NULL) {
		for (uint32_t i=0; i < len; i++)
			rx[i] = LatticeBitParser::reverseByte(jrx[i]);
	}
	return 0;
}

int Lattice::spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
            uint32_t timeout, bool verbose)
{
	uint8_t rx;
	uint8_t dummy[2];
	uint8_t tmp;
	uint8_t tx = LatticeBitParser::reverseByte(cmd);
	uint32_t count = 0;

	/* CS is low until state goes to EXIT1_IR
	 * so manually move to state machine to stay is this
	 * state as long as needed
	 */
	_jtag->set_state(Jtag::SHIFT_DR);
	_jtag->read_write(&tx, NULL, 8, 0);

	do {
		_jtag->read_write(dummy, &rx, 8, 0);
		tmp = (LatticeBitParser::reverseByte(rx));
		count ++;
		if (count == timeout){
			printf("timeout: %x %x %u\n", tmp, rx, count);
			break;
		}

		if (verbose) {
			printf("%x %x %x %u\n", tmp, mask, cond, count);
		}
	} while ((tmp & mask) != cond);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	if (count == timeout) {
		printf("%x\n", tmp);
		std::cout << "wait: Error" << std::endl;
		return -ETIME;
	} else
		return 0;
}
