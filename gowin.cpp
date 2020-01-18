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
#include <stdint.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>

#include <iostream>

#include "ftdijtag.hpp"
#include "gowin.hpp"
#include "progressBar.hpp"
#include "display.hpp"
#include "fsparser.hpp"

using namespace std;

#define NOOP				0x02
#define ERASE_SRAM			0x05
#define READ_SRAM			0x03
#define XFER_DONE			0x09
#define READ_IDCODE			0x11
#define INIT_ADDR			0x12
#define READ_USERCODE		0x13
#define CONFIG_ENABLE		0x15
#define XFER_WRITE			0x17
#define CONFIG_DISABLE		0x3A
#define RELOAD				0x3C
#define STATUS_REGISTER		0x41
#  define STATUS_CRC_ERROR			(1 << 0)
#  define STATUS_BAD_COMMAND		(1 << 1)
#  define STATUS_ID_VERIFY_FAILED	(1 << 2)
#  define STATUS_TIMEOUT			(1 << 3)
#  define STATUS_MEMORY_ERASE		(1 << 5)
#  define STATUS_PREAMBLE			(1 << 6)
#  define STATUS_SYSTEM_EDIT_MODE	(1 << 7)
#  define STATUS_PRG_SPIFLASH_DIRECT (1 << 8)
#  define STATUS_NON_JTAG_CNF_ACTIVE (1 << 10)
#  define STATUS_BYPASS				(1 << 11)
#  define STATUS_GOWIN_VLD			(1 << 12)
#  define STATUS_DONE_FINAL			(1 << 13)
#  define STATUS_SECURITY_FINAL		(1 << 14)
#  define STATUS_READY				(1 << 15)
#  define STATUS_POR				(1 << 16)
#  define STATUS_FLASH_LOCK			(1 << 17)
#define EF_PROGRAM			0x71
#define EFLASH_ERASE		0x75

Gowin::Gowin(FtdiJtag *jtag, const string filename, bool flash_wr, bool sram_wr,
		bool verbose): Device(jtag, filename, verbose)
{
	_fs = NULL;
	if (_filename != "") {
		if (_file_extension == "fs") {
			if (flash_wr && sram_wr)
				throw std::runtime_error("both write-flash and write-sram can't be set");
			if (flash_wr)
				_mode = Device::FLASH_MODE;
			else
				_mode = Device::MEM_MODE;
			_fs = new FsParser(_filename, _mode == Device::MEM_MODE, _verbose);
			_fs->parse();
		} else {
			throw std::runtime_error("incompatible file format");
		}
	}
	_jtag->setClkFreq(2500000, 0);
}

Gowin::~Gowin()
{
	if (_fs)
		delete _fs;
}

void Gowin::reset()
{
	wr_rd(RELOAD, NULL, 0, NULL, 0);
	wr_rd(NOOP, NULL, 0, NULL, 0);
}

void Gowin::programFlash()
{
	uint8_t *data;
	int length;

	data = _fs->getData();
	length = _fs->getLength();

	/* erase SRAM */
	if (!EnableCfg())
		return;
	eraseSRAM();
	wr_rd(XFER_DONE, NULL, 0, NULL, 0);
	wr_rd(NOOP, NULL, 0, NULL, 0);
	if (!DisableCfg())
		return;

	if (!EnableCfg())
		return;
	if (!eraseFLASH())
		return;
	if (!DisableCfg())
		return;
	wr_rd(RELOAD, NULL, 0, NULL, 0);
	wr_rd(NOOP, NULL, 0, NULL, 0);
	/* test status a faire */
	if (!flashFLASH(data, length))
		return;
	if (!DisableCfg())
		return;
	wr_rd(RELOAD, NULL, 0, NULL, 0);
	wr_rd(NOOP, NULL, 0, NULL, 0);
	if (_verbose)
		printInfo("%08x\n", readUserCode());
}

void Gowin::program(unsigned int offset)
{
	(void) offset;

	uint8_t *data;
	uint32_t status;
	int length;

	if (_filename == "" || !_fs)
		return;

	if (_mode == FLASH_MODE) {
		programFlash();
		return;
	}

	if (_verbose) {
		displayReadReg(readStatusReg());
	}

	data = _fs->getData();
	length = _fs->getLength();

	wr_rd(READ_IDCODE, NULL, 0, NULL, 0);

	/* erase SRAM */
	if (!EnableCfg())
		return;
	eraseSRAM();
	if (!DisableCfg())
		return;

	/* load bitstream in SRAM */
	if (!EnableCfg())
		return;
	if (!flashSRAM(data, length))
		return;
	if (!DisableCfg())
		return;

	/* check if file checksum == checksum in FPGA */
	status = readUserCode();
	if (_fs->checksum() != status)
		printError("SRAM Flash: FAIL");
	else
		printSuccess("SRAM Flash: Success");
	if (_verbose)
		displayReadReg(readStatusReg());
}

