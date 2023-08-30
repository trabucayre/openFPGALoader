// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <stdexcept>

#include "jtag.hpp"
#include "gowin.hpp"
#include "display.hpp"
#include "fsparser.hpp"
#include "rawParser.hpp"
#include "spiFlash.hpp"

#include <byteswap.h>

using namespace std;

#ifdef STATUS_TIMEOUT
// defined in the Windows headers included by libftdi.h
#undef STATUS_TIMEOUT
#endif

#define NOOP				0x02
#define ERASE_SRAM			0x05
#define READ_SRAM			0x03
#define XFER_DONE			0x09
#define READ_IDCODE			0x11
#define INIT_ADDR			0x12
#define READ_USERCODE		0x13
#define CONFIG_ENABLE		0x15
#define XFER_WRITE			0x17
#define CONFIG_DISABLE		0x3A
#define RELOAD				0x3C
#define STATUS_REGISTER		0x41
#  define STATUS_CRC_ERROR			(1 << 0)
#  define STATUS_BAD_COMMAND		(1 << 1)
#  define STATUS_ID_VERIFY_FAILED	(1 << 2)
#  define STATUS_TIMEOUT			(1 << 3)
#  define STATUS_MEMORY_ERASE		(1 << 5)
#  define STATUS_PREAMBLE			(1 << 6)
#  define STATUS_SYSTEM_EDIT_MODE	(1 << 7)
#  define STATUS_PRG_SPIFLASH_DIRECT (1 << 8)
#  define STATUS_NON_JTAG_CNF_ACTIVE (1 << 10)
#  define STATUS_BYPASS				(1 << 11)
#  define STATUS_GOWIN_VLD			(1 << 12)
#  define STATUS_DONE_FINAL			(1 << 13)
#  define STATUS_SECURITY_FINAL		(1 << 14)
#  define STATUS_READY				(1 << 15)
#  define STATUS_POR				(1 << 16)
#  define STATUS_FLASH_LOCK			(1 << 17)

#define EF_PROGRAM			0x71
#define EFLASH_ERASE		0x75
#define SWITCH_TO_MCU_JTAG		0x7a

/* BSCAN spi (external flash) (see below for details) */
/* most common pins def */
#define BSCAN_SPI_SCK           (1 << 1)
#define BSCAN_SPI_CS            (1 << 3)
#define BSCAN_SPI_DI            (1 << 5)
#define BSCAN_SPI_DO            (1 << 7)
#define BSCAN_SPI_MSK           (1 << 6)
/* GW1NSR-4C pins def */
#define BSCAN_GW1NSR_4C_SPI_SCK (1 << 7)
#define BSCAN_GW1NSR_4C_SPI_CS  (1 << 5)
#define BSCAN_GW1NSR_4C_SPI_DI  (1 << 3)
#define BSCAN_GW1NSR_4C_SPI_DO  (1 << 1)
#define BSCAN_GW1NSR_4C_SPI_MSK (1 << 0)

#define HZ_TO_US (2)
#define HZ_TO_MS (HZ_TO_US * 1000)

