// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2023 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include "remoteBitbang_client.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <map>
#include <stdexcept>
#include <string>
#include <regex>
#include <utility>
#include <vector>

#include "display.hpp"

using namespace std;

#define TCK_OFFSET 2
#define TMS_OFFSET 1
#define TDI_OFFSET 0
#define TCK_BIT    (1 << TCK_OFFSET)
#define TMS_BIT    (1 << TMS_OFFSET)
#define TDI_BIT    (1 << TDI_OFFSET)

RemoteBitbang_client::RemoteBitbang_client(const std::string &ip_addr, int port,
		int8_t verbose):
	_xfer_buf(NULL), _num_bytes(0), _last_tms(TMS_BIT),
	_last_tdi(0), _buffer_size(2048), _sock(0), _port(port)
{
	(void) verbose;
	/* create client to server */
	if (!open_connection(ip_addr))
		throw std::runtime_error("connection failure");

	/* set led to low */
	if (xfer_pkt('b', NULL) < 0)
		throw std::runtime_error("can't set led low");

	_xfer_buf = reinterpret_cast<uint8_t *>(malloc(sizeof(uint8_t)
				* _buffer_size));
	if (!_xfer_buf)
		throw std::runtime_error("can't allocate internal buffer");
}

RemoteBitbang_client::~RemoteBitbang_client()
{
	// flush buffers before quit
	if (_num_bytes != 0)
		flush();

	// set led high
	if (xfer_pkt('B', NULL) < 0)
		printf("can't set led high");

	// send close request
	if (xfer_pkt('Q', NULL) < 0)
		printf("can't send close request");

	// cleanup
	if (_xfer_buf)
		free(_xfer_buf);
	// close socket
	close(_sock);
}

int RemoteBitbang_client::writeTMS(const uint8_t *tms, uint32_t len,
		bool flush_buffer)
{
	// empty buffer
	// if asked flush
	if (len == 0)
		return ((flush_buffer) ? flush() : 0);

	uint8_t base_v = '0' + _last_tdi;
	for (uint32_t pos = 0; pos < len; pos++) {
		// buffer full -> write
		if (_num_bytes == _buffer_size)
			ll_write(NULL);
		_last_tms = (tms[pos >> 3] & (1 << (pos & 0x07))) ? TMS_BIT : 0;
		_xfer_buf[_num_bytes++] = base_v + _last_tms;
		_xfer_buf[_num_bytes++] = base_v + _last_tms + TCK_BIT;
	}

	// flush where it's asked or if the buffer is full
	if (flush_buffer || _num_bytes == _buffer_size * 8)
		return flush();
	return len;
}

int RemoteBitbang_client::writeTDI(const uint8_t *tx, uint8_t *rx, uint32_t len,
		bool end)
{
	if (len == 0)  // nothing to do
		return 0;

	uint8_t base_v = '0' + _last_tms;
	for (uint32_t pos = 0; pos < len; pos++) {
		if (_num_bytes == _buffer_size)
			ll_write(NULL);  // NULL because _num_bytes is always 0 when read
		_last_tdi = (tx[pos >> 3] & (1 << (pos & 0x07))) ? TDI_BIT : 0;
		if (end && pos == len - 1) {
			_last_tms = TMS_BIT;
			base_v = '0' + _last_tms;
		}
		_xfer_buf[_num_bytes++] = base_v + _last_tdi;
		_xfer_buf[_num_bytes++] = base_v + _last_tdi + TCK_BIT;
		if (rx) {
			uint8_t tdo;
			ll_write(&tdo);
			if (tdo == '1')
				rx[pos >> 3] |= (1 << (pos & 0x07));
			else
				rx[pos >> 3] &= ~(1 << (pos & 0x07));
		}
	}

	return len;
}

// toggle clk with constant TDI and TMS.
int RemoteBitbang_client::toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len)
{
	// nothing to do
	if (clk_len == 0)
		return 0;
	if (_num_bytes != 0)
		flush();

	_last_tms = tms;
	_last_tdi = tdi;
	uint8_t val = (_last_tms | _last_tdi);

	// flush buffer before starting
	if (_num_bytes != 0)
		flush();

	for (uint32_t len = 0; len < clk_len; len++) {
		if (len == _num_bytes)
			ll_write(NULL);
		_xfer_buf[_num_bytes++] = '0' + val;
		_xfer_buf[_num_bytes++] = '0' + (val | TCK_BIT);
	}

	ll_write(NULL);

	return clk_len;
}

int RemoteBitbang_client::flush()
{
	return ll_write(NULL);
}

int RemoteBitbang_client::setClkFreq(uint32_t clkHz)
{
	printWarn("clock speed is not configurable");
	return clkHz;
}

bool RemoteBitbang_client::open_connection(const string &ip_addr)
{
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(_port);
	addr.sin_addr.s_addr = inet_addr(ip_addr.c_str());

	_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_sock == -1) {
		printError("Socket creation error");
		return false;
	}

	if (connect(_sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		printError("Connection error");
		close(_sock);
		return false;
	}

	int one = 1;
	setsockopt(_sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&one,
			sizeof(one));

	return true;
}

ssize_t RemoteBitbang_client::xfer_pkt(uint8_t instr, uint8_t *rx)
{
	ssize_t len;
	// 1. instruction
	if ((len = write(_sock, &instr, 1)) == -1) {
		printError("Send instruction failed with error " +
				std::to_string(len));
		return -1;
	}

	if (rx) {
		len = recv(_sock, rx, 1, 0);
		if (len < 0) {
			printError("Receive error");
			return len;
		}
	}

	return (rx) ? 1 : 0;
}

bool RemoteBitbang_client::ll_write(uint8_t *tdo)
{
	if (_num_bytes == 0)
		return true;

	ssize_t len;
	// write current buffer
	if ((len = write(_sock, _xfer_buf, _num_bytes)) == -1) {
		printError("Send error error: " + std::to_string(len));
		return false;
	}
	_num_bytes = 0;

	// read only one char (if tdo is not null
	if (tdo) {
		if (xfer_pkt('R', tdo) < 0) {
			printError("read request error");
			return false;
		}
	}

	return true;
}
