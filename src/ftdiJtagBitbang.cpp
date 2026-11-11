// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <libusb.h>
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "display.hpp"
#include "ftdiJtagBitbang.hpp"
#include "ftdipp_mpsse.hpp"

FtdiJtagBitBang::FtdiJtagBitBang(const cable_t &cable,
			const jtag_pins_conf_t *pin_conf, const std::string &dev,
			const std::string &serial, uint32_t clkHZ, int8_t verbose):
			FTDIpp_MPSSE(cable, dev, serial, clkHZ, verbose), _bitmode(0),
			_curr_tms(0), _curr_tdi(0), _rx_size(0)
{
	unsigned char *ptr;

	/* Validate pins */
	uint8_t pins[] = {pin_conf->tck_pin, pin_conf->tms_pin,
		pin_conf->tdi_pin, pin_conf->tdo_pin};
	for (uint32_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
		if (pins[i] > FT232RL_RI || pins[i] < FT232RL_TXD)
			throw std::runtime_error("FtdiJtagBitBang: Invalid pin ID " +
				std::to_string(pins[i]));
	}

	_tck_pin = 1 << pin_conf->tck_pin;
	_tms_pin = 1 << pin_conf->tms_pin;
	_tdi_pin = 1 << pin_conf->tdi_pin;
	_tdo_pin = 1 << pin_conf->tdo_pin;

	/* store FTDI TX Fifo size */
	switch (_pid) {
		case 0x6001:  // FT232R
			_rx_size = 256;
			break;
		case 0x6015:  // FT231X
			_rx_size = 512;
			break;
		default:
			_rx_size = _buffer_size;
	}

	/* RX Fifo size (rx: USB -> FTDI)
	 * is 128 or 256 Byte and MaxPacketSize ~= 64Byte
	 * but we let subsystem (libftdi, libusb, linux)
	 * sending with the correct size -> this reduce hierarchical calls
	 */
	_buffer_size = 4096;

	/* _buffer_size has changed -> resize buffer */
	ptr = (unsigned char *)realloc(_buffer, sizeof(char) * _buffer_size);
	if (!ptr)
		throw std::runtime_error("FtdiJtagBitBang: buffer realloc failed\n");
	_buffer = ptr;

	if (setClkFreq(clkHZ) < 0)
		throw std::runtime_error("FtdiJtagBitBang: failed to set clock freq");

	if (init(1, _tck_pin | _tms_pin | _tdi_pin, BITMODE_BITBANG) != 0)
		throw std::runtime_error("FtdiJtagBitBang: low level FTDI init failed");

	if (setBitmode(BITMODE_BITBANG) < 0)
		throw std::runtime_error("FtdiJtagBitBang: failed to set bitmode");
}

FtdiJtagBitBang::~FtdiJtagBitBang()
{
}

int FtdiJtagBitBang::setClkFreq(uint32_t clkHz)
{
	uint32_t user_clk = clkHz;
	if (clkHz > 3000000) {
		printWarn("Jtag probe limited to 3MHz");
		clkHz = 3000000;
	}
	printInfo("Jtag frequency : requested " + std::to_string(user_clk) +
			"Hz -> real " + std::to_string(clkHz) + "Hz");
	int ret = ftdi_set_baudrate(_ftdi, clkHz);
	if (ret < 0)
		printError("FtdiJtagBitBang: Failed to set baudrate error: " +
			std::to_string(ret));
	else
		JtagInterface::_clkHZ = clkHz;
	return ret;
}

int FtdiJtagBitBang::setBitmode(uint8_t mode)
{
	if (_bitmode == mode)
		return 0;

	int ret = ftdi_set_bitmode(_ftdi, _tck_pin | _tms_pin | _tdi_pin, mode);
	if (ret < 0) {
		printError("FtdiJtagBitBang: Failed to set bitmode");
		return ret;
	}
	_bitmode = mode;

#if (FTDI_VERSION < 105)
	ftdi_usb_purge_rx_buffer(_ftdi);
	ftdi_usb_purge_tx_buffer(_ftdi);
#else
	ftdi_tcioflush(_ftdi);
#endif
	return ret;
}

int FtdiJtagBitBang::writeTMS(const uint8_t *tms, uint32_t len,
	bool flush_buffer, const uint8_t tdi)
{
	int ret;

	/* nothing to send
	 * but maybe need to flush internal buffer
	 */
	if (len == 0) {
		if (flush_buffer)
			return flush();
		return 0;
	}

	_curr_tdi = tdi ? _tdi_pin : 0;

	/* check for at least one bit space in buffer */
	if (_num + 2 > _buffer_size) {
		ret = flush();
		if (ret < 0)
			return ret;
	}

	/* fill buffer to reduce USB transaction */
	for (uint32_t i = 0; i < len; i++) {
		_curr_tms = ((tms[i >> 3] & (1 << (i & 0x07)))? _tms_pin : 0);
		const uint8_t val = _curr_tdi | _curr_tms;
		_buffer[_num++] = val;
		_buffer[_num++] = val | _tck_pin;

		if (_num + 2 > _buffer_size) {
			ret = write(NULL, 0);
			if (ret < 0)
				return ret;
		}
	}

	/* security check: try to flush buffer */
	if (flush_buffer) {
		ret = write(NULL, 0);
		if (ret < 0)
			return ret;
	}

	return len;
}

