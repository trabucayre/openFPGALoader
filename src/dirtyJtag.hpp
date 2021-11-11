// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_DIRTYJTAG_HPP_
#define SRC_DIRTYJTAG_HPP_

#include <libusb.h>

#include "jtagInterface.hpp"

/*!
 * \file DirtyJtag.hpp
 * \class DirtyJtag
 * \brief concrete class between jtag implementation and FTDI capable bitbang mode
 * \author Gwenhael Goavec-Merou
 */

class DirtyJtag : public JtagInterface {
 public:
	DirtyJtag(uint32_t clkHZ, uint8_t verbose);
	virtual ~DirtyJtag();

	int setClkFreq(uint32_t clkHZ) override;

	/* TMS */
	int writeTMS(uint8_t *tms, uint32_t len, bool flush_buffer) override;
	/* TDI */
	int writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;
	/* clk */
	int toggleClk(uint8_t tms, uint8_t tdo, uint32_t clk_len) override;

	/*!
	 * \brief return internal buffer size (in byte).
	 * \return _buffer_size divided by 2 (two byte for clk) and divided by 8 (one
	 * state == one byte)
	 */
	int get_buffer_size() override { return 0;}

	bool isFull() override { return false;}

	int flush() override;

 private:
	uint8_t _verbose;

	int sendBitBang(uint8_t mask, uint8_t val, uint8_t *read, bool last);
	void getVersion();

    libusb_device_handle *dev_handle;
	libusb_context *usb_ctx;
	uint8_t _tdi;
	uint8_t _tms;
	uint8_t _version;
};
#endif  // SRC_DIRTYJTAG_HPP_
