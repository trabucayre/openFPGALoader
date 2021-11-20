// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2021 Martin Beynon <martin.beynon@abaco.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <utility>
#include <vector>

#include "display.hpp"
#include "feaparser.hpp"

/* FEAbits element defines */
#  define FEA_I2C_DG_FIL_EN			(1 << 0)	/* I2C deglitch filter enable for Primary I2C Port 0=Disabled (Default), 1=Enabled */
#  define FEA_FLASH_PROT_SEC_SEL	(0x7 << 1)	/* Flash Protection Sector Selection */
#  define FEA_MY_ASSP_EN			(1 << 4)	/* MY_ASSP Enabled 0=Disabled (Default), 1=Enabled */
#  define FEA_PROG_PERSIST			(1 << 5)	/* PROGRAMN Persistence 0=Enabled (Default), 1=Disabled */
#  define FEA_INITN_PERSIST			(1 << 6)	/* INITN Persistence 0=Disabled (Default), 1=Enabled */
#  define FEA_DONE_PERSIST			(1 << 7)	/* DONE Persistence 0=Disabled (Default), 1=Enabled */
#  define FEA_JTAG_PERSIST			(1 << 8)	/* JTAG Port Persistence 0=Enabled (Default), 1=Disabled */
#  define FEA_SSPI_PERSIST			(1 << 9)	/* Slave SPI Port Persistence 0=Enabled (Default), 1=Disabled */
#  define FEA_I2C_PERSIST			(1 << 10)	/* I²C Port Persistence 0=Enabled (Default), 1=Disabled */
#  define FEA_MSPI_PERSIST			(1 << 11)	/* Master SPI Port Persistence 0=Disabled (Default), 1=Enabled */
#  define FEA_BOOT_SEQ_SEL			(0x07 << 12)	/* Boot Sequence selection (used along with Master SPI Port Persistence bit) */
#  define FEA_I2C_DG_RANGE_SEL		(1 << 15)	/* I2C deglitch filter range selection on primary I2C port2 0= 8 to 25 ns range (Default) 1= 16 to 50 ns range */
#  define FEA_VERSION_RB_PROT		(1 << 16)	/* Version Rollback Protection1 0= Disabled (Default) 1= Enabled */
#  define FEA_RESERVED_ZERO			(0xffff << 17)

/* Feature Row element defines */
#  define FEATURE_CUSTOM_ID			(0xffffffff)	/* 32 bits of Custom ID code */

#  define FEATURE_TRACE_ID			(0xff << 0)	/* 8 bits for the user programmable TraceID */
#  define FEATURE_I2C_SLAVE_ADDR	(0xff << 8)	/* 8 bits for the user programmable I2C Slave Address */
#  define FEATURE_DUAL_BOOT_ADDR	(0xffff << 16)  /* 16 bits for Dual boot address (Most significant 16- bit of address for secondary boot from external flash) */

#  define FEATURE_MASTER_RETRY_CNT	(0x3 << 2)	/* Master Retry Count */
#  define FEATURE_MASTER_TIMER_CNT	(0x0f << 4)	/* Master Timer  Count */
#  define FEATURE_SLAVE_IDLE_TIMER_CNT	(0x0f << 8)	/* Slave Idle Timer Count */
#  define FEATURE_SFDP_CONT_FAIL	(1 << 14)	/* SFDP Continue on Fail */
#  define FEATURE_SFDP_EN			(1 << 15)	/* SFDP Enable */
#  define FEATURE_BULK_ERASE_DISABLE	(1 << 16)	/* No Bulk Erase */
#  define FEATURE_32BIT_SPIM		(1 << 17)	/* 32-bit SPIM */
#  define FEATURE_MCLK_BYPASS		(1 << 18)	/* MCLK Bypass */
#  define FEATURE_LSBF				(1 << 19)	/* LSBF */
#  define FEATURE_RX_EDGE			(1 << 20)
#  define FEATURE_TX_EDGE			(1 << 21)
#  define FEATURE_CPOL				(1 << 22)
#  define FEATURE_CPHA				(1 << 23)
#  define FEATURE_HSE_CLOCK_SEL		(0x3 << 24)
#  define FEATURE_EBR_ENABLE		(1 << 26)
#  define FEATURE_SSPI_AUTO			(1 << 28)	/* SSPI Auto */
#  define FEATURE_CPU				(1 << 29)	/* CPU */
#  define FEATURE_CORE_CLK_SEL		(0x03 << 30)	/* Core Clock Sel */


using namespace std;

FeaParser::FeaParser(string filename, bool verbose):
	ConfigBitstreamParser(filename, ConfigBitstreamParser::BIN_MODE, verbose),
	_feabits(0), _has_feabits(false)
{
	for (int i=0; i < 3; i++)
		_featuresRow[i] = 0;
}

