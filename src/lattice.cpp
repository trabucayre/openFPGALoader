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
#include "lattice.hpp"
#include "latticeBitParser.hpp"
#include "mcsParser.hpp"
#include "progressBar.hpp"
#include "rawParser.hpp"
#include "display.hpp"
#include "part.hpp"
#include "spiFlash.hpp"

using namespace std;

#define ISC_ENABLE					0xC6		/* ISC_ENABLE - Offline Mode */
#  define ISC_ENABLE_FLASH_MODE		(1 << 3)
#  define ISC_ENABLE_SRAM_MODE		(0 << 3)
#define ISC_ENABLE_TRANSPARANT		0x74		/* This command is used to put the device in transparent mode */
#define ISC_DISABLE					0x26		/* ISC_DISABLE */
#define READ_DEVICE_ID_CODE			0xE0		/* IDCODE_PUB */
#define FLASH_ERASE					0x0E		/* ISC_ERASE */
/* Flash areas as defined for Lattice MachXO3L/LF */
#  define FLASH_ERASE_UFM			(1<<3)
#  define FLASH_ERASE_CFG   		(1<<2)
#  define FLASH_ERASE_FEATURE		(1<<1)
#  define FLASH_ERASE_SRAM			(1<<0)
#  define FLASH_ERASE_ALL       	0x0F
/* Flash areas as defined for Lattice MachXO3D, used with command: ISC_ERASE */
#  define FLASH_SEC_CFG0			(1<<8)
#  define FLASH_SEC_CFG1			(1<<9)
#  define FLASH_SEC_UFM0			(1<<10)
#  define FLASH_SEC_UFM1			(1<<11)
#  define FLASH_SEC_UFM2			(1<<12)
#  define FLASH_SEC_UFM3			(1<<13)
// #  define FLASH_SEC_CSEC			(1<<14)		/* not defined in later documentation */
// #  define FLASH_SEC_USEC			(1<<15)		/* not defined in later documentation */
#  define FLASH_SEC_PKEY			(1<<16)
#  define FLASH_SEC_AKEY			(1<<17)
#  define FLASH_SEC_FEA 			(1<<18)
#  define FLASH_SEC_ALL				0x7FF
/* This uses the same defines as above (for ISC_ERASE)
 * The Lattice Standard Doc for MachXO3D has incorrect list of operands for this
 * command.
 * This is document is more correct:
 *    fpga-tn-02119-1-1-using-hardened-control-functions-machxo3d-reference.pdf
 */
#define RESET_CFG_ADDR					0x46		/* LSC_INIT_ADDRESS */
/* Set the Page Address pointer to the Flash page specified */
#define LSC_WRITE_ADDRESS				0xB4
#  define FLASH_SET_ADDR_CFG0 			0x00
#  define FLASH_SET_ADDR_UFM0			0x01
#  define FLASH_SET_ADDR_FEA			0x03
#  define FLASH_SET_ADDR_CFG1			0x04
#  define FLASH_SET_ADDR_UFM1			0x05
#  define FLASH_SET_ADDR_PKEY			0x06
//#  define FLASH_SET_ADDR_CSEC			0x07		/* not defined in later documentation */
#  define FLASH_SET_ADDR_UFM2			0x08
#  define FLASH_SET_ADDR_UFM3 			0x09
#  define FLASH_SET_ADDR_AKEY			0x0A
//#  define FLASH_SET_ADDR_USEC			0x0B		/* not defined in later documentation */
/* Set the Page Address Pointer to the beginning of the UFM sectors.
 * It appears that this function is required when setting address to UFM sectors
 * The LSC_INIT_ADDRESS doesn't work when the sector is set to UFMx ... */
