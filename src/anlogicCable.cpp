// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <libusb.h>
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <map>
#include <vector>
#include <string>

#include "anlogicCable.hpp"
#include "display.hpp"

using namespace std;

#define ANLOGICCABLE_VID 0x0547
#define ANLOGICCABLE_PID 0x1002

#define ANLOGICCABLE_CONF_EP  0x08
#define ANLOGICCABLE_WRITE_EP 0x06
#define ANLOGICCABLE_READ_EP  0x82

#define ANLOGICCABLE_FREQ_CMD 0x01

enum analogicCablePin {
	ANLOGICCABLE_TCK_PIN = (1 << 2),
	ANLOGICCABLE_TDI_PIN = (1 << 1),
	ANLOGICCABLE_TMS_PIN = (1 << 0)
};

enum analogicCableFreq {
	ANLOGICCABLE_FREQ_6M   = 0,
	ANLOGICCABLE_FREQ_3M   = 0x4,
	ANLOGICCABLE_FREQ_2M   = 0x8,
	ANLOGICCABLE_FREQ_1M   = 0x14,
	ANLOGICCABLE_FREQ_600K = 0x24,
	ANLOGICCABLE_FREQ_400K = 0x38,
	ANLOGICCABLE_FREQ_200K = 0x70,
	ANLOGICCABLE_FREQ_100K = 0xe8,
	ANLOGICCABLE_FREQ_90K  = 0xff
};

AnlogicCable::AnlogicCable(uint32_t clkHZ):
			dev_handle(NULL), usb_ctx(NULL)
{
	int ret;

	if (libusb_init(&usb_ctx) < 0) {
		cerr << "libusb init failed" << endl;
		throw std::exception();
	}

	dev_handle = libusb_open_device_with_vid_pid(usb_ctx,
					ANLOGICCABLE_VID, ANLOGICCABLE_PID);
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

	if (setClkFreq(clkHZ) < 0) {
		cerr << "Fail to set frequency" << endl;
		throw std::exception();
	}
}

AnlogicCable::~AnlogicCable()
{
	if (dev_handle)
		libusb_close(dev_handle);
	if (usb_ctx)
		libusb_exit(usb_ctx);
}

int AnlogicCable::setClkFreq(uint32_t clkHZ)
{
	int actual_length;
	int ret, req_freq = clkHZ;

	uint8_t buf[] = {ANLOGICCABLE_FREQ_CMD, 0};

	if (clkHZ > 6000000) {
		printWarn("Anlogic JTAG probe limited to 6MHz");
		clkHZ = 6000000;
	}

	if (clkHZ >= 6000000) {
		buf[1] = ANLOGICCABLE_FREQ_6M;
		clkHZ = 6000000;
	} else if (clkHZ >= 3000000) {
		buf[1] = ANLOGICCABLE_FREQ_3M;
		clkHZ = 3000000;
	} else if (clkHZ >= 1000000) {
		buf[1] = ANLOGICCABLE_FREQ_1M;
		clkHZ = 1000000;
	} else if (clkHZ >= 600000) {
		buf[1] = ANLOGICCABLE_FREQ_600K;
		clkHZ = 600000;
	} else if (clkHZ >= 400000) {
		buf[1] = ANLOGICCABLE_FREQ_400K;
		clkHZ = 400000;
	} else if (clkHZ >= 200000) {
		buf[1] = ANLOGICCABLE_FREQ_200K;
		clkHZ = 200000;
	} else if (clkHZ >= 100000) {
		buf[1] = ANLOGICCABLE_FREQ_100K;
		clkHZ = 100000;
	} else if (clkHZ >= 90000) {
		buf[1] = ANLOGICCABLE_FREQ_90K;
		clkHZ = 90000;
	}

	ret = libusb_bulk_transfer(dev_handle, ANLOGICCABLE_CONF_EP,
			        buf, 2, &actual_length, 1000);
	if (ret < 0) {
		cerr << "setClkFreq: usb bulk write failed " << ret << endl;
		return -EXIT_FAILURE;
	}

	printWarn("Jtag frequency : requested " + std::to_string(req_freq) +
			"Hz -> real " + std::to_string(clkHZ) + "Hz");

	_clkHZ = clkHZ;

	return clkHZ;
}

