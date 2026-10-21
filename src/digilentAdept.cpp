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

#include "digilentAdept.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "display.hpp"

/* USB Endpoints */
#define ADEPT_CMD_WRITE_EP   0x01
#define ADEPT_CMD_READ_EP    0x82
#define ADEPT_DATA_WRITE_EP  0x03
#define ADEPT_DATA_READ_EP   0x84

/* Control Requests (0xc0) */
#define CTRL_GET_PRODUCT_NAME  0xe1
#define CTRL_GET_USER_NAME     0xe2
#define CTRL_GET_SERIAL_NUMBER 0xe4
#define CTRL_SET_SERIAL_NUMBER 0xe5
#define CTRL_GET_FW_VERSION    0xe6
#define CTRL_GET_CAPS          0xe7
#define CTRL_GET_PRODUCT_ID    0xe9

/* Application IDs */
#define APP_SYS   0
#define APP_DMGT  1
#define APP_DJTG  2
#define APP_DEPP  4

/* Generic App Commands */
#define CMD_APP_ENABLE     0
#define CMD_APP_DISABLE    1
#define CMD_APP_GET_PORTS  2

/* SYS Commands */
#define CMD_SYS_RESET      3

/* DMGT Commands */
#define CMD_DMGT_GET_CAPS      2
#define CMD_DMGT_CONFIG_RESET  6
#define CMD_DMGT_QUERY_DONE    8

/* DJTG Commands */
#define CMD_DJTG_SET_SPEED            3
#define CMD_DJTG_GET_SPEED            4
#define CMD_DJTG_SET_TMS_TDI_TCK      5
#define CMD_DJTG_GET_TMS_TDI_TDO_TCK  6
#define CMD_DJTG_CLOCK_TCK            7
#define CMD_DJTG_PUT_TDI_BITS         8
#define CMD_DJTG_GET_TDO_BITS         9
#define CMD_DJTG_PUT_TMS_TDI_BITS     10
#define CMD_DJTG_PUT_TMS_BITS         11

/* DEPP Commands */
#define CMD_DEPP_SET_TIMEOUT  3
#define CMD_DEPP_PUT_REG      4
#define CMD_DEPP_GET_REG      5
#define CMD_DEPP_PUT_REG_SET  6
#define CMD_DEPP_GET_REG_SET  7