#define LSC_INIT_ADDR_UFM				0x47
#  define FLASH_UFM_ADDR_UFM0			(1<<10)
#  define FLASH_UFM_ADDR_UFM1			(1<<11)
#  define FLASH_UFM_ADDR_UFM2			(1<<12)
#  define FLASH_UFM_ADDR_UFM3			(1<<13)
#define PROG_CFG_FLASH					0x70		/* LSC_PROG_INCR_NV */
#define READ_BUSY_FLAG					0xF0		/* LSC_CHECK_BUSY */
#  define CHECK_BUSY_FLAG_BUSY          (1 << 7)
/* The busy flag defines bit 7 as busy, but busy flags returns 1 for busy (bit 0). */
#define REG_CFG_FLASH					0x73		/* LSC_READ_INCR_NV */
#define PROG_FEATURE_ROW				0xE4		/* LSC_PROG_FEATURE */
#define READ_FEATURE_ROW        		0xE7		/* LSC_READ_FEATURE */
/* See feaParser.hpp for FEATURE definitions */
#define PROG_FEABITS					0xF8		/* LSC_PROG_FEABITS */
#define READ_FEABITS            		0xFB		/* LSC_READ_FEABITS */
/* See feaParser.hpp for FEAbit definitions */
#define PROG_DONE						0x5E		/* ISC_PROGRAM_DONE - This command is used to program the done bit */
#define REFRESH							0x79		/* LSC_REFRESH */
#define READ_STATUS_REGISTER    		0x3C		/* LSC_READ_STATUS */
#  define REG_STATUS_DONE				(1 << 8)	/* Flash or SRAM Done Flag (ISC_EN=0 -> 1 Successful Flash to SRAM transfer, ISC_EN=1 -> 1 Programmed) */
#  define REG_STATUS_ISC_EN				(1 << 9)	/* Enable Configuration Interface (1=Enable, 0=Disable) */
#  define REG_STATUS_BUSY				(1 << 12)	/* Busy Flag (1 = Busy) */
#  define REG_STATUS_FAIL				(1 << 13)	/* Fail Flag (1 = Operation failed) */
#  define REG_STATUS_PP_CFG				(1 << 15)	/* Password Protection All Enabled for CFG0 and CFG1 flash sectors 0=Disabled (Default), 1=Enabled */
#  define REG_STATUS_PP_FSK				(1 << 16)	/* Password Protection Enabled for Feature and Security Key flash sectors 0=Disabled (Default), 1=Enabled */
#  define REG_STATUS_PP_UFM				(1 << 17)	/* Password Protection enabled for all UFM flash sectors 0=Disabled (Default), 1=Enabled */
#  define REG_STATUS_AUTH_DONE			(1 << 18)	/* Authentication done */
#  define REG_STATUS_PRI_BOOT_FAIL		(1 << 21)	/* Primary boot failure (1= Fail) even though secondary boot successful */
#  define REG_STATUS_CNF_CHK_MASK		(0x0f << 23)	/* Configuration Status Check */
#  define REG_STATUS_MACHXO3D_CNF_CHK_MASK	(0x0f << 22)	/* Configuration Status Check */
#  define REG_STATUS_EXEC_ERR			(1 << 26)	/*** NOT specified for MachXO3D ***/
#  define REG_STATUS_DEV_VERIFIED		(1 << 27)	/* I=0 Device verified correct, I=1 Device failed to verify */
#define READ_STATUS_REGISTER_1			0x3D        	/* LSC_READ_STATUS_1 */
#  define REG_STATUS1_FLASH_SEL			(0x0f << 0)	/* Flash sector selection 1=CFG0, 2=CFG1, 4=FEATURE, 5=Pub Key, 6=AES Key, 8=UFM0, 10=UFM1, 11=UFM2, 12=UFM3 */
#  define REG_STATUS1_ERASE_DISABLE		(1 << 4)	/* Erase operation is prohibited (1 = Erase disable) */
#  define REG_STATUS1_PROG_DISABLE		(1 << 5)	/* Program operation is prohibited (1 = Programing disable) */
#  define REG_STATUS1_READ_DISABLE		(1 << 6)	/* Read operation is prohibited (1 = Read disable) */
#  define REG_STATUS1_HS_LOCK_SEL		(1 << 7)	/* Hard/Soft Lock Selection (1 = Hard Lock, 0 = Soft Lock) */
#  define REG_STATUS1_AUTH_MODE			(0x03 << 8)	/* Authentication mode: 0x: No Authentication, 10: HMAC Authentication, 11: ECDSA Signature Verification */
#  define REG_STATUS1_AUTH_DONE_CFG0		(1 << 10)	/* Authentication done for CFG0 (1 = Authentication successful) */
#  define REG_STATUS1_AUTH_DONE_CFG1		(1 << 11)	/* Authentication done for CFG1 (1 = Authentication successful) */
#  define REG_STATUS1_FLASH_DONE_CFG0		(1 << 12)	/* Flash done bit is programmed of CFG0 (1= Programmed, 0=Unprogrammed) */
#  define REG_STATUS1_FLASH_DONE_CFG1		(1 << 13)	/* Flash done bit is programmed of CFG1 (1= Programmed, 0=Unprogrammed) */
#  define REG_STATUS1_SEC_PLUS_EN_CFG0		(1 << 14)	/* Security Plus enabled for CFG0 (1 = Enabled, 0 = Disabled) */
#  define REG_STATUS1_SEC_PLUS_EN_CFG1		(1 << 15)	/* Security Plus enabled for CFG1 (1 = Enabled, 0 = Disabled) */
#  define REG_STATUS1_BITSTR_VERSION		(1 << 16)	/* Bitstream version: 1 = Bitstream in CFG0 is latter (newer) than CFG1, 0 = Bitstream in CFG1 is latter (newer) than CFG0 */
#  define REG_STATUS1_BOOT_SEQ_SEL		(0x03 << 17)	/* Boot Sequence selection (used along with Master SPI Port Persistence bit) */
#  define REG_STATUS1_MSPI_PERS			(1 << 20)	/* Master SPI Port Persistence 0=Disabled (Default), 1=Enabled */
#  define REG_STATUS1_I2C_DG_FILTER		(1 << 21)	/* I2C deglitch filter enable for Primary I2C Port 0=Disabled (Default), 1=Enabled */
#  define REG_STATUS1_I2C_DG_RANGE		(1 << 22)	/* I2C deglitch filter range selection on primary I2C port 0= 8 to 25 ns range (Default), 1= 16 to 50 ns range */
#define PROG_ECDSA_PUBKEY0				0x59		/* This command is used to program the first 128 bits of the ECDSA Public Key. */
#define READ_ECDSA_PUBKEY0				0x5A		/* This command is used to read the first 128 bits of the ECDSA Public Key. */
#define PROG_ECDSA_PUBKEY1				0x5B		/* This command is used to program the second 128 bits of the ECDSA Public Key. */
#define READ_ECDSA_PUBKEY1				0x5C		/* This command is used to read the second 128 bits of the ECDSA Public Key. */
#define PROG_ECDSA_PUBKEY2				0x61		/* This command is used to program the third 128 bits of the ECDSA Public Key. */
#define READ_ECDSA_PUBKEY2				0x62		/* This command is used to read the third 128 bits of the ECDSA Public Key. */
#define PROG_ECDSA_PUBKEY3				0x63		/* This command is used to program the fourth 128 bits of the ECDSA Public Key. */
#define READ_ECDSA_PUBKEY3				0x64		/* This command is used to read the fourth 128 bits of the ECDSA Public Key. */
#define ISC_NOOP						0xff		/* This command is no operation command (NOOP) or null operation. */

#define PUBKEY_LENGTH_BYTES				64			/* length of the public key (MachXO3D) in bytes */