Gowin::Gowin(Jtag *jtag, const string filename, const string &file_type, std::string mcufw,
		Device::prog_type_t prg_type, bool external_flash,
		bool verify, int8_t verbose): Device(jtag, filename, file_type,
		verify, verbose), is_gw1n1(false), is_gw2a(false), is_gw5a(false),
		_external_flash(external_flash),
		_spi_sck(BSCAN_SPI_SCK), _spi_cs(BSCAN_SPI_CS),
		_spi_di(BSCAN_SPI_DI), _spi_do(BSCAN_SPI_DO),
		_spi_msk(BSCAN_SPI_MSK)
{
	_fs = NULL;
	_mcufw = NULL;

	uint32_t idcode = _jtag->get_target_device_id();

	if (prg_type == Device::WR_FLASH)
		_mode = Device::FLASH_MODE;
	else
		_mode = Device::MEM_MODE;

	if (!_file_extension.empty()) {
		if (_file_extension == "fs") {
			try {
				_fs = new FsParser(_filename, _mode == Device::MEM_MODE, _verbose);
			} catch (std::exception &e) {
				throw std::runtime_error(e.what());
			}
		} else {
			/* non fs file is only allowed with external flash */
			if (!external_flash)
				throw std::runtime_error("incompatible file format");
			try {
				_fs = new RawParser(_filename, false);
			} catch (std::exception &e) {
				throw std::runtime_error(e.what());
			}
		}

		printInfo("Parse file ", false);
		if (_fs->parse() == EXIT_FAILURE) {
			printError("FAIL");
			delete _fs;
			throw std::runtime_error("can't parse file");
		} else {
			printSuccess("DONE");
		}

		if (_verbose)
			_fs->displayHeader();

		/* for fs file check match with targeted device */
		if (_file_extension == "fs") {
			string idcode_str = _fs->getHeaderVal("idcode");
			uint32_t fs_idcode = std::stoul(idcode_str.c_str(), NULL, 16);
			if ((fs_idcode & 0x0fffffff) != idcode) {
				char mess[256];
				sprintf(mess, "mismatch between target's idcode and bitstream idcode\n"
					"\tbitstream has 0x%08X hardware requires 0x%08x", fs_idcode, idcode);
				throw std::runtime_error(mess);
			}
		}
	}
	_jtag->setClkFreq(2500000);

	/* erase and program flash differ for GW1N1 */
	if (idcode == 0x0900281B)
		is_gw1n1 = true;
	/* bscan spi external flash differ for GW1NSR-4C */
	if (idcode == 0x0100981b) {
		_spi_sck = BSCAN_GW1NSR_4C_SPI_SCK;
		_spi_cs  = BSCAN_GW1NSR_4C_SPI_CS;
		_spi_di  = BSCAN_GW1NSR_4C_SPI_DI;
		_spi_do  = BSCAN_GW1NSR_4C_SPI_DO;
		_spi_msk = BSCAN_GW1NSR_4C_SPI_MSK;
	}

	/*
	 * GW2 series has no internal flash and uses new bitstream checksum
	 * algorithm that is not yet supported.
	 */
	switch (idcode) {
		case 0x0000081b: /* GW2A(R)-18(C) */
		case 0x0000281b: /* GW2A(R)-55(C) */
			_external_flash = true;
			/* FIXME: implement GW2 checksum calculation */
			skip_checksum = true;
			is_gw2a = true;
			break;
		case 0x0001081b: /* GW5AST-138 */
		case 0x0001181b: /* GW5AT-138 */
		case 0x0001281b: /* GW5A-25 */
			_external_flash = true;
			/* FIXME: implement GW5 checksum calculation */
			skip_checksum = true;
			is_gw5a = true;
			break;
	};

	if (mcufw.size() > 0) {
		if (idcode != 0x0100981b)
			throw std::runtime_error("Microcontroller firmware flashing only supported on GW1NSR-4C");

		_mcufw = new RawParser(mcufw, false);

		if (_mcufw->parse() == EXIT_FAILURE) {
			printError("FAIL");
			delete _mcufw;
			throw std::runtime_error("can't parse file");
		} else {
			printSuccess("DONE");
		}
	}
}

Gowin::~Gowin()
{
	if (_fs)
		delete _fs;
	if (_mcufw)
		delete _mcufw;
}

bool Gowin::send_command(uint8_t cmd)
{
	_jtag->shiftIR(&cmd, nullptr, 8);
	_jtag->toggleClk(8);

	return true;
}

uint32_t Gowin::readReg32(uint8_t cmd)
{
	uint32_t reg = 0, tmp = 0xff;
	send_command(cmd);
	_jtag->shiftDR((uint8_t *)&tmp, (uint8_t *)&reg, 32);
	return le32toh(reg);
}

void Gowin::reset()
{
	send_command(RELOAD);
	send_command(NOOP);
}

void Gowin::programFlash()
{
	const uint8_t *data = _fs->getData();
	int length = _fs->getLength();
	
	send_command(0x3a);
	send_command(0);
	_jtag->go_test_logic_reset();
	usleep(500*1000);
	
	eraseSRAM();
	if (!eraseFLASH())
		return;
	/* test status a faire */
	if (!writeFLASH(0, data, length))
		return;
	if (_mcufw) {
		const uint8_t *mcu_data = _mcufw->getData();
		int mcu_length = _mcufw->getLength();
		if (!writeFLASH(0x380, mcu_data, mcu_length))
			return;
	}
	if (_verify)
		printWarn("writing verification not supported");

	/* check if file checksum == checksum in FPGA */
	if (!skip_checksum)
		checkCRC();

	if (_verbose)
		displayReadReg(readStatusReg());
}

