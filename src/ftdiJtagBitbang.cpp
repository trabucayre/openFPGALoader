// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <libusb.h>
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <stdexcept>

#include "display.hpp"
#include "ftdiJtagBitbang.hpp"
#include "ftdipp_mpsse.hpp"

using namespace std;

#define DEBUG 0

#ifdef DEBUG
#define display(...) \
	do { \
		if (_verbose) fprintf(stdout, __VA_ARGS__); \
	}while(0)
#else
#define display(...) do {}while(0)
#endif

FtdiJtagBitBang::FtdiJtagBitBang(const cable_t &cable,
			const jtag_pins_conf_t *pin_conf, string dev, const std::string &serial,
			uint32_t clkHZ, uint8_t verbose):
			FTDIpp_MPSSE(cable, dev, serial, clkHZ, verbose), _bitmode(0),
			_curr_tms(0), _rx_size(0)
{
	unsigned char *ptr;

	/* Validate pins */
	uint8_t pins[] = {pin_conf->tck_pin, pin_conf->tms_pin,
		pin_conf->tdi_pin, pin_conf->tdo_pin};
	for (uint32_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
		if (pins[i] > FT232RL_RI || pins[i] < FT232RL_TXD) {
			printf("%d\n", pins[i]);
			printError("Invalid pin ID");
			throw std::exception();
		}
	}

	_tck_pin = 1 << pin_conf->tck_pin;
	_tms_pin = 1 << pin_conf->tms_pin;
	_tdi_pin = 1 << pin_conf->tdi_pin;
	_tdo_pin = 1 << pin_conf->tdo_pin;

	/* store FTDI TX Fifo size */
	if (_pid == 0x6001)  // FT232R
		_rx_size = 256;
	else if (_pid == 0x6015)  // FT231X
		_rx_size = 512;
	else
		_rx_size = _buffer_size;

	/* RX Fifo size (rx: USB -> FTDI)
	 * is 128 or 256 Byte and MaxPacketSize ~= 64Byte
	 * but we let subsystem (libftdi, libusb, linux)
	 * sending with the correct size -> this reduce hierarchical calls
	 */
	_buffer_size = 4096;

	/* _buffer_size has changed -> resize buffer */
	ptr = (unsigned char *)realloc(_buffer, sizeof(char) * _buffer_size);
	if (!ptr)
		throw std::runtime_error("_buffer realloc failed\n");
	_buffer = ptr;

	setClkFreq(clkHZ);

	if (init(1, _tck_pin | _tms_pin | _tdi_pin, BITMODE_BITBANG) != 0)
		throw std::runtime_error("low level FTDI init failed");
	setBitmode(BITMODE_BITBANG);
}

FtdiJtagBitBang::~FtdiJtagBitBang()
{
}

int FtdiJtagBitBang::setClkFreq(uint32_t clkHZ)
{
	uint32_t user_clk = clkHZ;
	if (clkHZ > 3000000) {
		printWarn("Jtag probe limited to 3MHz");
		clkHZ = 3000000;
	}
	printInfo("Jtag frequency : requested " + std::to_string(user_clk) +
			"Hz -> real " + std::to_string(clkHZ) + "Hz");
	int ret = ftdi_set_baudrate(_ftdi, clkHZ);
	printf("ret %d\n", ret);
	return ret;
}

int FtdiJtagBitBang::setBitmode(uint8_t mode)
{
	if (_bitmode == mode)
		return 0;
	_bitmode = mode;

	int ret = ftdi_set_bitmode(_ftdi, _tck_pin | _tms_pin | _tdi_pin, _bitmode);
#if (FTDI_VERSION < 105)
	ftdi_usb_purge_rx_buffer(_ftdi);
	ftdi_usb_purge_tx_buffer(_ftdi);
#else
	ftdi_tcioflush(_ftdi);
#endif
	return ret;
}

int FtdiJtagBitBang::writeTMS(uint8_t *tms, uint32_t len, bool flush_buffer)
{
	int ret;

	/* nothing to send
	 * but maybe need to flush internal buffer
	 */
	if (len == 0) {
		if (flush_buffer) {
			ret = flush();
			return ret;
		}
		return 0;
	}

	/* check for at least one bit space in buffer */
	if (_num+2 > _buffer_size) {
		ret = flush();
		if (ret < 0)
			return ret;
	}

	/* fill buffer to reduce USB transaction */
	for (uint32_t i = 0; i < len; i++) {
		_curr_tms = ((tms[i >> 3] & (1 << (i & 0x07)))? _tms_pin : 0);
		uint8_t val = _tdi_pin | _curr_tms;
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

int FtdiJtagBitBang::writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end)
{
	uint32_t iter;
	uint32_t xfer_size = (rx) ? _rx_size : _buffer_size;
	if (len * 2 + 1 < xfer_size) {
		iter = len;
	} else {
		iter = xfer_size >> 1;  // two tx / bit
		iter = (iter / 8) * 8;
	}

	uint8_t *rx_ptr = rx;

	if (len == 0)
		return 0;
	if (rx)
		memset(rx, 0, len/8);

	/* quick fix: use an empty buffer */
	if (_num != 0)
		flush();

	for (uint32_t i = 0, pos = 0; i < len; i++) {
		/* keep tms or
		 * set tms high if it's last bit and end true */
		if (end && (i == len -1))
			_curr_tms = _tms_pin;
		uint8_t val = _curr_tms;

		if (tx)
			val |= ((tx[i >> 3] & (1 << (i & 0x07)))? _tdi_pin : 0);
		_buffer[_num    ] = val;
		_buffer[_num + 1] = val | _tck_pin;

		_num += 2;

		pos++;
		/* flush buffer */
		if (pos == iter) {
			pos = 0;
			write((rx) ? rx_ptr : NULL, iter);
			if (rx)
				rx_ptr += (iter/8);
		}
	}

	/* security check: try to flush buffer */
	if (_num != 0) {
		write((rx && _num > 1) ? rx_ptr : NULL, _num / 2);
	}

	return len;
}

int FtdiJtagBitBang::toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len)
{
	int xfer_len = clk_len;

	int val = ((tms) ? _tms_pin : 0) | ((tdi) ? _tdi_pin : 0);
	while (xfer_len > 0) {
		if (_num + 2 > _buffer_size)
			if (write(NULL, 0) < 0)
				return -EXIT_FAILURE;
		_buffer[_num++] = val | _tck_pin;
		_buffer[_num++] = val;

		xfer_len--;
	}

	/* flush */
	write(NULL, 0);

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

	setBitmode((tdo) ? BITMODE_SYNCBB : BITMODE_BITBANG);

	ret = ftdi_write_data(_ftdi, _buffer, _num);
	if (ret != _num) {
		printf("problem %d written\n", ret);
		return ret;
	}

	if (tdo) {
		ret = ftdi_read_data(_ftdi, _buffer, _num);
		if (ret != _num) {
			printf("problem %d read\n", ret);
			return ret;
		}
		/* need to reconstruct received word 
		 * even bit are discarded since JTAG read in rising edge
		 * since jtag is LSB first we need to shift right content by 1
		 * and add 0x80 (1 << 7) or 0
		 * the buffer may contains some tms bit, so start with i
		 * equal to fill exactly nb_bit bits
		 * */
		for (int i = (_num-(nb_bit *2) + 1), offset=0; i < _num; i+=2, offset++) {
			tdo[offset >> 3] = (((_buffer[i] & _tdo_pin) ? 0x80 : 0x00) |
							(tdo[offset >> 3] >> 1));
		}
	}
	_num = 0;
	return ret;
}
