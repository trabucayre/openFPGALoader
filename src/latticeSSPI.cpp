// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2025 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include "latticeSSPI.hpp"

#include <string.h>
#include <unistd.h>

#define __STDC_FORMAT_MACROS
#include <cinttypes>
#include <iomanip>
#include <iostream>
#include <list>
#include <sstream>
#include <string>

#include "device.hpp"
#include "ftdispi.hpp"
#include "latticeBitParser.hpp"
#include "progressBar.hpp"

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

uint32_t LatticeSSPI::char_array_to_word(const uint8_t *in)
{
	return static_cast<uint32_t>(in[0] << 24) |
		static_cast<uint32_t>(in[1] << 16) |
		static_cast<uint32_t>(in[2] <<  8) |
		static_cast<uint32_t>(in[3] <<  0);
}

bool LatticeSSPI::EnableISC(uint8_t /*flash_mode*/)
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

	return char_array_to_word(rx);
}

/*
 * Read Only command.
 * 8bits command + 24 dummy bits + len rx
 * len: nb bits
 */
bool LatticeSSPI::cmd_class_a(uint8_t cmd, uint8_t *rx, uint32_t len)
{
	const uint32_t xferByteLen = (len + 7) / 8;
	const uint32_t kBytetLen = xferByteLen + 3;  // Convert bits to bytes after adding dummy
	uint8_t lrx[kBytetLen];
	uint8_t ltx[kBytetLen];
	memset(ltx, 0x00, kBytetLen);

	_spi->spi_put(cmd, ltx, lrx, kBytetLen);

	memcpy(rx, &lrx[3], xferByteLen);

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

bool LatticeSSPI::flashErase(uint32_t /*mask*/)
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
	LatticeBitParser _bit(_filename, false, _verbose);

	printInfo("Open file: ", false);
	printSuccess("DONE");

	const bool err = _bit.parse();

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
	const int length = _bit.getLength() / 8;

	/* read ID Code 0xE0 and compare to bitstream */
	const uint32_t bit_idcode = std::stoul(_bit.getHeaderVal("idcode").c_str(), NULL, 16);
	const uint32_t idcode = idCode();
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

	int size = 1024;

	ProgressBar progress("Loading", length, 50, _quiet);

	for (int i = 0; i < length; i += size) {
		progress.display(i);

		if (length < i + size)
			size = length-i;

		_spi->spi_put(&data[i], NULL, size);
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
	const uint8_t nop[] = {0xff, 0xff, 0xff, 0xff};
	_spi->spi_put(nop, NULL, 4);

	return true;
}

void LatticeSSPI::program(unsigned int /*offset*/, bool /*unprotect_flash*/)
{
	if (_mode == FLASH_MODE)
		throw std::runtime_error("Flash mode not avaible when programming in Slave SPI");

	const bool retval = program_mem();
	if (!retval)
		throw std::exception();
}

typedef struct {
	std::string description;
	uint8_t offset;
	uint8_t size;
	std::map<int, std::string> reg_cnt;
} reg_struct_t;

#define REG_ENTRY(_description, _offset, _size, ...) \
	{_description, _offset, _size, {__VA_ARGS__}}

static const std::map<std::string, std::list<reg_struct_t>> reg_content = {
	{"StatusRegister", std::list<reg_struct_t>{
		REG_ENTRY("Transparent Mode",         0, 1, {0, "No"},         {1, "Yes"}),
		REG_ENTRY("Config Target Selection",  1, 3, {0, "SRAM array"}, {1, "Efuse"}),
		REG_ENTRY("JTAG Active",              4, 1, {0, "No"},         {1, "Yes"}),
		REG_ENTRY("PWD Protect",              5, 1, {0, "No"},         {1, "Yes"}),
		REG_ENTRY("Decrypt Enable",           7, 1, {0, "No"},         {1, "Yes"}),
		REG_ENTRY("Done",                     8, 1, {0, "No"},         {1, "Yes"}),
		REG_ENTRY("ISC Enable",               9, 1, {0, "No"},         {1, "Yes"}),
		REG_ENTRY("Write Enable",            11, 1, {0, "Not Writable"}, {1, "Writable"}),
		REG_ENTRY("Read Enable",             11, 1, {0, "Not Readable"}, {1, "Readable"}),
		REG_ENTRY("Busy Flag",               12, 1, {0, "No"},         {1, "Yes"}),
		REG_ENTRY("Fail Flag",               13, 1, {0, "No"},         {1, "Yes"}),
		REG_ENTRY("FFEA OTP",                14, 1, {0, "No"},         {1, "Yes"}),
		REG_ENTRY("Decrypt Only",            15, 1, {0, "No"},         {1, "Yes"}),
		REG_ENTRY("PWD Enable",              16, 1, {0, "No"},         {1, "Yes"}),
		REG_ENTRY("Std Preamble",            20, 1, {0, "No"},         {1, "Yes"}),
		REG_ENTRY("SPIm Fail1",              21, 1, {0, "No"},         {1, "Yes"}),
		REG_ENTRY("BSE Error Code",          22, 3,
			{ 0, "No err"},
			{ 1, "ID ERR"},
			{ 2, "CMD ERR"},
			{ 3, "CRC ERR"},
			{ 4, "Preamble ERR"},
			{ 5, "Abort ERR"},
			{ 6, "Overflow ERR"},
			{ 7, "SDM EOF"},
			{ 8, "Authentication ERR"},
			{ 9, "Authentication Setup ERR"},
			{10, "Bitstream Engine Timeout ERR"}),
		REG_ENTRY("EXEC Error",              26, 1, {0, "No"}, {1, "Yes"}),
		REG_ENTRY("Device failed to verify", 27, 1, {0, "No"}, {1, "Yes"}),
		REG_ENTRY("Invalid Command",         28, 1, {0, "No"}, {1, "Yes"}),
		REG_ENTRY("SED Error",               29, 1, {0, "No"}, {1, "Yes"}),
		REG_ENTRY("Bypass Mode",             30, 1, {0, "No"}, {1, "Yes"}),
		REG_ENTRY("FT Mode",                 31, 1, {0, "No"}, {1, "Yes"}),

	}},
};

void LatticeSSPI::displayReadReg(uint32_t dev)
{
	auto reg = reg_content.find("StatusRegister");
	if (reg == reg_content.end()) {
		printError("Unknown register StatusRegister");
		return;
	}

	std::stringstream raw_val;
	raw_val << "0x" << std::hex << dev;
	printSuccess("Register raw value: " + raw_val.str());

	const std::list<reg_struct_t> regs = reg->second;
	for (reg_struct_t r: regs) {
		uint8_t offset = r.offset;
		uint8_t size = r.size;
		uint32_t mask = (1 << size) - 1;
		uint32_t val = (dev >> offset) & mask;
		std::stringstream ss, desc;
		desc << r.description;
		ss << std::setw(24) << std::left << r.description;
		if (r.reg_cnt.size() != 0) {
			ss << r.reg_cnt[val];
		} else {
			std::stringstream hex_val;
			hex_val << "0x" << std::hex << val;
			ss << hex_val.str();
		}

		printInfo(ss.str());
	}
}