bool Gowin::EnableCfg()
{
	wr_rd(CONFIG_ENABLE, NULL, 0, NULL, 0);
	return pollFlag(STATUS_SYSTEM_EDIT_MODE, STATUS_SYSTEM_EDIT_MODE);
}

bool Gowin::DisableCfg()
{
	wr_rd(CONFIG_DISABLE, NULL, 0, NULL, 0);
	wr_rd(NOOP, NULL, 0, NULL, 0);
	return pollFlag(STATUS_SYSTEM_EDIT_MODE, 0);
}

int Gowin::idCode()
{
	uint8_t device_id[4];
	wr_rd(READ_IDCODE, NULL, 0, device_id, 4);
	return device_id[3] << 24 |
					device_id[2] << 16 |
					device_id[1] << 8  |
					device_id[0];
}

uint32_t Gowin::readStatusReg()
{
	uint32_t reg;
	uint8_t rx[4];
	wr_rd(STATUS_REGISTER, NULL, 0, rx, 4);
	reg = rx[3] << 24 | rx[2] << 16 | rx[1] << 8 | rx[0];
	return reg;
}

uint32_t Gowin::readUserCode()
{
	uint8_t rx[4];
	wr_rd(READ_USERCODE, NULL, 0, rx, 4);
	return rx[3] << 24 | rx[2] << 16 | rx[1] << 8 | rx[0];
}

