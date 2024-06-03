// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2021 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_SPIFLASHDB_HPP_
#define SRC_SPIFLASHDB_HPP_

#include <stdint.h>

#include <map>
#include <string>

typedef enum {
	STATR = 0,  /* status register */
	FUNCR = 1,  /* function register */
	CONFR = 2,  /* configuration register */
	NONER = 99, /* "none" register */
} tb_loc_t;

typedef struct {
	std::string manufacturer; /**< manufacturer name */
	std::string model;        /**< chip name */
	uint32_t nr_sector;       /**< number of sectors */
	bool sector_erase;        /**< 64KB erase support */
	/* missing 32KB erase */
	bool subsector_erase;     /**< 4KB erase support */
	bool has_extended;
	bool tb_otp;              /**< TOP/BOTTOM One Time Programming */
	uint8_t tb_offset;        /**< TOP/BOTTOM bit offset */
	tb_loc_t tb_register;     /**< TOP/BOTTOM location (register) */
	uint8_t bp_len;           /**< BPx length */
	uint8_t bp_offset[4];     /**< BP[0:3] bit offset */
} flash_t;

static std::map <uint32_t, flash_t> flash_list = {
	{0x010216, {
		.manufacturer = "spansion",
		.model = "S25FL064P / EPCS64",
		.nr_sector = 128,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = true,
		.tb_otp = false,
		.tb_offset = (1 << 5),
		.tb_register = CONFR,
		.bp_len = 3,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), 0}}
	},
	{0x010219, {
		.manufacturer = "spansion",
		.model = "S25FL256S",
		.nr_sector = 512,
		.sector_erase = true,
		.subsector_erase = false,
		.has_extended = true,
		.tb_otp = true,
		.tb_offset = (1 << 5),
		.tb_register = CONFR,
		.bp_len = 3,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), 0}}
	},
	{0x010220, {
		.manufacturer = "spansion",
		.model = "S25FL512S",
		.nr_sector = 1024,
		.sector_erase = true,
		.subsector_erase = false,
		.has_extended = true,
		.tb_otp = true,
		.tb_offset = (1 << 5),
		.tb_register = CONFR,
		.bp_len = 3,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), 0}}
	},
	{0x012018, {
		.manufacturer = "spansion",
		.model = "S25FL128S",
		.nr_sector = 256,
		.sector_erase = true,
		.subsector_erase = false,
		.has_extended = true,
		.tb_otp = true,
		.tb_offset = (1 << 5),
		.tb_register = CONFR,
		.bp_len = 3,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), 0}}
	},
	{0x016018, {
		.manufacturer = "spansion",
		.model = "S25FL128L",
		.nr_sector = 256,
		.sector_erase = true,
		.subsector_erase = false,
		.has_extended = true,
		.tb_otp = false,
		.tb_offset = (1 << 6),
		.tb_register = STATR,
		.bp_len = 4,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), (1 << 5)}}
	},
	{0x016019, {
		.manufacturer = "spansion",
		.model = "S25FL256L",
		.nr_sector = 512,
		.sector_erase = true,
		.subsector_erase = false,
		.has_extended = true,
		.tb_otp = false,
		.tb_offset = (1 << 6),
		.tb_register = STATR,
		.bp_len = 4,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), (1 << 5)}}
	},
	/* https://datasheet.octopart.com/M25P16-VME6G-STMicroelectronics-datasheet-7623188.pdf */
	{0x00202015, {
		.manufacturer = "ST",
		.model = "M25P16",
		.nr_sector = 32,
		.sector_erase = true,
		.subsector_erase = false,
		.has_extended = false,
		.tb_otp = true,
		.tb_offset = 0, // unused
		.tb_register = STATR,
		.bp_len = 3,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), 0}}
	},
	/* https://pdf1.alldatasheet.com/datasheet-pdf/download/104949/STMICROELECTRONICS/M25P32.html */
	{0x00202016, {
		.manufacturer = "ST",
		.model = "M25P32",
		.nr_sector = 64,
		.sector_erase = true,
		.subsector_erase = false,
		.has_extended = false,
		.tb_otp = true,
		.tb_offset = 0, // unused
		.tb_register = STATR,
		.bp_len = 3,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), 0}}
	},
	{0x0020ba16, {
		.manufacturer = "micron",
		.model = "N25Q32",
		.nr_sector = 64,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = true,
		.tb_otp = false,
		.tb_offset = (1 << 5),
		.tb_register = STATR,
		.bp_len = 3,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), 0}}
	},
	{0x0020ba17, {
		.manufacturer = "micron",
		.model = "N25Q64",
		.nr_sector = 128,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = true,
		.tb_otp = false,
		.tb_offset = (1 << 5),
		.tb_register = STATR,
		.bp_len = 4,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), (1 << 6)}}
	},
  {0x0020bb18, {
		/* https://www.micron.com/-/media/client/global/documents/products/data-sheet/nor-flash/serial-nor/n25q/n25q_128mb_1_8v_65nm.pdf */
		/* MT25QU128ABA has the same JEDEC-standard signature: https://media-www.micron.com/-/media/client/global/documents/products/data-sheet/nor-flash/serial-nor/mt25q/die-rev-a/mt25q_qlhs_u_128_aba_0.pdf */
		/* Differences: https://media-www.micron.com/-/media/client/global/documents/products/technical-note/nor-flash/tn2501_migrating_n25q_to_mt25ql.pdf */
		.manufacturer = "micron",
		.model = "MT25/N25Q128_1_8V",
		.nr_sector = 256,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = true,
		.tb_otp = false,
		.tb_offset = (1 << 5),
		.tb_register = STATR,
		.bp_len = 4,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), (1 << 6)}}
	},
	{0x0020ba18, {
		/* https://media-www.micron.com/-/media/client/global/documents/products/data-sheet/nor-flash/serial-nor/n25q/n25q_128mb_3v_65nm.pdf */
		.manufacturer = "micron",
		.model = "N25Q128_3V",
		.nr_sector = 256,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = true,
		.tb_otp = false,
		.tb_offset = (1 << 5),
		.tb_register = STATR,
		.bp_len = 4,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), (1 << 6)}}
	},
	{0x0020ba19, {
		.manufacturer = "micron",
		.model = "N25Q256",
		.nr_sector = 512,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = true,
		.tb_otp = false,
		.tb_offset = (1 << 5),
		.tb_register = STATR,
		.bp_len = 4,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), (1 << 6)}}
	},
	{0x0020bb19, {
		.manufacturer = "micron",
		.model = "N25Q256A",
		.nr_sector = 512,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = true,
		.tb_otp = false,
		.tb_offset = (1 << 5),
		.tb_register = STATR,
		.bp_len = 4,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), (1 << 6)}}
	},
	{0x0020bb21, {
		.manufacturer = "micron",
		.model = "MT25QU01G",
		.nr_sector = 2048,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = true,
		.tb_otp = false,
		.tb_offset = (1 << 5),
		.tb_register = STATR,
		.bp_len = 4,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), (1 << 6)}}
	},
	{0x0020bb22, {
		.manufacturer = "micron",
		.model = "MT25QU02G",
		.nr_sector = 4096,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = true,
		.tb_otp = false,
		.tb_offset = (1 << 5),
		.tb_register = STATR,
		.bp_len = 4,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), (1 << 6)}}
	},
	{0xbf258d, {
		.manufacturer = "microchip",
		.model = "SST25VF040B",
		.nr_sector = 8,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = false,
		.tb_otp = false,
		.tb_offset = 0,
		.tb_register = NONER,
		.bp_len = 4,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), (1 << 5)}}
	},
	{0xBF2642, {
		.manufacturer = "microchip",
		.model = "SST26VF032B",
		.nr_sector = 64,
		.sector_erase = false,
		.subsector_erase = true,
		.has_extended = false,
		.tb_otp = false,
		.tb_offset = 0,
		.tb_register = NONER,
		.bp_len = 0,
		.bp_offset = {0, 0, 0, 0}}
	},
	{0xBF2643, {
		.manufacturer = "microchip",
		.model = "SST26VF064B",
		.nr_sector = 128,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = false,
		.tb_otp = false,
		.tb_offset = 0,
		.tb_register = NONER,
		.bp_len = 0,
		.bp_offset = {0, 0, 0, 0}}
	},
	{0x9d6016, {
		.manufacturer = "ISSI",
		.model = "IS25LP032",
		.nr_sector = 64,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = false,
		.tb_otp = true,
		.tb_offset = (1 << 1),
		.tb_register = FUNCR,
		.bp_len = 4,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), (1 << 5)}}
	},
	{0x9d6017, {
		.manufacturer = "ISSI",
		.model = "IS25LP064",
		.nr_sector = 128,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = false,
		.tb_otp = true,
		.tb_offset = (1 << 1),
		.tb_register = FUNCR,
		.bp_len = 4,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), (1 << 5)}}
	},
	{0x9d6018, {
		.manufacturer = "ISSI",
		.model = "IS25LP128",
		.nr_sector = 256,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = false,
		.tb_otp = true,
		.tb_offset = (1 << 1),
		.tb_register = FUNCR,
		.bp_len = 4,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), (1 << 5)}}
	},
	/* https://www.issi.com/WW/pdf/IS25LP(WP)256D.pdf */
	{0x9d6019, {
		.manufacturer = "ISSI",
		.model = "IS25LP256",
		.nr_sector = 512,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = false,
		.tb_otp = true,
		.tb_offset = (1 << 1),
		.tb_register = FUNCR,
		.bp_len = 4,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), (1 << 5)}}
	},
	{0xc22016, {
	/* https://www.macronix.com/Lists/Datasheet/Attachments/8933/MX25L3233F,%203V,%2032Mb,%20v1.7.pdf */
		.manufacturer = "Macronix",
		.model = "MX25L3233F",
		.nr_sector = 256,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = false,
		.tb_otp = true,
		.tb_offset = (1 << 3),
		.tb_register = CONFR,
		.bp_len = 5,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), (1 << 5)}}
	},
	{0xc22018, {
	/* https://www.macronix.com/Lists/Datasheet/Attachments/8934/MX25L12833F,%203V,%20128Mb,%20v1.0.pdf */
		.manufacturer = "Macronix",
		.model = "MX25L12833",
		.nr_sector = 256,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = false,
		.tb_otp = true,
		.tb_offset = (1 << 3),
		.tb_register = CONFR,
		.bp_len = 5,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), (1 << 5)}}
	},
  {0xc2201a, {
      /* https://www.macronix.com/Lists/Datasheet/Attachments/8745/MX25L51245G,%203V,%20512Mb,%20v1.7.pdf */
		.manufacturer = "Macronix",
		.model = "MX25L51245G",
		.nr_sector = 1024,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = false,
		.tb_otp = true,
		.tb_offset = (1 << 3),
		.tb_register = CONFR,
		.bp_len = 5,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), (1 << 5)}}
	},
	{0xc22817, {
	/* https://www.macronix.com/Lists/Datasheet/Attachments/8868/MX25R6435F,%20Wide%20Range,%2064Mb,%20v1.6.pdf */
		.manufacturer = "Macronix",
		.model = "MX25R6435F",
		.nr_sector = 128,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = false,
		.tb_otp = true,
		.tb_offset = (1 << 3),
		.tb_register = CONFR,
		.bp_len = 4,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), (1 << 5)}}
	},
	{0xef4014, {
	/* https://cdn-shop.adafruit.com/datasheets/W25Q80BV.pdf */
		.manufacturer = "Winbond",
		.model = "W25Q80BV",
		.nr_sector = 16,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = false,
		.tb_otp = false,
		.tb_offset = (1 << 5),
		.tb_register = STATR,
		.bp_len = 3,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), 0}}
	},
	{0xef4015, {
		.manufacturer = "Winbond",
		.model = "W25Q16",
		.nr_sector = 32,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = false,
		.tb_otp = false,
		.tb_offset = (1 << 5),
		.tb_register = STATR,
		.bp_len = 3,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), 0}}
	},
	{0xef4016, {
		.manufacturer = "Winbond",
		.model = "W25Q32",
		.nr_sector = 64,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = false,
		.tb_otp = false,
		.tb_offset = (1 << 5),
		.tb_register = STATR,
		.bp_len = 3,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), 0}}
	},
	{0xef4017, {
		.manufacturer = "Winbond",
		.model = "W25Q64",
		.nr_sector = 128,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = false,
		.tb_otp = false,
		.tb_offset = (1 << 5),
		.tb_register = STATR,
		.bp_len = 3,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), 0}}
	},
	{0xef4018, {
		.manufacturer = "Winbond",
		.model = "W25Q128",
		.nr_sector = 256,
		.sector_erase = true,
		.subsector_erase = true,
		.has_extended = false,
		.tb_otp = false,
		.tb_offset = (1 << 5),
		.tb_register = STATR,
		.bp_len = 3,
		.bp_offset = {(1 << 2), (1 << 3), (1 << 4), 0}}
	},
        {0xba6015, {
                .manufacturer = "Zetta",
                .model = "ZD25WQ16CSIGT",
                .nr_sector = 32,
                .sector_erase = true,
                .subsector_erase = true,
                .has_extended = false,
                .tb_otp = false,
                .tb_offset = (1 << 5),
                .tb_register = STATR,
                .bp_len = 3,
                .bp_offset = {(1 << 2), (1 << 3), (1 << 4), 0}}
        },

};

#endif  // SRC_SPIFLASHDB_HPP_
