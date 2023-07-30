// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2023 Alexey Starikovskiy <aystarik@gmail.com>
 */
#pragma once

#include <libusb.h>

#include "jtagInterface.hpp"

class CH347Jtag : public JtagInterface {
 public:
	CH347Jtag(uint32_t clkHZ, int8_t verbose);
	virtual ~CH347Jtag();

	int setClkFreq(uint32_t clkHZ) override;

	/* TMS */
	int writeTMS(uint8_t *tms, uint32_t len, bool flush_buffer) override;
	/* TDI */
	int writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;
	/* clk */
	int toggleClk(uint8_t tms, uint8_t tdo, uint32_t clk_len) override;

	int get_buffer_size() override { return 0;}

	bool isFull() override { return false;}

	int flush() override {return 0;}

 private:
	bool _verbose;
	int setClk(const uint8_t &factor);

	libusb_device_handle *dev_handle;
	libusb_context *usb_ctx;
	struct libusb_transfer *wtrans, *rtrans;
	int rcomplete, wcomplete;
	uint8_t ibuf[512];
	uint8_t obuf[512];
	int usb_xfer(unsigned wlen, unsigned rlen, unsigned *actual);
};
