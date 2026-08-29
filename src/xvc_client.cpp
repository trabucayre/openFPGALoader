// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include "xvc_client.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <map>
#include <stdexcept>
#include <string>
#include <regex>
#include <utility>
#include <vector>

#include "display.hpp"


XVC_client::XVC_client(const std::string &ip_addr, int port,
		uint32_t clkHz, int8_t verbose):
	_verbose(verbose > 0), _xfer_buf(NULL), _tms(NULL), _tditdo(NULL),
	_num_bits(0), _last_tms(0), _last_tdi(0), _buffer_size(0),
	_sock(INVALID_SOCKET), _port(port)
{
	if (!xvc_socket_init())
		throw std::runtime_error("socket layer initialization failure");

	if (!open_connection(ip_addr))
		throw std::runtime_error("connection failure");

	uint8_t buffer[2048];
	if (xfer_pkt("getinfo:", NULL, 0, buffer, 2048) <= 0)
		throw std::runtime_error("can't read info");

	std::regex r("[_:]");
	std::string rep((const char *)buffer);

	std::sregex_token_iterator start{ rep.begin(), rep.end(), r, -1 }, end;
	std::vector<std::string> toto(start, end);

	if (toto.size() != 3)
		throw std::runtime_error("wrong getinfo: answer");

	_server_name = std::move(toto[0]);
	_server_vers = std::move(toto[1]);
	_buffer_size = stoi(toto[2]) / 2;  // buffer_size is for tms + tdi

	_xfer_buf = reinterpret_cast<uint8_t *>(malloc(sizeof(uint8_t)
				* ((2*_buffer_size) + 4)));
	_tms = reinterpret_cast<uint8_t *>(malloc(sizeof(uint8_t) * _buffer_size));
	_tditdo = reinterpret_cast<uint8_t *>(malloc(sizeof(uint8_t) *
				_buffer_size));
	if (!_xfer_buf || !_tms || !_tditdo)
		throw std::runtime_error("buffer allocation failure");

	char disp[2048];
	snprintf(disp, sizeof(disp), "detected %s version %s packet size %u",
			_server_name.c_str(), _server_vers.c_str(), _buffer_size);

	printInfo(disp);

	setClkFreq(clkHz);
}

XVC_client::~XVC_client()
{
	// flush buffers before quit
	if (_num_bits != 0)
		flush();

	// cleanup
	if (_xfer_buf)
		free(_xfer_buf);
	if (_tms)
		free(_tms);
	if (_tditdo)
		free(_tditdo);
	// close socket
	xvc_close_socket(_sock);
	xvc_socket_cleanup();
}

int XVC_client::writeTMS(const uint8_t *tms, uint32_t len, bool flush_buffer,
		__attribute__((unused)) const uint8_t tdi)
{
	// empty buffer
	// if asked flush
	if (len == 0)
		return ((flush_buffer) ? flush() : 0);

	for (uint32_t pos = 0; pos < len; pos++) {
		// buffer full -> write
		if (_num_bits == _buffer_size * 8) {
			// write
			if(!ll_write(NULL))
				throw std::runtime_error("xvc ll_write fails");
			_num_bits = 0;
		}

		_last_tms = (tms[pos >> 3] & (1 << (pos & 0x07))) ? 1 : 0;

		if (_last_tms)
			_tms[(_num_bits >> 3)] |= (1 << (_num_bits & 0x07));
		else
			_tms[(_num_bits >> 3)] &= ~(1 << (_num_bits & 0x07));
		if (_last_tdi)
			_tditdo[(_num_bits >> 3)] |= (1 << (_num_bits & 0x07));
		else
			_tditdo[(_num_bits >> 3)] &= ~(1 << (_num_bits & 0x07));
		_num_bits++;
	}

	// flush where it's asked or if the buffer is full
	if (flush_buffer || _num_bits == _buffer_size * 8)
		return flush();
	return len;
}

