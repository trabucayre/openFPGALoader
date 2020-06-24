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

#include "dirtyJtag.hpp"
#include "display.hpp"

using namespace std;

#define DIRTYJTAG_VID 0x1209
#define DIRTYJTAG_PID 0xC0CA

#define DIRTYJTAG_WRITE_EP    0x01
#define DIRTYJTAG_READ_EP     0x82

enum dirtyJtagCmd {
	CMD_STOP =  0x00,
	CMD_INFO =  0x01,
	CMD_FREQ =  0x02,
	CMD_XFER =  0x03,
	CMD_SETSIG = 0x04,
	CMD_GETSIG = 0x05,
	CMD_CLK =    0x06
};

enum dirtyJtagSig {
	SIG_TCK =   (1 << 1),
	SIG_TDI =   (1 << 2),
	SIG_TDO =   (1 << 3),
	SIG_TMS =   (1 << 4)
};

DirtyJtag::DirtyJtag(uint32_t clkHZ, bool verbose):
			_verbose(verbose),
			dev_handle(NULL), usb_ctx(NULL), _tdi(0), _tms(0)
{
	int ret;

	if (libusb_init(&usb_ctx) < 0) {
		cerr << "libusb init failed" << endl;
		throw std::exception();
	}

	dev_handle = libusb_open_device_with_vid_pid(usb_ctx,
					DIRTYJTAG_VID, DIRTYJTAG_PID);
	if (!dev_handle) {
		cerr << "fails to open device" << endl;
		libusb_exit(usb_ctx);
		throw std::exception();
	}

	ret = libusb_claim_interface(dev_handle, 0);
	if (ret) {
		cerr << "libusb error while claiming DirtyJTAG interface #0" << endl;
		libusb_close(dev_handle);
		libusb_exit(usb_ctx);
		throw std::exception();
	}

	if (clkHZ > 600000) {
		printInfo("DirtyJTAG probe limited to 600kHz");
		clkHZ = 600000;
	}
	if (setClkFreq(clkHZ) < 0) {
		cerr << "Fail to set frequency" << endl;
		throw std::exception();
	}
}

DirtyJtag::~DirtyJtag()
{
	if (dev_handle)
		libusb_close(dev_handle);
	if (usb_ctx)
		libusb_exit(usb_ctx);
}

int DirtyJtag::setClkFreq(uint32_t clkHZ)
{
	int actual_length;
	int ret;
	uint8_t buf[] = {CMD_FREQ,
					static_cast<uint8_t>(0xff & ((clkHZ / 1000) >> 8)),
					static_cast<uint8_t>(0xff & ((clkHZ / 1000)     )),
					CMD_STOP};
	ret = libusb_bulk_transfer(dev_handle, DIRTYJTAG_WRITE_EP,
			        buf, 4, &actual_length, 1000);
	if (ret < 0) {
		cerr << "setClkFreq: usb bulk write failed " << ret << endl;
		return -EXIT_FAILURE;
	}

	return clkHZ;
}

int DirtyJtag::writeTMS(uint8_t *tms, int len, bool flush_buffer)
{
	(void) flush_buffer;

	if (len == 0)
		return 0;

	for (int i = 0; i < len; i++) {
		bool val = (tms[i >> 3] & (1 << (i & 0x07)));
		sendBitBang(SIG_TMS, ((val)?SIG_TMS : 0), NULL, (i == len-1));
	}
	return len;
}

int DirtyJtag::toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len)
{
	int actual_length;
	uint8_t buf[] = {CMD_CLK,
				static_cast<uint8_t>(((tms) ? SIG_TMS : 0) | ((tdi) ? SIG_TDI : 0)),
				0,
				CMD_STOP};
	while (clk_len > 0) {
		buf[2] = (clk_len > 100) ? 100 : (uint8_t)clk_len;

		int ret = libusb_bulk_transfer(dev_handle, DIRTYJTAG_WRITE_EP,
				buf, 4, &actual_length, 1000);
		if (ret < 0) {
			cerr << "toggleClk: usb bulk write failed " << ret << endl;
			return -EXIT_FAILURE;
		}
		clk_len -= buf[2];
	}

	return EXIT_SUCCESS;
}

int DirtyJtag::flush()
{
	return 0;
}

