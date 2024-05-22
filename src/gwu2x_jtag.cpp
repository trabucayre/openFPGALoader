// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2024 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <libusb.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "cable.hpp"
#include "display.hpp"
#include "gwu2x_jtag.hpp"
#include "libusb_ll.hpp"

/*
 * TCK -> GPIOL0
 * TMS -> GPIOL1
 * TDI -> GPIOL2
 * TDO -> GPIOL3
 */
enum {
	GWU2X_TMS_LSB_WRO            = 0x5B,
	GWU2X_TMS_LSB_RDWR           = 0x5C,
	GWU2X_TCK                    = 0x9b,
	GWU2X_TDI_LSB_BIT_WRO        = 0x6B,
	GWU2X_TDI_LSB_BIT_RDWR       = 0x6C,
	GWU2X_TDI_LSB_BYTE_WRO       = 0x7B,
	GWU2X_TDI_LSB_BYTE_RDWR      = 0x7C,
	GWU2X_SET_FREQ_FAST          = 0xAB,
	GWU2X_SET_FREQ_SLOW          = 0xAC,
	GWU2X_READBACK_BUFFER_FORCED = 0x8B,
	GWU2X_READBACK_BUFFER        = 0xDB, /* 0x11: LSB, 0xff: MSB */
	GWU2X_GPIO_CONF_LOW          = 0x20, /* GPIO0-7 */
	GWU2X_GPIO_CONF_HIGH         = 0x21, /* GPIO8-15 */
	GWU2X_GPIO_READ_LOW          = 0x22, /* GPIO0-7 */
	GWU2X_GPIO_READ_HIGH         = 0x23, /* GPIO8-15 */
	GWU2X_CPOL_SETTING           = 0xCB,
};

enum {
	READBACK_LSB = 0x11,
	READBACK_MSB = 0xff,
};

GowinGWU2x::GowinGWU2x(cable_t *cable, uint32_t clkHz, int8_t verbose):
	libusb_ll(0, 0, verbose), _verbose(verbose > 1), _cable(cable),
	_usb_dev(nullptr), _dev(nullptr), _xfer_buf(nullptr), _xfer_pos(0),
	_buffer_len(256 + 2 + 1)
{
	const int found = get_devices_list(_cable);
	if (found == 0)
		throw std::runtime_error("No cable found");
	if (found > 1)
		throw std::runtime_error("More than one cable found");
	std::vector<struct libusb_device *> dev_list = usb_dev_list();

	/* here we have only one device present */
	_usb_dev = dev_list[0];

	int ret = libusb_open(_usb_dev, &_dev);
	if (ret < 0)
		throw std::runtime_error("Failed to open device");

	ret = libusb_claim_interface(_dev, 0);
	if (ret < 0) {
		char mess[256];
		snprintf(mess, 256, "Error claiming interface with error %s", libusb_error_name(ret));
		throw std::runtime_error(mess);
	}

	_xfer_buf = new uint8_t[_buffer_len];  // one full TDI packet + readback cmd

	/* cable configuration */
	if (!store_seq(GWU2X_GPIO_CONF_LOW,  // gpio0-7
			cable->config.bit_low_dir,   // direction
			cable->config.bit_low_val))  // value
		throw std::runtime_error("Error: low pins configuration failed");
	if (!store_seq(GWU2X_GPIO_CONF_HIGH,  // gpio8-15
			cable->config.bit_high_dir,   // direction
			cable->config.bit_high_val))  // value
		throw std::runtime_error("Error: high pins configuration failed");
	if (!xfer(nullptr, 0))
		throw std::runtime_error("Error: pin configuration failed");

	if (setClkFreq(clkHz) < 0)
		throw std::runtime_error("Error: clock frequency configuration failed");
}

GowinGWU2x::~GowinGWU2x()
{
	flush();
	delete _xfer_buf;
	/* nothing about interface ? */
	libusb_close(_dev);
}

