// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2023 Alexey Starikovskiy <aystarik@gmail.com>
 */

#define _DEFAULT_SOURCE

#include <libusb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "ch347jtag.hpp"
#include "display.hpp"

using namespace std;

#define CH347JTAG_VID 0x1a86
#define CH347T_JTAG_PID 0x55dd    //ch347T
#define CH347F_JTAG_PID 0x55de    //ch347F

#define KHZ(n) (uint32_t)((n)*UINT32_C(1000))
#define MHZ(n) (uint32_t)((n)*UINT32_C(1000000))
#define GHZ(n) (uint32_t)((n)*UINT32_C(1000000000))

#define CH347TJTAG_INTF       2
#define CH347FJTAG_INTF 	  4
#define CH347JTAG_WRITE_EP    0x06
#define CH347JTAG_READ_EP     0x86

#define CH347JTAG_TIMEOUT     1000

enum CH347JtagCmd {
	CMD_BYTES_WO = 0xd3,
	CMD_BYTES_WR = 0xd4,
	CMD_BITS_WO  = 0xd1,
	CMD_BITS_WR  = 0xd2,
	CMD_CLK      = 0xd0,
};

enum CH347JtagSig {
	SIG_TCK =       0b1,
	SIG_TMS =      0b10,
	SIG_TDI =   0b10000,
};

// defer should only be used with rlen == 0

int CH347Jtag::usb_xfer(unsigned wlen, unsigned rlen, unsigned *ract, bool defer) 
{
	int actual_length = 0;
	if (_verbose) {
		fprintf(stderr, "usb_xfer: deferred: %ld\n", obuf - _obuf);
	}
	if (defer && !rlen && obuf - _obuf + wlen < (MAX_BUFFER - 12)) {
		obuf += wlen;
		return 0;
	}

	if (obuf - _obuf > MAX_BUFFER) {
		throw runtime_error("buffer overflow");
	}

	wlen += obuf - _obuf;
	if (wlen > MAX_BUFFER) {
		throw runtime_error("buffer overflow");
	}
	obuf = _obuf;

	if (wlen == 0) {
		return 0;
	}

	if (_verbose) {
		fprintf(stderr, "obuf[%d] = {", wlen);
		for (unsigned i = 0; i < wlen; ++i) {
			fprintf(stderr, "%02x ", obuf[i]);
		}
		fprintf(stderr, "}\n\n");
	}

	int r = 0;
	if (wlen) {
		if ((r = libusb_bulk_transfer(dev_handle, CH347JTAG_WRITE_EP, obuf, wlen, &actual_length, CH347JTAG_TIMEOUT)) < 0 ) {
			return r;
		}
	}
	if (_verbose) {
		fprintf(stderr, "obuf[%d] = {", wlen);
		for (unsigned i = 0; i < wlen; ++i) {
			fprintf(stderr, "%02x ", obuf[i]);
		}
		fprintf(stderr, "}\n\n");
	}
	obuf = _obuf;
	int rlen_total = 0;
	uint8_t *pibuf = ibuf;
	if (rlen){
		while (rlen) {
			if ((r = libusb_bulk_transfer(dev_handle, CH347JTAG_READ_EP, pibuf, rlen, &actual_length, CH347JTAG_TIMEOUT)) < 0 ) {
				return r;
			}
			if (_verbose) {
				fprintf(stderr, "ibuf[%d] = {", actual_length);
				for (int i = rlen_total; i < rlen_total + actual_length; ++i) {
					fprintf(stderr, "%02x ", ibuf[i]);
				}
				fprintf(stderr, "}\n\n");
			}
			rlen -= actual_length;
			pibuf += actual_length;
			rlen_total += actual_length;
		}
		*ract = rlen_total;
	}
	return 0;
}

