// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2024 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_GWU2X_JTAG_HPP__
#define SRC_GWU2X_JTAG_HPP__

#include <libusb.h>

#include <cstdint>
#include <cstring>

#include "cable.hpp"
#include "jtagInterface.hpp"
#include "libusb_ll.hpp"

class GowinGWU2x: public JtagInterface, private libusb_ll
{
 public:
	GowinGWU2x(cable_t *cable, uint32_t clkHz, int8_t verbose);
	~GowinGWU2x();

	int setClkFreq(uint32_t clkHz) override;

	/*!
	 * \brief flush TMS internal buffer (ie. transmit to converter)
	 * \param tdo: pointer for read operation. May be NULL
	 * \param len: number of bit to send
	 * \return number of bit send/received
	 */
	int writeTMS(const uint8_t *tms, uint32_t len, bool flush_buffer, const uint8_t tdi = 1) override;

	/*!
	 * \brief send TDI bits (mainly in shift DR/IR state)
	 * \param tdi: array of TDI values (used to write)
	 * \param tdo: array of TDO values (used when read)
	 * \param len: number of bit to send/receive
	 * \param end: in JTAG state machine last bit and tms are set in same time
	 *             but only in shift[i|d]r, if end is false tms remain the same.
	 * \return number of bit written and/or read
	 */
	int writeTDI(const uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;

	/*!
	 * \brief toggle clk without touch of TDI/TMS
	 * \param tms: state of tms signal
	 * \param tdi: state of tdi signal
	 * \param clk_len: number of clock cycle
	 * \return number of clock cycle send
	 */
	int toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len) override;

	/*!
	 * \brief return internal buffer size (in byte)
	 * \return internal buffer size
	 */
	int get_buffer_size() override {return static_cast<int>(_buffer_len);}

	/*!
	 * \brief return status of internal buffer
	 * \return true when internal buffer is full
	 */
	bool isFull() override {return _xfer_pos == _buffer_len;}

	/*!
	 * \brief force internal flush buffer
	 * \return 1 if success, 0 if nothing to write, -1 is something wrong
	 */
	int flush() override {
		if (_xfer_pos == 0)
			return 0;
		return xfer(nullptr, 0) ? 1 : -1;
	}
 private:
	bool xfer(uint8_t *rx, uint16_t rx_len, uint16_t timeout = 1000);
	bool store_seq(const uint8_t &opcode, const uint8_t &len,
		const uint8_t &data, const bool readback = false);
	bool init_device();
	void displayCmd();    // debug purpose: translate sequence to human
						  // readable sequence
	bool _verbose;
	cable_t *_cable;
	struct libusb_device *_usb_dev;
	struct libusb_device_handle *_dev;
	uint8_t *_xfer_buf;   /* internal buffer */
	uint32_t _xfer_pos;   /* number of Bytes already stored in _xfer_buf */
	uint32_t _buffer_len; /* _xfer_buf capacity (Byte) */
};

#endif  // SRC_GWU2X_JTAG_HPP__