void Gowin::programExtFlash(unsigned int offset, bool unprotect_flash) {
	_jtag->setClkFreq(10000000);

	if (!enableCfg())
		throw std::runtime_error("Error: fail to enable configuration");

	eraseSRAM();
	send_command(XFER_DONE);
	send_command(NOOP);

	if (!is_gw2a) {
		send_command(0x3D);
	} else {
		disableCfg();
		send_command(NOOP);
	}

	SPIFlash spiFlash(this, unprotect_flash,
					  (_verbose ? 1 : (_quiet ? -1 : 0)));
	spiFlash.reset();
	spiFlash.read_id();
	spiFlash.display_status_reg(spiFlash.read_status_reg());
	const uint8_t *data = _fs->getData();
	int length = _fs->getLength();

	if (spiFlash.erase_and_prog(offset, data, length / 8) != 0)
		throw std::runtime_error("Error: write to flash failed");
	if (_verify)
		if (!spiFlash.verify(offset, data, length / 8, 256))
			throw std::runtime_error("Error: flash vefication failed");
	if (!is_gw2a) {
		if (!disableCfg())
			throw std::runtime_error("Error: fail to disable configuration");
	}

	reset();

}

void Gowin::programSRAM() {
	if (_verbose) {
		displayReadReg(readStatusReg());
	}
	/* Work around FPGA stuck in Bad Command status */
	if (is_gw5a) {
		reset();
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(1000000);
	}

	eraseSRAM();

	if (!writeSRAM(_fs->getData(), _fs->getLength()))
		return;

	/* ocheck if file checksum == checksum in FPGA */
	if (!skip_checksum)
		checkCRC();
	if (_verbose)
		displayReadReg(readStatusReg());
}

void Gowin::program(unsigned int offset, bool unprotect_flash)
{
	if (!_fs)
		return;

	if (_mode == FLASH_MODE) {
		if (is_gw5a)
			throw std::runtime_error("Error: write to flash on GW5A is not yet supported");
		if (_external_flash)
			programExtFlash(offset, unprotect_flash);
		else
			programFlash();
	} else if (_mode == MEM_MODE)
		programSRAM();

	return;
}

void Gowin::checkCRC()
{
	uint32_t ucode = readUserCode();
	uint16_t checksum = static_cast<FsParser *>(_fs)->checksum();
	if (static_cast<uint16_t>(0xffff & ucode) == checksum) {
		printSuccess("CRC check: Success");
		return;
	}
	/* no match:
	 * user code register contains checksum or
	 * user_code when set_option -user_code
	 * is used, try to compare with this value
	 */
	string hdr = "";
	char mess[256];
	try {
		hdr = _fs->getHeaderVal("checkSum");
		if (!hdr.empty()) {
		} else {
			uint32_t user_code = strtol(hdr.c_str(), NULL, 16);
			if (ucode != user_code) {
				snprintf(mess, 256, "Read 0x%08x (checksum: 0x%08x, user_code: 0x%08x)\n",
					 ucode, checksum, user_code);
				printError("CRC check : FAIL");
				printError(mess);
			}
			printSuccess("CRC check: Success");
		}
	} catch (std::exception &e) {}
	snprintf(mess, 256, "Read: 0x%08x checksum: 0x%04x\n", ucode, checksum);
	printError("CRC check : FAIL");
	printError(mess);
}

bool Gowin::enableCfg()
{
	send_command(CONFIG_ENABLE);
	return pollFlag(STATUS_SYSTEM_EDIT_MODE, STATUS_SYSTEM_EDIT_MODE);
}

bool Gowin::disableCfg()
{
	send_command(CONFIG_DISABLE);
	send_command(NOOP);
	return pollFlag(STATUS_SYSTEM_EDIT_MODE, 0);
}

uint32_t Gowin::idCode()
{
	return readReg32(READ_IDCODE);
}

