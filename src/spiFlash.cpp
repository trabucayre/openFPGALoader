/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>

#include "jtag.hpp"
#include "ftdipp_mpsse.hpp"
#include "progressBar.hpp"
#include "spiFlash.hpp"

#define USER1 0x02

static uint8_t reverseByte(uint8_t src)
{
	uint8_t dst = 0;
	for (int i=0; i < 8; i++) {
		dst = (dst << 1) | (src & 0x01);
		src >>= 1;
	}
	return dst;
}


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
#define FLASH_WRNVCR   0x81
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
#define FLASH_WRVCR    0x81
#define FLASH_RDVCR    0x85
/* */
#define FLASH_WRVECR   0x61
#define FLASH_RDVECR   0x65

SPIFlash::SPIFlash(Jtag *jtag, bool verbose):_jtag(jtag), _verbose(verbose)
{
}

/* 
 * jtag : jtag interface
 * cmd  : opcode for SPI flash
 * tx   : buffer to send
 * rx   : buffer to fill
 * len  : number of byte to send/receive (cmd not comprise)
 *        so to send only a cmd set len to 0 (or omit this param)
 */

void SPIFlash::jtag_write_read(uint8_t cmd,
			uint8_t *tx, uint8_t *rx, uint16_t len)
{
	int xfer_len = len + 1 + ((rx == NULL) ? 0 : 1);
	uint8_t jtx[xfer_len];
	jtx[0] = reverseByte(cmd);
	/* uint8_t jtx[xfer_len] = {reverseByte(cmd)}; */
	uint8_t jrx[xfer_len];
	if (tx != NULL) {
		for (int i=0; i < len; i++)
			jtx[i+1] = reverseByte(tx[i]);
	}
	/* addr BSCAN user1 */
	_jtag->shiftIR(USER1, 6);
	/* send first already stored cmd,
	 * in the same time store each byte
	 * to send next
	 */
	_jtag->shiftDR(jtx, (rx == NULL)? NULL: jrx, 8*xfer_len);

	if (rx != NULL) {
		for (int i=0; i < len; i++)
			rx[i] = reverseByte(jrx[i+1] >> 1) | (jrx[i+2] & 0x01);
	}
}

int SPIFlash::wait(uint8_t mask, uint8_t cond, uint32_t timeout, bool verbose)
{
	uint8_t rx[2];
	uint8_t tmp;
	uint8_t tx = reverseByte(FLASH_RDSR);
	uint32_t count = 0;

	_jtag->shiftIR(USER1, 6, Jtag::UPDATE_IR);
	_jtag->set_state(Jtag::SHIFT_DR);
	_jtag->read_write(&tx, NULL, 8, 0);

	do {
		_jtag->read_write(NULL, rx, 8*2, 0);
		tmp = (reverseByte(rx[0]>>1)) | (0x01 & rx[1]); 
		count ++;
		if (count == timeout){
			printf("timeout: %x %x %x\n", tmp, rx[0], rx[1]);
			break;
		}
		if (tmp & ~0x3) {
			printf("Error: rx %x %x %x\n", tmp, reverseByte(rx[0]), rx[1]);
			count = timeout;
			break;
		}
		if (verbose) {
			printf("%x %x %x %d\n", tmp, mask, cond, count);
		}
	} while ((tmp & mask) != cond);
	_jtag->go_test_logic_reset();

	if (count == timeout) {
		printf("%x\n", tmp);
		std::cout << "wait: Error" << std::endl;
		return -1;
	} else
		return 0;
}

int SPIFlash::bulk_erase()
{
	if (write_enable() == -1)
		return -1;
	jtag_write_read(FLASH_BE, NULL, NULL, 0);
	return wait(FLASH_RDSR_WIP, 0x00, 100000, true);
}

int SPIFlash::sector_erase(int addr)
{
	uint8_t tx[3] = {(uint8_t)(0xff & (addr >> 16)),
								(uint8_t)(0xff & (addr >> 8)),
								(uint8_t)(addr & 0xff)};
	jtag_write_read(FLASH_SE, tx, NULL, 3);
	return 0;
}

int SPIFlash::sectors_erase(int base_addr, int size)
{
	int start_addr = base_addr;
	int end_addr = (size + 0xffff) & ~0xffff;
	ProgressBar progress("Erasing", end_addr, 50);
	for (int addr = start_addr; addr < end_addr; addr += 0x10000) {
		if (write_enable() == -1)
			return -1;
		if (sector_erase(addr) == -1)
			return -1;
		if (wait(FLASH_RDSR_WIP, 0x00, 100000, false) == -1)
			return -1;
		progress.display(addr);
	}
	progress.done();
	return 0;
}

