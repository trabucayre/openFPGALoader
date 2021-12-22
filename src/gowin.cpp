// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <stdexcept>

#include "jtag.hpp"
#include "gowin.hpp"
#include "progressBar.hpp"
#include "display.hpp"
#include "fsparser.hpp"
#include "rawParser.hpp"
#include "spiFlash.hpp"

using namespace std;

#ifdef STATUS_TIMEOUT
// defined in the Windows headers included by libftdi.h
#undef STATUS_TIMEOUT
#endif

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

/* BSCAN spi (external flash) (see below for details) */
/* most common pins def */
#define BSCAN_SPI_SCK           (1 << 1)
#define BSCAN_SPI_CS            (1 << 3)
#define BSCAN_SPI_DI            (1 << 5)
#define BSCAN_SPI_DO            (1 << 7)
#define BSCAN_SPI_MSK           ((0x01 << 6))
/* GW1NSR-4C pins def */
#define BSCAN_GW1NSR_4C_SPI_SCK (1 << 7)
#define BSCAN_GW1NSR_4C_SPI_CS  (1 << 5)
#define BSCAN_GW1NSR_4C_SPI_DI  (1 << 3)
#define BSCAN_GW1NSR_4C_SPI_DO  (1 << 1)
#define BSCAN_GW1NSR_4C_SPI_MSK ((0x01 << 0))

Gowin::Gowin(Jtag *jtag, const string filename, const string &file_type,
		Device::prog_type_t prg_type, bool external_flash,
		bool verify, int8_t verbose): Device(jtag, filename, file_type,
		verify, verbose), is_gw1n1(false), _external_flash(external_flash),
		_spi_sck(BSCAN_SPI_SCK), _spi_cs(BSCAN_SPI_CS),
		_spi_di(BSCAN_SPI_DI), _spi_do(BSCAN_SPI_DO),
		_spi_msk(BSCAN_SPI_MSK)
{
	_fs = NULL;
	uint32_t idcode = _jtag->get_target_device_id();;

	if (prg_type == Device::WR_FLASH)
		_mode = Device::FLASH_MODE;
	else
		_mode = Device::MEM_MODE;

	if (!_file_extension.empty()) {
		if (_file_extension == "fs") {
			try {
				_fs = new FsParser(_filename, _mode == Device::MEM_MODE, _verbose);
			} catch (std::exception &e) {
				throw std::runtime_error(e.what());
			}
		} else {
			/* non fs file is only allowed with external flash */
			if (!external_flash)
				throw std::runtime_error("incompatible file format");
			try {
				_fs = new RawParser(_filename, false);
			} catch (std::exception &e) {
				throw std::runtime_error(e.what());
			}
		}

		printInfo("Parse file ", false);
		if (_fs->parse() == EXIT_FAILURE) {
			printError("FAIL");
			delete _fs;
			throw std::runtime_error("can't parse file");
		} else {
			printSuccess("DONE");
		}

		if (_verbose)
			_fs->displayHeader();

		/* for fs file check match with targeted device */
		if (_file_extension == "fs") {
			string idcode_str = _fs->getHeaderVal("idcode");
			uint32_t fs_idcode = std::stoul(idcode_str.c_str(), NULL, 16);
			if ((fs_idcode & 0x0fffffff) != idcode) {
				throw std::runtime_error("mismatch between target's idcode and fs idcode");
			}
		}
	}
	_jtag->setClkFreq(2500000);

	/* erase and program flash differ for GW1N1 */
	if (idcode == 0x0900281B)
		is_gw1n1 = true;
	/* bscan spi external flash differ for GW1NSR-4C */
	if (idcode == 0x0100981b) {
		_spi_sck = BSCAN_GW1NSR_4C_SPI_SCK;
		_spi_cs  = BSCAN_GW1NSR_4C_SPI_CS;
		_spi_di  = BSCAN_GW1NSR_4C_SPI_DI;
		_spi_do  = BSCAN_GW1NSR_4C_SPI_DO;
		_spi_msk = BSCAN_GW1NSR_4C_SPI_MSK;
	}
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
	int status;
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
	/* test status a faire */
	if (!flashFLASH(data, length))
		return;
	if (_verify)
		printWarn("writing verification not supported");
	if (!DisableCfg())
		return;
	wr_rd(RELOAD, NULL, 0, NULL, 0);
	wr_rd(NOOP, NULL, 0, NULL, 0);

	/* wait for reload */
	usleep(2*150*1000);

	/* check if file checksum == checksum in FPGA */
	status = readUserCode();
	int checksum = static_cast<FsParser *>(_fs)->checksum();
	if (checksum != status) {
		printError("CRC check : FAIL");
		printf("%04x %04x\n", checksum, status);
	} else {
		printSuccess("CRC check: Success");
	}

	if (_verbose)
		displayReadReg(readStatusReg());
}

