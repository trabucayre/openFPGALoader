// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2021 Martin Beynon <martin.beynon@abaco.com>
 */
#ifndef FEAPARSER_HPP_
#define FEAPARSER_HPP_

#include <stdint.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "configBitstreamParser.hpp"

/* FEAbits element defines */
#  define FEA_I2C_DG_FIL_EN				(1 << 0)	    /* I2C deglitch filter enable for Primary I2C Port 0=Disabled (Default), 1=Enabled */
#  define FEA_FLASH_PROT_SEC_SEL		(0x7 << 1)	    /* Flash Protection Sector Selection */
#  define FEA_MY_ASSP_EN				(1 << 4)	    /* MY_ASSP Enabled 0=Disabled (Default), 1=Enabled */
#  define FEA_PROG_PERSIST				(1 << 5)	    /* PROGRAMN Persistence 0=Enabled (Default), 1=Disabled */
#  define FEA_INITN_PERSIST				(1 << 6)	    /* INITN Persistence 0=Disabled (Default), 1=Enabled */
#  define FEA_DONE_PERSIST				(1 << 7)	    /* DONE Persistence 0=Disabled (Default), 1=Enabled */
#  define FEA_JTAG_PERSIST				(1 << 8)	    /* JTAG Port Persistence 0=Enabled (Default), 1=Disabled */
#  define FEA_SSPI_PERSIST				(1 << 9)	    /* Slave SPI Port Persistence 0=Enabled (Default), 1=Disabled */
#  define FEA_I2C_PERSIST				(1 << 10)	    /* I²C Port Persistence 0=Enabled (Default), 1=Disabled */
#  define FEA_MSPI_PERSIST				(1 << 11)	    /* Master SPI Port Persistence 0=Disabled (Default), 1=Enabled */
#  define FEA_BOOT_SEQ_SEL				(0x07 << 12)    /* Boot Sequence selection (used along with Master SPI Port Persistence bit) */
#  define FEA_I2C_DG_RANGE_SEL			(1 << 15)	    /* I2C deglitch filter range selection on primary I2C port2 0= 8 to 25 ns range (Default) 1= 16 to 50 ns range */
#  define FEA_VERSION_RB_PROT			(1 << 16)	    /* Version Rollback Protection1 0= Disabled (Default) 1= Enabled (Checks if current version of bitstream is similar to the one that is going to be downloaded) */
#  define FEA_RESERVED_ZERO				(0xffff << 17)

/* Feature Row element defines */
#  define FEATURE_CUSTOM_ID				(0xffffffff)	/* 32 bits of Custom ID code */

#  define FEATURE_TRACE_ID				(0xff << 0)	    /* 8 bits for the user programmable TraceID */
#  define FEATURE_I2C_SLAVE_ADDR		(0xff << 8)	    /* 8 bits for the user programmable I2C Slave Address */
#  define FEATURE_DUAL_BOOT_ADDR		(0xffff << 16)	/* 16 bits for Dual boot address (Most significant 16- bit of address for secondary boot from external flash) */

#  define FEATURE_MASTER_RETRY_CNT		(0x3 << 2)		/* Master Retry Count */
#  define FEATURE_MASTER_TIMER_CNT		(0x0f << 4)	    /* Master Timer  Count */
#  define FEATURE_SLAVE_IDLE_TIMER_CNT	(0x0f << 8)	    /* Slave Idle Timer Count */
#  define FEATURE_SFDP_CONT_FAIL		(1 << 14)		/* SFDP Continue on Fail */
#  define FEATURE_SFDP_EN				(1 << 15)		/* SFDP Enable */
#  define FEATURE_BULK_ERASE_DISABLE	(1 << 16)		/* No Bulk Erase */
#  define FEATURE_32BIT_SPIM			(1 << 17)		/* 32-bit SPIM */
#  define FEATURE_MCLK_BYPASS			(1 << 18)		/* MCLK Bypass */
#  define FEATURE_LSBF					(1 << 19)		/* LSBF */
#  define FEATURE_RX_EDGE				(1 << 20)
#  define FEATURE_TX_EDGE				(1 << 21)
#  define FEATURE_CPOL					(1 << 22)
#  define FEATURE_CPHA					(1 << 23)
#  define FEATURE_HSE_CLOCK_SEL			(0x3 << 24)
#  define FEATURE_EBR_ENABLE			(1 << 26)
#  define FEATURE_SSPI_AUTO				(1 << 28)	    /* SSPI Auto */
#  define FEATURE_CPU					(1 << 29)	    /* CPU */
#  define FEATURE_CORE_CLK_SEL			(0x03 << 30)    /* Core Clock Sel */

class FeaParser: public ConfigBitstreamParser {
	public:
		FeaParser(std::string filename, bool verbose = false);
		int parse() override;
		void displayHeader() override;

		uint32_t* featuresRow() {return _featuresRow;}
		uint32_t feabits() {return _feabits;}

	private:
		std::vector<std::string>readFeaFile();
        void parseFeatureRowAndFeabits(const std::vector<std::string> &content);

		uint32_t _featuresRow[3];
		uint32_t _feabits;
		bool _has_feabits;

        std::istringstream _ss;
};

#endif  // FEAPARSER_HPP_
