// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022-2026 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <cstdio>
#include <cstring>
#include <unistd.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "display.hpp"
#include "fx2_ll.hpp"
#include "xilinxPlatformCableUSB.hpp"

#define XPCU_BREQUEST             0xB0
#define XPCU_INITIALIZED_VID      0x03fd
#define XPCU_INITIALIZED_PID      0x0008
#define XPCU_CMD_DISABLE          0x10
#define XPCU_CMD_ENABLE           0x18
#define XPCU_CMD_SET_SPEED        0x28
#define XPCU_CMD_STATUS           0x38
#define XPCU_CMD_RETURN_CONSTANT  0x40
#define XPCU_CMD_GET_VERSION      0x50
#define XPCU_CMD_GPIO_TRANSFER    0xA6
#define XPCU_EP_JTAG_OUT          0x02
#define XPCU_EP_JTAG_IN           0x86
#define XPCU_STATUS_CONNECTED     0x40
#define XPCU_SPEED_CLASS_ENABLE   0x10
#define XPCU_VERSION_CPLD         0x01
#define XPCU_VERSION_CONST1       0x02
#define XPCU_VERSION_CONST2       0x03

#define TCK_OFFSET    0
#define TDO_OFFSET    4
#define TDI_OFFSET    0
#define TMS_OFFSET    4

#define TCK_IDX       (1 << TCK_OFFSET)
#define TDO_IDX       (1 << TDO_OFFSET)
#define TDI_IDX       (1 << TDI_OFFSET)
#define TMS_IDX       (1 << TMS_OFFSET)

XilinxPlatformCableUSB::XilinxPlatformCableUSB(const uint16_t vid,
	const uint16_t pid,
	uint32_t clkHz,
	const std::string &firmware_path,
	int8_t verbose): _verbose(verbose), _nb_bit(0), _nb_tdo_bit(0),
		_curr_tms(0), _curr_tdi(0), _buffer_size(4096),
		_buffer_bit_size((_buffer_size / 2 * 4) - 1)
{
	std::string firmware_file;
	/* firmare path must be known:
	 * 1/ provided by user
	 * 2/ from Vivado install directory
	 * 3/ from ISE install directory
	 */
	if (firmware_path.empty() && strlen(ISE_DIR) == 0 && strlen(VIVADO_DIR) == 0) {
		printError("missing FX2 firmware");
		printError("use --probe-firmware with something");
		printError("like /opt/Xilinx/14.7/ISE_DS/ISE/bin/lin64/xusb_xp2.hex for ISE");
		printError("or   /opt/Xilinx/Vivado/VERSION/data/xicom/xusb_xp2.hex for Vivado");
		printError("Or use -DISE_DIR=/opt/Xilinx/14.7 / -DVIVADO_DIR=/opt/Xilinx/Vivado/VERSION at build time");
		throw std::runtime_error("xilinxPlatformCableUSB: missing firmware");
	}

	/* Extract firmware according to possibilities */
	if (!firmware_path.empty())
		firmware_file = firmware_path;
	else if (strlen(VIVADO_DIR) > 0)
		firmware_file = VIVADO_DIR "/data/xicom/";
	else if (strlen(ISE_DIR) > 0)
		firmware_file = ISE_DIR "/ISE_DS/ISE/bin/lin64/";

	if (firmware_path.empty()) {
		if (pid == 0x0d)
			firmware_file += "xusb_emb.hex";
		else
			firmware_file += "xusb_xp2.hex";
	}
	printInfo("firmware_file : " + firmware_file);

	try {
		fx2 = std::make_unique<FX2_ll>(vid, pid, XPCU_INITIALIZED_VID,
				XPCU_INITIALIZED_PID, firmware_file);
	} catch (std::exception &e) {
		printError(e.what());
		throw std::runtime_error("lowlevel init failed");
	}

	fx2->set_interface_alt_setting(0, 1);

	displayCableVersion();

	/* Write GPIO bit */
	fx2->write_ctrl(XPCU_BREQUEST, 0x030, nullptr, 0, (1 << 3));

	if (!enableDevice(true))
		throw std::runtime_error("Unable to enable device");

	uint8_t buf[1];
	if (!fx2->read_ctrl(XPCU_BREQUEST, XPCU_CMD_STATUS, buf, 1))
		throw std::runtime_error("Unable to read status.");

	char mess[64];
	snprintf(mess, sizeof(mess), "status %02x connected: %s",
			buf[0], (buf[0] & XPCU_STATUS_CONNECTED) ? "yes" : "no");
	printInfo(mess);

	_in_buf = std::make_unique<uint8_t[]>(_buffer_size);

	setClkFreq(clkHz);
}

XilinxPlatformCableUSB::~XilinxPlatformCableUSB()
{
	flush();
	enableDevice(false);
}