DigilentAdept::DigilentAdept(uint32_t clkHz, int8_t verbose,
	uint16_t vid, uint16_t pid, const std::string &serial):
	_verbose(verbose > 0), _ctx(nullptr), _dev_handle(nullptr),
	_speed(4000000), _cable_enabled(false), _if_claimed(false)
{
	int ret = libusb_init(&_ctx);
	if (ret < 0) {
		throw std::runtime_error("DigilentAdept: libusb_init failed: " + std::to_string(ret));
	}

	libusb_device **devs = nullptr;
	ssize_t cnt = libusb_get_device_list(_ctx, &devs);
	if (cnt < 0) {
		close();
		throw std::runtime_error("DigilentAdept: failed to get device list");
	}

	libusb_device *target_dev = nullptr;
	for (ssize_t i = 0; i < cnt; i++) {
		struct libusb_device_descriptor desc;
		if (libusb_get_device_descriptor(devs[i], &desc) < 0)
			continue;

		if (desc.idVendor == vid && desc.idProduct == pid) {
			if (!serial.empty()) {
				libusb_device_handle *handle = nullptr;
				if (libusb_open(devs[i], &handle) == 0) {
					char sn_buf[64] = {0};
					// Try control transfer first (Adept protocol)
					int r = libusb_control_transfer(handle, 0xc0, CTRL_GET_SERIAL_NUMBER, 0, 0,
					                               reinterpret_cast<uint8_t*>(sn_buf), 12, 1000);
					if (r <= 0 && desc.iSerialNumber) {
						r = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber,
						                                       reinterpret_cast<uint8_t*>(sn_buf), sizeof(sn_buf));
					}
					libusb_close(handle);
					if (r > 0 && serial == sn_buf) {
						target_dev = devs[i];
						break;
					}
				}
			} else {
				target_dev = devs[i];
				break;
			}
		}
	}

	if (target_dev) {
		ret = libusb_open(target_dev, &_dev_handle);
	} else {
		ret = -1;
	}
	libusb_free_device_list(devs, 1);

	if (ret < 0 || !_dev_handle) {
		close();
		throw std::runtime_error("DigilentAdept: failed to open Digilent Adept USB device (1443:0007)");
	}

	/* Detach kernel driver if active */
	if (libusb_kernel_driver_active(_dev_handle, 0) == 1) {
		libusb_detach_kernel_driver(_dev_handle, 0);
	}

	ret = libusb_claim_interface(_dev_handle, 0);
	if (ret < 0) {
		close();
		throw std::runtime_error("DigilentAdept: failed to claim interface 0: " + std::to_string(ret));
	}
	_if_claimed = true;

	/* Read device info if verbose */
	if (_verbose) {
		uint8_t prod_name[32] = {0};
		libusb_control_transfer(_dev_handle, 0xc0, CTRL_GET_PRODUCT_NAME, 0, 0, prod_name, 28, 1000);
		uint8_t user_name[32] = {0};
		libusb_control_transfer(_dev_handle, 0xc0, CTRL_GET_USER_NAME, 0, 0, user_name, 16, 1000);
		uint8_t sn[32] = {0};
		libusb_control_transfer(_dev_handle, 0xc0, CTRL_GET_SERIAL_NUMBER, 0, 0, sn, 12, 1000);
		uint8_t fw[2] = {0};
		libusb_control_transfer(_dev_handle, 0xc0, CTRL_GET_FW_VERSION, 0, 0, fw, 2, 1000);
		printInfo("Adept Device: " + std::string(reinterpret_cast<char*>(prod_name)) +
		          " (" + std::string(reinterpret_cast<char*>(user_name)) +
		          "), S/N: " + std::string(reinterpret_cast<char*>(sn)) +
		          ", FW: " + std::to_string(fw[1]) + "." + std::to_string(fw[0]));
	}

	/* Initialize system */
	uint8_t reset_payload[4] = {0, 0, 0, 0};
	cmd(APP_SYS, CMD_SYS_RESET, 0, reset_payload, sizeof(reset_payload), nullptr, 0);

	/* Enable DJTG port */
	ret = cmd(APP_DJTG, CMD_APP_ENABLE, 0, nullptr, 0, nullptr, 0);
	if (ret < 0) {
		close();
		throw std::runtime_error("DigilentAdept: failed to enable JTAG port");
	}
	_cable_enabled = true;

	if (clkHz == 0)
		clkHz = 4000000;
	setClkFreq(clkHz);
}

DigilentAdept::~DigilentAdept()
{
	close();
}

void DigilentAdept::close()
{
	if (_dev_handle) {
		if (_cable_enabled)
			cmd(APP_DJTG, CMD_APP_DISABLE, 0, nullptr, 0, nullptr, 0);
		if (_if_claimed)
			libusb_release_interface(_dev_handle, 0);
		libusb_close(_dev_handle);
		_cable_enabled = false;
		_if_claimed = false;
		_dev_handle = nullptr;
	}
	/* FIXME: missing reattach */
	if (_ctx) {
		libusb_exit(_ctx);
		_ctx = nullptr;
	}
}

