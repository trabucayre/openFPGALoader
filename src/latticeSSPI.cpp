// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2025 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#define __STDC_FORMAT_MACROS
#include <iostream>
#include <string>
#include <cinttypes>

#include <string.h>
#include <unistd.h>

#include "device.hpp"
#include "ftdispi.hpp"
#include "latticeBitParser.hpp"
#include "progressBar.hpp"

#include "latticeSSPI.hpp"

#define SSPI_READ_ID               0xE0
#define SSPI_ISC_READ_STATUS       0x3C
#define ISC_ENABLE                 0xC6        /* ISC_ENABLE - Offline Mode */
#define ISC_DISABLE                0x26        /* ISC_DISABLE */
#define READ_BUSY_FLAG			   0xF0
#define FLASH_ERASE                0x0E

#  define REG_STATUS_DONE          (1 << 8)    /* Flash or SRAM Done Flag (ISC_EN=0 -> 1 Successful Flash to SRAM transfer, ISC_EN=1 -> 1 Programmed) */
#  define REG_STATUS_ISC_EN        (1 << 9)    /* Enable Configuration Interface (1=Enable, 0=Disable) */
#  define REG_STATUS_BUSY          (1 << 12)   /* Busy Flag (1 = Busy) */
#  define REG_STATUS_FAIL          (1 << 13)   /* Fail Flag (1 = Operation failed) */
#  define REG_STATUS_PP_CFG        (1 << 15)   /* Password Protection All Enabled for CFG0 and CFG1 flash sectors 0=Disabled (Default), 1=Enabled */
#  define REG_STATUS_PP_FSK        (1 << 16)   /* Password Protection Enabled for Feature and Security Key flash sectors 0=Disabled (Default), 1=Enabled */
#  define REG_STATUS_PP_UFM        (1 << 17)   /* Password Protection enabled for all UFM flash sectors 0=Disabled (Default), 1=Enabled */
#  define REG_STATUS_AUTH_DONE     (1 << 18)   /* Authentication done */
#  define REG_STATUS_PRI_BOOT_FAIL (1 << 21)   /* Primary boot failure (1= Fail) even though secondary boot successful */
#  define REG_STATUS_CNF_CHK_MASK  (0x0f << 23) /* Configuration Status Check */
#  define REG_STATUS_EXEC_ERR      (1 << 26)   /*** NOT specified for MachXO3D ***/
#  define REG_STATUS_DEV_VERIFIED  (1 << 27)   /* I=0 Device verified correct, I=1 Device failed to verify */

LatticeSSPI::LatticeSSPI(FtdiSpi *spi, const std::string &filename,
			const std::string &file_type,
			const int8_t verbose):
	Device(NULL, filename, file_type, false, verbose), _spi(spi)
{
	_spi->setMode(0);
}

uint32_t LatticeSSPI::char_array_to_word(uint8_t *in)
{
	return static_cast<uint32_t>(in[0] << 24) |
		static_cast<uint32_t>(in[1] << 16) |
		static_cast<uint32_t>(in[2] <<  8) |
		static_cast<uint32_t>(in[3] <<  0);
}

bool LatticeSSPI::EnableISC(uint8_t flash_mode)
{
	cmd_class_c(ISC_ENABLE);

	if (!pollBusyFlag())
		return false;
	if (!checkStatus(REG_STATUS_ISC_EN, REG_STATUS_ISC_EN))
		return false;
	return true;
}

bool LatticeSSPI::DisableISC()
{
	cmd_class_c(ISC_DISABLE);

	if (!pollBusyFlag())
		return false;
	if (!checkStatus(0, REG_STATUS_ISC_EN))
		return false;
	return true;
}

uint32_t LatticeSSPI::readStatusReg()
{
	uint8_t rx[4];

	cmd_class_a(SSPI_ISC_READ_STATUS, rx, 4 * 8);

	return static_cast<uint64_t>(char_array_to_word(rx));
}

/*
 * Read Only command.
 * 8bits command + 24 dummy bits + len rx
 * len: nb bits
 */
bool LatticeSSPI::cmd_class_a(uint8_t cmd, uint8_t *rx, uint32_t len)
{
	const uint32_t xferByteLen = (len + 7) / 8;
	const uint32_t kBytetLen = xferByteLen + 3; // Convert bits to bytes after adding dummy
	uint8_t lrx[kBytetLen];
	uint8_t ltx[kBytetLen];
	memset(ltx, 0x00, kBytetLen);

	_spi->spi_put(cmd, ltx, lrx, kBytetLen);

	memcpy(rx, &lrx[3], xferByteLen);

	return true;
}

bool LatticeSSPI::cmd_class_b(uint8_t cmd, uint8_t *tx, uint32_t len)
{
	throw std::runtime_error("Not implemented");
	return true;
}

bool LatticeSSPI::cmd_class_c(uint8_t cmd)
{
	const uint8_t ltx[3] = {0x00, 0x00, 0x00};

	_spi->spi_put(cmd, ltx, NULL, 3);

	return true;
}

uint32_t LatticeSSPI::idCode()
{
	uint8_t rx[4];
	cmd_class_a(SSPI_READ_ID, rx, 32);
	return char_array_to_word(rx);
}

bool LatticeSSPI::pollBusyFlag(bool verbose)
{
	uint8_t rx;
	int timeout = 0;
	do {
		cmd_class_a(READ_BUSY_FLAG, &rx, 8);
		if (verbose)
			printf("pollBusyFlag :%02x\n", rx);
		if (timeout == 100000000){
			std::cerr << "timeout" << std::endl;
			return false;
		} else {
			timeout++;
		}
	} while (rx != 0);

	return true;
}