int CH347Jtag::setClk(const uint8_t &factor) {
	// flush the obuf
	usb_xfer(0, 0, 0, false); // is called from constructor, don't replace with virtual flush()
	memset(obuf, 0, 16);
	obuf[0] = CMD_CLK;
	obuf[1] = 6;
	obuf[4] = factor;
	unsigned actual = 0;
	int rv = usb_xfer(9, 4, &actual, false);
	if (rv || actual != 4)
		return -1;
	if (ibuf[0] != 0xd0 || ibuf[3] != 0)
		return -1;
	return 0;
}

CH347Jtag::CH347Jtag(uint32_t clkHZ, int8_t verbose, int vid, int pid, uint8_t bus_addr, uint8_t dev_addr):
      _verbose(verbose>1), dev_handle(NULL), usb_ctx(NULL), obuf(_obuf)
{
	libusb_device** devs;
	int actual_length = 0;
	int i = 0;
	ssize_t cnt;
	struct libusb_device_descriptor desc;
	struct libusb_device *dev;
	int rv;
	if (libusb_init(&usb_ctx) < 0) {
		printError("libusb init failed");
		goto err_exit;
	}
	cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0) goto err_exit;
	while ((dev = devs[i++]) != NULL)  {
		if (libusb_get_device_descriptor(dev, &desc) < 0)
			continue;
		if (desc.idVendor != vid || (desc.idProduct != pid && desc.idProduct != CH347F_JTAG_PID && desc.idProduct != CH347T_JTAG_PID))
    		continue;
		if (bus_addr != 0 && dev_addr != 0 && (libusb_get_bus_number(dev) != bus_addr || libusb_get_device_address(dev) != dev_addr))
    		continue;
		libusb_open(dev, &dev_handle);
		break;
	}
	libusb_free_device_list(devs, 1);
	if (!dev_handle) {
		printError("fails to open device");
		goto usb_exit;
	}
	dev = libusb_get_device(dev_handle);
	if (!dev) {
		printError("Couldnt get bus number and address of device");
		goto usb_exit;
	}

	rv = libusb_get_device_descriptor(dev, &desc);
	if (rv < 0) {
		printError("failed to get device descriptor");
		goto usb_exit;
	}
	_jtagIntf = (desc.idProduct == CH347T_JTAG_PID) ? CH347TJTAG_INTF : CH347FJTAG_INTF;

	if (desc.bcdDevice < 0x241 && desc.idProduct == CH347T_JTAG_PID) {
		_is_largerPack = false;
		printWarn("Old version of the chip, JTAG might not work");
	}else{
		_is_largerPack = true;
	}
	#if (!defined(WIN32) && !defined(_WIN64))
	//Windows will fails this API call
	if (libusb_set_auto_detach_kernel_driver(dev_handle, true) != LIBUSB_SUCCESS) {
		printError("libusb error wrile setting auto-detach of kernel driver");
		goto usb_exit;
	}
	#endif
	if (libusb_claim_interface(dev_handle, _jtagIntf)) {
		printError("libusb error while claiming CH347JTAG interface");
		goto usb_close;
	}
	rtrans = libusb_alloc_transfer(0);
	wtrans = libusb_alloc_transfer(0);
	if (!rtrans || !wtrans) {
		printError("libusb failed to alloc transfers");
		goto usb_release;
	}
	libusb_bulk_transfer(dev_handle, CH347JTAG_READ_EP, ibuf, 512,
		&actual_length, CH347JTAG_TIMEOUT);
	_setClkFreq(clkHZ);
	return;
usb_release:
	libusb_release_interface(dev_handle, _jtagIntf);
usb_close:
	libusb_close(dev_handle);
usb_exit:
	libusb_exit(usb_ctx);
err_exit:
	throw std::exception();
}

CH347Jtag::~CH347Jtag()
{
	if (rtrans) libusb_free_transfer(rtrans);
	if (wtrans) libusb_free_transfer(wtrans);

	if (dev_handle) {
		libusb_release_interface(dev_handle, _jtagIntf);
		libusb_close(dev_handle);
		dev_handle = 0;
	}
	if (usb_ctx) {
		libusb_exit(usb_ctx);
		usb_ctx = 0;
	}
}

