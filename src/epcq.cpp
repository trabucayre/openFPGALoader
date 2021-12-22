// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "epcq.hpp"

#define RD_STATUS_REG       0x05
#  define STATUS_REG_WEL    (0x01 << 1)
#  define STATUS_REG_WIP    (0x01 << 0)
#define RD_BYTE_REG         0x03
#define RD_DEV_ID_REG       0x9F
#define RD_SILICON_ID_REG   0xAB
#define RD_FAST_READ_REG    0x0B
/* TBD */
#define WR_ENABLE_REG       0x06
#define WR_DISABLE_REG      0x04
#define WR_STATUS_REG       0x01
#define WR_BYTES_REG        0x02
/* TBD */
#define ERASE_BULK_REG      0xC7
#define ERASE_SECTOR_REG    0xD8
#define ERASE_SUBSECTOR_REG 0x20
#define RD_SFDP_REG_REG     0x5A

#define SECTOR_SIZE			65536

void EPCQ::dumpJICFile(char *jic_file, char *out_file, size_t max_len)
{
	int offset = 0xA1;
	unsigned char c;
	size_t i = 0;

	FILE *jic = fopen(jic_file, "r");
	fseek(jic, offset, SEEK_SET);
	FILE *out = fopen(out_file, "w");
	for (i=0; i < max_len && (1 == fread(&c, 1, 1, jic)); i++) {
		fprintf(out, "%zx %x\n", i, c);
	}
	fclose(jic);
	fclose(out);
}

void EPCQ::read_id()
{
	unsigned char rx_buf[5];
	/* read EPCQ device id */
	/* 2 dummy_byte + 1byte */
	_spi->spi_put(0x9F, NULL, rx_buf, 3);
	_device_id = rx_buf[2];
	if (_verbose)
		printf("device id 0x%x attendu 0x15\n", _device_id);
	/* read EPCQ silicon id */
	/* 3 dummy_byte + 1 byte*/
	_spi->spi_put(0xAB, NULL, rx_buf, 4);
	_silicon_id = rx_buf[3];
	if (_verbose)
		printf("silicon id 0x%x attendu 0x14\n", _silicon_id);
}

void EPCQ::reset()
{
	printf("reset\n");
	_spi->spi_put(0x66, NULL, NULL, 0);
	_spi->spi_put(0x99, NULL, NULL, 0);
}

EPCQ::EPCQ(SPIInterface *spi, bool unprotect_flash, int8_t verbose):
	SPIFlash(spi, unprotect_flash, verbose), _device_id(0), _silicon_id(0)
{}

EPCQ::~EPCQ()
{}