bool LatticeSSPI::flashErase(uint32_t  mask)
{
	const uint8_t tx[] = {0x01, 0x00, 0x00};
	_spi->spi_put(FLASH_ERASE, tx, 0, 3);

	if (!pollBusyFlag())
		return false;

	if (!checkStatus(0, REG_STATUS_FAIL))
		return false;

	return true;
}

bool LatticeSSPI::program_mem()
{
	bool err;
	LatticeBitParser _bit(_filename, false, _verbose);

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

	/* Prepare bitstream */
	const uint8_t *data = _bit.getData();
	int length = _bit.getLength()/8;

	/* read ID Code 0xE0 and compare to bitstream */
	uint32_t bit_idcode = std::stoul(_bit.getHeaderVal("idcode").c_str(), NULL, 16);
	uint32_t idcode = idCode();
	if (idcode != bit_idcode) {
		char mess[256];
		snprintf(mess, 256, "mismatch between target's idcode and bitstream idcode\n"
			"\tbitstream has 0x%08X hardware requires 0x%08x", bit_idcode, idcode);
		printError(mess);
		return false;
	}

	if (_verbose) {
		printf("IDCode : 0x%08x\n", idcode);
		displayReadReg(readStatusReg());
	}

	/* LSC_REFRESH 0x79 -- "Equivalent to toggle PROGRAMN pin"
	 * We REFRESH only if the fpga is in a status of error due to
	 * the previous bitstream. For example, this happens if
	 * no bitstream is present on the SPI FLASH
	 */
	cmd_class_c(0x79);
	usleep(8000);

	/* ISC Enable 0xC6 */
	printInfo("Enable configuration: ", false);
	if (!EnableISC(0x00)) {
		printError("FAIL");
		displayReadReg(readStatusReg());
		return false;
	} else {
		printSuccess("DONE");
	}
	displayReadReg(readStatusReg());

	/* ISC ERASE
	 * For Nexus family (from svf file): 1 byte to tx 0x00
	 */
	printInfo("SRAM erase: ", false);
	//uint32_t mask_erase[1] = {FLASH_ERASE_SRAM};

	if (flashErase(0x01/*mask_erase[0]*/) == false) {
		printError("FAIL");
		displayReadReg(readStatusReg());
		return false;
	} else {
		printSuccess("DONE");
	}

	/* LSC_INIT_ADDRESS */
	cmd_class_c(0x46);

	/* Switch to manual CS.
	 * This signal must be low for LSC_BITSTREAM_BURST and
	 * all bitstream TX.
	 */
	_spi->setCSmode(FtdiSpi::SPI_CS_MANUAL);
	_spi->clearCs();
	cmd_class_c(0x7A);

	uint8_t tmp[1024];
	int size = 1024;

	ProgressBar progress("Loading", length, 50, _quiet);

	for (int i = 0; i < length; i += size) {
		progress.display(i);

		if (length < i + size) {
			size = length-i;
		}

		for (int ii = 0; ii < size; ii++)
			tmp[ii] = data[i+ii];

		_spi->spi_put(tmp, NULL, size);
	}
	progress.done();
	/* Switch CS to Automatic mode */
	_spi->setCs();
	_spi->setCSmode(FtdiSpi::SPI_CS_AUTO);

	usleep(1000);

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
	uint8_t nop[] = {0xff, 0xff, 0xff, 0xff};
	_spi->spi_put(nop, NULL, 4);

	return true;
}

void LatticeSSPI::program(unsigned int offset, bool unprotect_flash)
{
	bool retval = true;
	if (_mode == FLASH_MODE)
		throw std::runtime_error("Flash mode not avaible when programming in Slave SPI");

	retval = program_mem();
	if (!retval)
		throw std::exception();
}

void LatticeSSPI::displayReadReg(uint32_t dev)
{
	uint8_t err;
	printf("displayReadReg 0x%08x\n", dev);
	if (dev & 1<<0) printf("\tTRAN Mode\n");
	printf("\tConfig Target Selection : %" PRIx32 "\n", (dev >> 1) & 0x07);
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
	if (dev & 1 << 17) printf("\tUFM OTP\n");
	if (dev & 1 << 18) printf("\tASSP\n");
	if (dev & 1 << 19) printf("\tSDM Enable\n");
	if (dev & 1 << 20) printf("\tEncryption PreAmble\n");
	if (dev & 1 << 21) printf("\tStd PreAmble\n");
	if (dev & 1 << 22) printf("\tSPIm Fail1\n");
	err = (dev >> 23)&0x07;

	printf("\tBSE Error Code\n");
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
			printf("Authentication ERR\n");
			break;
		case 9:
			printf("Authentication Setup ERR\n");
			break;
		case 10:
			printf("Bitstream Engine Timeout ERR\n");
			break;
		default:
			printf("unknown error: %x\n", err);
	}

	if (dev & REG_STATUS_EXEC_ERR) printf("\tEXEC Error\n");
	if ((dev >> 27) & 0x01) printf("\tDevice failed to verify\n");
	if ((dev >> 28) & 0x01) printf("\tInvalid Command\n");
	if ((dev >> 29) & 0x01) printf("\tSED Error\n");
	if ((dev >> 30) & 0x01) printf("\tBypass Mode\n");
	if ((dev >> 31) & 0x01) printf("\tFT Mode\n");
}