int GowinGWU2x::writeTMS(const uint8_t *tms, uint32_t len, bool flush_buffer,
		const uint8_t tdi)
{
	const uint8_t tdi_bit = (tdi) ? 0x80 : 0x00;
	uint8_t tms_buf = tdi_bit;
	uint8_t idx = 0;  // bit index in tms_buf

	/* As FTDI devices TMS instruction is 3 bytes long and can send up to
	 * 7bits
	 * 2nd byte tells number of TMS bits to send (0: 1bit, 6: 7bits)
	 * 3rd byte is the sequence of TMS values LSB first. Offset 7 is the TDI
	 * value for all TMS bits
	 */
	for (uint32_t pos = 0; pos < len; pos++) {
		const uint8_t tms_byte = tms[pos >> 3];
		const uint8_t bit_shift = pos & 0x07;
		const uint8_t tms_bit = (tms_byte >> bit_shift) & 0x01;
		if (tms_bit)
			tms_buf |= (1 << idx);
		idx += 1;
		/* if we have 7bits or if it's the last iteration
		 * flush the buffer, and restart for the next sequence
		 */
		if (idx == 7 || pos == len - 1) {
			if (!store_seq(GWU2X_TMS_LSB_WRO, idx - 1, tms_buf))
				return -1;
			idx = 0;
			tms_buf = tdi_bit;
		}
	}

	if (flush_buffer) {
		if (!xfer(nullptr, 0)) {
			return -1;
		}
	}

	return static_cast<int>(len);
}

/*
 * Write / Read data in one to three steps
 * a sequence of up to 256 Bytes per packet
 * a sequence of up to 8 bits
 * the final bit (MSB) using TMS transition when end is true
 */
int GowinGWU2x::writeTDI(const uint8_t *tx, uint8_t *rx, uint32_t len, bool end)
{
	const uint32_t real_len = len - (end ? 1 : 0);  // if end: last bit is sent with TMS
	const uint32_t byte_len = real_len >> 3;   // convert bit len to byte len (floor)
	const uint32_t bit_len = real_len & 0x07;  // extract remaining bits
	uint8_t *tx_ptr = (uint8_t *)tx;
	uint8_t *rx_ptr = rx;

	/* if the buffer is not empty, some tms bits are present
	 * flush to keep maximum size available
	 */
	if (_xfer_pos != 0) {
		if (!xfer(nullptr, 0))
			return -1;
	}

	// 1. Byte sequence with up to 256 Bytes
	if (byte_len > 0) {
		uint32_t xfer_len = 256;
		for (uint32_t pos = 0; pos < byte_len; pos += xfer_len) {
			if (pos + 256 > byte_len)
				xfer_len = byte_len - pos;
			_xfer_buf[_xfer_pos++] = (rx) ? GWU2X_TDI_LSB_BYTE_RDWR : GWU2X_TDI_LSB_BYTE_WRO;
			_xfer_buf[_xfer_pos++] = xfer_len - 1;
			memcpy(&_xfer_buf[_xfer_pos], tx_ptr, xfer_len);
			_xfer_pos += xfer_len;
			if (rx)
				_xfer_buf[_xfer_pos++] = GWU2X_READBACK_BUFFER_FORCED;
			if (!xfer((rx) ? rx_ptr : nullptr, xfer_len))
				return -1;
			tx_ptr += xfer_len;
			if (rx_ptr)
				rx_ptr += xfer_len;
		}
	}

	/* when end with a sequence of bits
	 * don't do two usb transfers and
	 * postpone it to end sequence
	 */
	const bool postponed_read = (bit_len != 0 && end);

	// 2. Remaining bits between 1 and 7.
	if (bit_len != 0) {
		/* write up to 8 bits
		 * may be less if end, but not more
		 * the buffer read must be correctly aligned according to bit_len
		 */
		if (!store_seq(GWU2X_TDI_LSB_BIT_WRO + (rx != nullptr),
			static_cast<uint8_t>(bit_len - 1), *tx_ptr, !postponed_read && rx != nullptr))
			return -1;
		// When not end, flush buffer
		// NOTE: with a more clever global system why not
		// postning it for the next call ?
		if (!postponed_read) {
			/* unlike FTDI bits are filed LSB to MSB ie
			 * for 2 bits: 0b000000XX
			 * for 7 bits: 0b0XXXXXXX
			 * no needs to shift here
			 */
			if (!xfer((rx) ? rx_ptr : nullptr, 1))
				return -1;
		}
	}

	// 3. End using TMS instruction and last bit.
	if (end) {
		uint8_t rx_byte;
		/* we are in SHIFTDR or SHIFTIR -> move to next state */
		const uint8_t last_bit = (*tx_ptr >> bit_len) & 0x01;
		if (!store_seq(GWU2X_TMS_LSB_WRO + (rx != nullptr), 0,
			static_cast<uint8_t>(((last_bit) ? 0x80 : 0x00) | 0x01),
			rx != nullptr))
			return -1;
		if (!xfer((rx)? &rx_byte : nullptr, 1))
			return -1;
		if (rx) {
			if (postponed_read) {
				*rx_ptr = rx_byte;
			} else {
				const uint8_t tdo_bit = 1 << (bit_len);
				if (rx_byte & 0x01)  // here only one bit is read (always LSB)
					*rx_ptr |= tdo_bit;
				else
					*rx_ptr &= ~tdo_bit;
				}
		}
	}
	return static_cast<int>(len);
}

