// SPDX-License-Identifier: Apache-2.0
/*
 * Digilent Adept JTAG Driver for openFPGALoader
 *
 * Copyright (C) 2026 openFPGALoader contributors
 *
 * Based on the reverse engineered protocol and research from adepttool by:
 *   Marcin Kościelnicki <koriakin@0x04.net> (https://github.com/mwkmwkmwk/adepttool)
 *
 * Driver architecture and C++ implementation adapted for openFPGALoader
 * through AI-assisted coding.
 */

#ifndef SRC_DIGILENTADEPT_HPP_
#define SRC_DIGILENTADEPT_HPP_

#include <libusb.h>
#include <cstdint>
#include <string>
#include <vector>

#include "cable.hpp"
#include "jtagInterface.hpp"

class DigilentAdept : public JtagInterface {
 public:
	DigilentAdept(uint32_t clkHZ, int8_t verbose, uint16_t vid = 0x1443, uint16_t pid = 0x0007,
	              const std::string &serial = "");
	virtual ~DigilentAdept();

	int setClkFreq(uint32_t clkHZ) override;
	int writeTMS(const uint8_t *tms, uint32_t len, bool flush_buffer, const uint8_t tdi = 1) override;
	int writeTDI(const uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;
	int toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len) override;
	int get_buffer_size() override { return 1024; }
	bool isFull() override { return false; }
	int flush() override { return 0; }

	/* Device Management support */
	bool configReset(bool assert_reset);
	int queryDone();

 private:
	bool _verbose;
	libusb_context *_ctx;
	libusb_device_handle *_dev_handle;
	uint32_t _speed;

	int cmd(uint8_t app, uint8_t cmd_id, uint8_t port,
	        const uint8_t *payload, uint16_t payload_len,
	        uint8_t *reply_payload, uint16_t reply_len,
	        uint32_t *stats_sent = nullptr, uint32_t *stats_recvd = nullptr);

	int cmd_long(uint8_t app, uint8_t cmd_id, uint8_t port,
	             const uint8_t *payload, uint16_t payload_len,
	             const uint8_t *tx_data, uint32_t tx_len,
	             uint8_t *rx_data, uint32_t rx_len,
	             uint32_t *stats_sent = nullptr, uint32_t *stats_recvd = nullptr);
};

#endif  // SRC_DIGILENTADEPT_HPP_
