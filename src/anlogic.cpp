/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
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

#include <string.h>
#include "anlogic.hpp"
#include "anlogicBitParser.hpp"
#include "jtag.hpp"
#include "device.hpp"
#include "progressBar.hpp"
#include "spiFlash.hpp"

#define REFRESH      0x01
#define IDCODE       0x06
#define JTAG_PROGRAM 0x30
#define SPI_PROGRAM  0x39
#define CFG_IN       0x3b
#define JTAG_START   0x3d
#define BYPASS       0xFF

#define IRLENGTH 8

Anlogic::Anlogic(Jtag *jtag, const std::string &filename,
	Device::prog_type_t prg_type, int8_t verbose):
	Device(jtag, filename, verbose), _svf(_jtag, _verbose)
{
	if (_filename != "") {
		if (_file_extension == "svf")
			_mode = Device::MEM_MODE;
		else if (_file_extension == "bit") {
			if (prg_type == Device::WR_SRAM)
				_mode = Device::MEM_MODE;
			else
				_mode = Device::SPI_MODE;
		} else
			throw std::exception();
	}
}
Anlogic::~Anlogic()
{}

void Anlogic::reset()
{
	_jtag->shiftIR(BYPASS, IRLENGTH);
	_jtag->shiftIR(REFRESH, IRLENGTH);
	_jtag->toggleClk(15);
	_jtag->shiftIR(BYPASS, IRLENGTH);
	_jtag->toggleClk(200000);
}

void Anlogic::program(unsigned int offset)
{
	if (_mode == Device::NONE_MODE)
		return;

	if (_file_extension == "svf") {
		_svf.parse(_filename);
		return;
	}

	AnlogicBitParser bit(_filename, (_mode == Device::MEM_MODE), _verbose);
	bit.parse();
	uint8_t *data = bit.getData();
	int len = bit.getLength() / 8;

	if (_mode == Device::SPI_MODE) {
		SPIFlash flash(this, _verbose);

		for (int i = 0; i < 5; i++)
			_jtag->shiftIR(BYPASS, IRLENGTH);
		//Verify Device id.
		//SIR 8 TDI (06) ;
		//SDR 32 TDI (00000000) TDO (0a014c35) MASK (ffffffff) ;
		//Boundary Scan Chain Contents
		//Position 1: BG256
		//Loading device with 'refresh' instruction.
		_jtag->shiftIR(REFRESH, IRLENGTH);
		//Loading device with 'bypass' & 'spi_program' instruction.
		_jtag->shiftIR(BYPASS, IRLENGTH);
		_jtag->shiftIR(SPI_PROGRAM, IRLENGTH);
		for (int i = 0; i < 4; i++)
			_jtag->toggleClk(50000);

		flash.reset();
		flash.read_id();
		flash.read_status_reg();

		flash.erase_and_prog(offset, data, len);

		//Loading device with 'bypass' instruction.
		_jtag->shiftIR(BYPASS, IRLENGTH);
		////Loading device with 'refresh' instruction.
		_jtag->shiftIR(REFRESH, IRLENGTH);
		_jtag->toggleClk(20);
		////Loading device with 'bypass' instruction.
		for (int i = 0; i < 4; i++) {
			_jtag->shiftIR(BYPASS, IRLENGTH);
			_jtag->toggleClk(20);
		}
		_jtag->shiftIR(BYPASS, IRLENGTH);
		_jtag->toggleClk(10000);

		return;
	}
	if (_mode == Device::MEM_MODE) {

		// Loading device with 'bypass' instruction.
		_jtag->shiftIR(BYPASS, IRLENGTH);
		_jtag->shiftIR(BYPASS, IRLENGTH);
		// Verify Device id.
		// SIR 8 TDI (06) ;
		// SDR 32 TDI (00000000) TDO (0a014c35) MASK (ffffffff) ;
		// Boundary Scan Chain Contents
		// Position 1: BG256
		// Loading device with 'refresh' instruction.
		_jtag->shiftIR(REFRESH, IRLENGTH);
		// Loading device with 'bypass' & 'spi_program' instruction.
		_jtag->shiftIR(BYPASS, IRLENGTH);
		_jtag->shiftIR(SPI_PROGRAM, IRLENGTH);
		_jtag->toggleClk(50000);
		// Loading device with 'jtag program' instruction.
		_jtag->shiftIR(JTAG_PROGRAM, IRLENGTH);
		_jtag->toggleClk(15);
		// Loading device with a `cfg_in` instruction.
		_jtag->shiftIR(CFG_IN, IRLENGTH);
		_jtag->toggleClk(15);

		_jtag->set_state(Jtag::SHIFT_DR);
		ProgressBar progress("Loading", len, 50, _quiet);
		int pos = 0;
		uint8_t *ptr = data;
		while (len > 0) {
			int xfer_len = (len > 512)?512:len;
			_jtag->read_write(ptr, NULL, xfer_len*8, (len - xfer_len == 0));
			len -= xfer_len;
			progress.display(pos);
			pos += xfer_len;
			ptr+=xfer_len;
		}

		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		progress.done();
		_jtag->toggleClk(100);
		// Loading device with a `jtag start` instruction.
		_jtag->shiftIR(JTAG_START, IRLENGTH);
		_jtag->toggleClk(15);
		// Loading device with 'bypass' instruction.
		_jtag->shiftIR(BYPASS, IRLENGTH);
		_jtag->toggleClk(1000);
		// ??
		_jtag->shiftIR(0x31, IRLENGTH);
		_jtag->toggleClk(100);
		_jtag->shiftIR(JTAG_START, IRLENGTH);
		_jtag->toggleClk(15);
		_jtag->shiftIR(BYPASS, IRLENGTH);
		_jtag->toggleClk(15);
	}
}