int SPIFlash::write_page(int addr, uint8_t *data, int len)
{
	uint8_t tx[len+3];
	tx[0] = (uint8_t)(0xff & (addr >> 16));
	tx[1] = (uint8_t)(0xff & (addr >> 8));
	tx[2] = (uint8_t)(addr & 0xff);

	/*uint8_t tx[len+3] = {(uint8_t)(0xff & (addr >> 16)),
								(uint8_t)(0xff & (addr >> 8)),
								(uint8_t)(addr & 0xff)};*/
	for (int i=0; i < len; i++) {
		tx[i+3] = data[i];
	}
	if (write_enable() == -1)
		return -1;

	jtag_write_read(FLASH_PP, tx, NULL, len+3);
	return wait(FLASH_RDSR_WIP, 0x00, 1000);
}

int SPIFlash::erase_and_prog(int base_addr, uint8_t *data, int len)
{
	ProgressBar progress("Writing", len, 50);
	if (sectors_erase(0, len) == -1)
		return -1;
	
	uint8_t *ptr = data;
	int size = 0;
	for (int addr = base_addr; addr < len; addr += size, ptr+=size) {
		size = (addr + 256 > len)?(len-addr) : 256;
		if (write_page(addr, ptr, size) == -1)
			return -1;
		progress.display(addr);
	}
	progress.done();
	return 0;
}

void SPIFlash::reset()
{
	uint8_t data[8] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	jtag_write_read(0xff, data, NULL, 8);
}

void SPIFlash::read_id()
{
	int len = 4;
	uint8_t rx[512];

	jtag_write_read(0x9F, NULL, rx, 4);
	int d = 0;
	for (int i=0; i < 4; i++) {
		d = d << 8;
		d |= (0x00ff & (int)rx[i]);
		if (_verbose)
			printf("%x ", rx[i]);
	}

	if (_verbose)
		printf("read %x\n", d);
	/* read extented */
	len += (d & 0x0ff);

	jtag_write_read(0x9F, NULL, rx, len);

	/* must be 0x20BA1810 ... */

	printf("Detail: \n");
	printf("Jedec ID          : %02x\n", rx[0]);
	printf("memory type       : %02x\n", rx[1]);
	printf("memory capacity   : %02x\n", rx[2]);
	printf("EDID + CFD length : %02x\n", rx[3]);
	printf("EDID              : %02x%02x\n", rx[5], rx[4]);
	printf("CFD               : ");
	if (_verbose) {
		for (int i = 6; i < len; i++)
			printf("%02x ", rx[i]);
		printf("\n");
	}
}

uint8_t SPIFlash::read_status_reg()
{
	uint8_t rx;
	jtag_write_read(FLASH_RDSR, NULL, &rx, 1);
	if (_verbose) {
		printf("RDSR : %02x\n", rx);
		printf("WIP  : %d\n", rx&0x01);
		printf("WEL  : %d\n", (rx>>1)&0x01);
		printf("BP   : %x\n", (((rx>>6)&0x01)<<3) | ((rx >> 2) & 0x07));
		printf("TB   : %d\n", (((rx>>5)&0x01)));
		printf("SRWD : %d\n", (((rx>>7)&0x01)));
	}
	return rx;
}

void SPIFlash::power_up()
{
	jtag_write_read(FLASH_POWER_UP, NULL, NULL, 0);
}

void SPIFlash::power_down()
{
	jtag_write_read(FLASH_POWER_DOWN, NULL, NULL, 0);
}

int SPIFlash::write_enable()
{
	jtag_write_read(FLASH_WREN, NULL, NULL, 0);
	/* wait WEL */
	if (wait(FLASH_RDSR_WEL, FLASH_RDSR_WEL, 1000)) {
		printf("write en: Error\n");
		return -1;
	}

	if (_verbose)
		std::cout << "write en: Success" << std::endl;
	return 0;
}

int SPIFlash::write_disable()
{
	jtag_write_read(FLASH_WRDIS, NULL, NULL, 0);
	/* wait ! WEL */
	int ret = wait(FLASH_RDSR_WEL, 0x00, 1000);
	if (ret == -1)
		printf("write disable: Error\n");
	else if (_verbose)
		printf("write disable: Success\n");
	return ret;
}

int SPIFlash::disable_protection()
{
	uint8_t data = 0x00;
	jtag_write_read(FLASH_WRSR, &data, NULL, 1);
	if (wait(0xff, 0, 1000) < 0)
		return -1;

	/* read status */
	if (read_status_reg() != 0) {
		std::cout << "disable protection failed" << std::endl;
		return -1;
	} else
		return 0;
}