int DigilentAdept::cmd(uint8_t app, uint8_t cmd_id, uint8_t port,
                       const uint8_t *payload, uint16_t payload_len,
                       uint8_t *reply_payload, uint16_t reply_len,
                       uint32_t *stats_sent, uint32_t *stats_recvd)
{
	std::vector<uint8_t> pkt(4 + payload_len);
	pkt[0] = static_cast<uint8_t>(payload_len + 3);
	pkt[1] = app;
	pkt[2] = cmd_id;
	pkt[3] = port;
	if (payload && payload_len > 0)
		memcpy(&pkt[4], payload, payload_len);

	int transferred = 0;
	int ret = libusb_bulk_transfer(_dev_handle, ADEPT_CMD_WRITE_EP,
		pkt.data(), pkt.size(), &transferred, 2000);
	if (ret < 0) {
		if (_verbose)
			printError("Adept cmd bulk write failed: " + std::to_string(ret));
		return ret;
	}

	uint8_t reply_buf[64] = {0};
	ret = libusb_bulk_transfer(_dev_handle, ADEPT_CMD_READ_EP,
	                           reply_buf, sizeof(reply_buf), &transferred, 2000);
	if (ret < 0) {
		if (_verbose)
			printError("Adept cmd bulk read failed: " + std::to_string(ret));
		return ret;
	}

	if (transferred < 2)
		return -1;

	uint8_t status = reply_buf[1] & 0x3f;
	if (status != 0) {
		if (_verbose)
			printError("Adept cmd returned status error: " + std::to_string(status));
		return -status;
	}

	int idx = 2;
	if (reply_buf[1] & 0x80) {
		if (transferred < idx + 4) return -1;
		uint32_t s = reply_buf[idx] | (reply_buf[idx+1] << 8) |
		             (reply_buf[idx+2] << 16) | (reply_buf[idx+3] << 24);
		if (stats_sent) *stats_sent = s;
		idx += 4;
	}
	if (reply_buf[1] & 0x40) {
		if (transferred < idx + 4) return -1;
		uint32_t r = reply_buf[idx] | (reply_buf[idx+1] << 8) |
		             (reply_buf[idx+2] << 16) | (reply_buf[idx+3] << 24);
		if (stats_recvd) *stats_recvd = r;
		idx += 4;
	}

	if (reply_payload && reply_len > 0) {
		int avail = transferred - idx;
		int copy_len = (avail < reply_len) ? avail : reply_len;
		if (copy_len > 0)
			memcpy(reply_payload, &reply_buf[idx], copy_len);
	}

	return 0;
}

struct AsyncContext {
	bool done = false;
	int status = 0;
	int actual_length = 0;
};

static void LIBUSB_CALL async_cb(struct libusb_transfer *xfer) {
	auto *ctx = static_cast<AsyncContext*>(xfer->user_data);
	ctx->done = true;
	ctx->status = xfer->status;
	ctx->actual_length = xfer->actual_length;
}

/* FIXME: oe is similar to test if rx_data is nullptr */
int DigilentAdept::cmd_long(uint8_t cmd_id, bool oe, bool pin,
	uint32_t xfer_bits, const uint8_t *tx_data, uint8_t *rx_data,
	uint32_t *stats_sent, uint32_t *stats_recvd)
{
	const uint32_t xfer_len = (xfer_bits + 7) / 8;
	const uint32_t tx_len = (tx_data) ? xfer_len : 0;
	const uint32_t rx_len = (rx_data) ? xfer_len : 0;
	const uint8_t payload[] {
		static_cast<uint8_t>(oe  ? 1 : 0),
		static_cast<uint8_t>(pin ? 1 : 0),
		static_cast<uint8_t>((xfer_bits >>  0) & 0xff),
		static_cast<uint8_t>((xfer_bits >>  8) & 0xff),
		static_cast<uint8_t>((xfer_bits >> 16) & 0xff),
		static_cast<uint8_t>((xfer_bits >> 24) & 0xff)
	};
	int ret = cmd(APP_DJTG, cmd_id, 0, payload, 6, nullptr, 0);
	if (ret < 0)
		return ret;

	if (tx_len > 0 || rx_len > 0) {
		AsyncContext tx_ctx, rx_ctx;
		struct libusb_transfer *tx_xfer = nullptr;
		struct libusb_transfer *rx_xfer = nullptr;

		if (tx_len > 0) {
			tx_xfer = libusb_alloc_transfer(0);
			if (!tx_xfer)
				return -1;
			libusb_fill_bulk_transfer(tx_xfer, _dev_handle, ADEPT_DATA_WRITE_EP,
				const_cast<uint8_t*>(tx_data), tx_len,
				async_cb, &tx_ctx, 5000);
			ret = libusb_submit_transfer(tx_xfer);
			if (ret < 0) {
				libusb_free_transfer(tx_xfer);
				return ret;
			}
		} else {
			tx_ctx.done = true;
		}

		if (rx_data && rx_len > 0) {
			rx_xfer = libusb_alloc_transfer(0);
			if (!rx_xfer) {
				if (tx_xfer)
					libusb_free_transfer(tx_xfer);
				return -1;
			}
			libusb_fill_bulk_transfer(rx_xfer, _dev_handle, ADEPT_DATA_READ_EP,
				rx_data, rx_len, async_cb, &rx_ctx, 5000);
			ret = libusb_submit_transfer(rx_xfer);
			if (ret < 0) {
				if (tx_xfer)
					libusb_free_transfer(tx_xfer);
				libusb_free_transfer(rx_xfer);
				return ret;
			}
		} else {
			rx_ctx.done = true;
		}

		int r = 0;
		while (r >= 0 && (!tx_ctx.done || !rx_ctx.done)) {
			struct timeval tv = {1, 0};
			r = libusb_handle_events_timeout_completed(_ctx, &tv, nullptr);
		}

		if (tx_xfer) {
			if (tx_ctx.status != LIBUSB_TRANSFER_COMPLETED && _verbose) {
				printError("Adept cmd_long: tx transfer failed status: " +
					std::to_string(tx_ctx.status));
			}
			libusb_free_transfer(tx_xfer);
		}

		if (rx_xfer) {
			if (rx_ctx.status != LIBUSB_TRANSFER_COMPLETED && _verbose) {
				printError("Adept cmd_long: rx transfer failed status: " +
					std::to_string(rx_ctx.status));
			}
			libusb_free_transfer(rx_xfer);
		}
	}

	uint32_t sent = 0, recvd = 0;
	ret = cmd(APP_DJTG, cmd_id | 0x80, 0, nullptr, 0, nullptr, 0,
		&sent, &recvd);
	if (stats_sent)
		*stats_sent = sent;
	if (stats_recvd)
		*stats_recvd = recvd;
	return ret;
}

