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
			FTDIpp_MPSSE(dev, cable.interface, clkHZ, verbose), _bitmode(0), _nb_bit(0)
{
	init_internal(cable, pin_conf);
}

FtdiJtagBitBang::FtdiJtagBitBang(const FTDIpp_MPSSE::mpsse_bit_config &cable,
		   const jtag_pins_conf_t *pin_conf, uint32_t clkHZ, bool verbose):
		   FTDIpp_MPSSE(cable.vid, cable.pid, cable.interface, clkHZ, verbose),
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

	_buffer_size = 128;  // TX Fifo size

	_in_buf = (unsigned char *)malloc(sizeof(unsigned char) * _buffer_size);
	bzero(_in_buf, _buffer_size);
	init(5, _tck_pin | _tms_pin | _tdi_pin, BITMODE_BITBANG,
		(FTDIpp_MPSSE::mpsse_bit_config &)cable);
	setBitmode(BITMODE_BITBANG);
}

int FtdiJtagBitBang::setBitmode(uint8_t mode)
{
	if (_bitmode == mode)
		return 0;
	_bitmode = mode;
	return ftdi_set_bitmode(_ftdi, _tck_pin | _tms_pin | _tdi_pin, _bitmode);
}

/**
 * store tms in
 * internal buffer
 */
int FtdiJtagBitBang::storeTMS(uint8_t *tms, int nb_bit, uint8_t tdi, bool read)
{
	(void) read;
	int xfer_len = nb_bit;
	/* need to check for available space in buffer */
	if (nb_bit == 0)
		return 0;

	while (xfer_len > 0) {
		int xfer = xfer_len;
		if ((_nb_bit + 2*xfer) > _buffer_size)
			xfer = (_buffer_size - _nb_bit) >> 1;

		for (int i = 0; i < xfer; i++, _nb_bit += 2) {
			_in_buf[_nb_bit] = ((tdi)?_tdi_pin:0) |
					(((tms[i >> 3] >> (i & 0x7)) & 0x01)? _tms_pin:0);
			_in_buf[_nb_bit + 1] = _in_buf[_nb_bit] | _tck_pin;
		}

		xfer_len -= xfer;
		if (xfer_len != 0)
			write(NULL);
	}
	return nb_bit;
}

int FtdiJtagBitBang::writeTMS(uint8_t *tdo, int len)
{
	(void) len;
	return write(tdo);
}

/**
 * store tdi in
 * internal buffer with tms
 * size must be <= 8
 */
int FtdiJtagBitBang::storeTDI(uint8_t tdi, int nb_bit, bool read)
{
	for (int i = 0; i < nb_bit; i++, _nb_bit += 2) {
		_in_buf[_nb_bit] =
				((tdi & (1 << (i & 0x7)))?_tdi_pin:0);
		_in_buf[_nb_bit + 1] = _in_buf[_nb_bit] | _tck_pin;
	}
	return nb_bit;
}

/**
 * store tdi in
 * internal buffer
 * since TDI is used in shiftDR and shiftIR, tms is always set to 0
 */
int FtdiJtagBitBang::storeTDI(uint8_t *tdi, int nb_byte, bool read)
{
	/* need to check for available space in buffer */
	for (int i = 0; i < nb_byte * 8; i++, _nb_bit += 2) {
		_in_buf[_nb_bit] =
				((tdi[i >> 3] & (1 << (i & 0x7)))?_tdi_pin:0);
		_in_buf[_nb_bit + 1] = _in_buf[_nb_bit] | _tck_pin;
	}
	return nb_byte;
}

int FtdiJtagBitBang::writeTDI(uint8_t *tdo, int nb_bit)
{
	(void) nb_bit;
	return write(tdo);
}


int FtdiJtagBitBang::write(uint8_t *tdo)
{
	int ret = 0;
	if (_nb_bit == 0)
		return 0;

	setBitmode((tdo) ? BITMODE_SYNCBB : BITMODE_BITBANG);

	ret = ftdi_write_data(_ftdi, _in_buf, _nb_bit);
	if (ret != _nb_bit)
		printf("problem %d written\n", ret);

	if (tdo) {
		ret = ftdi_read_data(_ftdi, _in_buf, _nb_bit);
		if (ret != _nb_bit)
			printf("problem %d read\n", ret);
		/* need to reconstruct received word 
		 * even bit are discarded since JTAG read in rising edge
		 * since jtag is LSB first we need to shift right content by 1
		 * and add 0x80 (1 << 7) or 0
		 * */
		for (int i = 1, offset=0; i < _nb_bit; i+=2, offset++) {
			tdo[offset >> 3] = (((_in_buf[i] & _tdo_pin) ? 0x80 : 0x00) |
							(tdo[offset >> 3] >> 1));
		}
	}
	_nb_bit = 0;
	return ret;
}
