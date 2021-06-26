// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>

#include "ftdipp_mpsse.hpp"
#include "progressBar.hpp"
#include "spiFlash.hpp"
#include "spiInterface.hpp"

/* read/write status register : 0B addr + 0 dummy */
#define FLASH_WRSR     0x01
#define FLASH_RDSR     0x05
#	define FLASH_RDSR_WIP	(0x01)
#	define FLASH_RDSR_WEL	(0x02)
/* flash program */
#define FLASH_PP       0x02
/* write [en|dis]able : 0B addr + 0 dummy */
#define FLASH_WRDIS    0x04
#define FLASH_WREN     0x06
/* Read OTP : 3 B addr + 8 clk cycle*/
#define FLASH_ROTP     0x4B
#define FLASH_POWER_UP 0xAB
#define FLASH_POWER_DOWN 0xB9
/* read/write non volatile register: 0B addr + 0 dummy */
#define FLASH_RDNVCR   0xB5
#define FLASH_WRNVCR   0xB1
/* read/write volatile register */
#define FLASH_RDVCR    0x85
#define FLASH_WRVCR    0x81
/* bulk erase */
#define FLASH_BE       0xC7
/* sector (64kb) erase */
#define FLASH_SE       0xD8
/* read/write lock register : 3B addr + 0 dummy */
#define FLASH_WRLR     0xE5
#define FLASH_RDLR     0xE8
/* read/clear flag status register : 0B addr + 0 dummy */
#define FLASH_CLFSR    0x50
#define FLASH_RFSR     0x70
/* */
#define FLASH_WRVECR   0x61
#define FLASH_RDVECR   0x65

/* microchip SST26VF032B / SST26VF032BA */
/* Read Block Protection Register */
#define FLASH_RBPR 0x72
/* Global Block Protection unlock */
#define FLASH_ULBPR 0x98

SPIFlash::SPIFlash(SPIInterface *spi, int8_t verbose):_spi(spi), _verbose(verbose)
{
}

int SPIFlash::bulk_erase()
{
	if (write_enable() == -1)
		return -1;
	_spi->spi_put(FLASH_BE, NULL, NULL, 0);
	return _spi->spi_wait(FLASH_RDSR, FLASH_RDSR_WIP, 0x00, 100000, true);
}

int SPIFlash::sector_erase(int addr)
{
	uint8_t tx[4];
	tx[0] = (uint8_t)(FLASH_SE           );
	tx[1] = (uint8_t)(0xff & (addr >> 16));
	tx[2] = (uint8_t)(0xff & (addr >>  8));
	tx[3] = (uint8_t)(0xff & (addr      ));
	_spi->spi_put(tx, NULL, 4);
	return 0;
}

int SPIFlash::sectors_erase(int base_addr, int size)
{
	int ret = 0;
	int start_addr = base_addr;
	int end_addr = (base_addr + size + 0xffff) & ~0xffff;
	ProgressBar progress("Erasing", end_addr, 50, _verbose < 0);

	for (int addr = start_addr; addr < end_addr; addr += 0x10000) {
		if (write_enable() == -1) {
			ret = -1;
			break;
		}
		if (sector_erase(addr) == -1) {
			ret = -1;
			break;
		}
		if (_spi->spi_wait(FLASH_RDSR, FLASH_RDSR_WIP, 0x00, 100000, false) == -1) {
			ret = -1;
			break;
		}
		progress.display(addr);
	}
	if (ret == 0)
		progress.done();
	else
		progress.fail();

	return ret;
}

int SPIFlash::write_page(int addr, uint8_t *data, int len)
{
	uint8_t tx[len+3];
	tx[0] = (uint8_t)(0xff & (addr >> 16));
	tx[1] = (uint8_t)(0xff & (addr >>  8));
	tx[2] = (uint8_t)(0xff & (addr      ));

	memcpy(tx+3, data, len);

	if (write_enable() == -1)
		return -1;

	_spi->spi_put(FLASH_PP, tx, NULL, len+3);
	return _spi->spi_wait(FLASH_RDSR, FLASH_RDSR_WIP, 0x00, 1000);
}

int SPIFlash::read(int base_addr, uint8_t *data, int len)
{
	uint8_t tx[len+3];
	uint8_t rx[len+3];
	tx[0] = (uint8_t)(0xff & (base_addr >> 16));
	tx[1] = (uint8_t)(0xff & (base_addr >>  8));
	tx[2] = (uint8_t)(0xff & (base_addr      ));

	int ret = _spi->spi_put(0x03, tx, rx, len+3);
	if (ret == 0)
		memcpy(data, rx+3, len);
	else
		printf("error\n");
	return ret;
}

int SPIFlash::erase_and_prog(int base_addr, uint8_t *data, int len)
{
	if (_jedec_id == 0)
		read_id();

	/* check Block Protect Bits */
	if (_jedec_id == 0xbf2642bf) { // microchip SST26VF032B
		if (!global_unlock())
			return -1;
	} else {
		uint8_t status = read_status_reg();
		if ((status & 0x1c) !=0) {
			if (write_enable() != 0)
				return -1;
			if (disable_protection() != 0)
				return -1;
		}
	}

	ProgressBar progress("Writing", len, 50, _verbose < 0);
	if (sectors_erase(base_addr, len) == -1)
		return -1;

	uint8_t *ptr = data;
	int size = 0;
	for (int addr = 0; addr < len; addr += size, ptr+=size) {
		size = (addr + 256 > len)?(len-addr) : 256;
		if (write_page(base_addr + addr, ptr, size) == -1)
			return -1;
		progress.display(addr);
	}
	progress.done();
	return 0;
}