uint32_t Gowin::readStatusReg()
{
	return readReg32(STATUS_REGISTER);
}

uint32_t Gowin::readUserCode()
{
	return readReg32(READ_USERCODE);
}

void Gowin::displayReadReg(uint32_t dev)
{
	printf("displayReadReg %08x\n", dev);
	if (dev & STATUS_CRC_ERROR)
		printf("\tCRC Error\n");
	if (dev & STATUS_BAD_COMMAND)
		printf("\tBad Command\n");
	if (dev & STATUS_ID_VERIFY_FAILED)
		printf("\tID Verify Failed\n");
	if (dev & STATUS_TIMEOUT)
		printf("\tTimeout\n");
	if (dev & STATUS_MEMORY_ERASE)
		printf("\tMemory Erase\n");
	if (dev & STATUS_PREAMBLE)
		printf("\tPreamble\n");
	if (dev & STATUS_SYSTEM_EDIT_MODE)
		printf("\tSystem Edit Mode\n");
	if (dev & STATUS_PRG_SPIFLASH_DIRECT)
		printf("\tProgram spi flash directly\n");
	if (dev & STATUS_NON_JTAG_CNF_ACTIVE)
		printf("\tNon-jtag is active\n");
	if (dev & STATUS_BYPASS)
		printf("\tBypass\n");
	if (dev & STATUS_GOWIN_VLD)
		printf("\tGowin VLD\n");
	if (dev & STATUS_DONE_FINAL)
		printf("\tDone Final\n");
	if (dev & STATUS_SECURITY_FINAL)
		printf("\tSecurity Final\n");
	if (dev & STATUS_READY)
		printf("\tReady\n");
	if (dev & STATUS_POR)
		printf("\tPOR\n");
	if (dev & STATUS_FLASH_LOCK)
		printf("\tFlash Lock\n");
}

bool Gowin::pollFlag(uint32_t mask, uint32_t value)
{
	uint32_t status;
	int timeout = 0;
	do {
		status = readStatusReg();
		if (_verbose)
			printf("pollFlag: %x (%x)\n", status, status & mask);
		if (timeout == 100000000){
			printError("timeout");
			return false;
		}
		timeout++;
	} while ((status & mask) != value);

	return true;
}

/* TN653 p. 17-21 */
bool Gowin::writeFLASH(uint32_t page, const uint8_t *data, int length)
{
	uint8_t xpage[256];
	length /= 8;
	for (int off = 0; off < length; off += 256) {
		int l = 256;
		if (length - off < l) {
			memset(xpage, 0xff, sizeof(xpage));
			l = length - off;
		}
		memcpy(xpage, &data[off], l);
		unsigned addr = off / 4 + page;
		if (addr) {
			sendClkUs(120);
		} else {
			// autoboot pattern
			static uint8_t pat[4] = {'G', 'W', '1', 'N'};
			memcpy(xpage, pat, 4);
		}

		send_command(CONFIG_ENABLE);
		send_command(EF_PROGRAM);

		unsigned w = htole32(addr);
		_jtag->shiftDR((uint8_t *)&w, nullptr, 32);
		sendClkUs(120);
		for (int y = 0; y < 64; ++y) {
			memcpy(&w, &xpage[y * 4], 4);
			w = bswap_32(w);
			_jtag->shiftDR((uint8_t *)&w, nullptr, 32);
			sendClkUs((is_gw1n1) ? 32 : 16);
		}
		sendClkUs((is_gw1n1) ? 2400 : 6);
		usleep(200);
	}
	send_command(CONFIG_DISABLE);
	send_command(NOOP);

	usleep(600*1000);
	send_command(CONFIG_DISABLE);
	send_command(NOOP);

	send_command(RELOAD);
	send_command(NOOP);
	if (_verbose)
		displayReadReg(readStatusReg());
	sleep(1);

	return true;
}

bool Gowin::connectJtagToMCU()
{
	send_command(SWITCH_TO_MCU_JTAG);
	return true;
}