int DigilentAdept::setClkFreq(uint32_t clkHz)
{
	if (clkHz > 15000000) clkHz = 15000000;
	if (clkHz < 100000) clkHz = 100000;

	const uint8_t req[] = {
		static_cast<uint8_t>((clkHz >>  0) & 0xff),
		static_cast<uint8_t>((clkHz >>  8) & 0xff),
		static_cast<uint8_t>((clkHz >> 16) & 0xff),
		static_cast<uint8_t>((clkHz >> 24) & 0xff)
	};

	uint8_t rep[4] = {0};
	int ret = cmd(APP_DJTG, CMD_DJTG_SET_SPEED, 0, req, 4, rep, 4);
	if (ret == 0) {
		_speed = (static_cast<uint32_t>(rep[0]) << 0) |
			(static_cast<uint32_t>(rep[1]) <<  8) |
			(static_cast<uint32_t>(rep[2]) << 16) |
			(static_cast<uint32_t>(rep[3]) << 24);
		_clkHZ = _speed;
		if (_verbose)
			printInfo("Adept JTAG speed set to " + std::to_string(_speed) + " Hz");
		return _speed;
	}
	return -1;
}

int DigilentAdept::toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len)
{
	if (clk_len == 0) return 0;

	const int ret = cmd_long(CMD_DJTG_CLOCK_TCK, tms, tdi, clk_len,
		nullptr, nullptr, nullptr, nullptr);
	return (ret < 0) ? -1 : 0;
}

int DigilentAdept::writeTMS(const uint8_t *tms, uint32_t len,
	bool flush_buffer, const uint8_t tdi)
{
	(void)flush_buffer;
	if (len == 0)
		return 0;

	uint32_t xfer_len = len; // number of bits to emit
	uint32_t bit_offset = 0;
	/* size limited to 2048: only one buffer and re-use it
	 * during all xfer sequences
	 */
	std::vector<uint8_t> tx_buf(2048, 0);

	while (xfer_len > 0) {
		const uint32_t pkt_len = std::min<uint32_t>(xfer_len, 2048);

		for (uint32_t b = 0; b < pkt_len; b++) {
			const uint32_t src_bit = bit_offset + b;
			const uint16_t tx_idx = b >> 3;
			const uint16_t tx_bit = (1 << (b & 7));
			if (tms[src_bit >> 3] & (1 << (src_bit & 7)))
				tx_buf[tx_idx] |= tx_bit;
			else
				tx_buf[tx_idx] &= ~tx_bit;
		}

		uint32_t sent = 0;
		const int ret = cmd_long(CMD_DJTG_PUT_TMS_BITS, false, tdi, pkt_len,
			tx_buf.data(), nullptr, &sent, nullptr);
		if (ret < 0)
			return -1;
		xfer_len -= pkt_len;
		bit_offset += pkt_len;
	}

	return len;
}