int CH347Jtag::_setClkFreq(uint32_t clkHZ)
{
    int setClk_index = 0;
	uint32_t speed_clock_larger_pack[8] = {
		KHZ(468.75), KHZ(937.5), MHZ(1.875), MHZ(3.75),
		MHZ(7.5), MHZ(15), MHZ(30), MHZ(60)
	};
	uint32_t speed_clock_standard_pack[6] = {
		MHZ(1.875), MHZ(3.75),
		MHZ(7.5), MHZ(15), MHZ(30), MHZ(60)
	};
	uint32_t *ptr = _is_largerPack ? speed_clock_larger_pack : speed_clock_standard_pack;
	int size = (_is_largerPack?sizeof(speed_clock_larger_pack):sizeof(speed_clock_standard_pack)) / sizeof(uint32_t);
    for (int i = 0; i < size; ++i) {
		if (clkHZ > ptr[i] && clkHZ <= ptr[i+1]){
			setClk_index = i + 1;
		}
	}
	if (setClk(setClk_index)) {
		printError("failed to set clock rate");
		return 0;
	}
	char mess[256];
	snprintf(mess, 256, "JTAG TCK frequency set to %.3f MHz\n\n", (double)ptr[setClk_index] / MHZ(1));
	printInfo(mess);
	return _clkHZ;
}

int CH347Jtag::writeTMS(const uint8_t *tms, uint32_t len, bool flush_buffer,
		__attribute__((unused)) const uint8_t tdi)
{
	// if (get_obuf_length() < (int)(len * 2 + 4)) { // check if there is enough room left
	// 	flush();
	// }
 	flush();
	uint8_t *ptr = obuf;
	for (uint32_t i = 0; i < len; ++i) {
		if (ptr == obuf) {
			*ptr++ = CMD_BITS_WO;
			ptr += 2;  // leave place for length;
		}
		uint8_t x = ((tms[i >> 3] & (1 << (i & 7))) ? SIG_TMS : 0);
		*ptr++ = x;
		*ptr++ = x | SIG_TCK;
		int wlen = ptr - obuf;
		if (wlen + 1 >= get_obuf_length() || i == len - 1) {
			*ptr++ = x;  // clear TCK
			wlen = ptr - obuf;
			obuf[1] = wlen - 3;
			obuf[2] = (wlen - 3) >> 8;
			int ret = usb_xfer(wlen, 0, 0, !flush_buffer);
			if (ret < 0) {
				cerr << "writeTMS: usb bulk write failed: " <<
					libusb_strerror(static_cast<libusb_error>(ret)) << endl;
				return -EXIT_FAILURE;
			}
			ptr = obuf;
		}
	}
	return len;
}

int CH347Jtag::toggleClk(uint8_t tms, uint8_t tdi, uint32_t len)
{
	uint8_t bits = 0;
	if (tms) bits |= SIG_TMS;
	if (tdi) bits |= SIG_TDI;
	if (!bits && len > 7) {
		return writeTDI(0, 0, len, false);
	}
	
	if (get_obuf_length() < (int)(len * 2 + 4)) {
		flush();
	}

	uint8_t *ptr = obuf;
	for (uint32_t i = 0; i < len; ++i) {
		if (ptr == obuf) {
			*ptr++ = CMD_BITS_WO;
			ptr += 2;  // leave place for length;
		}
		*ptr++ = bits;
		*ptr++ = bits | SIG_TCK;
		int wlen = ptr - obuf;
		if (wlen + 1 >= get_obuf_length() || i == len - 1) {
			*ptr++ = bits;  // clear TCK
			wlen = ptr - obuf;
			obuf[1] = wlen - 3;
			obuf[2] = (wlen - 3) >> 8;
			int ret = usb_xfer(wlen, 0, 0, true);
			if (ret < 0) {
				cerr << "writeCLK: usb bulk write failed: " <<
					libusb_strerror(static_cast<libusb_error>(ret)) << endl;
				return -EXIT_FAILURE;
			}
			ptr = obuf;
		}
	}
	return EXIT_SUCCESS;
}

