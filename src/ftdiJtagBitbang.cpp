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

#include <libusb.h>
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <map>
#include <vector>
#include <string>

#include "ftdiJtagBitbang.hpp"
#include "ftdipp_mpsse.hpp"

using namespace std;

#define DEBUG 0

#ifdef DEBUG
#define display(...) \
	do { \
		if (_verbose) fprintf(stdout, __VA_ARGS__); \
	}while(0)
#else
#define display(...) do {}while(0)
#endif

FtdiJtagBitBang::FtdiJtagBitBang(const FTDIpp_MPSSE::mpsse_bit_config &cable,
			const jtag_pins_conf_t *pin_conf, string dev, uint32_t clkHZ, bool verbose):
			FTDIpp_MPSSE(cable, dev, clkHZ, verbose), _bitmode(0), _nb_bit(0),
			_curr_tms(0)
{
	init_internal(cable, pin_conf);
}

FtdiJtagBitBang::FtdiJtagBitBang(const FTDIpp_MPSSE::mpsse_bit_config &cable,
		   const jtag_pins_conf_t *pin_conf, uint32_t clkHZ, bool verbose):
		   FTDIpp_MPSSE(cable, clkHZ, verbose),
		   _bitmode(0), _nb_bit(0)
{
	init_internal(cable, pin_conf);
}

FtdiJtagBitBang::~FtdiJtagBitBang()
{
	free(_in_buf);
}

void FtdiJtagBitBang::init_internal(const FTDIpp_MPSSE::mpsse_bit_config &cable,
		  const jtag_pins_conf_t *pin_conf)
{
	_tck_pin = (1 << pin_conf->tck_pin);
	_tms_pin = (1 << pin_conf->tms_pin);
	_tdi_pin = (1 << pin_conf->tdi_pin);
	_tdo_pin = (1 << pin_conf->tdo_pin);

	_buffer_size = 512;  // TX Fifo size

	setClkFreq(_clkHZ);

	_in_buf = (unsigned char *)malloc(sizeof(unsigned char) * _buffer_size);
	bzero(_in_buf, _buffer_size);
	init(1, _tck_pin | _tms_pin | _tdi_pin, BITMODE_BITBANG,
		(FTDIpp_MPSSE::mpsse_bit_config &)cable);
	setBitmode(BITMODE_BITBANG);
}

int FtdiJtagBitBang::setClkFreq(uint32_t clkHZ)
{
	if (clkHZ > 3000000)
		clkHZ = 3000000;
	int ret = ftdi_set_baudrate(_ftdi, clkHZ);
	printf("ret %d\n", ret);
	return ret;
}

int FtdiJtagBitBang::setBitmode(uint8_t mode)
{
	if (_bitmode == mode)
		return 0;
	_bitmode = mode;

	int ret = ftdi_set_bitmode(_ftdi, _tck_pin | _tms_pin | _tdi_pin, _bitmode);
	ftdi_usb_purge_rx_buffer(_ftdi);
	ftdi_usb_purge_tx_buffer(_ftdi);
	return ret;
}

int FtdiJtagBitBang::writeTMS(uint8_t *tms, int len, bool flush_buffer)
{
	int ret;

	/* nothing to send
	 * but maybe need to flush internal buffer
	 */
	if (len == 0) {
		if (flush_buffer) {
			ret = flush();
			return ret;
		}
		return 0;
	}

	/* check for at least one bit space in buffer */
	if (_nb_bit+2 > _buffer_size) {
		ret = flush();
		if (ret < 0)
			return ret;
	}

	/* fill buffer to reduce USB transaction */
	for (int i = 0; i < len; i++) {
		_curr_tms = ((tms[i >> 3] & (1 << (i & 0x07)))? _tms_pin : 0);
		uint8_t val = _tdi_pin | _curr_tms;
		_in_buf[_nb_bit++] = val;
		_in_buf[_nb_bit++] = val | _tck_pin;

		if (_nb_bit + 2 > _buffer_size) {
			ret = write(NULL, 0);
			if (ret < 0)
				return ret;
		}
	}

	/* security check: try to flush buffer */
	if (flush_buffer) {
		ret = write(NULL, 0);
		if (ret < 0)
			return ret;
	}

	return len;
}

int FtdiJtagBitBang::writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end)
{
	uint32_t iter;
	if (len * 2 + 1 < (uint32_t)_buffer_size) {
		iter = len;
	} else {
		iter = _buffer_size >> 1;  // two tx / bit
		iter = (iter / 8) * 8;
	}

	uint8_t *rx_ptr = rx;

	if (len == 0)
		return 0;
	if (rx)
		bzero(rx, len/8);

	for (uint32_t i = 0, pos = 0; i < len; i++) {
		/* keep tms or
		 * set tms high if it's last bit and end true */
		if (end && (i == len -1))
			_curr_tms = _tms_pin;
		uint8_t val = _curr_tms;

		if (tx)
			val |= ((tx[i >> 3] & (1 << (i & 0x07)))? _tdi_pin : 0);
		_in_buf[_nb_bit    ] = val;
		_in_buf[_nb_bit + 1] = val | _tck_pin;

		_nb_bit += 2;

		pos++;
		/* flush buffer */
		if (pos == iter) {
			pos = 0;
			write((rx) ? rx_ptr : NULL, iter);
			if (rx)
				rx_ptr += (iter/8);
		}
	}

	/* security check: try to flush buffer */
	if (_nb_bit != 0) {
		write((rx && _nb_bit > 1) ? rx_ptr : NULL, _nb_bit / 2);
	}

	return len;
}

int FtdiJtagBitBang::toggleClk(uint8_t tms, uint8_t tdo, uint32_t clk_len)
{
	(void) tms; (void) tdo; (void) clk_len;
	return -1;
}

int FtdiJtagBitBang::flush()
{
	return write(NULL, 0);
}

int FtdiJtagBitBang::write(uint8_t *tdo, int nb_bit)
{
	int ret = 0;
	if (_nb_bit == 0)
		return 0;

	setBitmode((tdo) ? BITMODE_SYNCBB : BITMODE_BITBANG);

	ret = ftdi_write_data(_ftdi, _in_buf, _nb_bit);
	if (ret != _nb_bit) {
		printf("problem %d written\n", ret);
		return ret;
	}

	if (tdo) {
		ret = ftdi_read_data(_ftdi, _in_buf, _nb_bit);
		if (ret != _nb_bit) {
			printf("problem %d read\n", ret);
			return ret;
		}
		/* need to reconstruct received word 
		 * even bit are discarded since JTAG read in rising edge
		 * since jtag is LSB first we need to shift right content by 1
		 * and add 0x80 (1 << 7) or 0
		 * the buffer may contains some tms bit, so start with i
		 * equal to fill exactly nb_bit bits
		 * */
		for (int i = (_nb_bit-(nb_bit *2) + 1), offset=0; i < _nb_bit; i+=2, offset++) {
			tdo[offset >> 3] = (((_in_buf[i] & _tdo_pin) ? 0x80 : 0x00) |
							(tdo[offset >> 3] >> 1));
		}
	}
	_nb_bit = 0;
	return ret;
}