Lattice::Lattice(Jtag *jtag, const string filename, const string &file_type,
	Device::prog_type_t prg_type, std::string flash_sector, bool verify, int8_t verbose):
		Device(jtag, filename, file_type, verify, verbose),
		SPIInterface(filename, verbose, 0, verify),
		_fpga_family(UNKNOWN_FAMILY), _flash_sector(LATTICE_FLASH_UNDEFINED)
{
	if (prg_type == Device::RD_FLASH) {
		_mode = READ_MODE;
	} else if (!_file_extension.empty()) {
		if (_file_extension == "jed" || _file_extension == "mcs" || _file_extension == "fea" || _file_extension == "pub") {
			_mode = Device::FLASH_MODE;
		} else if (_file_extension == "bit" || _file_extension == "bin") {
			if (prg_type == Device::WR_FLASH)
				_mode = Device::FLASH_MODE;
			else
				_mode = Device::MEM_MODE;
		} else { /* unknown type: */
			if (prg_type == Device::WR_FLASH) /* to flash: OK */
				_mode = Device::FLASH_MODE;
			else /* otherwise: KO */
				throw std::runtime_error("incompatible file format");
		}
	}
	/* check device family */
	uint32_t idcode = _jtag->get_target_device_id();
	string family = fpga_list[idcode].family;
	if (family == "MachXO2") {
		_fpga_family = MACHXO2_FAMILY;
	} else if (family == "MachXO3LF") {
		_fpga_family = MACHXO3_FAMILY;
	} else if (family == "MachXO3D") {
		_fpga_family = MACHXO3D_FAMILY;

		if (flash_sector == "CFG0") {
			_flash_sector = LATTICE_FLASH_CFG0;
			printInfo("Flash Sector: CFG0", true);
		} else if (flash_sector == "CFG1") {
			_flash_sector = LATTICE_FLASH_CFG1;
			printInfo("Flash Sector: CFG1", true);
		} else if (flash_sector == "UFM0") {
			_flash_sector = LATTICE_FLASH_UFM0;
			printInfo("Flash Sector: UFM0", true);
		} else if (flash_sector == "UFM1") {
			_flash_sector = LATTICE_FLASH_UFM1;
			printInfo("Flash Sector: UFM1", true);
		} else if (flash_sector == "UFM2") {
			_flash_sector = LATTICE_FLASH_UFM2;
			printInfo("Flash Sector: UFM2", true);
		} else if (flash_sector == "UFM3") {
			_flash_sector = LATTICE_FLASH_UFM3;
			printInfo("Flash Sector: UFM3", true);
		} else if (flash_sector == "FEA") {
			_flash_sector = LATTICE_FLASH_FEA;
			printInfo("Flash Sector: FEA", true);
		} else if (flash_sector == "PKEY") {
			_flash_sector = LATTICE_FLASH_PKEY;
			printInfo("Flash Sector: PKEY", true);
		} else {
			printError("Unknown flash sector");
			throw std::exception();
		}
	} else if (family == "ECP5") {
		_fpga_family = ECP5_FAMILY;
	} else if (family == "CrosslinkNX") {
		_fpga_family = NEXUS_FAMILY;
	} else if (family == "CertusNX") {
		_fpga_family = NEXUS_FAMILY;
	} else {
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

	printInfo("Open file: ", false);
	printSuccess("DONE");

	err = _bit.parse();

	printInfo("Parse file: ", false);
	if (err == EXIT_FAILURE) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}

	if (_verbose)
		_bit.displayHeader();

	/* read ID Code 0xE0 and compare to bitstream */
	uint32_t bit_idcode = std::stoul(_bit.getHeaderVal("idcode").c_str(), NULL, 16);
	uint32_t idcode = idCode();
	if (idcode != bit_idcode) {
		char mess[256];
		sprintf(mess, "mismatch between target's idcode and bitstream idcode\n"
			"\tbitstream has 0x%08X hardware requires 0x%08x", bit_idcode, idcode);
		printError(mess);
		return false;
	}

	if (_verbose) {
		printf("IDCode : %x\n", idcode);
		displayReadReg(readStatusReg());
	}

	/* The command code 0x1C is not listed in the manual? */
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
	int next_state = Jtag::SHIFT_DR;

	ProgressBar progress("Loading", length, 50, _quiet);

	for (int i = 0; i < length; i += size) {
		progress.display(i);

		if (length < i + size) {
			size = length-i;
			next_state = Jtag::RUN_TEST_IDLE;
		}

		for (int ii = 0; ii < size; ii++)
			tmp[ii] = ConfigBitstreamParser::reverseByte(data[i+ii]);

		_jtag->shiftDR(tmp, NULL, size*8, next_state);
	}

	uint32_t status_mask;
	if (_fpga_family == MACHXO3D_FAMILY)
		status_mask = REG_STATUS_MACHXO3D_CNF_CHK_MASK;
	else
		status_mask = REG_STATUS_CNF_CHK_MASK;

	if (checkStatus(0, status_mask)) {
		progress.done();
	} else {
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

bool Lattice::program_intFlash(JedParser& _jed)
{
	uint64_t featuresRow;
	uint16_t feabits;
	uint8_t eraseMode = 0;
	vector<string> ufm_data, cfg_data, ebr_data;

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
	if (_verify) {
		if (Verify(cfg_data) == false)
			return false;
	}

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

bool Lattice::prepare_flash_access()
{
	/* clear SRAM before SPI access */
	if (!clearSRAM())
		return false;
	/*IR = 0h3A, DR=0hFE,0h68. Enter RUNTESTIDLE.
	 * thank @GregDavill
	 * https://twitter.com/GregDavill/status/1251786406441086977
	 */
	_jtag->shiftIR(0x3A, 8, Jtag::EXIT1_IR);
	uint8_t tmp[2] = {0xFE, 0x68};
	_jtag->shiftDR(tmp, NULL, 16);
	return true;
}

bool Lattice::post_flash_access()
{
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

bool Lattice::clearSRAM()
{
	uint32_t erase_op;

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

	if (_fpga_family == MACHXO3D_FAMILY)
		erase_op = 0x0;
	else
		erase_op = FLASH_ERASE_SRAM;

	/* ISC ERASE */
	printInfo("SRAM erase: ", false);
	if (flashErase(erase_op) == false) {
		printError("FAIL");
		displayReadReg(readStatusReg());
		return false;
	} else {
		printSuccess("DONE");
	}

	return DisableISC();
}

bool Lattice::program_extFlash(unsigned int offset, bool unprotect_flash)
{
	int ret;
	ConfigBitstreamParser *_bit;

	printInfo("Open file ", false);
	try {
		if (_file_extension == "mcs")
			_bit = new McsParser(_filename, true, _verbose);
		else if (_file_extension == "bit")
			_bit = new LatticeBitParser(_filename, _verbose);
		else
			_bit = new RawParser(_filename, false);
		printSuccess("DONE");
	} catch (std::exception &e) {
		printError("FAIL");
		printError(e.what());
		return false;
	}


	printInfo("Parse file ", false);
	if (_bit->parse() == EXIT_FAILURE) {
		printError("FAIL");
		delete _bit;
		return false;
	} else {
		printSuccess("DONE");
	}

	if (_verbose)
		_bit->displayHeader();

	if (_file_extension == "bit") {
		uint32_t bit_idcode = std::stoul(_bit->getHeaderVal("idcode").c_str(), NULL, 16);
		uint32_t idcode = idCode();
		if (idcode != bit_idcode) {
			char mess[256];
			sprintf(mess, "mismatch between target's idcode and bitstream idcode\n"
				"\tbitstream has 0x%08X hardware requires 0x%08x", bit_idcode, idcode);
			printError(mess);
			delete _bit;
			return false;
		}
	}

	ret = SPIInterface::write(offset, _bit->getData(), _bit->getLength() / 8,
			unprotect_flash);

	delete _bit;
	return ret;
}

bool Lattice::program_flash(unsigned int offset, bool unprotect_flash)
{
	/* read ID Code 0xE0 */
	if (_verbose) {
		printf("IDCode : %x\n", idCode());
		displayReadReg(readStatusReg());
	}

	bool retval;
	if (_file_extension == "jed") {
		bool err;

		JedParser _jed(_filename, _verbose);
		printInfo("Open file ", false);
		printSuccess("DONE");

		err = _jed.parse();
		printInfo("Parse file ", false);
		if (err == EXIT_FAILURE) {
			printError("FAIL");
			return false;
		} else {
			printSuccess("DONE");
		}
		if (_verbose)
			_jed.displayHeader();

		/* clear current SRAM content */
		clearSRAM();

		if (_fpga_family == MACHXO3D_FAMILY)
			retval = program_intFlash_MachXO3D(_jed);
		else
			retval = program_intFlash(_jed);
		return post_flash_access() && retval;
	} else if (_file_extension == "fea") {
		/* clear current SRAM content */
		clearSRAM();
		retval = program_fea_MachXO3D();
		return post_flash_access() && retval;
	} else if (_file_extension == "pub") {
		/* clear current SRAM content */
		clearSRAM();
		retval = program_pubkey_MachXO3D();
	} else {
		return program_extFlash(offset, unprotect_flash);
	}

	return true;
}

void Lattice::program(unsigned int offset, bool unprotect_flash)
{
	bool retval = true;
	if (_mode == FLASH_MODE)
		retval = program_flash(offset, unprotect_flash);
	else if (_mode == MEM_MODE)
		retval = program_mem();
	if (!retval)
		throw std::exception();
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
	wr_rd(READ_STATUS_REGISTER, tx, 4, rx, 4);
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
	if (tx) {
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
		wr_rd(READ_BUSY_FLAG, NULL, 0, &rx, 1);
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

bool Lattice::flashErase(uint32_t  mask)
{
	if (_fpga_family == MACHXO3D_FAMILY) {
		uint8_t tx[2] = {
			(uint8_t)((mask >> 8) & 0xff),
			(uint8_t)((mask >> 16) & 0xff)
		};
		wr_rd(FLASH_ERASE, tx, 2, NULL, 0);
	} else {
		uint8_t tx[1] = {(uint8_t)(mask & 0xff)};
		wr_rd(FLASH_ERASE, tx, 1, NULL, 0);
	}
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
	ProgressBar progress("Writing " + name, data.size(), 50, _quiet);
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

bool Lattice::Verify(std::vector<std::string> data, bool unlock, uint32_t flash_area)
{
	uint8_t tx_buf[16], rx_buf[16];
	if (unlock)
		EnableISC(0x08);

	if (_fpga_family == MACHXO3D_FAMILY) {
		uint8_t tx[2] = { (
			uint8_t)((flash_area >> 8) & 0xff),
			(uint8_t)((flash_area >> 16) & 0xff)
		};
		wr_rd(RESET_CFG_ADDR, tx, 2, NULL, 0);
	} else {
		wr_rd(RESET_CFG_ADDR, NULL, 0, NULL, 0);
	}

	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);

	tx_buf[0] = REG_CFG_FLASH;
	_jtag->shiftIR(tx_buf, NULL, 8, Jtag::PAUSE_IR);

	memset(tx_buf, 0, 16);
	bool failure = false;
	ProgressBar progress("Verifying", data.size(), 50, _quiet);
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

	if (failure)
		progress.fail();
	else
		progress.done();

	return !failure;
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

	if (tx) {
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

	if (tx) {
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
	_jtag->shiftDR(&tx, NULL, 8, Jtag::SHIFT_DR);

	do {
		_jtag->shiftDR(dummy, &rx, 8, Jtag::SHIFT_DR);
		tmp = (LatticeBitParser::reverseByte(rx));
		count++;
		if (count == timeout){
			printf("timeout: %x %x %u\n", tmp, rx, count);
			break;
		}

		if (verbose) {
			printf("%x %x %x %u\n", tmp, mask, cond, count);
		}
	} while ((tmp & mask) != cond);
	_jtag->shiftDR(dummy, &rx, 8, Jtag::RUN_TEST_IDLE);
	if (count == timeout) {
		printf("%x\n", tmp);
		std::cout << "wait: Error" << std::endl;
		return -ETIME;
	}
	return 0;
}


/*************************** MODS FOR MacXO3D *********************************/

bool Lattice::programFeatureRow_MachXO3D(uint8_t* feature_row)
{
	uint8_t tx[16] = { 0 };
	uint8_t rx[15] = { 0 };

	for (int i = 0; i < 12; i++)
		tx[i] = feature_row[i];

	if (_verbose) {
		printf("\tProgramming feature row: [0x");
		for (int i = 11; i >= 0; i--) {
			printf("%02x", feature_row[i]);
		}
		printf("]\n");
	}

	wr_rd(PROG_FEATURE_ROW, tx, 16, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	wr_rd(0xff, NULL, 0, NULL, 0);
	if (!pollBusyFlag())
		return false;

	if (_verbose || _verify) {
		wr_rd(READ_FEATURE_ROW, NULL, 0, rx, 15);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(2);
	}

	if (_verbose) {
		printf("\tReadback Feature Row: [0x");
		for(int i = 11; i >= 0; i--) {
			printf("%02x", rx[i]);
		}
		printf("]\n");
	}

	if (_verify) {
		for(int i = 0; i < 15; i++) {
			if (feature_row[i] != rx[i]) {
				printf("\tVerify Failed...\n");
				return false;
			}
		}
	}

	return true;
}

bool Lattice::programFeabits_MachXO3D(uint32_t feabits)
{
	uint8_t tx[4] = { 0 };
	uint8_t rx[5] = { 0 };

	memset(tx, 0, sizeof(tx));
	for (int i = 0; i < 4; i++) {
		tx[i] = (feabits >> (8*i)) & 0xff;
	}

	if (_verbose) {
		printf("\tProgramming FEAbits: [0x");
		for (int i = 3; i >= 0; i--) {
			printf("%02x", tx[i]);
		}
		printf("]\n");
	}

	wr_rd(PROG_FEABITS, tx, 4, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	wr_rd(0xff, NULL, 0, NULL, 0);
	if (!pollBusyFlag())
		return false;

	if (_verbose || _verify) {
		wr_rd(READ_FEABITS, NULL, 0, rx, 5);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(2);
	}

	if (_verbose) {
		printf("\tReadback Feabits: [0x");
		for(int i = 4; i >= 0; i--) {
			printf("%02x", rx[i]);
		}
		printf("]\n");
	}

	if (_verify) {
		for(int i = 0; i < 4; i++) {
			if (((feabits >> (8*i)) & 0xff) != rx[i]) {
				printf("\tVerify Failed...\n");
				return false;
			}
		}
	}

	return true;
}

bool Lattice::programPubKey_MachXO3D(uint8_t* pubkey)
{
	uint8_t rxkey[PUBKEY_LENGTH_BYTES] = { 0 };
	uint8_t tx[16];
	int i;

	if (_verbose) {
		printf("\tProgramming ECDSA PubKey: [");
		for (i = 0; i < PUBKEY_LENGTH_BYTES; i++) {
			printf("%02x", pubkey[i]);
		}
		printf("]\n");
	}

	for(i = 0; i < 16; i++) {
		tx[i] = pubkey[63 - i];
	}
	wr_rd(PROG_ECDSA_PUBKEY0, tx, 16, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	wr_rd(0xff, NULL, 0, NULL, 0);
	if (!pollBusyFlag())
		return false;

	for(i = 0; i < 16; i++) {
		tx[i] = pubkey[47 - i];
	}
	wr_rd(PROG_ECDSA_PUBKEY1, tx, 16, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	wr_rd(0xff, NULL, 0, NULL, 0);
	if (!pollBusyFlag())
		return false;

	for(i = 0; i < 16; i++) {
		tx[i] = pubkey[31 - i];
	}
	wr_rd(PROG_ECDSA_PUBKEY2, tx, 16, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	wr_rd(0xff, NULL, 0, NULL, 0);
	if (!pollBusyFlag())
		return false;

	for(i = 0; i < 16; i++) {
		tx[i] = pubkey[15 - i];
	}
	wr_rd(PROG_ECDSA_PUBKEY3, tx, 16, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	wr_rd(0xff, NULL, 0, NULL, 0);
	if (!pollBusyFlag())
		return false;

	if (_verbose || _verify) {
		/* read the current feature row */
		wr_rd(READ_ECDSA_PUBKEY0, NULL, 0, rxkey, 16);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(2);

		wr_rd(READ_ECDSA_PUBKEY1, NULL, 0, rxkey + 16, 16);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(2);

		wr_rd(READ_ECDSA_PUBKEY2, NULL, 0, rxkey + 32, 16);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(2);

		wr_rd(READ_ECDSA_PUBKEY3, NULL, 0, rxkey + 48, 16);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(2);
	}

	if (_verbose) {
		printf("Readback PubKey: [");
		for (i=PUBKEY_LENGTH_BYTES-1; i >= 0; i--) {
			printf("%02x", rxkey[i]);
			if (i && (i%16 == 0)) printf(" ");
		}
		printf("]\n");
	}

	if (_verify) {
		for (int i = 0; i < PUBKEY_LENGTH_BYTES; i++) {
			if (pubkey[i] != rxkey[PUBKEY_LENGTH_BYTES - i - 1]) {
				printf("\tVerify Failed...\n");
				return false;
			}
		}
	}

	return true;
}

bool Lattice::program_fea_MachXO3D()
{
	bool err;
	uint8_t rx[15] = { 0 };
	uint8_t tx[16] = { 0 };
	bool same = true;

	FeaParser _fea(_filename, _verbose);
	printInfo("Open file: ", false);
	printSuccess("DONE");

	err = _fea.parse();
	printInfo("Parse file: ", false);
	if (err == EXIT_FAILURE) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}
	if (_verbose)
		_fea.displayHeader();

	/* bypass */
	wr_rd(ISC_NOOP, NULL, 0, NULL, 0);
	/* ISC Enable 0xC6 with operand of 0x08 (Enable Offline mode) */
	printInfo("Enable configuration: ", false);
	if (!EnableISC(0x08)) {
		printError("FAIL");
		displayReadReg(readStatusReg());
		return false;
	} else {
		printSuccess("DONE");
	}

	/* read the current feature row */
	wr_rd(READ_FEATURE_ROW, NULL, 0, rx, 12);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	if (_verbose) {
		printf("Read Feature Row: [0x");
		for(int i = 11; i >= 0; i--) {
			printf("%02x", rx[i]);
		}
		printf("]\n");
	}

	uint8_t* feature_row = (uint8_t*)_fea.featuresRow();
	for (int i = 0; i < 12; i++) {
		if (feature_row[i] != rx[i])
			same = false;
	}

	/* read the current FEAbits */
	uint32_t feabits = _fea.feabits();
	wr_rd(READ_FEABITS, NULL, 0, rx, 6);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	if (_verbose) {
		printf("Read Feabits: [0x");
		for(int i = 4; i >= 0; i--) {
			printf("%02x", rx[i]);
		}
		printf("]\n");
	}

	for (int i = 0; i < 4; i++) {
		if ((feabits >> (i * 8) & 0xff) != rx[i])
			same = false;
	}

	printf("Feature Row / Feabits Compare: %s\n", same ? "Same" : "Different");
	if (same == false) {
		/* LSC_INIT_ADDRESS */
		tx[0] = (uint8_t)((FLASH_SEC_FEA >> 8) & 0xff);
		tx[1] = (uint8_t)((FLASH_SEC_FEA >> 16) & 0xff);
		if (_verbose)
			printf("Selected address (I): 0x%x 0x%x\n", tx[0], tx[1]);
		wr_rd(RESET_CFG_ADDR, tx, 2, NULL, 0);

		/* ISC ERASE */
		printInfo("Flash erase: ", false);
		if (flashErase(FLASH_SEC_FEA) == false) {
			printError("FAIL");
			return false;
		} else {
			printSuccess("DONE");
		}

		/* FEATURE Row */
		printInfo("Program Feature Row: ", true);
		if (!programFeatureRow_MachXO3D(feature_row)) {
			printError("FAIL");
			return false;
		} else {
			printSuccess("DONE");
		}

		/* FEAbits */
		printInfo("Program FEAbits: ", true);
		if (!programFeabits_MachXO3D(feabits)) {
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
	wr_rd(ISC_NOOP, NULL, 0, NULL, 0);

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

bool Lattice::program_intFlash_MachXO3D(JedParser& _jed)
{
	uint32_t erase_op = 0, prog_op = 0;
	vector<string> data;
	int offset, fuse_count;

	/* bypass */
	wr_rd(ISC_NOOP, NULL, 0, NULL, 0);
	/* ISC Enable 0xC6 with operand of 0x08 (Enable Offline mode) */
	printInfo("Enable configuration: ", false);
	if (!EnableISC(0x08)) {
		printError("FAIL");
		displayReadReg(readStatusReg());
		return false;
	} else {
		printSuccess("DONE");
	}

	/* this is the size of an CFGx+UFMx area in bits (hence the / 128) */
	fuse_count = _jed.get_fuse_count() / 128;

	for (size_t i = 0; i < _jed.nb_section(); i++) {
		std::string area_name;

		data = _jed.data_for_section(i);
		if (data.size() < 1) {
			/* if no data, nothing to do */
			continue;
		}
		string note = _jed.noteForSection(i);
		offset = _jed.offset_for_section(i) / 128;

		erase_op = 0;
		prog_op = 0;

		/* if the offset > total fuse count, then this file must be configured
		 * for the 2nd config sector (CFG1), so adjust offset */
		while (offset >= fuse_count) {
			offset -= fuse_count;
		}
		/* If the offset is greater than the size of the config area then we're
		 * programming the UFM area (UFM 0/1) */
		if (offset >= 12542) {
			offset -= 12542;
		}

		if (note == "END CONFIG DATA") {
			printf("Processing PADDING data (offset: %d (0x%x))\n", offset, offset);
			/* PADDING - this should be padding to CFGx area that we're currently
			 * programming therefore the flash area will already have been erased...
			 * NOTE: We have to write this data if we're using bitstream authentication
			 * - even if it's all zeros.
			 */
			erase_op = 0;
			/* We need to use the 'LSC_WRITE_ADDRESS' command to set not only the
			 * flash sector but also the page number. */
			if (_flash_sector == LATTICE_FLASH_CFG0) {
				prog_op = (FLASH_SET_ADDR_CFG0 << 14) | (offset);
				area_name = "Padding (CFG0)";
			} else if (_flash_sector == LATTICE_FLASH_CFG1) {
				prog_op = (FLASH_SET_ADDR_CFG1 << 14) | (offset);
				area_name = "Padding (CFG1)";
			}

			/* offset should not be zero */
			if (offset == 0) {
				printf("Warning: offset (%d) is for programming PADDING\n", offset);
			}
		} else if (note == "EBR_INIT DATA") {
			printf("Processing EBR_INIT data (offset: %d (0x%x))\n", offset, offset);
			/* EBR - Embedded Block RAM initialisation data */
			if (offset == 0) {
				if (_flash_sector == LATTICE_FLASH_CFG0) {
					erase_op = FLASH_SEC_UFM0;
					prog_op = FLASH_UFM_ADDR_UFM0;
					area_name = "EBR (UFM0)";
				} else if (_flash_sector == LATTICE_FLASH_CFG1) {
					erase_op = FLASH_SEC_UFM1;
					prog_op = FLASH_UFM_ADDR_UFM1;
					area_name = "EBR (UFM1)";
				}
			} else {
				/* NOT SUPPORTING NON-ZERO OFFSET WRITES...*/
				continue;
			}
		} else if (note.compare(0, 16, "USER MEMORY DATA") == 0) {
			printf("Processing UFM data (offset: %d (0x%x))\n", offset, offset);
			if ((_flash_sector == LATTICE_FLASH_CFG0)||
						(_flash_sector == LATTICE_FLASH_UFM0)) {
				if (offset == 0) {
					erase_op = FLASH_SEC_UFM0;
					prog_op = FLASH_UFM_ADDR_UFM0;
				} else {
					erase_op = 0;
					/* We need to use the 'LSC_WRITE_ADDRESS' command to set the
					 * flash sector and the page number. */
					prog_op = (FLASH_SET_ADDR_UFM0 << 14) | (offset);
				}
				area_name = "UFM0";
			} else if ((_flash_sector == LATTICE_FLASH_CFG1)||
							(_flash_sector == LATTICE_FLASH_UFM1)) {
				if (offset == 0) {
					erase_op = FLASH_SEC_UFM1;
					prog_op = FLASH_UFM_ADDR_UFM1;
				} else {
					erase_op = 0;
					/* We need to use the 'LSC_WRITE_ADDRESS' command to set the
					 * flash sector and the page number. */
					prog_op = (FLASH_SET_ADDR_UFM1 << 14) | (offset);
				}
				area_name = "UFM1";
			} else if (_flash_sector == LATTICE_FLASH_UFM2) {
				if (offset == 0) {
					erase_op = FLASH_SEC_UFM2;
					prog_op = FLASH_UFM_ADDR_UFM2;
				} else {
					erase_op = 0;
					/* We need to use the 'LSC_WRITE_ADDRESS' command to set the
					 * flash sector and the page number. */
					prog_op = (FLASH_SET_ADDR_UFM2 << 14) | (offset);
				}
				area_name = "UFM2";
			} else if (_flash_sector == LATTICE_FLASH_UFM3) {
				if (offset == 0) {
					erase_op = FLASH_SEC_UFM3;
					prog_op = FLASH_UFM_ADDR_UFM3;
				} else {
					erase_op = 0;
					/* We need to use the 'LSC_WRITE_ADDRESS' command to set the
					 * flash sector and the page number. */
					prog_op = (FLASH_SET_ADDR_UFM3 << 14) | (offset);
				}
				area_name = "UFM3";
			}
		} else {
			printf("Processing CFG data (offset: %d (0x%x))\n", offset, offset);
			if (_flash_sector == LATTICE_FLASH_CFG0) {
				erase_op = FLASH_SEC_CFG0;
				prog_op = FLASH_SEC_CFG0;
				area_name = "Data (CFG0)";
			} else if (_flash_sector == LATTICE_FLASH_CFG1) {
				erase_op = FLASH_SEC_CFG1;
				prog_op = FLASH_SEC_CFG1;
				area_name = "Data (CFG1)";
			}

			/* offset should be zero */
			if (offset != 0) {
				printf("Warning: offset (%d) is not 0 for programming CFG\n", offset);
			}
		}

		if (erase_op > 0) {
			/* ISC ERASE */
			printInfo("Flash erase: ", false);
			if (flashErase(erase_op) == false) {
				printError("FAIL");
				return false;
			}
			printSuccess("DONE");
		}

		if (offset == 0) {
			/* LSC_INIT_ADDRESS */
			uint8_t tx[2] = {
				(uint8_t)((prog_op >> 8) & 0xff),
				(uint8_t)((prog_op >> 16) & 0xff)
			};
			printf("address (I): 0x%x 0x%x\n", tx[0], tx[1]);
			wr_rd(RESET_CFG_ADDR, tx, 2, NULL, 0);
		} else {
			/* LSC_WRITE_ADDRESS */
			uint8_t tx[3] = {
				(uint8_t)(prog_op & 0xff),
				(uint8_t)((prog_op >> 8) & 0xff),
				(uint8_t)((prog_op >> 16) & 0x03)
			};
			printf("address (W): 0x%x 0x%x 0x%x\n", tx[0], tx[1], tx[2]);
			wr_rd(LSC_WRITE_ADDRESS, tx, 3, NULL, 0);
		}
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(1000);

		/* flash CfgFlash */
		if (false == flashProg(0, area_name, data))
			return false;

		/* verify write */
		if (_verify) {
			if (Verify(data, false, prog_op) == false)
				return false;
		}
	}

	/* @TODO: missing usercode update */

	/* LSC_INIT_ADDRESS */
	if (_flash_sector == LATTICE_FLASH_CFG0) {
		uint8_t tx[2] = {
			(uint8_t)((FLASH_SEC_CFG0 >> 8) & 0xff),
			(uint8_t)((FLASH_SEC_CFG0 >> 16) & 0xff)
		};
		wr_rd(RESET_CFG_ADDR, tx, 2, NULL, 0);
	} else if (_flash_sector == LATTICE_FLASH_CFG1) {
		uint8_t tx[2] = {
			(uint8_t)((FLASH_SEC_CFG1 >> 8) & 0xff),
			(uint8_t)((FLASH_SEC_CFG1 >> 16) & 0xff)
		};
		wr_rd(RESET_CFG_ADDR, tx, 2, NULL, 0);
	}
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);

	/* ISC program done 0x5E */
	printInfo("Write program Done: ", false);
	if (writeProgramDone() == false) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}

	/* bypass */
	wr_rd(ISC_NOOP, NULL, 0, NULL, 0);

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

bool Lattice::program_pubkey_MachXO3D()
{
	bool err, same = true;
	int len, i, j;
	uint8_t pubkey[PUBKEY_LENGTH_BYTES];
	uint8_t rxkey[PUBKEY_LENGTH_BYTES];

	RawParser _pk(_filename, false);
	printInfo("Open file: ", false);
	printSuccess("DONE");

	err = _pk.parse();
	printInfo("Parse file: ", false);
	if (err == EXIT_FAILURE) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}

	uint8_t* data = _pk.getData();
	len =  _pk.getLength()/8;

	if (data[0] == 0x0f && data[1] == 0xf0) {
		for (i = 2; i < len; i++) {
			if (data[i] == 0xf0 && data[i+1] == 0x0f) {
				if (_verbose) printf("Header: [%.*s]\n", i-2, ((char *)data)+2);
				i+=2;
				break;
			}
		}

		memcpy(pubkey, data+i, PUBKEY_LENGTH_BYTES);
		i += PUBKEY_LENGTH_BYTES;
/*
		As read from file:
		...
		7dbc273a6e614a0f5289070524a1a59d
		3a5d518b5cff00bc521f1ef62c4227ce
		dd7987ecb63768e3310864f4b44daf90
		ebf86ce8a9b17842821551a85b2235cc
		...

		As Sent by diamond programmer:
		0x59: cc35225ba85115824278b1a9e86cf8eb
		0x5B: 90af4db4f4640831e36837b6ec8779dd
		0x61: ce27422cf61e1f52bc00ff5c8b515d3a
		0x63: 9da5a124050789520f4a616e3a27bc7d
*/
		if (_verbose) {
			printf("PubKey: [");
			for (j=0; j < PUBKEY_LENGTH_BYTES; j++) {
				if (j && (j%16 == 0)) printf(" ");
				printf("%02x", pubkey[j]);
			}
			printf("]\n");
		}

		if (_verbose) {
			printf("Trailing bytes: [");
			for (; i < len; i++) {
				printf("%02x ", data[i]);
			}
			printf("\b]\n");
		}
	}
	else {
		printError("Failed to find header in public key file");
		return false;
	}


	/* bypass */
	wr_rd(ISC_NOOP, NULL, 0, NULL, 0);
	/* ISC Enable 0xC6 with operand of 0x08 (Enable Offline mode) */
	printInfo("Enable configuration: ", false);
	if (!EnableISC(0x08)) {
		printError("FAIL");
		displayReadReg(readStatusReg());
		return false;
	} else {
		printSuccess("DONE");
	}

	/* read the current feature row */
	wr_rd(READ_ECDSA_PUBKEY0, NULL, 0, rxkey, 16);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	wr_rd(READ_ECDSA_PUBKEY1, NULL, 0, rxkey + 16, 16);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	wr_rd(READ_ECDSA_PUBKEY2, NULL, 0, rxkey + 32, 16);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	wr_rd(READ_ECDSA_PUBKEY3, NULL, 0, rxkey + 48, 16);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	if (_verbose) {
		printf("Read PubKey: [");
		for (j=PUBKEY_LENGTH_BYTES-1; j >= 0; j--) {
			printf("%02x", rxkey[j]);
			if (j && (j%16 == 0)) printf(" ");
		}
		printf("]\n");
	}

	for (int i = 0; i < PUBKEY_LENGTH_BYTES; i++) {
		if (pubkey[i] != rxkey[PUBKEY_LENGTH_BYTES - i - 1])
			same = false;
	}

	printf("PubKey Compare: %s\n", same ? "Same" : "Different");
	if (same == false) {
		uint8_t tx[2];
		/* LSC_INIT_ADDRESS */
		tx[0] = (uint8_t)((FLASH_SEC_PKEY >> 8) & 0xff);
		tx[1] = (uint8_t)((FLASH_SEC_PKEY >> 16) & 0xff);
		if (_verbose)
			printf("Selected address (I): 0x%x 0x%x\n", tx[0], tx[1]);
		wr_rd(RESET_CFG_ADDR, tx, 2, NULL, 0);

		/* ISC ERASE */
		printInfo("Flash erase: ", false);
		if (flashErase(FLASH_SEC_PKEY) == false) {
			printError("FAIL");
			return false;
		}
		else {
			printSuccess("DONE");
		}

		/* Public Key */
		printInfo("Program Public Key: ", true);
		if (!programPubKey_MachXO3D(pubkey)) {
			printError("FAIL");
			return false;
		}
		else {
			printSuccess("DONE");
		}
	}

	/* Programming and Verify the AUTH_EN2 and AUTH_EN1 Fuses..."
	 * -- This is undocumented (extracted from USB capture) */
	uint8_t tx_byte = 0x03;
	wr_rd(0xc4, &tx_byte, 1, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	wr_rd(ISC_NOOP, NULL, 0, NULL, 0);

	/* lattice diamond sends this twice... ? */
	wr_rd(0xc4, &tx_byte, 1, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	wr_rd(ISC_NOOP, NULL, 0, NULL, 0);

	if (_verbose) {
		wr_rd(READ_STATUS_REGISTER_1, NULL, 0, rxkey, 4);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(2);

		printf("Auth Mode: [%s] (0x%x)\n", (rxkey[1] & 0x03 ? "ECDSA Signature Verification" : rxkey[1] & 0x01 ? "HMAC Authentication" : "No Authentication"), rxkey[1] & 0x03);
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
	wr_rd(ISC_NOOP, NULL, 0, NULL, 0);

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