int GowinGWU2x::toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len)
{
	/* Gowin GWU2X has a specific command to produces
	 * a sequence sequence to up to 65536 cycles
	 * 0: 1 clk cycle, 0xffff: 65535 clk cycles
	 */
	/* No need to check/flush buffer will be done by store_seq */
	uint32_t len = 0;
	if (_verbose)
		printf("toggleClk : %02x %02x %u\n", tms, tdi, clk_len);
	
	for (uint32_t length = clk_len; length > 0; length -= len) {
		len = ((length > 65536) ? 65536 : length) - 1;
		if (!store_seq(GWU2X_TCK,
				static_cast<uint8_t>((len >> 0) & 0xff),
				static_cast<uint8_t>((len >> 8) & 0xff)))
			return -1;
		len += 1;
	}
	/* flush before return */
	if (_xfer_pos != 0) {
		if (!xfer(nullptr, 0))
			return -1;
	}
	return static_cast<int>(clk_len);
}

enum {
	EP_IN  = 0x81,
	EP_OUT = 0x02,
};

/* when readback we consider it's for the current sequence, not
 * for current content
 */
bool GowinGWU2x::store_seq(const uint8_t &opcode, const uint8_t &len,
	const uint8_t &data, const bool readback)
{
	if (_verbose) {
		char message[256];
		snprintf(message, 256, "store seq %02x %02x %02x %d",
				opcode, len, data, readback);
		printInfo(message);
	}
	const size_t xfer_len = 3 + readback;
	if (_xfer_pos + xfer_len > _buffer_len) {
		if (!xfer(nullptr, 0))
			return false;
	}
	_xfer_buf[_xfer_pos++] = opcode;
	_xfer_buf[_xfer_pos++] = len;
	_xfer_buf[_xfer_pos++] = data;
	if (readback)
		_xfer_buf[_xfer_pos++] = GWU2X_READBACK_BUFFER_FORCED;
	return true;
}

bool GowinGWU2x::xfer(uint8_t *rx, uint16_t rx_len, const uint16_t timeout)
{
	int actual_length;
	int ret;
	if (_xfer_pos == 0)  // nothing to do
		return true;

	if (_verbose) {
		printInfo("Write " + std::to_string(_xfer_pos) + " Bytes");
		printf("\t");
		for (uint32_t i = 0; i < _xfer_pos; i++) {
			char message[6];
			snprintf(message, 6, "0x%02x ", _xfer_buf[i]);
			printSuccess(message, false);
		}
		printf("\n");
		displayCmd();
	}

	ret = libusb_bulk_transfer(_dev, EP_OUT,
			_xfer_buf, _xfer_pos, &actual_length, timeout);
	if (ret < 0) {
		printError("Write failed with error " + std::to_string(ret));
		return false;
	}
	_xfer_pos = 0;
	if (!rx)  // message sent, nothing to read -> quit
		return true;
	uint8_t *rx_ptr = rx;
	uint32_t dummy;
	ret = libusb_bulk_transfer(_dev, 0x83, (uint8_t*)&dummy, 4, &actual_length, timeout);
	if (_verbose)
		printf("ret: %d %u %d\n", ret, dummy, actual_length);
	do {
		ret = libusb_bulk_transfer(_dev, EP_IN, rx_ptr, rx_len,
			&actual_length, timeout);
		if (ret < 0) {
			char message[256];
			snprintf(message, 256, "Failed to read: %d %s\n", ret,
				libusb_strerror(static_cast<libusb_error>(ret)));
			printError(message);
			return false;
		}
		if (_verbose) {
			printf("%d %d %d\n", ret, rx_len, actual_length);
			for (int ii = 0; ii < rx_len; ii++)
				printf("%02x ", rx_ptr[ii]);
			printf("\n");
		}
		rx_ptr += actual_length;
		rx_len -= actual_length;
	} while (rx_len > 0);

	return true;
}