int XVC_client::writeTDI(const uint8_t *tx, uint8_t *rx, uint32_t len, bool end)
{
	if (len == 0)  // nothing to do
		return 0;
	if (_num_bits != 0)  // flush buffer to simplify next step
		flush();

	uint32_t xfer_len = _buffer_size * 8;  // default to buffer capacity
	uint8_t tms = (_last_tms) ? 0xff : 0x00;  // set tms byte
	const uint8_t *tx_ptr = tx;
	uint8_t *rx_ptr = rx;  // use pointer to simplify algo

	/* write by burst */
	for (uint32_t rest = 0; rest < len; rest += xfer_len) {
		if ((xfer_len + rest) > len)  // len < buffer size
			xfer_len = len - rest;  // reduce xfer len
		uint16_t tt = (xfer_len + 7) >> 3;  // convert to Byte
		memset(_tms, tms, tt);  // fill tms buffer
		memcpy(_tditdo, tx_ptr, tt);  // fill tdi buffer
		_num_bits = xfer_len;  // set buffer size in bit
		if (end && xfer_len + rest == len) {  // last sequence: set tms 1
			_last_tms = 1;
			uint16_t idx = _num_bits - 1;
			_tms[(idx >> 3)] |= (1 << (idx & 0x07));
		}
		if(!ll_write((rx) ? rx_ptr : NULL))  // write
			throw std::runtime_error("xvc ll_write fails");

		tx_ptr += tt;
		if (rx)
			rx_ptr += tt;
	}

	return len;
}

// toggle clk with constant TDI and TMS. More or less same idea as writeTDI
int XVC_client::toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len)
{
	// nothing to do
	if (clk_len == 0)
		return 0;
	if (_num_bits != 0)
		flush();

	_last_tms = tms;
	_last_tdi = tdi;
	uint8_t curr_tms = (tms) ? 0xff: 0x00;
	uint8_t curr_tdi = (tdi) ? 0xff: 0x00;

	uint32_t len = clk_len;

	// flush buffer before starting
	if (_num_bits != 0)
		flush();

	memset(_tditdo, curr_tdi, _buffer_size);
	memset(_tms, curr_tms, _buffer_size);
	do {
		_num_bits = _buffer_size * 8;
		if (len < _num_bits)
			_num_bits = len;
		len -= _num_bits;
		if(!ll_write(NULL))
			throw std::runtime_error("xvc ll_write fails");
	} while (len > 0);

	return clk_len;
}

int XVC_client::flush()
{
	return ll_write(NULL);
}


int XVC_client::setClkFreq(uint32_t clkHz)
{
	double clk_periodf = (1e9 / clkHz);
	uint32_t clk_period = (uint32_t)floor(clk_periodf);

	_xfer_buf[0] = static_cast<uint8_t>((clk_period >>  0) & 0xff);
	_xfer_buf[1] = static_cast<uint8_t>((clk_period >>  8) & 0xff);
	_xfer_buf[2] = static_cast<uint8_t>((clk_period >> 16) & 0xff);
	_xfer_buf[3] = static_cast<uint8_t>((clk_period >> 24) & 0xff);

	if (xfer_pkt("settck:", _xfer_buf, 4, _xfer_buf, 4) <= 0) {
		printError("setClkFreq: fail to configure frequency");
		return -EXIT_FAILURE;
	}

	printf("freq %d %lf %d %d\n", clkHz, clk_periodf, clk_period,
			atoi((const char *)_xfer_buf));
	printf("%x %x %x %x\n", _xfer_buf[0], _xfer_buf[1],
			_xfer_buf[2], _xfer_buf[3]);

	_clkHZ = clkHz;
	return _clkHZ;
}