/* TN653 p. 9 */
bool Gowin::writeSRAM(const uint8_t *data, int length)
{
	send_command(CONFIG_ENABLE); // config enable
	send_command(INIT_ADDR); // address initialize
	send_command(XFER_WRITE); // transfer configuration data
	_jtag->shiftDR(data, NULL, length);
	send_command(CONFIG_DISABLE); // config disable
	send_command(NOOP); // noop

	sleep(1);

	if (readStatusReg() & STATUS_DONE_FINAL)
		return true;
	else
		return false;
}

/* Erase SRAM:
 * TN653 p.14-17
 */
bool Gowin::eraseFLASH()
{
	printInfo("Erase Flash ", false);
	send_command(CONFIG_ENABLE);
	send_command(EFLASH_ERASE);
	uint32_t tx = 0;
	_jtag->shiftDR((uint8_t *)&tx, NULL, 32);
	if (is_gw1n1) {
		for (unsigned i = 0; i < 64; ++i)
			_jtag->shiftDR((uint8_t *)&tx, NULL, 32);
	}
	sendClkUs(150*1000);
	if (_verbose)
		displayReadReg(readStatusReg());
	send_command(CONFIG_DISABLE);
	send_command(NOOP);
	send_command(CONFIG_DISABLE);
	send_command(NOOP);
	send_command(RELOAD);
	send_command(NOOP);
	if (_verbose)
		displayReadReg(readStatusReg());
	usleep(500*1000);
	if (_verbose)
		displayReadReg(readStatusReg());
	printSuccess("Done");
	return true;
}

void Gowin::sendClkUs(unsigned us)
{
	//unsigned freq = _jtag->getClkFreq() / 1000000;
	//freq = (freq) ? freq : 1;
	unsigned clocks = us * 2; // at 2MHz 1us ~ 2 clocks
	_jtag->toggleClk(clocks);
}

/* Erase SRAM:
 * TN653 p.9-10, 14 and 31
 */
bool Gowin::eraseSRAM()
{
	printInfo("Erase SRAM ", false);
	send_command(CONFIG_ENABLE);
	send_command(ERASE_SRAM);
	send_command(NOOP);
	sendClkUs(32*1000);
	send_command(XFER_DONE);
	send_command(NOOP);

	//sendClkUs(500);
	send_command(CONFIG_DISABLE);
	send_command(NOOP);
	if (readStatusReg() & STATUS_MEMORY_ERASE) {
		printSuccess("Done");
		return true;
	} else {
		printError("FAIL");
		return false;
	}
}

/* SPI wrapper
 * extflash access may be done using specific mode or
 * boundary scan. But former is only available with mode=[11]
 * so use Bscan
 *
 * it's a bitbanging mode with:
 * Pins Name of SPI Flash | SCLK | CS  | DI  | DO  |
 * Bscan Chain[7:0]       | 7  6 | 5 4 | 3 2 | 1 0 |
 * (ctrl & data)          | 0    | 0   | 0   | 1   |
 * ctrl 0 -> out, 1 -> in
 * data 1 -> high, 0 -> low
 * but all byte must be bit reversal...
 */

int Gowin::spi_put(uint8_t cmd, const uint8_t *tx, uint8_t *rx, uint32_t len)
{
	uint8_t jrx[len+1], jtx[len+1];
	jtx[0] = cmd;
	if (tx)
		memcpy(jtx+1, tx, len);
	else
		memset(jtx+1, 0, len);
	int ret = spi_put(jtx, (rx)? jrx : NULL, len+1);
	if (rx)
		memcpy(rx, jrx+1, len);
	return ret;
}

