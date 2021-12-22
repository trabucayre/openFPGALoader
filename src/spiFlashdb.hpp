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
	STATR = 0, /* status register */
	FUNCR = 1, /* function register */
	CONFR = 2  /* configuration register */
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
	{0x012018, {
		.manufacturer = "spansion",
		.model = "S25FL128S",
		.nr_sector = 256,
		.sector_erase = true,
		.subsector_erase = false,
		.has_extended = true,
		.tb_otp = false,
		.tb_offset = (1 << 5),
		.tb_register = CONFR,
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
	{0x0020ba18, {
		.manufacturer = "micron",
		.model = "N25Q128",
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
};

#endif  // SRC_SPIFLASHDB_HPP_