bool XVC_client::open_connection(const std::string &ip_addr)
{
	struct addrinfo hints;
	struct addrinfo *res = nullptr;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	/* NOTE: this used to build the sockaddr_in "by hand" with
	 * inet_addr(ip_addr.c_str()) -- but inet_addr() only understands
	 * dotted-decimal IP literals ("192.168.1.5"), NOT hostnames. Given
	 * a hostname it silently fails and returns INADDR_NONE
	 * (255.255.255.255), and connect()ing to that bogus address is
	 * what produces "WSA error 10049" (WSAEADDRNOTAVAIL) on Windows
	 * (and a similar failure on POSIX). getaddrinfo() resolves both
	 * IP literals and hostnames correctly, on every platform. */
	int gai_ret = getaddrinfo(ip_addr.c_str(), std::to_string(_port).c_str(),
			&hints, &res);
	if (gai_ret != 0) {
		printError("Address resolution error for \"" + ip_addr + "\": " +
#if defined(_WIN32) || defined(_WIN64)
			xvc_last_socket_error());
#else
			std::string(gai_strerror(gai_ret)));
#endif
		return false;
	}

	_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (_sock == INVALID_SOCKET) {
		printError("Socket creation error");
		freeaddrinfo(res);
		return false;
	}

	if (connect(_sock, res->ai_addr,
			static_cast<socklen_t>(res->ai_addrlen)) == SOCKET_ERROR) {
		printError("Connection error: " + xvc_last_socket_error());
		xvc_close_socket(_sock);
		freeaddrinfo(res);
		return false;
	}

	freeaddrinfo(res);
	return true;
}

static
long sendall(SOCKET sock, const void* raw, size_t cnt)
{
	const uint8_t *buf = (const uint8_t*)raw;
	size_t remaining = cnt;
	while (remaining) {
		long ret = xvc_send(sock, buf, remaining);
		if (ret==0) // should not happen on a connected TCP socket
			throw std::logic_error("platform TCP send() returns zero?!?");
		if (ret < 0)
			return ret;
		buf += ret;
		remaining -= ret;
	}
	return (long)cnt; // success
}

static
ssize_t recvall(SOCKET sock, void* raw, size_t cnt)
{
	uint8_t *buf = (uint8_t*)raw;
	size_t remaining = cnt;
	while (remaining) {
		long ret = xvc_recv(sock, buf, remaining);
		if (ret < 0)
			return ret;
		if (ret == 0)  // peer closed the connection mid-reply
			break;
		buf += ret;
		remaining -= ret;
	}
	return cnt - remaining;
}

ssize_t XVC_client::xfer_pkt(const std::string &instr,
		const uint8_t *tx, uint32_t tx_size,
		uint8_t *rx, uint32_t rx_size, bool rx_exact)
{
	ssize_t len = tx_size;
	std::vector<uint8_t> buffer(instr.size() + ((tx) ? tx_size : 0));
	memcpy(buffer.data(), instr.c_str(), instr.size());
	if (tx)
		memcpy(buffer.data() + instr.size(), tx, tx_size);

	if (sendall(_sock, buffer.data(), buffer.size()) == -1) {
		printError("Send failed");
		return -1;
	}

	if (rx) {
		if (rx_exact) {
			len = recvall(_sock, rx, rx_size);
		} else {
			len = xvc_recv(_sock, rx, rx_size);
		}
		if (len < 0) {
			printError("Receive error: " + xvc_last_socket_error());
			return len;
		} else if (len == 0) {
			fprintf(stderr, "Client orderly shut down the connection.\n");
		}
		rx[len] = '\0';
		if (_verbose) {
			printInfo("received " + std::to_string(len) + " Bytes (" +
					std::to_string(len * 8) + ")");
			printf("\t");
			for (int i = 0; i < len; i++)
				printf("%02x ", rx[i]);
			printf("\n");
		}
	}

	return len;
}

bool XVC_client::ll_write(uint8_t *tdo)
{
	int ret;
	if (_num_bits == 0)
		return true;
	uint32_t numbytes = (_num_bits + 7) >> 3;

	_xfer_buf[0] = static_cast<uint8_t>((_num_bits >>  0) & 0xff);
	_xfer_buf[1] = static_cast<uint8_t>((_num_bits >>  8) & 0xff);
	_xfer_buf[2] = static_cast<uint8_t>((_num_bits >> 16) & 0xff);
	_xfer_buf[3] = static_cast<uint8_t>((_num_bits >> 24) & 0xff);
	memcpy(_xfer_buf + 4, _tms, numbytes);
	memcpy(_xfer_buf + 4 + numbytes, _tditdo, numbytes);

	if ((ret = xfer_pkt("shift:\0", _xfer_buf, (2 * numbytes) + 4,
					_tditdo, numbytes, true)) < 0)
		return false;
	_num_bits = 0;  // clear counter

	if (tdo)
		memcpy(tdo, _tditdo, numbytes);

	return true;
}