/* fill a vector with consecutive lines, begining with 0 or 1, until EOF
 * \brief read a line with '\r''\n' or '\n' termination
 * check if last char is '\r'
 * \return a vector of lines without [\r]\n
 */
vector<string> FeaParser::readFeaFile()
{
	vector<string> lines;

	while (true) {
		string buffer;
		std::getline(_ss, buffer, '\n');
		if (buffer.empty())
			break;

		/* if '\r' is present -> drop */
		if (buffer.back() == '\r')
			buffer.pop_back();

		if (buffer.front() == '0' || buffer.front() == '1')
			lines.push_back(buffer);
	}

	return lines;
}

void FeaParser::displayHeader()
{
	if (_has_feabits) {
		printf("\nFeature Row: [0x");
		for (int i = 2; i >= 0; i--) {
			printf("%08x", _featuresRow[i]);
		}
		printf("]\n");

		printf("\tCore Clock Select     : 0x%x\n", (_featuresRow[2] >> 30) & 0x03);
		printf("\tCPU                   : %d\n",
			((_featuresRow[2] & FEATURE_CPU)? 1 : 0));
		printf("\tSSPI Auto             : %s\n",
			((_featuresRow[2] & FEATURE_SSPI_AUTO)?"Enabled":"Disabled"));
		printf("\tReserved Zero (1)     : 0x%x\n", (_featuresRow[2] >> 27) & 0x01);
		printf("\tEBR Enable            : %s\n",
			((_featuresRow[2] & FEATURE_EBR_ENABLE)?"Yes":"No"));
		printf("\tHSE Clock Select      : 0x%x\n", (_featuresRow[2] >> 24) & 0x03);
		printf("\tCPHA                  : %s\n",
			((_featuresRow[2] & FEATURE_CPHA)?"Enabled":"Disabled"));
		printf("\tCPOL                  : %s\n",
			((_featuresRow[2] & FEATURE_CPOL)?"Enabled":"Disabled"));
		printf("\tTx Edge               : %s\n",
			((_featuresRow[2] & FEATURE_TX_EDGE)?"Enabled":"Disabled"));
		printf("\tRx Edge               : %s\n",
			((_featuresRow[2] & FEATURE_RX_EDGE)?"Enabled":"Disabled"));
		printf("\tLSBF                  : %s\n",
			((_featuresRow[2] & FEATURE_LSBF)?"Enabled":"Disabled"));
		printf("\tMClock Bypass         : %s\n",
			((_featuresRow[2] & FEATURE_MCLK_BYPASS)?"Enabled":"Disabled"));
		printf("\t32-bit SPIM           : %s\n",
			((_featuresRow[2] & FEATURE_32BIT_SPIM)?"Enabled":"Disabled"));
		printf("\tBulk Erase Disable    : %s\n",
			((_featuresRow[2] & FEATURE_BULK_ERASE_DISABLE)?"Yes":"No"));
		printf("\tSFDP Enable           : %s\n",
			((_featuresRow[2] & FEATURE_SFDP_EN)?"Yes":"No"));
		printf("\tSFDP Continue on Fail : %s\n",
			((_featuresRow[2] & FEATURE_SFDP_CONT_FAIL)?"Yes":"No"));
		printf("\tReserved Zero (2)     : 0x%x\n", (_featuresRow[2] >> 12) & 0x03);
		printf("\tSlave Idle Timer Count: %d\n",  (_featuresRow[2] >> 8) & 0x0f);
		printf("\tMaster Timer Count    : %d\n",  (_featuresRow[2] >> 4) & 0x0f);
		printf("\tMaster Retry Count    : %d\n",  (_featuresRow[2] >> 2) & 0x03);
		printf("\tReserved Zero (2)     : 0x%x\n",  _featuresRow[2] & 0x03);

		printf("\tDual Boot Address     : 0x%x\n",  (_featuresRow[1] >> 16) & 0xffff);
		printf("\tI2C Slave Address     : 0x%x\n",  (_featuresRow[1] >> 8) & 0xff);
		printf("\tCustom Trace ID       : 0x%x\n",  _featuresRow[1] & 0xff);
		printf("\tCustom ID Code        : 0x%x\n",  _featuresRow[0]);


		printf("\nFEAbits: [0x%08x]\n", _feabits);
		printf("\tReserved Zero (16)	: 0x%x\n", (_feabits >> 17) & 0xffff);
		printf("\tRollback Protection   : %s\n",
			((_feabits & FEA_VERSION_RB_PROT)?"Enabled":"Disabled"));
		printf("\tI2C Deglitch Range	: %s\n",
			((_feabits & FEA_I2C_DG_RANGE_SEL)?"(1) 16 to 50 ns":"(0) 8 to 25 ns"));
		int boot_mode = (_feabits >> 12) & 0x07;
		printf("\tBoot Mode             : ");
		if ((_feabits & FEA_MSPI_PERSIST) == 0) {
			if (boot_mode == 0)
				printf("Dual Boot, CFG0 - CFG1\n");
			else if (boot_mode == 1)
				printf("Dual Boot, CFG1 - CFG0\n");
			else if (boot_mode == 3)
				printf("Single Boot, CFG0\n");
			else if (boot_mode == 4)
				printf("Single Boot, CFG1\n");
			else if (boot_mode == 5)
				printf("Dual Boot, Boot from former bitstream first\n");
			else if (boot_mode == 7)
				printf("Dual Boot, Boot from latter bitstream first\n");
			else if ((boot_mode & 0x03) == 2)
				printf("Dual Boot, No Boot\n");
			else
				printf("Unknown boot sequence selection");
		} else {
			if (boot_mode == 0)
				printf("Dual Boot, CFG0 - Ext\n");
			else if ((boot_mode & 0x03) == 1)
				printf("Single Boot, Ext\n");
			else if (boot_mode == 2)
				printf("Dual Boot, Ext - CFG0\n");
			else if ((boot_mode & 0x03) == 3)
				printf("Dual Boot, Ext - Ext\n");
			else if (boot_mode == 4)
				printf("Dual Boot, CFG1 - Ext\n");
			else if (boot_mode == 6)
				printf("Dual Boot, Ext - CFG1\n");
			else
				printf("Unknown boot sequence selection");
		}
		printf("\tMSPI Enable          : %s\n",
			((_feabits & FEA_MSPI_PERSIST)?"Yes":"No"));
		printf("\tI2C Disable          : %s\n",
			((_feabits & FEA_I2C_PERSIST)?"Yes":"No"));
		printf("\tSSPI Disable         : %s\n",
			((_feabits & FEA_SSPI_PERSIST)?"Yes":"No"));
		printf("\tJTAG Disable         : %s\n",
			((_feabits & FEA_JTAG_PERSIST)?"Yes":"No"));
		printf("\tDONE Enable          : %s\n",
			((_feabits & FEA_DONE_PERSIST)?"Yes":"No"));
		printf("\tINIT Enable          : %s\n",
			((_feabits & FEA_INITN_PERSIST)?"Yes":"No"));
		printf("\tPROGRAM Disable      : %s\n",
			((_feabits & FEA_PROG_PERSIST)?"Yes":"No"));
		printf("\tCustom ID Enable     : %s\n",
			((_feabits & FEA_MY_ASSP_EN)?"Yes":"No"));

		int flash_prot = (_feabits >> 1) & 0x07;
		printf("\tFlash Protection     : ");
		if (flash_prot == 0) {
			printf("None\n");
		} else {
			if (flash_prot & 0x04)
				printf("CFG0 & CFG1 ");
			if (flash_prot & 0x02)
				printf("Feature, Security Keys ");
			if (flash_prot & 0x01)
				printf("All UFMs");
			printf("\n");
		}
		printf("\tI2C Deglitch Filter   : %s\n",
			((_feabits & FEA_I2C_DG_FIL_EN)?"Enabled":"Disabled"));
	}
}

/* Features Row & Feabits - this data is in a separate .fea file (MachXO3D)
 * DATA:
 * 1: xxxx\n : feature Row (96 bits)
 * 2: yyyy*\n : feabits (32 bits)
 */
void FeaParser::parseFeatureRowAndFeabits(const vector<string> &content)
{
	printf("Parsing Feature Row & FEAbits...\n");

	string featuresRow = content[0];
	//printf("Features: [%s]\n", featuresRow.c_str());
	for (size_t i = 0; i < featuresRow.size(); i++)
		_featuresRow[3 - (i/32) - 1] |= ((featuresRow[i] - '0') << (32 - (i%32) - 1));

	string feabits = content[1];
	//printf("Feabits: [%s]\n", feabits.c_str());
	_feabits = 0;
	for (size_t i = 0; i < feabits.size(); i++) {
		_feabits |= ((feabits[i] - '0') << (feabits.size() - i - 1));
	}
}

int FeaParser::parse()
{
	std::vector<string>lines;

	_ss.str(_raw_data);

	lines = readFeaFile();
	/* empty or end of file */
	if (lines.size() > 0) {
		parseFeatureRowAndFeabits(lines);
		_has_feabits = true;
	}

	return EXIT_SUCCESS;
}