int Gowin::spi_put(const uint8_t *tx, uint8_t *rx, uint32_t len)
{
	if (is_gw2a) {
		uint8_t jtx[len];
		uint8_t jrx[len];
		if (rx)
			len++;
		if (tx != NULL) {
			for (uint32_t i = 0; i < len; i++)
				jtx[i] = FsParser::reverseByte(tx[i]);
		}
		bool ret = send_command(0x16);
		if (!ret)
			return -1;
		_jtag->set_state(Jtag::EXIT2_DR);
		_jtag->shiftDR(jtx, (rx)? jrx:NULL, 8*len);
		if (rx) {
			for (uint32_t i=0; i < len; i++) {
				rx[i] = FsParser::reverseByte(jrx[i]>>1) |
					(jrx[i+1]&0x01);
			}
		}
	} else {
		/* set CS/SCK/DI low */
		uint8_t t = _spi_msk | _spi_do;
		t &= ~_spi_cs;
		_jtag->shiftDR(&t, NULL, 8);
		_jtag->toggleClk(6);
		_jtag->flush();

		/* send bit/bit full tx content (or set di to 0 when NULL) */
		for (uint32_t i = 0; i < len * 8; i++) {
			uint8_t r;
			t = _spi_msk | _spi_do;
			if (tx != NULL && tx[i>>3] & (1 << (7-(i&0x07))))
				t |= _spi_di;
			_jtag->shiftDR(&t, NULL, 8);
			_jtag->toggleClk(6);
			t |= _spi_sck;
			_jtag->shiftDR(&t, (rx) ? &r : NULL, 8);
			_jtag->toggleClk(6);
			_jtag->flush();
			/* if read reconstruct bytes */
			if (rx) {
				if (r & _spi_do)
					rx[i >> 3] |= 1 << (7-(i & 0x07));
				else
					rx[i >> 3] &= ~(1 << (7-(i & 0x07)));
			}
		}
		/* set CS and unset SCK (next xfer) */
		t &= ~_spi_sck;
		t |= _spi_cs;
		_jtag->shiftDR(&t, NULL, 8);
		_jtag->toggleClk(6);
		_jtag->flush();
	}
	return 0;
}

int Gowin::spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
		uint32_t timeout, bool verbose)
{
	uint8_t tmp;
	uint32_t count = 0;

	if (is_gw2a) {
		uint8_t rx[3];
		uint8_t tx[3];
		tx[0] = FsParser::reverseByte(cmd);

		do {
			bool ret = send_command(0x16);
			if (!ret)
				return -1;
			_jtag->set_state(Jtag::EXIT2_DR);
			_jtag->shiftDR(tx, rx, 8 * 3);

			tmp = (FsParser::reverseByte(rx[1]>>1)) | (0x01 & rx[2]);
			count ++;
			if (count == timeout) {
				printf("timeout: %x %x %x\n", tmp, rx[0], rx[1]);
				break;
			}
			if (verbose) {
				printf("%x %x %x %u\n", tmp, mask, cond, count);
			}
		} while ((tmp & mask) != cond);
	} else {
		uint8_t t;

		/* set CS/SCK/DI low */
		t = _spi_msk | _spi_do;
		_jtag->shiftDR(&t, NULL, 8);
		_jtag->toggleClk(6);

		/* send command bit/bit */
		for (int i = 0; i < 8; i++) {
			t = _spi_msk | _spi_do;
			if ((cmd & (1 << (7-i))) != 0)
				t |= _spi_di;
			_jtag->shiftDR(&t, NULL, 8);
			_jtag->toggleClk(6);
			t |= _spi_sck;
			_jtag->shiftDR(&t, NULL, 8);
			_jtag->toggleClk(6);
			_jtag->flush();
		}

		t = _spi_msk | _spi_do;
		do {
			tmp = 0;
			/* read status register bit/bit with di == 0 */
			for (int i = 0; i < 8; i++) {
				uint8_t r;
				t &= ~_spi_sck;
				_jtag->shiftDR(&t, NULL, 8);
				_jtag->toggleClk(6);
				t |= _spi_sck;
				_jtag->shiftDR(&t, &r, 8);
				_jtag->toggleClk(6);
				_jtag->flush();
				if ((r & _spi_do) != 0)
					tmp |= 1 << (7-i);
			}

			count++;
			if (count == timeout) {
				printf("timeout: %x\n", tmp);
				break;
			}
			if (verbose)
				printf("%x %x %x %u\n", tmp, mask, cond, count);
		} while ((tmp & mask) != cond);

		/* set CS & unset SCK (next xfer) */
		t &= ~_spi_sck;
		t |= _spi_cs;
		_jtag->shiftDR(&t, NULL, 8);
		_jtag->toggleClk(6);
		_jtag->flush();
	}

	if (count == timeout) {
		printf("%02x\n", tmp);
		std::cout << "wait: Error" << std::endl;
		return -ETIME;
	}

	return 0;
}