int DigilentAdept::writeTDI(const uint8_t *tx, uint8_t *rx, uint32_t len, bool end)
{
	/* nothing to do ? */
	if (len == 0)
		return 0;
	bool oe = (rx != nullptr);
	// number of Bytes to send with a round after removing 1 when
	// end is true (ie last bit is sent during state transition).
	uint32_t bytes = (len - (end ? 1 : 0)) / 8; // floor(round(xx))
	// number of residual bits to sent
	const uint32_t bits = len - bytes * 8;
	uint8_t *rx_ptr = rx;
	const uint8_t *tx_ptr = tx;
	uint32_t sent = 0, recvd = 0;
	std::vector<uint8_t> tx_buff(2048, 0xff); // temporary buffer with
											  // the largest size possible

	/* sanity check */
	if (bits >= 8) {
		printError("bits mismatch expected size < 8");
		return -1;
	}

	/* 1. Send as much as possible bytes in large sequence */
	while (bytes > 0) {
		uint32_t xfer_len = std::min<uint32_t>(2048, bytes);

		/* clear the buffer or fill with tx data */
		if (tx)
			memcpy(tx_buff.data(), tx_ptr, xfer_len);
		else
			memset(tx_buff.data(), 0xff, xfer_len);

		const int ret = cmd_long(CMD_DJTG_PUT_TDI_BITS, oe, 0, xfer_len * 8,
			tx_buff.data(), (oe ? rx_ptr : nullptr), &sent, &recvd);
		if (ret < 0)
			return -1;

		/* update remaining bytes to sent and tx/rx buffer offset */
		bytes -= xfer_len;
		if (tx)
			tx_ptr += xfer_len;
		if (rx)
			rx_ptr += xfer_len;
	}

	/* 2. Send remaining bits. When not end this corresponds to all
	 * remaining bits. When end the last bit will be sent after.
	 * In both cases this step will sent up to 7 bits.
	 */
	if (bits > 0) {
		/* no needs to worry about offset, previous command have sent
		 * bytes so next step start by sending LSB from the last tx
		 * Byte
		 */
		tx_buff[0] = (tx ? tx_ptr[0] : 0xff);
		const int ret = cmd_long(CMD_DJTG_PUT_TDI_BITS, oe, 0, bits,
			tx_buff.data(), (oe ? rx_ptr : nullptr), &sent, &recvd);
		if (ret < 0)
			return -1;
		/* Fix rx align */
		if (rx)
			*rx_ptr >>= (8 - bits);
	}

	/* 3. Send last bit with tms high only when end is true */
	if (end) {
		uint8_t last_rx = 0;
		const uint8_t last_tx = (tx ? ((*tx_ptr >> bits) & 0x01) : 1);
		const int ret = cmd_long(CMD_DJTG_PUT_TDI_BITS, oe, 0, bits,
			&last_tx, (oe ? &last_rx: nullptr), &sent, &recvd);
		if (ret < 0)
			return -1;
		if (rx)
			*rx_ptr |= (0x80 & last_rx) >> (7 - bits);
	}

	return len;
}

bool DigilentAdept::configReset(bool assert_reset)
{
	const uint8_t p = (assert_reset ? 1 : 0);
	return (cmd(APP_DMGT, CMD_DMGT_CONFIG_RESET, 0, &p, 1, nullptr, 0) == 0);
}

int DigilentAdept::queryDone()
{
	uint8_t done_val = 0;
	if (cmd(APP_DMGT, CMD_DMGT_QUERY_DONE, 0, nullptr, 0, &done_val, 1) == 0)
		return done_val;
	return -1;
}