int FtdiJtagBitBang::writeTDI(const uint8_t *tx, uint8_t *rx, uint32_t len,
	bool end)
{
	if (len == 0)
		return 0;

	int ret;
	uint32_t iter;
	const uint32_t xfer_size = (rx) ? _rx_size : _buffer_size;
	if (len * 2 + 1 < xfer_size) {
		iter = len;
	} else {
		iter = xfer_size >> 1;  // two tx / bit
		iter = (iter / 8) * 8;
	}

	uint8_t *rx_ptr = rx;

	if (rx)
		memset(rx, 0, (len + 7) / 8);

	/* quick fix: use an empty buffer */
	if (_num != 0)
		if ((ret = flush()) < 0)
			return ret;

	for (uint32_t i = 0, pos = 0; i < len; i++) {
		/* keep tms or
		 * set tms high if it's last bit and end true */
		if (end && (i == len -1))
			_curr_tms = _tms_pin;

		if (tx)
			_curr_tdi = ((tx[i >> 3] & (1 << (i & 0x07)))? _tdi_pin : 0);

		const uint8_t val = _curr_tms | _curr_tdi;
		_buffer[_num    ] = val;
		_buffer[_num + 1] = val | _tck_pin;

		_num += 2;

		pos++;
		/* flush buffer */
		if (pos == iter) {
			pos = 0;
			if ((ret = write((rx) ? rx_ptr : NULL, iter)) < 0)
				return ret;
			if (rx)
				rx_ptr += (iter/8);
		}
	}

	/* security check: try to flush buffer */
	if (_num != 0) {
		if ((ret = write((rx && _num > 1) ? rx_ptr : NULL, _num / 2)) < 0)
			return ret;
	}

	return len;
}

int FtdiJtagBitBang::toggleClk(uint8_t /*tms*/, uint8_t /*tdi*/,
	uint32_t clk_len)
{
	int ret;

	const uint8_t val = _curr_tms | _curr_tdi;

	for (uint32_t i = 0; i < clk_len; i++) {
		if (_num + 2 > _buffer_size) {
			ret = write(NULL, 0);
			if (ret < 0)
				return ret;
		}
		_buffer[_num++] = val;
		_buffer[_num++] = val | _tck_pin;
	}

	/* flush */
	if ((ret = write(NULL, 0)) < 0)
		return ret;

	return clk_len;
}

int FtdiJtagBitBang::flush()
{
	return write(NULL, 0);
}

int FtdiJtagBitBang::write(uint8_t *tdo, int nb_bit)
{
	int ret = 0;
	if (_num == 0)
		return 0;

	ret = setBitmode((tdo) ? BITMODE_SYNCBB : BITMODE_BITBANG);
	if (ret < 0) {
		printError("FtdiJtagBitBang: write() failed to set bit mode "
			"with error " + std::to_string(ret));
		return ret;
	}

	ret = ftdi_write_data(_ftdi, _buffer, _num);
	if (ret != _num) {
		printError("FtdiJtagBitBang::write failed with error " +
			std::to_string(ret));
		return ret;
	}

	if (tdo) {
		uint32_t rd_len = _num;
		uint8_t timeout = 100;
		uint8_t *ptr = _buffer;
		/* ftdi_read_data may provides _num char in one
		 * pass, but may also provides less when the baudrate
		 * is slow -> loop until _num bytes have been read
		 */
		do {
			ret = ftdi_read_data(_ftdi, ptr, rd_len);
			if (ret < 0) {
				printError("FtdiJtagBitBang::write failed to readback "
					"with error " + std::to_string(ret));
				return ret;
			} else if (ret == 0) {
				if (++timeout >= 100) {
					printError("FtdiJtagBitBang::write timeout");
					return -1;
				}
			}
			rd_len -= ret;
			ptr += ret;
		} while (rd_len > 0);

		/* need to reconstruct received word 
		 * even bit are discarded since JTAG read in rising edge
		 * since jtag is LSB first we need to shift right content by 1
		 * and add 0x80 (1 << 7) or 0
		 * the buffer may contains some tms bit, so start with i
		 * equal to fill exactly nb_bit bits
		 * */
		uint32_t offset = 0;
		for (int i = (_num - (nb_bit << 1) + 1); i < _num; i += 2) {
			const uint32_t off = offset >> 3;
			tdo[off] = (((_buffer[i] & _tdo_pin) ? 0x80 : 0x00) |
					(tdo[off] >> 1));
			offset++;
		}
		/* All full bytes are correctly aligned
		 * the last one, if nb_bit is not multiple of 8 is mis-aligned
		 * fix this after reconstruct
		 */
		const uint8_t bit_shift = nb_bit & 0x07;
		if (bit_shift & 0x07)  // modulo 8
			tdo[nb_bit >> 3] >>= 8 - (bit_shift);
	}
	_num = 0;
	return ret;
}