int XilinxPlatformCableUSB::setClkFreq(uint32_t clkHz)
{
	/* speed table: index Hz; bit 4 must always be set in the speed class */
	static constexpr uint32_t speeds[] = {12000000, 6000000, 3000000, 1500000, 750000};
	uint8_t speed = 4;  /* default: slowest */
	for (uint8_t i = 0; i < sizeof(speeds) / sizeof(speeds[0]); i++) {
		if (speeds[i] <= clkHz) {
			speed = i;
			break;
		}
	}
	if (!fx2->write_ctrl(XPCU_BREQUEST, XPCU_CMD_SET_SPEED, nullptr, 0,
				speed | XPCU_SPEED_CLASS_ENABLE)) {
		printError("setClkFreq: failed to set speed");
		return -1;
	}
	_clkHZ = speeds[speed];
	printInfo("Jtag frequency : requested " + std::to_string(clkHz) +
			" Hz -> real " + std::to_string(_clkHZ) + " Hz");
	return _clkHZ;
}

int XilinxPlatformCableUSB::writeTMS(const uint8_t *tms, uint32_t len,
		bool flush_buffer, const uint8_t tdi)
{
	int ret;

	if (len == 0)
		return flush_buffer ? flush() : 0;

	_curr_tdi = tdi ? 1 : 0;

	for (uint32_t i = 0; i < len; i++) {
		_curr_tms = (tms[i >> 3] >> (i & 0x07)) & 0x01;
		if (storeBit(_curr_tdi, _curr_tms, 1, 0)) {
			if (write(nullptr, 0) < 0)
				return -1;
		}
	}

	if (flush_buffer) {
		ret = flush();
		if (ret < 0)
			return ret;
	}

	return len;
}

int XilinxPlatformCableUSB::writeTDI(const uint8_t *tx, uint8_t *rx, uint32_t len, bool end)
{
	if (len == 0)
		return 0;

	if (rx && _nb_bit != 0) {
		if (write(nullptr, 0) < 0)
			return -1;
	}

	uint32_t rx_offset = 0;

	for (uint32_t i = 0; i < len; i++) {
		bool last_bit = (i == len - 1 && end);
		_curr_tdi = tx ? (0x01 & ((tx[i >> 3]) >> (i & 0x07))) : 0;

		if (last_bit)
			_curr_tms = 1;

		if (storeBit(_curr_tdi, _curr_tms, 1, rx ? 1 : 0)) {
			uint32_t bits = _nb_tdo_bit;
			if (write(rx, rx_offset) < 0)
				return -1;
			rx_offset += bits;
		}
	}

	if (_nb_bit != 0 && (end || rx)) {
		if (write(rx, rx_offset) < 0)
			return -1;
	}

	return len;
}

int XilinxPlatformCableUSB::toggleClk([[maybe_unused]] uint8_t tms,
	[[maybe_unused]] uint8_t tdi, uint32_t clk_len)
{
	for (uint32_t i = 0; i < clk_len; i++) {
		if (storeBit(_curr_tdi, _curr_tms, 1, 0)) {
			if (write(nullptr, 0) < 0)
				return -1;
		}
	}

	/* Flush buffer if not empty */
	if (_nb_bit != 0) {
		if (flush() < 0)
			return -1;
	}

	return clk_len;
}

int XilinxPlatformCableUSB::flush()
{
	return write(nullptr, 0);
}

/* TMS 1st nibble, TDI 2nd nibble, TDO 3rd nibble, TCK 4th nibble
 *   byte n    byte n+1
 *  [7:4 3:0] [7:4 3:0]
 *   TMS TDI   TDO TCK
 */
bool XilinxPlatformCableUSB::storeBit(uint8_t tdi, uint8_t tms,
		uint8_t tck, uint8_t tdo) noexcept
{
	const uint32_t buf_pos = (_nb_bit >> 2) << 1;
	const uint8_t bit_pos = _nb_bit & 0x03;

	if (bit_pos == 0)
		_in_buf[buf_pos] = _in_buf[buf_pos + 1] = 0;

	if (tms)
		_in_buf[buf_pos] |= (TMS_IDX << bit_pos);
	if (tdi)
		_in_buf[buf_pos] |= (TDI_IDX << bit_pos);
	if (tdo)
		_in_buf[buf_pos + 1] |= (TDO_IDX << bit_pos);
	if (tck)
		_in_buf[buf_pos + 1] |= (TCK_IDX << bit_pos);

	_nb_bit++;
	if (tdo)
		_nb_tdo_bit++;

	return _nb_bit >= _buffer_bit_size;
}

/* Compute how many bytes EP6 will return for nb_bit TDO bits.
 * The device uses a shift-register encoding: a 16-bit register that grows
 * to 32-bit after 16 bits, then a new register starts every 32 bits.
 */
uint32_t XilinxPlatformCableUSB::rxBufSize(uint32_t nb_bit) noexcept
{
	const uint32_t full_groups = nb_bit / 32;
	const uint32_t rem = nb_bit & 31u;
	return full_groups * 4 + (rem == 0 ? 0 : (rem > 16 ? 4 : 2));
}