int CH347Jtag::writeTDI(const uint8_t *tx, uint8_t *rx, uint32_t len, bool end)
{
	if (len == 0)
		return 0;
	unsigned bytes = (len - (end ? 1 : 0)) / 8;
	unsigned bits = len - bytes * 8;
	uint8_t *rptr = rx;
	const uint8_t *tptr = tx;
	const uint8_t *txend = tx + bytes;
	uint8_t cmd = (rx != nullptr) ? CMD_BYTES_WR : CMD_BYTES_WO;
	while (tptr < txend) {
		if (get_obuf_length() < 4) {
			flush();
		}
		int avail = get_obuf_length() - 3;
		int chunk = (txend - tptr < avail)? txend - tptr: avail;
		if (tx) {
			memcpy(&obuf[3], tptr, chunk);
		} else {
			memset(&obuf[3], 0, chunk);
		}
		tptr += chunk;
		// write header
		obuf[0] = cmd;
		obuf[1] = chunk;
		obuf[2] = chunk >> 8;
		unsigned actual_length = 0;
		int ret = usb_xfer(chunk + 3, (rx) ? chunk + 3 : 0, &actual_length, rx == 0 && get_obuf_length());
		if (ret < 0) {
			cerr << "writeTDI: usb bulk read failed: " <<
				libusb_strerror(static_cast<libusb_error>(ret)) << endl;
			return -EXIT_FAILURE;
		}
		if (!rx)
			continue;
		unsigned size = ibuf[1] + ibuf[2] * 0x100;
		if (ibuf[0] != CMD_BYTES_WR || actual_length - 3 != size) {
			cerr << "writeTDI: invalid read data: " << ret << endl;
			return -EXIT_FAILURE;
		}
		memcpy(rptr, &ibuf[3], size);
		rptr += size;
	}
	unsigned actual_length;
	if (bits == 0)
		return EXIT_SUCCESS;
	cmd = (rx) ? CMD_BITS_WR : CMD_BITS_WO;
	if (get_obuf_length() < (int)(4 + bits * 2)) {
		flush();
	}
	uint8_t *ptr = &obuf[3];
	uint8_t x = 0;
	const uint8_t *bptr = tx + bytes;
	for (unsigned i = 0; i < bits; ++i) {
		uint8_t txb = (tx) ? bptr[i >> 3] : 0;
		uint8_t _tdi = (txb & (1 << (i & 7))) ? SIG_TDI : 0;
		x = _tdi;
		if (end && i == bits - 1) {
			x |= SIG_TMS;
		}
		*ptr++ = x;
		*ptr++ = x | SIG_TCK;
	}
	*ptr++ = x & ~SIG_TCK;
	unsigned wlen = ptr - obuf;
	obuf[0] = cmd;
	obuf[1] = wlen - 3;
	obuf[2] = (wlen - 3) >> 8;
	int ret = usb_xfer(wlen, (rx) ? (bits + 3) : 0, &actual_length, rx == nullptr);

	if (ret < 0) {
		cerr << "writeTDI: usb bulk read failed: " <<
			libusb_strerror(static_cast<libusb_error>(ret)) << endl;
		return -EXIT_FAILURE;
	}
	if (!rx)
		return EXIT_SUCCESS;

	unsigned size = ibuf[1] + ibuf[2] * 0x100;

	if (ibuf[0] != CMD_BITS_WR || actual_length - 3 != size) {
		cerr << "writeTDI: invalid read data: " << endl;
		return -EXIT_FAILURE;
	}
	for (unsigned i = 0; i < size; ++i) {
		if (ibuf[3 + i] == 0x01) {
			*rptr |= (0x01 << i);
		}else{
			*rptr &= ~(0x01 << i);
		}	
	}
	return EXIT_SUCCESS;
}