int Anlogic::idCode()
{
	unsigned char tx_data[4];
	unsigned char rx_data[4];

	tx_data[0] = IDCODE;
	_jtag->go_test_logic_reset();
	_jtag->shiftIR(tx_data, NULL, IRLENGTH);
	memset(tx_data, 0, 4);
	_jtag->shiftDR(tx_data, rx_data, 32);
	return ((rx_data[0] & 0x000000ff) |
		((rx_data[1] << 8) & 0x0000ff00) |
		((rx_data[2] << 16) & 0x00ff0000) |
		((rx_data[3] << 24) & 0xff000000));
}

/* SPI wrapper
 * For read operation a delay of one bit is added
 * So add one bit more and move everything by one
 * In write only operations to care about this delay
 */

int Anlogic::spi_put(uint8_t cmd, uint8_t *tx, uint8_t *rx, uint32_t len)
{
	int xfer_len = len + 1;
	if (rx)
		xfer_len++;
	uint8_t jtx[xfer_len];
	uint8_t jrx[xfer_len];

	jtx[0] = AnlogicBitParser::reverseByte(cmd);
	if (tx != NULL) {
		for (uint32_t i = 0; i < len; i++)
			jtx[i+1] = AnlogicBitParser::reverseByte(tx[i]);
	}

	/* write anlogic command before sending packet */
	uint8_t op = 0x60;
	_jtag->shiftDR(&op, NULL, 8);

	_jtag->shiftDR(jtx, (rx == NULL)? NULL: jrx, 8*xfer_len);
	if (rx != NULL) {
		for (uint32_t i=0; i < len; i++)
			rx[i] = AnlogicBitParser::reverseByte(jrx[i+1]>>1)
				| (jrx[i+2]&0x01);
	}
	return 0;
}
int Anlogic::spi_put(uint8_t *tx, uint8_t *rx, uint32_t len)
{
	int xfer_len = len;
	if (rx)
		xfer_len++;
	uint8_t jtx[xfer_len];
	uint8_t jrx[xfer_len];

	if (tx != NULL) {
		for (uint32_t i = 0; i < len; i++)
			jtx[i] = AnlogicBitParser::reverseByte(tx[i]);
	}

	/* write anlogic command before sending packet */
	uint8_t op = 0x60;
	_jtag->shiftDR(&op, NULL, 8);

	_jtag->shiftDR(jtx, (rx == NULL)? NULL: jrx, 8*xfer_len);
	if (rx != NULL) {
		for (uint32_t i=0; i < len; i++)
			rx[i] = AnlogicBitParser::reverseByte(jrx[i]>>1) |
				(jrx[i+1]&0x01);
	}
	return 0;
}
int Anlogic::spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
		uint32_t timeout, bool verbose)
{
	uint8_t rx[3];
	uint8_t tx[3];
	tx[0] = AnlogicBitParser::reverseByte(cmd);
	uint8_t op = 0x60;
	uint8_t tmp;
	uint32_t count = 0;

	do {
		_jtag->shiftDR(&op, NULL, 8);
		_jtag->shiftDR(tx, rx, 8 * 3);
		tmp = (AnlogicBitParser::reverseByte(rx[1]>>1)) | (0x01 & rx[2]);
		count ++;
		if (count == timeout) {
			printf("timeout: %x %x %x\n", tmp, rx[0], rx[1]);
			break;
		}
		if (verbose) {
			printf("%x %x %x %u\n", tmp, mask, cond, count);
		}
	} while ((tmp & mask) != cond);

	if (count == timeout) {
		printf("%02x\n", tmp);
		std::cout << "wait: Error" << std::endl;
		return -ETIME;
	}

	return 0;
}
