// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_ANLOGICCABLE_HPP_
#define SRC_ANLOGICCABLE_HPP_

#include <libusb.h>

#include "jtagInterface.hpp"

/*!
 * \file AnlogicCable.hpp
 * \class AnlogicCable
 * \brief concrete class between jtag implementation and anlogic cable
 * \author Gwenhael Goavec-Merou
 */

class AnlogicCable : public JtagInterface {
 public:
	AnlogicCable(uint32_t clkHZ);
	virtual ~AnlogicCable();

	int setClkFreq(uint32_t clkHZ) override;

	/* TMS */
	int writeTMS(uint8_t *tms, uint32_t len, bool flush_buffer) override;
	/* TDI */
	int writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;
	/* clk */
	int toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len) override;

	/*!
	 * \brief return internal buffer size (in byte).
	 * \return _buffer_size divided by 2 (two byte for clk) and divided by 8 (one
	 * state == one byte)
	 */
	int get_buffer_size() override { return 0;}

	bool isFull() override { return false;}

	int flush() override;

 private:
	int write(uint8_t *in_buf, uint8_t *out_buf, int len, int rd_len);

    libusb_device_handle *dev_handle;
	libusb_context *usb_ctx;
};
#endif  // SRC_ANLOGICCABLE_HPP_