void Gowin::program(unsigned int offset, bool unprotect_flash)
{
	(void) offset;

	uint8_t *data;
	uint32_t status;
	int length;

	if (_mode == NONE_MODE || !_fs)
		return;

	data = _fs->getData();
	length = _fs->getLength();

	if (_mode == FLASH_MODE) {
		if (!_external_flash) { /* write into internal flash */
			programFlash();
		} else { /* write bitstream into external flash */
			_jtag->setClkFreq(10000000);

			if (!EnableCfg())
				throw std::runtime_error("Error: fail to enable configuration");

			eraseSRAM();
			wr_rd(XFER_DONE, NULL, 0, NULL, 0);
			wr_rd(NOOP, NULL, 0, NULL, 0);

			wr_rd(0x3D, NULL, 0, NULL, 0);

			SPIFlash spiFlash(this, unprotect_flash,
					(_verbose ? 1 : (_quiet ? -1 : 0)));
			spiFlash.reset();
			spiFlash.read_id();
			spiFlash.display_status_reg(spiFlash.read_status_reg());
			if (spiFlash.erase_and_prog(offset, data, length / 8) != 0)
				throw std::runtime_error("Error: write to flash failed");
			if (_verify)
				if (!spiFlash.verify(offset, data, length / 8, 256))
					throw std::runtime_error("Error: flash vefication failed");
			if (!DisableCfg())
				throw std::runtime_error("Error: fail to disable configuration");

			reset();
		}

		return;
	}

	if (_verbose) {
		displayReadReg(readStatusReg());
	}

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
	uint32_t checksum = static_cast<FsParser *>(_fs)->checksum();
	if (checksum != status) {
		printError("SRAM Flash: FAIL");
		printf("%04x %04x\n", checksum, status);
	} else {
		printSuccess("SRAM Flash: Success");
	}
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
	memset(xfer_tx, 0, xfer_len);
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
		_jtag->flush();
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
	memset(tt, 0, 39);

	_jtag->go_test_logic_reset();

	/* we have to send
	 * bootcode a X=0, Y=0 (4Bytes)
	 * 5 x 32 dummy bits
	 * full bitstream
	 */
	int buffer_length = byte_length+(6*4);
	unsigned char bufvalues[]={
									0x47, 0x57, 0x31, 0x4E,
									0xff, 0xff , 0xff, 0xff,
									0xff, 0xff , 0xff, 0xff,
									0xff, 0xff , 0xff, 0xff,
									0xff, 0xff , 0xff, 0xff,
									0xff, 0xff , 0xff, 0xff};

	int nb_xpage = buffer_length/256;
	if (nb_xpage * 256 != buffer_length) {
		nb_xpage++;
		buffer_length = nb_xpage * 256;
	}

	unsigned char buffer[buffer_length];
	/* fill theorical size with 0xff */
	memset(buffer, 0xff, buffer_length);
	/* fill first page with code */
	memcpy(buffer, bufvalues, 6*4);
	/* bitstream just after opcode */
	memcpy(buffer+6*4, data, byte_length);


	ProgressBar progress("write Flash", buffer_length, 50, _quiet);

	for (int i=0, xpage=0; xpage < nb_xpage; i+=(nb_iter*4), xpage++) {
		wr_rd(CONFIG_ENABLE, NULL, 0, NULL, 0);
		wr_rd(EF_PROGRAM, NULL, 0, NULL, 0);
		if (xpage != 0)
			_jtag->toggleClk(312);
		addr = xpage << 6;
		tmp[3] = 0xff&(addr >> 24);
		tmp[2] = 0xff&(addr >> 16);
		tmp[1] = 0xff&(addr >> 8);
		tmp[0] = addr&0xff;
		_jtag->shiftDR(tmp, NULL, 32);
		_jtag->toggleClk(312);

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

			if (!is_gw1n1)
				_jtag->toggleClk(40);
		}
		if (is_gw1n1) {
			//usleep(10*2400*2);
			uint8_t tt2[6008/8];
			memset(tt2, 0, 6008/8);
			_jtag->toggleClk(6008);
		}
		progress.display(i);
	}
	/* 2.2.6.6 */
	_jtag->set_state(Jtag::RUN_TEST_IDLE);

	progress.done();
	return true;
}

