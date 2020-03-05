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

#include "ftdiJtagMPSSE.hpp"
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

FtdiJtagMPSSE::FtdiJtagMPSSE(const FTDIpp_MPSSE::mpsse_bit_config &cable,
			string dev, unsigned char interface, uint32_t clkHZ, bool verbose):
			FTDIpp_MPSSE(dev, interface, clkHZ, verbose), _ch552WA(false)
{
	init_internal(cable);
}

FtdiJtagMPSSE::FtdiJtagMPSSE(const FTDIpp_MPSSE::mpsse_bit_config &cable,
		   unsigned char interface, uint32_t clkHZ, bool verbose):
		   FTDIpp_MPSSE(cable.vid, cable.pid, interface, clkHZ, verbose),
		   _ch552WA(false)
{
	init_internal(cable);
}

FtdiJtagMPSSE::~FtdiJtagMPSSE()
{
	int read;
	/* Before shutdown, we must wait until everything is shifted out
	 * Do this by temporary enabling loopback mode, write something
	 * and wait until we can read it back
	 */
	static unsigned char tbuf[16] = { SET_BITS_LOW, 0xff, 0x00,
		SET_BITS_HIGH, 0xff, 0x00,
		LOOPBACK_START,
		MPSSE_DO_READ |
		MPSSE_DO_WRITE | MPSSE_WRITE_NEG | MPSSE_LSB,
		0x04, 0x00,
		0xaa, 0x55, 0x00, 0xff, 0xaa,
		LOOPBACK_END
	};
	mpsse_store(tbuf, 16);
	read = mpsse_read(tbuf, 5);
	if (read != 5)
		fprintf(stderr,
			"Loopback failed, expect problems on later runs %d\n", read);

	free(_in_buf);
}

void FtdiJtagMPSSE::init_internal(const FTDIpp_MPSSE::mpsse_bit_config &cable)
{
	/* search for iProduct -> need to have
	 * ftdi->usb_dev (libusb_device_handler) -> libusb_device ->
	 * libusb_device_descriptor
	 */
	struct libusb_device * usb_dev = libusb_get_device(_ftdi->usb_dev);
	struct libusb_device_descriptor usb_desc;
	unsigned char iProduct[200];
	libusb_get_device_descriptor(usb_dev, &usb_desc);
	libusb_get_string_descriptor_ascii(_ftdi->usb_dev, usb_desc.iProduct,
		iProduct, 200);

	display("iProduct : %s\n", iProduct);

	if (!strncmp((const char *)iProduct, "Sipeed-Debug", 12)) {
		_ch552WA = true;
	}

	display("%x\n", cable.bit_low_val);
	display("%x\n", cable.bit_low_dir);
	display("%x\n", cable.bit_high_val);
	display("%x\n", cable.bit_high_dir);

	_in_buf = (unsigned char *)malloc(sizeof(unsigned char) * _buffer_size);
	bzero(_in_buf, _buffer_size);
	init(5, 0xfb, BITMODE_MPSSE, (FTDIpp_MPSSE::mpsse_bit_config &)cable);
}

/**
 * store tms in
 * internal buffer
 * size must be <= 8
 */
int FtdiJtagMPSSE::storeTMS(uint8_t *tms, int nb_bit, uint8_t tdi, bool read)
{
	int xfer, pos = 0, tx = nb_bit;
	unsigned char buf[3]= {static_cast<unsigned char>(MPSSE_WRITE_TMS | MPSSE_LSB |
		MPSSE_BITMODE | MPSSE_WRITE_NEG | ((read) ? MPSSE_DO_READ : 0)),
		static_cast<unsigned char>(0),
		static_cast<unsigned char>(0)};

	if (nb_bit == 0)
		return 0;

	display("%s: %d %s %d %d %x\n", __func__, tdi, (read) ? "true" : "false",
			nb_bit, (nb_bit / 6) * 3, tms[0]);
	int plop = 0;

	while (nb_bit != 0) {
		xfer = (nb_bit > 6) ? 6 : nb_bit;
		buf[1] = xfer - 1;
		buf[2] = (tdi)?0x80 : 0x00;
		for (int i = 0; i < xfer; i++, pos++) {
			buf[2] |=
			(((tms[pos >> 3] & (1 << (pos & 0x07))) ? 1 : 0) << i);
		}
		nb_bit -= xfer;
		mpsse_store(buf, 3);
		plop += 3;
	}

	return tx;
}

int FtdiJtagMPSSE::writeTMS(uint8_t *tdo, int len)
{
	display("%s %s %d %d\n", __func__, (tdo)?"true":"false", len, (len/8)+1);

	if (tdo) {
		return mpsse_read(tdo, (len/8)+1);
	} else {
		int ret = mpsse_write();
		if (_ch552WA) {
			uint8_t c[len/8+1];
			ftdi_read_data(_ftdi, c, len/8+1);
		}
		return ret;
	}
}

/**
 * store tdi in internal buffer
 * size must be <= 8
 */
int FtdiJtagMPSSE::storeTDI(uint8_t tdi, int nb_bit, bool read)
{
	unsigned char tx_buf[3] = {(unsigned char)(MPSSE_LSB | MPSSE_WRITE_NEG |
		MPSSE_DO_WRITE | MPSSE_BITMODE | ((read) ? MPSSE_DO_READ : 0)),
		static_cast<unsigned char>(nb_bit - 1),
		tdi};
	mpsse_store(tx_buf, 3);

	return nb_bit;
}

/**
 * store tdi in internal buffer
 */
int FtdiJtagMPSSE::storeTDI(uint8_t *tdi, int nb_byte, bool read)
{
	unsigned char tx_buf[3] = {(unsigned char)(MPSSE_LSB | MPSSE_WRITE_NEG |
		MPSSE_DO_WRITE |
		((read) ? MPSSE_DO_READ : 0)),
		static_cast<unsigned char>((nb_byte - 1) & 0xff),
		static_cast<unsigned char>(((nb_byte - 1) >> 8) & 0xff)};
	mpsse_store(tx_buf, 3);
	mpsse_store(tdi, nb_byte);

	return nb_byte;
}

/* flush buffer
 * if tdo is not null read nb_bit
 */
int FtdiJtagMPSSE::writeTDI(uint8_t *tdo, int nb_bit)
{
	int nb_byte = (nb_bit < 8)? 1: (nb_bit >> 3);

	if (tdo) {
		return mpsse_read(tdo, nb_byte);
	} else {
		int ret = mpsse_write();
		if (_ch552WA) {
			uint8_t c[nb_byte];
			ftdi_read_data(_ftdi, c, nb_byte);
		}
		return ret;
	}
}