int DirtyJtag::writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end)
{
	int actual_length;
	uint32_t real_bit_len = len - (end ? 1 : 0);
	uint32_t real_byte_len = (len + 7) / 8;

	uint8_t tx_cpy[real_byte_len];
	uint8_t tx_buf[33], rx_buf[32];
	uint8_t *tx_ptr = tx, *rx_ptr = rx;

	if (tx)
		memcpy(tx_cpy, tx, real_byte_len);
	else
		bzero(tx_cpy, real_byte_len);
	tx_ptr = tx_cpy;

	/* first send 30 x 8 bits */
	tx_buf[0] = CMD_XFER;
	while (real_bit_len != 0) {
		uint8_t bit_to_send = (real_bit_len > 240) ? 240 : real_bit_len;
		uint8_t byte_to_send = (bit_to_send + 7) / 8;
		tx_buf[1] = bit_to_send;
		bzero(tx_buf + 2, 30);
		for (int i = 0; i < bit_to_send; i++)
			if (tx_ptr[i >> 3] & (1 << (i & 0x07)))
				tx_buf[2 + (i >> 3)] |= (0x80 >> (i & 0x07));

		tx_buf[2 + byte_to_send] = CMD_STOP;

		int ret = libusb_bulk_transfer(dev_handle, DIRTYJTAG_WRITE_EP,
		        (unsigned char *)tx_buf, 33, &actual_length, 1000);
		if (ret < 0) {
			cerr << "writeTDI: fill: usb bulk write failed " << ret << endl;
			return EXIT_FAILURE;
		}

		ret = libusb_bulk_transfer(dev_handle, DIRTYJTAG_READ_EP,
					rx_buf, 32, &actual_length, 1000);
		if (ret < 0) {
			cerr << "writeTDI: read: usb bulk write failed " << ret << endl;
			return EXIT_FAILURE;
		}

		if (rx) {
			for (int i = 0; i < bit_to_send; i++)
				rx_ptr[i >> 3] = (rx_ptr[i >> 3] >> 1) |
						(((rx_buf[i >> 3] << (i&0x07)) & 0x80));
			rx_ptr += byte_to_send;
		}

		real_bit_len -= bit_to_send;
		tx_ptr += byte_to_send;
	}

	/* this step exist only with [D|I]R_SHIFT */
	if (end) {
		int pos = len-1;
		uint8_t sig;
		unsigned char last_bit =
				(tx_cpy[pos >> 3] & (1 << (pos & 0x07))) ? SIG_TDI: 0;

		if (sendBitBang(SIG_TMS | SIG_TDI,
					SIG_TMS | (last_bit), &sig, true) != 0) {
			cerr << "writeTDI: last bit error" << endl;
			return -EXIT_FAILURE;
		}

		if (rx) {
			rx[pos >> 3] >>= 1;
			if (sig & SIG_TDO) {
				rx[pos >> 3] |= (1 << (pos & 0x07));;
			}
		}
	}
	return EXIT_SUCCESS;
}

int DirtyJtag::sendBitBang(uint8_t mask, uint8_t val, uint8_t *read, bool last)
{
	int actual_length;
	mask |= SIG_TCK;
	uint8_t buf[] = { CMD_SETSIG,
					static_cast<uint8_t>(mask),
					static_cast<uint8_t>(val),
					CMD_SETSIG,
					static_cast<uint8_t>(mask),
					static_cast<uint8_t>(val | SIG_TCK),
					CMD_STOP};

	if (libusb_bulk_transfer(dev_handle, DIRTYJTAG_WRITE_EP,
			buf, 7, &actual_length, 1000) < 0) {
		cerr << "sendBitBang: usb bulk write failed 1" << endl;
		return -EXIT_FAILURE;
	}

	if (read) {
		uint8_t rd_buf[] = {CMD_GETSIG, CMD_STOP};
		if (libusb_bulk_transfer(dev_handle, DIRTYJTAG_WRITE_EP,
				rd_buf, 2, &actual_length, 1000) < 0) {
			cerr << "sendBitBang: usb bulk write failed 3" << endl;
			return -EXIT_FAILURE;
		}

		if (libusb_bulk_transfer(dev_handle, DIRTYJTAG_READ_EP,
				read, 1, &actual_length, 1000) < 0) {
			cerr << "sendBitBang: usb bulk write failed 4" << endl;
			return -EXIT_FAILURE;
		}
	}

	if (last) {
		buf[2] &= ~SIG_TCK;
		buf[3] = CMD_STOP;
		if (libusb_bulk_transfer(dev_handle, DIRTYJTAG_WRITE_EP,
				buf, 4, &actual_length, 1000) < 0) {
			cerr << "sendBitBang usb bulk write failed" << endl;
			return -EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}
