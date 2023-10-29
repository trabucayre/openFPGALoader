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
#define CH347JTAG_PID 0x55dd

#define CH347JTAG_INTF        2
#define CH347JTAG_WRITE_EP    0x06
#define CH347JTAG_READ_EP     0x86

#define CH347JTAG_TIMEOUT     200

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

static void LIBUSB_CALL sync_cb(struct libusb_transfer *transfer) {
	int *complete = (int *)transfer->user_data;
	*complete = 1;
}

// defer should only be used with rlen == 0

int CH347Jtag::usb_xfer(unsigned wlen, unsigned rlen, unsigned *ract, bool defer) {
	if (_verbose) {
		fprintf(stderr, "usb_xfer: deferred: %ld\n", obuf - _obuf);
	}
	if (defer && !rlen && obuf - _obuf + wlen < MAX_BUFFER - 12) {
		obuf += wlen;
		return 0;
	}

	// write out whole buffer

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

	int wcomplete = 0, rcomplete = 0;
	libusb_fill_bulk_transfer(wtrans, dev_handle, CH347JTAG_WRITE_EP, obuf,
		wlen, sync_cb, &wcomplete, CH347JTAG_TIMEOUT);
	int r = libusb_submit_transfer(wtrans);
	if (r < 0)
		return r;
	if (rlen) {
		rcomplete = 0;
		libusb_fill_bulk_transfer(rtrans, dev_handle, CH347JTAG_READ_EP, ibuf,
			rlen, sync_cb, &rcomplete, CH347JTAG_TIMEOUT);
		r = libusb_submit_transfer(rtrans);
		if (r < 0)
			return r;
	} else {
		rcomplete = 1;
	}
	while (!wcomplete) {
		r = libusb_handle_events_completed(usb_ctx, &wcomplete);
		if (r < 0) {
			if (r != LIBUSB_ERROR_INTERRUPTED) {
				libusb_cancel_transfer(wtrans);
				if (rlen) libusb_cancel_transfer(rtrans);
			}
			continue;
		}
		if (!wtrans->dev_handle) {
			wtrans->status = LIBUSB_TRANSFER_NO_DEVICE;
			wcomplete = 1;
		}
	}
	while (!rcomplete) {
		r = libusb_handle_events_completed(usb_ctx, &rcomplete);
		if (r < 0) {
			if (r != LIBUSB_ERROR_INTERRUPTED)
				libusb_cancel_transfer(rtrans);
			continue;
		}
		if (!rtrans->dev_handle) {
			rtrans->status = LIBUSB_TRANSFER_NO_DEVICE;
			rcomplete = 1;
		}
	}
	if (wtrans->status != LIBUSB_TRANSFER_COMPLETED) {
		return LIBUSB_ERROR_IO;
	}
	if (rlen) {
		if (rtrans->status != LIBUSB_TRANSFER_COMPLETED) {
			return LIBUSB_ERROR_IO;
		}
		if ((int)rlen > rtrans->actual_length) {
			throw runtime_error("input buffer underflow");
		}
		if (ract) *ract = rtrans->actual_length;
		if (_verbose) {
			fprintf(stderr, "ibuf[%d] = {", rtrans->actual_length);
			for (int i = 0; i < rtrans->actual_length; ++i) {
				fprintf(stderr, "%02x ", ibuf[i]);
			}
			fprintf(stderr, "}\n\n");
		}
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

CH347Jtag::CH347Jtag(uint32_t clkHZ, int8_t verbose):
      _verbose(verbose>1), dev_handle(NULL), usb_ctx(NULL), obuf(_obuf)
{
	int actual_length = 0;
	struct libusb_device_descriptor desc;
	struct libusb_device *dev;
	int rv;
	if (libusb_init(&usb_ctx) < 0) {
		printError("libusb init failed");
		goto err_exit;
	}
	dev_handle = libusb_open_device_with_vid_pid(usb_ctx, CH347JTAG_VID,
		CH347JTAG_PID);
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

	if (desc.bcdDevice < 0x241) {
		printWarn("Old version of the chip, JTAG might not work");
	}

	if (libusb_set_auto_detach_kernel_driver(dev_handle, true)) {
		printError("libusb error wrile setting auto-detach of kernel driver");
		goto usb_exit;
	}
	if (libusb_claim_interface(dev_handle, CH347JTAG_INTF)) {
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
	libusb_release_interface(dev_handle, CH347JTAG_INTF);
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
		libusb_release_interface(dev_handle, CH347JTAG_INTF);
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
#if 1
	unsigned i = 0, sl = 60000000 >> 5;
	for (; i < 6; ++i, sl <<= 1) {
		if (clkHZ < sl)
			break;
	}
#else
	int i = 6;
	unsigned sl = 60000000;
	for (; i > 1; --i, sl >>= 1) {
		if (clkHZ > sl)
			break;
	}
#endif
	_clkHZ = sl;
	if (setClk(i)) {
		printError("failed to set clock rate");
		return 0;
	}
	char mess[256];
	snprintf(mess, 256, "JTAG TCK frequency set to %.3f MHz\n\n", _clkHZ/1e6);
	printInfo(mess);
	return _clkHZ;
}

int CH347Jtag::writeTMS(const uint8_t *tms, uint32_t len, bool flush_buffer,
		__attribute__((unused)) const uint8_t tdi)
{
	if (get_obuf_length() < (int)(len * 2 + 4)) { // check if there is enough room left
		flush();
	}
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
	for (unsigned i = 0; i < size / 8; ++i) {
		uint8_t b = 0;
		uint8_t *xb = &ibuf[3 + i * 8];
		for (unsigned j = 0; j < 8; ++j)
			b |= (xb[j] & 1) << j;
		*rptr++ = b;
	}
	return EXIT_SUCCESS;
}
