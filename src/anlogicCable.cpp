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


#define ANLOGICCABLE_VIDv1 0x0547
#define ANLOGICCABLE_VIDv2 0x336C
#define ANLOGICCABLE_PID   0x1002

#define ANLOGICCABLE_CONF_EP  0x08
#define ANLOGICCABLE_WRITE_EP 0x06
#define ANLOGICCABLE_READ_EP  0x82

#define ANLOGICCABLE_FREQ_CMD 0x01

/* Pinout:
 * PA0 - JTAG TDO
 * PA1 - JTAG TMS
 * PA2 - JTAG TDI
 * PA3 - JTAG TCK
 * ref: https://github.com/AnlogicInfo/anlogic-usbjtag
*/

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

#define ANLOGICCABLE_TDI_MSK ((ANLOGICCABLE_TDI_PIN << 4) | \
	ANLOGICCABLE_TDI_PIN)
#define ANLOGICCABLE_TMS_MSK ((ANLOGICCABLE_TMS_PIN << 4) | \
	ANLOGICCABLE_TMS_PIN)

AnlogicCable::AnlogicCable(uint32_t clkHz):
			dev_handle(NULL), usb_ctx(NULL), _curr_tdi(0), _curr_tms(0)
{
	int ret;

	if (libusb_init(&usb_ctx) < 0)
		throw std::runtime_error("libusb init failed");

	/* First: Try with original (old) VID */
	dev_handle = libusb_open_device_with_vid_pid(usb_ctx,
					ANLOGICCABLE_VIDv1, ANLOGICCABLE_PID);
	/* If not found: try with new VID */
	if (!dev_handle) {
		dev_handle = libusb_open_device_with_vid_pid(usb_ctx,
						ANLOGICCABLE_VIDv2, ANLOGICCABLE_PID);

		if (!dev_handle) {
			cleanup();
			throw std::runtime_error("fails to open device");
		}
	}

	ret = libusb_claim_interface(dev_handle, 0);
	if (ret) {
		cleanup();
		throw std::runtime_error("libusb error while claiming AnlogicCable "
			"interface #0");
	}

	if (setClkFreq(clkHz) < 0) {
		cleanup();
		throw std::runtime_error("Fail to set frequency");
	}
}

AnlogicCable::~AnlogicCable()
{
	cleanup();
}

void AnlogicCable::cleanup()
{
	if (dev_handle)
		libusb_close(dev_handle);
	dev_handle = nullptr;
	if (usb_ctx)
		libusb_exit(usb_ctx);
	usb_ctx = nullptr;
}

int AnlogicCable::setClkFreq(uint32_t clkHz)
{
	int actual_length;
	int ret, req_freq = clkHz;

	uint8_t buf[] = {ANLOGICCABLE_FREQ_CMD, 0};

	if (clkHz > 6000000) {
		printWarn("Anlogic JTAG probe limited to 6MHz");
	}

	if (clkHz >= 6000000) {
		buf[1] = ANLOGICCABLE_FREQ_6M;
		clkHz = 6000000;
	} else if (clkHz >= 3000000) {
		buf[1] = ANLOGICCABLE_FREQ_3M;
		clkHz = 3000000;
	} else if (clkHz >= 2000000) {
		buf[1] = ANLOGICCABLE_FREQ_2M;
		clkHz = 2000000;
	} else if (clkHz >= 1000000) {
		buf[1] = ANLOGICCABLE_FREQ_1M;
		clkHz = 1000000;
	} else if (clkHz >= 600000) {
		buf[1] = ANLOGICCABLE_FREQ_600K;
		clkHz = 600000;
	} else if (clkHz >= 400000) {
		buf[1] = ANLOGICCABLE_FREQ_400K;
		clkHz = 400000;
	} else if (clkHz >= 200000) {
		buf[1] = ANLOGICCABLE_FREQ_200K;
		clkHz = 200000;
	} else if (clkHz >= 100000) {
		buf[1] = ANLOGICCABLE_FREQ_100K;
		clkHz = 100000;
	} else {  // Fallback for all freq < 100kHz
		buf[1] = ANLOGICCABLE_FREQ_90K;
		clkHz = 90000;
	}

	ret = libusb_bulk_transfer(dev_handle, ANLOGICCABLE_CONF_EP,
					buf, 2, &actual_length, 1000);
	if (ret < 0) {
		printError("setClkFreq: usb bulk write failed " + std::to_string(ret));
		return -EXIT_FAILURE;
	}

	if (actual_length != 2) {
		printError("setClkFreq: usb bulk partial write " +
			std::to_string(actual_length) + " instead of 2");
		return -EXIT_FAILURE;
	}

	printWarn("Jtag frequency : requested " + std::to_string(req_freq) +
			"Hz -> real " + std::to_string(clkHz) + "Hz");

	_clkHZ = clkHz;

	return clkHz;
}