int XilinxPlatformCableUSB::write(uint8_t *rx, uint32_t rx_offset)
{
	if (_nb_bit == 0)
		return 0;

	/* N ops: N/4 pairs of 2 bytes each (round up to complete pair) */
	uint32_t xfer_tx = (_nb_bit >> 1) & ~0x01u;
	xfer_tx += ((_nb_bit & 0x03) != 0) ? 2 : 0;

	/* count is 0-indexed per protocol spec */
	if (!fx2->write_ctrl(XPCU_BREQUEST, XPCU_CMD_GPIO_TRANSFER, nullptr, 0,
				_nb_bit - 1)) {
		printError("Fails to write GPIO transfer control message");
		return -1;
	}

	if (fx2->write(XPCU_EP_JTAG_OUT, _in_buf.get(), xfer_tx) != (int)xfer_tx)
		return -1;

	if (rx) {
		if (_nb_tdo_bit != _nb_bit) {
			printError("Unable to decode mixed TDO/non-TDO transfer");
			return -1;
		}

		uint32_t xfer_rx = rxBufSize(_nb_tdo_bit);
		std::vector<uint8_t> rx_buf(xfer_rx);
		if (fx2->read(XPCU_EP_JTAG_IN, rx_buf.data(), xfer_rx) != (int)xfer_rx)
			return -1;

		/* Decode shift-register encoded TDO bits into rx.
		 * Each group of up to 32 bits occupies a 16 or 32-bit little-endian
		 * shift register: bit_k is at position (reg_size - group + k).
		 */
		uint32_t buf_off = 0;
		uint32_t remaining = _nb_tdo_bit;
		uint32_t bit_idx = 0;
		while (remaining > 0) {
			const uint32_t group = (remaining > 32) ? 32 : remaining;
			const uint32_t reg_size = (group > 16) ? 32 : 16;
			const uint32_t shift = reg_size - group;
			uint32_t reg = 0;
			for (uint32_t b = 0; b < reg_size / 8; b++)
				reg |= (uint32_t)rx_buf[buf_off + b] << (b * 8);
			const uint32_t base = rx_offset + bit_idx;
			if ((base & 7u) == 0 && (group & 7u) == 0) {
				uint8_t *out = rx + (base >> 3);
				for (uint32_t b = 0; b < group / 8; b++)
					out[b] = static_cast<uint8_t>((reg >> (shift + b * 8)) & 0xFF);
			} else {
				for (uint32_t k = 0; k < group; k++) {
					const uint32_t out_bit = base + k;
					if ((reg >> (shift + k)) & 1)
						rx[out_bit >> 3] |= (1 << (out_bit & 7));
					else
						rx[out_bit >> 3] &= ~(1 << (out_bit & 7));
				}
			}
			buf_off += reg_size / 8;
			bit_idx += group;
			remaining -= group;
		}
	}

	_nb_bit = 0;
	_nb_tdo_bit = 0;
	return 0;
}

bool XilinxPlatformCableUSB::enableDevice(bool enable)
{
	if (!fx2->write_ctrl(XPCU_BREQUEST,
				(enable ? XPCU_CMD_ENABLE : XPCU_CMD_DISABLE), nullptr, 0)) {
		char mess[64];
		snprintf(mess, sizeof(mess), "Unable to %s device",
				(enable ? "enable" : "disable"));
		printError(mess);
		return false;
	}
	return true;
}

void XilinxPlatformCableUSB::displayCableVersion()
{
	uint8_t buf[2];

	if (!fx2->read_ctrl(XPCU_BREQUEST, XPCU_CMD_RETURN_CONSTANT, buf, 2))
		throw std::runtime_error("Unable to read constant.");
	const uint16_t const0 = ((uint16_t)buf[0] << 8) | buf[1];

	if (!fx2->read_ctrl(XPCU_BREQUEST, XPCU_CMD_GET_VERSION, buf, 2))
		throw std::runtime_error("Unable to read firmware version.");
	const uint16_t fx2_firmware = ((uint16_t)buf[0] << 8) | buf[1];

	if (!fx2->read_ctrl(XPCU_BREQUEST, XPCU_CMD_GET_VERSION, buf, 2,
				XPCU_VERSION_CPLD))
		throw std::runtime_error("Unable to read CPLD version.");
	const uint16_t cpld_firmware = ((uint16_t)buf[0] << 8) | buf[1];

	if (!fx2->read_ctrl(XPCU_BREQUEST, XPCU_CMD_GET_VERSION, buf, 2,
				XPCU_VERSION_CONST1))
		throw std::runtime_error("Unable to read const 1.");
	const uint16_t const1 = ((uint16_t)buf[0] << 8) | buf[1];

	if (!fx2->read_ctrl(XPCU_BREQUEST, XPCU_CMD_GET_VERSION, buf, 2,
				XPCU_VERSION_CONST2))
		throw std::runtime_error("Unable to read const 2.");
	const uint16_t const2 = ((uint16_t)buf[0] << 8) | buf[1];

	printf("FX2 version:    %04x\n", fx2_firmware);
	printf("CPLD version:   %04x\n", cpld_firmware);
	printf("Const0 version: %04x\n", const0);
	printf("Const1 version: %04x\n", const1);
	printf("Const2 version: %04x\n", const2);
}