void SPIFlash::reset()
{
	uint8_t data[8];
	memset(data, 0xff, 8);
	_spi->spi_put(0xff, data, NULL, 8);
}

void SPIFlash::read_id()
{
	int len = 4;
	uint8_t rx[512];
	bool has_edid = false;

	_spi->spi_put(0x9F, NULL, rx, 4);
	_jedec_id = 0;
	for (int i=0; i < 4; i++) {
		_jedec_id = _jedec_id << 8;
		_jedec_id |= (0x00ff & (int)rx[i]);
		if (_verbose > 0)
			printf("%x ", rx[i]);
	}

	if (_verbose > 0)
		printf("read %x\n", _jedec_id);

	/* read extented */
	if ((_jedec_id & 0xff) != 0) {
		has_edid = true;
		len += (_jedec_id & 0x0ff);
		_spi->spi_put(0x9F, NULL, rx, len);
	}

	/* must be 0x20BA1810 ... */

	printf("Detail: \n");
	printf("Jedec ID          : %02x\n", rx[0]);
	printf("memory type       : %02x\n", rx[1]);
	printf("memory capacity   : %02x\n", rx[2]);
	if (has_edid) {
		printf("EDID + CFD length : %02x\n", rx[3]);
		printf("EDID              : %02x%02x\n", rx[5], rx[4]);
		printf("CFD               : ");
		if (_verbose > 0) {
			for (int i = 6; i < len; i++)
				printf("%02x ", rx[i]);
			printf("\n");
		} else {
			printf("\n");
		}
	}
}

uint8_t SPIFlash::read_status_reg()
{
	uint8_t rx;
	_spi->spi_put(FLASH_RDSR, NULL, &rx, 1);
	if (_verbose > 0) {
		printf("RDSR : %02x\n", rx);
		printf("WIP  : %d\n", rx&0x01);
		printf("WEL  : %d\n", (rx>>1)&0x01);
		printf("BP   : %x\n", (((rx>>6)&0x01)<<3) | ((rx >> 2) & 0x07));
		printf("TB   : %d\n", (((rx>>5)&0x01)));
		printf("SRWD : %d\n", (((rx>>7)&0x01)));
	}
	return rx;
}

uint16_t SPIFlash::readNonVolatileCfgReg()
{
	uint8_t rx[2];
	_spi->spi_put(FLASH_RDNVCR, NULL, rx, 2);
	if (_verbose > 0)
		printf("Non Volatile %x %x\n", rx[0], rx[1]);
	return (rx[1] << 8) | rx[0];
}

uint16_t SPIFlash::readVolatileCfgReg()
{
	uint8_t rx[2];
	_spi->spi_put(FLASH_RDVCR, NULL, rx, 2);
	if (_verbose > 0)
		printf("Volatile %x %x\n", rx[0], rx[1]);
	return (rx[1] << 8) | rx[0];
}

void SPIFlash::power_up()
{
	_spi->spi_put(FLASH_POWER_UP, NULL, NULL, 0);
}

void SPIFlash::power_down()
{
	_spi->spi_put(FLASH_POWER_DOWN, NULL, NULL, 0);
}

int SPIFlash::write_enable()
{
	_spi->spi_put(FLASH_WREN, NULL, NULL, 0);
	/* wait WEL */
	if (_spi->spi_wait(FLASH_RDSR, FLASH_RDSR_WEL, FLASH_RDSR_WEL, 1000)) {
		printf("write en: Error\n");
		return -1;
	}

	return 0;
}

int SPIFlash::write_disable()
{
	_spi->spi_put(FLASH_WRDIS, NULL, NULL, 0);
	/* wait ! WEL */
	int ret = _spi->spi_wait(FLASH_RDSR, FLASH_RDSR_WEL, 0x00, 1000);
	if (ret == -1)
		printf("write disable: Error\n");
	else if (_verbose > 0)
		printf("write disable: Success\n");
	return ret;
}

int SPIFlash::disable_protection()
{
	uint8_t data = 0x00;
	_spi->spi_put(FLASH_WRSR, &data, NULL, 1);
	if (_spi->spi_wait(FLASH_RDSR, 0xff, 0, 1000) < 0)
		return -1;

	/* read status */
	if (read_status_reg() != 0) {
		std::cout << "disable protection failed" << std::endl;
		return -1;
	} else
		return 0;
}

/* microchip SST26VF032B has a dedicated register
 * to read sectors (un)lock status and another one to unlock
 * sectors
 */

bool SPIFlash::global_unlock()
{
	if (write_enable() != 0)
		return false;
	_spi->spi_put(FLASH_ULBPR, NULL, NULL, 0);

	if (_spi->spi_wait(FLASH_RDSR, 0xff, 0, 1000) < 0)
		return false;

	/* check if all sectors are unlocked */
	uint8_t rx2[10];
	_spi->spi_put(FLASH_RBPR, NULL, rx2, 10);
	printf("Non Volatile\n");
	for (int i = 0; i < 10; i++) {
		if (rx2[i] != 0)
			return false;
	}
	return true;
}