bool Gowin::wr_rd(uint8_t cmd,
					uint8_t *tx, int tx_len,
					uint8_t *rx, int rx_len,
					bool verbose)
{
	int xfer_len = rx_len;
	if (tx_len > rx_len)
		xfer_len = tx_len;

	uint8_t xfer_tx[xfer_len], xfer_rx[xfer_len];
	bzero(xfer_tx, xfer_len);
	int i;
	if (tx != NULL) {
		for (i = 0; i < tx_len; i++)
			xfer_tx[i] = tx[i];
	}

	_jtag->shiftIR(&cmd, NULL, 8);
	_jtag->toggleClk(6);
	if (rx || tx) {
		_jtag->shiftDR(xfer_tx, (rx) ? xfer_rx : NULL, 8 * xfer_len);
		_jtag->toggleClk(6);
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

void Gowin::displayReadReg(uint32_t dev)
{
	printf("displayReadReg %08x\n", dev);
	if (dev & STATUS_CRC_ERROR)
		printf("\tCRC Error\n");
	if (dev & STATUS_BAD_COMMAND)
		printf("\tBad Command\n");
	if (dev & STATUS_ID_VERIFY_FAILED)
		printf("\tID Verify Failed\n");
	if (dev & STATUS_TIMEOUT)
		printf("\tTimeout\n");
	if (dev & STATUS_MEMORY_ERASE)
		printf("\tMemory Erase\n");
	if (dev & STATUS_PREAMBLE)
		printf("\tPreamble\n");
	if (dev & STATUS_SYSTEM_EDIT_MODE)
		printf("\tSystem Edit Mode\n");
	if (dev & STATUS_PRG_SPIFLASH_DIRECT)
		printf("\tProgram spi flash directly\n");
	if (dev & STATUS_NON_JTAG_CNF_ACTIVE)
		printf("\tNon-jtag is active\n");
	if (dev & STATUS_BYPASS)
		printf("\tBypass\n");
	if (dev & STATUS_GOWIN_VLD)
		printf("\tGowin VLD\n");
	if (dev & STATUS_DONE_FINAL)
		printf("\tDone Final\n");
	if (dev & STATUS_SECURITY_FINAL)
		printf("\tSecurity Final\n");
	if (dev & STATUS_READY)
		printf("\tReady\n");
	if (dev & STATUS_POR)
		printf("\tPOR\n");
	if (dev & STATUS_FLASH_LOCK)
		printf("\tFlash Lock\n");
}

bool Gowin::pollFlag(uint32_t mask, uint32_t value)
{
	uint32_t status;
	int timeout = 0;
	do {
		status = readStatusReg();
		if (_verbose)
			printf("pollFlag: %x\n", status);
		if (timeout == 100000000){
			printError("timeout");
			return false;
		}
		timeout++;
	} while ((status & mask) != value);

	return true;
}

/* TN653 p. 17-21 */
bool Gowin::flashFLASH(uint8_t *data, int length)
{
	uint8_t tx[4] = {0x4E, 0x31, 0x57, 0x47};
	uint8_t tmp[4];
	uint32_t addr;
	int nb_iter;
	int byte_length = length / 8;
	uint8_t tt[39];
	bzero(tt, 39);

	ProgressBar progress("Flash SRAM", byte_length, 50);
	_jtag->go_test_logic_reset();

	/* we have to send
	 * bootcode a X=0, Y=0 (4Bytes)
	 * 5 x 32 dummy bits
	 * full bitstream
	 */
	int buffer_length = byte_length+(6*4);
	unsigned char buffer[byte_length+(6*4)] = {
									0x47, 0x57, 0x31, 0x4E,
									0xff, 0xff , 0xff, 0xff,
									0xff, 0xff , 0xff, 0xff,
									0xff, 0xff , 0xff, 0xff,
									0xff, 0xff , 0xff, 0xff,
									0xff, 0xff , 0xff, 0xff};
	memcpy(buffer+6*4, data, byte_length);

	int nb_xpage = buffer_length/256;
	if (nb_xpage * 256 != buffer_length)
		nb_xpage++;

	for (int i=0, xpage=0; xpage < nb_xpage; i+=(nb_iter*4), xpage++) {
		wr_rd(CONFIG_ENABLE, NULL, 0, NULL, 0);
		wr_rd(EF_PROGRAM, NULL, 0, NULL, 0);
		_jtag->read_write(tt, NULL, 312, 0);
		addr = xpage << 6;
		tmp[3] = 0xff&(addr >> 24);
		tmp[2] = 0xff&(addr >> 16);
		tmp[1] = 0xff&(addr >> 8);
		tmp[0] = addr&0xff;
		_jtag->shiftDR(tmp, NULL, 32);
		_jtag->read_write(tt, NULL, 312, 0);

		int xoffset = xpage * 256;  // each page containt 256Bytes
		if (xoffset + 256 > buffer_length)
			nb_iter = (buffer_length-xoffset) / 4;
		else
			nb_iter = 64;

		for (int ypage = 0; ypage < nb_iter; ypage++) {
			unsigned char *t = buffer+xoffset + 4*ypage;
			for (int x=0; x < 4; x++)
				tx[3-x] = t[x];
			_jtag->shiftDR(tx, NULL, 32);
			_jtag->read_write(tt, NULL, 40, 0);
		}
		progress.display(i);
	}
	/* 2.2.6.6 */
	_jtag->set_state(FtdiJtag::RUN_TEST_IDLE);

	progress.done();
	return true;
}

/* TN653 p. 9 */
bool Gowin::flashSRAM(uint8_t *data, int length)
{
	int tx_len, tx_end;
	int byte_length = length / 8;

	ProgressBar progress("Flash SRAM", byte_length, 50);

	/* 2.2.6.4 */
	wr_rd(XFER_WRITE, NULL, 0, NULL, 0);

	/* 2.2.6.5 */
	_jtag->set_state(FtdiJtag::SHIFT_DR);

	for (int i=0; i < byte_length; i+=256) {
		if (i + 256 > byte_length) {  // last packet with some size
			tx_len = (byte_length - i) * 8;
			tx_end = 1;  // to move in EXIT1_DR
		} else {
			tx_len = 256 * 8;
			tx_end = 0;
		}
		_jtag->read_write(data+i, NULL, tx_len, tx_end);
		_jtag->flush();
		progress.display(i);
	}
	/* 2.2.6.6 */
	_jtag->set_state(FtdiJtag::RUN_TEST_IDLE);

	/* p.15 fig 2.11 */
	wr_rd(XFER_DONE, NULL, 0, NULL, 0);
	if (pollFlag(STATUS_DONE_FINAL, STATUS_DONE_FINAL)) {
		progress.done();
		return true;
	} else {
		progress.fail();
		return false;
	}
}

/* Erase SRAM:
 * TN653 p.14-17
 */
bool Gowin::eraseFLASH()
{
	unsigned char tx[4] = {0, 0, 0, 0};
	printInfo("erase Flash ", false);
	wr_rd(EFLASH_ERASE, NULL, 0, NULL, 0);
	_jtag->set_state(FtdiJtag::RUN_TEST_IDLE);
	_jtag->shiftDR(tx, NULL, 32);
	/* TN653 specifies to wait for 120ms with
	 * there are no bit in status register to specify
	 * when this operation is done so we need to wait
	 */
	usleep(120000);
	printSuccess("Done");
	return true;
}

/* Erase SRAM:
 * TN653 p.9-10, 14 and 31
 */
bool Gowin::eraseSRAM()
{
	printInfo("erase SRAM ", false);
	wr_rd(ERASE_SRAM, NULL, 0, NULL, 0);
	wr_rd(NOOP, NULL, 0, NULL, 0);

	/* TN653 specifies to wait for 4ms with
	 * clock generated but
	 * status register bit MEMORY_ERASE goes low when ERASE_SRAM
	 * is send and goes high after erase
	 * this check seems enough
	 */
	if (pollFlag(STATUS_MEMORY_ERASE, STATUS_MEMORY_ERASE)) {
		printSuccess("Done");
		return true;
	} else {
		printError("FAIL");
		return false;
	}
}