int GowinGWU2x::setClkFreq(uint32_t freqHz)
{
	if (freqHz < 120e3 || freqHz > 30e6) {
		printError("clk Frequency must be between 120kHz and 30MHz");
		return -1;
	}
	const uint16_t div = static_cast<uint16_t>(60e6 / freqHz);
	const int real_freq = static_cast<int>(60e6 / div);
	printInfo("User requested: " + std::to_string(freqHz) + " real frequency is "
		+ std::to_string(real_freq));
	_xfer_buf[_xfer_pos++] = GWU2X_SET_FREQ_FAST + ((freqHz < 240e3) ? 1 : 0);
	_xfer_buf[_xfer_pos++] = static_cast<uint8_t>(div - ((freqHz < 240e3) ? 256 : 0));
	if (!xfer(nullptr, 0, 1000))
		return -1;
	_clkHZ = real_freq;
	return static_cast<int>(_clkHZ);
}

void GowinGWU2x::displayCmd()
{
	uint32_t len = 0;
	while (len < _xfer_pos) {
		const uint8_t opcode = _xfer_buf[len++];
		const uint8_t b_len = _xfer_buf[len++];
		uint8_t tdi;
		uint8_t tms;
		char message[256];
		uint8_t bytes[256];
		switch(opcode) {
			case GWU2X_TMS_LSB_WRO:
				tdi = (_xfer_buf[len] >> 7) & 0x01;
				tms = (_xfer_buf[len++] & 0x7f);
				snprintf(message, 256,
					"TMS Write Only len %d TDI: %x TMS: %02x",
					b_len + 1, tdi, tms);
				break;
			case GWU2X_TMS_LSB_RDWR:
				tdi = (_xfer_buf[len] >> 7) & 0x01;
				tms = (_xfer_buf[len++] & 0x7f);
				snprintf(message, 256,
					"TMS Read/Write len %d TDI: %x TMS: %02x",
					b_len + 1, tdi, tms);
				break;
			case GWU2X_TDI_LSB_BIT_WRO:
				tdi = _xfer_buf[len++];
				snprintf(message, 256,
					"TDI bit Write Only len %d TDI: %2x",
					b_len + 1, tdi);
				break;
			case GWU2X_TDI_LSB_BIT_RDWR:
				tdi = _xfer_buf[len++];
				snprintf(message, 256,
					"TDI bit Read/Write len %d TDI: %2x",
					b_len + 1, tdi);
				break;
			case GWU2X_TDI_LSB_BYTE_WRO:
				memcpy(bytes, _xfer_buf, b_len + 1);
				len += b_len + 1;
				snprintf(message, 256,
					"TDI Byte Write Only len %d TDI: %2x",
					b_len + 1, bytes[0]);
				break;
			case GWU2X_TDI_LSB_BYTE_RDWR:
				memcpy(bytes, _xfer_buf, b_len + 1);
				len += b_len + 1;
				snprintf(message, 256,
					"TDI Byte Read/Write len %d TDI: %2x",
					b_len + 1, bytes[0]);
				break;
			case GWU2X_TCK:
				tdi = _xfer_buf[len++];
				snprintf(message, 256,
					"toggle Clock len %d",
					(b_len | (tdi << 8)) + 1);
				break;

			case GWU2X_GPIO_CONF_HIGH:
				tms = _xfer_buf[len++];
				snprintf(message, 256,
					"GPIO conf high: direction: %x value: %02x",
					b_len, tms);
				break;
			case GWU2X_GPIO_CONF_LOW:
				tms = _xfer_buf[len++];
				snprintf(message, 256,
					"GPIO conf low: direction: %x value: %02x",
					b_len, tms);
				break;
			case GWU2X_SET_FREQ_FAST:
			case GWU2X_SET_FREQ_SLOW:
				snprintf(message, 256,
					"Set clk freq prescaler: %02x",
					b_len);
				break;
			case GWU2X_READBACK_BUFFER_FORCED:
				len--;
				snprintf(message, 256, "readback buffer");
				break;
			default:
				snprintf(message, 256, "Unknown");
				printError(message);
				return;
		}
		printInfo(message);
	}
}