int AnlogicCable::writeTMS(const uint8_t *tms, uint32_t len, bool flush_buffer,
		const uint8_t tdi)
{
	(void) flush_buffer;

	if (len == 0)
		return 0;

	_curr_tdi = tdi;

	uint8_t buf[512];
	const uint8_t mask = (ANLOGICCABLE_TCK_PIN << 4) |
		((tdi) ? ANLOGICCABLE_TDI_MSK : 0);

	uint32_t full_len = len;
	const uint8_t *tx_ptr = tms;

	while (full_len > 0) {
		/* when len > buffer capacity -> limit to capacity
		 * else use len
		 */
		int xfer_len = (full_len > 512) ? 512 : full_len;

		for (int i = 0; i < xfer_len; i++) {
			buf[i] = mask;
			_curr_tms = (tx_ptr[i >> 3] & (1 << (i & 0x07))) ? 1 : 0;
			if (_curr_tms)
				buf[i] |= ANLOGICCABLE_TMS_MSK;
		}

		/* when last burst, complite rest of buffer with
		 * fixed state
		 */
		if (xfer_len < 512)
			memset(&buf[xfer_len], buf[xfer_len-1] | ANLOGICCABLE_TCK_PIN,
				512-xfer_len);

		if (write(buf, NULL, 512, 0) < 0)
			return -EXIT_FAILURE;

		full_len -= xfer_len;
		tx_ptr += xfer_len >> 3;  // advance per bytes
	}

	return len;
}

int AnlogicCable::toggleClk(uint8_t /*tms*/, uint8_t /*tdi*/, uint32_t clk_len)
{
	uint8_t buf[512];
	const uint8_t mask = (ANLOGICCABLE_TCK_PIN << 4) |
		((_curr_tms) ? ANLOGICCABLE_TMS_MSK : 0) |
		((_curr_tdi) ? ANLOGICCABLE_TDI_MSK : 0);
	const uint8_t last = mask | ANLOGICCABLE_TCK_PIN;

	uint32_t len = clk_len;
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

int AnlogicCable::writeTDI(const uint8_t *tx, uint8_t *rx, uint32_t len,
	bool end)
{
	uint8_t buf[512];
	uint8_t mask = (ANLOGICCABLE_TCK_PIN << 4) |
		((!tx && _curr_tdi) ? ANLOGICCABLE_TDI_MSK : 0);

	uint32_t full_len = len;
	const uint8_t *tx_ptr = tx;
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
				_curr_tdi = (tx_ptr[i >> 3] & (1 << (i & 0x07))) ? 1 : 0;
				if (_curr_tdi)
					buf[i] |= ANLOGICCABLE_TDI_MSK;
			}
			tx_ptr += (xfer_len >> 3);
		}

		/* when last burst, complite rest of buffer with
		 * fixed state
		 */
		const bool last = full_len <= 512;
		if (end & last) { /* set TMS high with the last bit */
			buf[xfer_len - 1] |= ANLOGICCABLE_TMS_MSK;
			_curr_tms = 1;
		}

		/* fill remaining part of the buffer with the same
		 * tms/tdi state and with tck high and stable
		 */
		if (xfer_len < 512) {
			memset(&buf[xfer_len], buf[xfer_len - 1] | ANLOGICCABLE_TCK_PIN,
				512 - xfer_len);
		}

		if (write(buf, (rx)?rx_ptr:NULL, 512, xfer_len) < 0)
			return -EXIT_FAILURE;

		if (rx)
			rx_ptr += (xfer_len >> 3);

		_curr_tms = (end & last) ? 1 : 0;  // Update tms when needed

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
		printError("write: usb bulk write failed " + std::to_string(ret));
		return -EXIT_FAILURE;
	}
	if (actual_length != len) {
		printError("write: usb bulk partial write " +
			std::to_string(actual_length) + " instead of " +
			std::to_string(len));
		return -EXIT_FAILURE;
	}

	/* all write must be followed by a read */
	ret = libusb_bulk_transfer(dev_handle, ANLOGICCABLE_READ_EP,
			in_buf, len, &actual_length, 1000);
	if (ret < 0) {
		printError("write: usb bulk read failed " + std::to_string(ret));
		return -EXIT_FAILURE;
	}
	if (actual_length != len) {
		printError("write: usb bulk partial read " +
			std::to_string(actual_length) + " instead of " +
			std::to_string(len));
		return -EXIT_FAILURE;
	}

	if (out_buf) {
		for (int i = 0; i < rd_len; i++) {
			const int shift = i >> 3;
			if ((i & 0x07) == 0)
				out_buf[shift] = 0;
			out_buf[shift] >>= 1;
			if ((in_buf[i] >> 4) & 0x01)
				out_buf[shift] |= 0x80;
		}
	}
	return len;
}
