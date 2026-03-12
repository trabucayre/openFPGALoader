// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_DIRTYJTAG_HPP_
#define SRC_DIRTYJTAG_HPP_

#include <libusb.h>

#include "jtagInterface.hpp"

/*!
 * \file dirtyJtag.hpp
 * \class DirtyJtag
 * \brief concrete class between jtag implementation and DirtyJTAG probe
 * \author Gwenhael Goavec-Merou
 */
class DirtyJtag : public JtagInterface {
 public:
	DirtyJtag(uint32_t clkHz, int8_t verbose, uint16_t vid = 0x1209, uint16_t pid = 0xC0CA);
	virtual ~DirtyJtag();

	int setClkFreq(uint32_t clkHz) override;

	/*!
	 * \brief drive TMS to move in JTAG state machine
	 * \param tms serie of TMS state
	 * \param len number of TMS state
	 * \param flush_buffer force flushing the buffer
	 * \param tdi TDI constant value
	 * \return number of state written
	 */
	int writeTMS(const uint8_t *tms, uint32_t len, bool flush_buffer, const uint8_t tdi = 1) override;

	/*!
	 * \brief send TDI bits (mainly in shift DR/IR state)
	 * \param tx array of TDI values (used to write)
	 * \param rx array of TDO values (used when read)
	 * \param len number of bit to send/receive
	 * \param end in JTAG state machine last bit and tms are set in same time
	 * \return number of bit written and/or read
	 */
	int writeTDI(const uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;

	/*!
	 * \brief toggle clock with static tms and tdi
	 * \param tms state of tms signal
	 * \param tdo state of tdo signal
	 * \param clk_len number of clock cycle
	 * \return number of clock cycle send
	 */
	int toggleClk(uint8_t tms, uint8_t tdo, uint32_t clk_len) override;

	/*!
	 * \brief return internal buffer size (in byte).
	 * \return _buffer_size divided by 2 (two byte for clk) and divided by 8 (one
	 * state == one byte)
	 */
	int get_buffer_size() override { return 0; }

	bool isFull() override { return false; }

	int flush() override;

	/* read gpio */
	uint8_t gpio_get();
	/* update selected gpio */
	bool gpio_set(uint8_t gpio);
	bool gpio_clear(uint8_t gpio);

 private:
	int8_t _verbose;

	int sendBitBang(uint8_t mask, uint8_t val, uint8_t *read, bool last);
	bool getVersion();
	bool _set_gpio_level(uint8_t gpio, uint8_t val);

	/* USB */
	void close_usb();
	libusb_device_handle *dev_handle;
	libusb_context *usb_ctx;

	uint8_t _tdi;
	uint8_t _tms;
	uint8_t _version;
};
#endif  // SRC_DIRTYJTAG_HPP_