/* TN653 p. 9 */
bool Gowin::flashSRAM(uint8_t *data, int length)
{
	int tx_len, tx_end;
	int byte_length = length / 8;

	ProgressBar progress("Flash SRAM", byte_length, 50, _quiet);

	/* 2.2.6.4 */
	wr_rd(XFER_WRITE, NULL, 0, NULL, 0);

	int xfer_len = 256;

	for (int i=0; i < byte_length; i+=xfer_len) {
		if (i + xfer_len > byte_length) {  // last packet with some size
			tx_len = (byte_length - i) * 8;
			tx_end = Jtag::EXIT1_DR;  // to move in EXIT1_DR
		} else {
			tx_len = xfer_len * 8;
			/* 2.2.6.5 */
			tx_end = Jtag::SHIFT_DR;
		}
		_jtag->shiftDR(data+i, NULL, tx_len, tx_end);
		//_jtag->flush();
		progress.display(i);
	}
	/* 2.2.6.6 */
	_jtag->set_state(Jtag::RUN_TEST_IDLE);

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
	uint8_t tt[37500 * 8];
	memset(tt, 0, 37500 * 8);
	unsigned char tx[4] = {0, 0, 0, 0};
	printInfo("erase Flash ", false);
	wr_rd(EFLASH_ERASE, NULL, 0, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);

	/* GW1N1 need 65 x 32bits
	 * others 1 x 32bits
	 */
	int nb_iter = (is_gw1n1)?65:1;
	for (int i = 0; i < nb_iter; i++) {
		_jtag->shiftDR(tx, NULL, 32);
		_jtag->toggleClk(6);
	}
	/* TN653 specifies to wait for 160ms with
	 * there are no bit in status register to specify
	 * when this operation is done so we need to wait
	 */
	//usleep(2*120000);
	//uint8_t tt[37500];
	_jtag->toggleClk(37500*8);
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

/* SPI wrapper
 * extflash access may be done using specific mode or
 * boundary scan. But former is only available with mode=[11]
 * so use Bscan
 *
 * it's a bitbanging mode with:
 * Pins Name of SPI Flash | SCLK | CS  | DI  | DO  |
 * Bscan Chain[7:0]       | 7  6 | 5 4 | 3 2 | 1 0 |
 * (ctrl & data)          | 0    | 0   | 0   | 1   |
 * ctrl 0 -> out, 1 -> in
 * data 1 -> high, 0 -> low
 * but all byte must be bit reversal...
 */

#define spi_gowin_write(_wr, _rd, _len) do { \
	_jtag->shiftDR(_wr, _rd, _len); \
	_jtag->toggleClk(6); } while (0)

int Gowin::spi_put(uint8_t cmd, uint8_t *tx, uint8_t *rx, uint32_t len)
{
	uint8_t jrx[len+1], jtx[len+1];
	jtx[0] = cmd;
	if (tx)
		memcpy(jtx+1, tx, len);
	else
		memset(jtx+1, 0, len);
	int ret = spi_put(jtx, (rx)? jrx : NULL, len+1);
	if (rx)
		memcpy(rx, jrx+1, len);
	return ret;
}

int Gowin::spi_put(uint8_t *tx, uint8_t *rx, uint32_t len)
{
	/* set CS/SCK/DI low */
	uint8_t t = _spi_msk | _spi_do;
	t &= ~_spi_cs;
	spi_gowin_write(&t, NULL, 8);
	_jtag->flush();

	/* send bit/bit full tx content (or set di to 0 when NULL) */
	for (uint32_t i = 0; i < len * 8; i++) {
		uint8_t r;
		t = _spi_msk | _spi_do;
		if (tx != NULL && tx[i>>3] & (1 << (7-(i&0x07))))
			t |= _spi_di;
		spi_gowin_write(&t, NULL, 8);
		t |= _spi_sck;
		spi_gowin_write(&t, (rx) ? &r : NULL, 8);
		_jtag->flush();
		/* if read reconstruct bytes */
		if (rx) {
			if (r & _spi_do)
				rx[i >> 3] |= 1 << (7-(i & 0x07));
			else
				rx[i >> 3] &= ~(1 << (7-(i & 0x07)));
		}
	}
	/* set CS and unset SCK (next xfer) */
	t &= ~_spi_sck;
	t |= _spi_cs;
	spi_gowin_write(&t, NULL, 8);
	_jtag->flush();

	return 0;
}

int Gowin::spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
		uint32_t timeout, bool verbose)
{
	uint32_t count = 0;
	uint8_t rx, t;

	/* set CS/SCK/DI low */
	t = _spi_msk | _spi_do;
	spi_gowin_write(&t, NULL, 8);

	/* send command bit/bit */
	for (int i = 0; i < 8; i++) {
		t = _spi_msk | _spi_do;
		if ((cmd & (1 << (7-i))) != 0)
			t |= _spi_di;
		spi_gowin_write(&t, NULL, 8);
		t |= _spi_sck;
		spi_gowin_write(&t, NULL, 8);
		_jtag->flush();
	}

	t = _spi_msk | _spi_do;
	do {
		rx = 0;
		/* read status register bit/bit with di == 0 */
		for (int i = 0; i < 8; i++) {
			uint8_t r;
			t &= ~_spi_sck;
			spi_gowin_write(&t, NULL, 8);
			t |= _spi_sck;
			spi_gowin_write(&t, &r, 8);
			_jtag->flush();
			if ((r & _spi_do) != 0)
				rx |= 1 << (7-i);
		}

		count++;
		if (count == timeout) {
			printf("timeout: %x\n", rx);
			break;
		}
		if (verbose)
			printf("%x %x %x %u\n", rx, mask, cond, count);
	} while ((rx & mask) != cond);

	/* set CS & unset SCK (next xfer) */
	t &= ~_spi_sck;
	t |= _spi_cs;
	spi_gowin_write(&t, NULL, 8);
	_jtag->flush();

	if (count == timeout) {
		printf("%02x\n", rx);
		std::cout << "wait: Error" << std::endl;
		return -ETIME;
	}

	return 0;
}
