// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022-2026 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_XILINXPLATFORMCABLEUSB_HPP_
#define SRC_XILINXPLATFORMCABLEUSB_HPP_

#include <cstdint>
#include <memory>
#include <string>

#include "fx2_ll.hpp"
#include "jtagInterface.hpp"

#define XPCU_DEFAULT_VID 0x03fd
#define XPCU_DEFAULT_PID 0x0013

/*!
 * \file xilinxPlatformCableUSB.hpp
 * \class XilinxPlatformCableUSB
 * \brief concrete class between jtag implementation and Xilinx USB Cable probe
 * \author Gwenhael Goavec-Merou
 */
class XilinxPlatformCableUSB : public JtagInterface {
 public:
	XilinxPlatformCableUSB(const uint16_t vid = XPCU_DEFAULT_VID,
		const uint16_t pid = XPCU_DEFAULT_PID,
		uint32_t clkHz = 750000,
		const std::string &firmware_path = "",
		int8_t verbose = 0);
	~XilinxPlatformCableUSB() override;

	int setClkFreq(uint32_t clkHz) override;

	/*!
	 * \brief drive TMS to move in JTAG state machine
	 * \param tms serie of TMS state
	 * \param len number of TMS state
	 * \param flush_buffer force flushing the buffer
	 * \param tdi TDI constant value
	 * \return number of state written
	 */
	int writeTMS(const uint8_t *tms, uint32_t len, bool flush_buffer,
		const uint8_t tdi = 1) override;

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
	 * \param tdi state of tdi signal
	 * \param clk_len number of clock cycle
	 * \return number of clock cycle send
	 */
	int toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len) override;

	/*!
	 * \brief return internal buffer size (in byte).
	 * \return _buffer_size
	 */
	int get_buffer_size() override { return static_cast<int>(_buffer_size); }

	bool isFull() override { return _nb_bit >= _buffer_bit_size; }

	int flush() override;

	/*!
	 * \brief display fx2 & CPLD firmwares version
	 */
	void displayCableVersion();

	/*!
	 * \brief enable device output
	 * \param[in] enable: enable or disable device
	 * \return true when transfer success, false otherwise
	 */
	bool enableDevice(bool enable);

 private:
	/* \brief pack one JTAG bit into the internal buffer
	 * \return true when buffer is full, false otherwise
	 */
	bool storeBit(uint8_t tdi, uint8_t tms, uint8_t tck, uint8_t tdo) noexcept;
	int write(uint8_t *rx, uint32_t rx_offset = 0);
	static uint32_t rxBufSize(uint32_t nb_bit) noexcept;

	int8_t _verbose;

	std::unique_ptr<uint8_t[]> _in_buf;
	uint32_t _nb_bit;
	uint32_t _nb_tdo_bit;
	uint8_t _curr_tms;
	uint8_t _curr_tdi;
	uint32_t _buffer_size;
	uint32_t _buffer_bit_size;
	std::unique_ptr<FX2_ll> fx2;
};

#endif  // SRC_XILINXPLATFORMCABLEUSB_HPP_