int AnlogicCable::writeTMS(uint8_t *tms, uint32_t len, bool flush_buffer)
{
	(void) flush_buffer;

	if (len == 0)
		return 0;

	uint8_t buf[512];
	uint8_t mask = (ANLOGICCABLE_TCK_PIN << 4);

	int full_len = len;
	uint8_t *tx_ptr = tms;

	while (full_len > 0) {
		/* when len > buffer capacity -> limit to capacity
		 * else use len
		 */
		int xfer_len = (full_len > 512) ? 512 : full_len;

		for (int i = 0; i < xfer_len; i++) {
			buf[i] = mask;
			if (tx_ptr[i >> 3] & (1 << (i & 0x07)))
				buf[i] |= (ANLOGICCABLE_TMS_PIN |
					(ANLOGICCABLE_TMS_PIN << 4));
		}

		/* when last burst, complite rest of buffer with
		 * fixed state
		 */
		if (xfer_len < 512) {
			memset(&buf[xfer_len], buf[xfer_len-1] | ANLOGICCABLE_TCK_PIN,
				512-xfer_len);
		}

		if (write(buf, NULL, 512, 0) < 0)
			return -EXIT_FAILURE;

		full_len -= xfer_len;
		tx_ptr += xfer_len;
	}

	return len;
}

int AnlogicCable::toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len)
{
	uint8_t buf[512];
	uint8_t mask = ((tms) ? ANLOGICCABLE_TMS_PIN : 0) |
		((tdi) ? ANLOGICCABLE_TDI_PIN : 0);
	mask |= (((mask & 0x0f) << 4) | ((ANLOGICCABLE_TCK_PIN << 4)));
	uint8_t last = mask | ANLOGICCABLE_TCK_PIN;

	int len = clk_len;
	while (len > 0) {
		/* when len > buffer capacity -> limit to capacity
		 * else use len
		 */
		int xfer_len = (len > 512) ? 512 : len;
		/* since value is always the same
		 * fill xfer_len byte with the same value
		 */
		memset(buf, mask, xfer_len);
		/* when last burst, complite rest of buffer with
		 * fixed state
		 */
		if (xfer_len < 512)
			memset(&buf[xfer_len], last, 512-xfer_len);

		if (write(buf, NULL, 512, 0) < 0)
			return -EXIT_FAILURE;

		len -= xfer_len;
	}

	return EXIT_SUCCESS;
}

int AnlogicCable::flush()
{
	return 0;
}

int AnlogicCable::writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end)
{
	uint8_t buf[512];
	uint8_t mask = (ANLOGICCABLE_TCK_PIN << 4);

	int full_len = len;
	uint8_t *tx_ptr = tx;
	uint8_t *rx_ptr = rx;

	while (full_len > 0) {
		/* when len > buffer capacity -> limit to capacity
		 * else use len
		 */
		int xfer_len = (full_len > 512) ? 512 : full_len;

		if (!tx) {
			memset(buf, mask, xfer_len);
		} else {
			for (int i = 0; i < xfer_len; i++) {
				buf[i] = mask;
				if (tx_ptr[i >> 3] & (1 << (i & 0x07)))
					buf[i] |= (ANLOGICCABLE_TDI_PIN |
						(ANLOGICCABLE_TDI_PIN << 4));
			}
			tx_ptr += (xfer_len >> 3);
		}

		/* when last burst, complite rest of buffer with
		 * fixed state
		 */
		if (xfer_len < 512) {
			if (end) { /* set TMS high with the last bit */
				buf[xfer_len-1] |= ((ANLOGICCABLE_TMS_PIN << 4) |
					(ANLOGICCABLE_TMS_PIN));
			}
			memset(&buf[xfer_len], buf[xfer_len-1] | ANLOGICCABLE_TCK_PIN,
				512-xfer_len);
		}

		if (write(buf, (rx)?rx_ptr:NULL, 512, xfer_len) < 0)
			return -EXIT_FAILURE;

		if (rx) {
			rx_ptr += (xfer_len >> 3);
		}

		full_len -= xfer_len;
	}

	return EXIT_SUCCESS;
}

int AnlogicCable::write(uint8_t *in_buf, uint8_t *out_buf, int len, int rd_len)
{
	int actual_length;
	int ret = libusb_bulk_transfer(dev_handle, ANLOGICCABLE_WRITE_EP,
			in_buf, len, &actual_length, 1000);
	if (ret < 0) {
		cerr << "write: usb bulk write failed " << ret << endl;
		return -EXIT_FAILURE;
	}
	/* all write must be followed by a read */
	ret = libusb_bulk_transfer(dev_handle, ANLOGICCABLE_READ_EP,
			in_buf, len, &actual_length, 1000);
	if (ret < 0) {
		cerr << "write: usb bulk read failed " << ret << endl;
		return -EXIT_FAILURE;
	}

	if (out_buf) {
		for (int i = 0; i < rd_len; i++) {
			out_buf[i >> 3] >>= 1;
			if ((in_buf[i] >> 4) & 0x01)
				out_buf[i >> 3] |= 0x80;
		}
	}
	return len;
}
