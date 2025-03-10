// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include "xvc_server.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>

#include "ftdiJtagMPSSE.hpp"
#ifdef ENABLE_LIBGPIOD
#include "libgpiodJtagBitbang.hpp"
#endif
#include "cable.hpp"
#include "display.hpp"

using namespace std;

XVC_server::XVC_server(int port, const cable_t & cable,
	const jtag_pins_conf_t * pin_conf, string dev,
	const string & serial, uint32_t clkHZ, int8_t verbose,
	const string & ip_adr, const bool invert_read_edge,
	const string & firmware_path):_verbose(verbose > 1),
			_jtag(NULL), _port(port), _sock(-1),
			_is_stopped(false), _must_stop(false),
			_buffer_size(2048), _state(Jtag::RUN_TEST_IDLE)
{
	(void)pin_conf;
	(void)ip_adr;
	(void)firmware_path;
	switch (cable.type) {
	case MODE_FTDI_SERIAL:
		_jtag = new FtdiJtagMPSSE(cable, dev, serial, clkHZ,
					  invert_read_edge, _verbose);
		break;
#ifdef ENABLE_LIBGPIOD
	case MODE_LIBGPIOD_BITBANG:
		_jtag = new LibgpiodJtagBitbang(pin_conf, dev, clkHZ, verbose);
		break;
#endif
#if 0
	case MODE_ANLOGICCABLE:
		_jtag = new AnlogicCable(clkHZ);
		break;
	case MODE_FTDI_BITBANG:
		if (pin_conf == NULL)
			throw std::exception();
		_jtag =
			new FtdiJtagBitBang(cable.config, pin_conf, dev, serial,
					clkHZ, _verbose);
		break;
	case MODE_CH552_JTAG:
		_jtag =
			new CH552_jtag(cable.config, dev, serial, clkHZ, _verbose);
		break;
	case MODE_DIRTYJTAG:
		_jtag = new DirtyJtag(clkHZ, _verbose);
		break;
	case MODE_JLINK:
		_jtag = new Jlink(clkHZ, _verbose);
		break;
	case MODE_USBBLASTER:
		_jtag = new UsbBlaster(cable.config.vid, cable.config.pid,
					   firmware_path, _verbose);
		break;
#ifdef ENABLE_CMSISDAP
	case MODE_CMSISDAP:
		_jtag =
			new CmsisDAP(cable.config.vid, cable.config.pid, _verbose);
		break;
#endif
#endif
	default:
		std::cerr << "Jtag: unknown cable type" << std::endl;
		throw std::exception();
	}

	_tmstdi = (unsigned char *)malloc(sizeof(unsigned char) * _buffer_size);
	_result = (unsigned char *)malloc(sizeof(unsigned char) * (_buffer_size / 2));
}

XVC_server::~XVC_server()
{
	close_connection();
	if (_jtag)
		delete _jtag;
	free(_tmstdi);
	free(_result);
}

bool XVC_server::open_connection()
{
	char hostname[256];
	memset(&_sock_addr, '\0', sizeof(_sock_addr));
	_sock_addr.sin_family = AF_INET;
	_sock_addr.sin_port = htons(_port);
	_sock_addr.sin_addr.s_addr = INADDR_ANY;

	_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_sock < 0) {
		printError("Socket creation error");
		return false;
	}

	int i = 1;
	setsockopt(_sock, SOL_SOCKET, SO_REUSEADDR, &i, sizeof i);

	if (::bind(_sock, (struct sockaddr*) &_sock_addr, sizeof(_sock_addr)) < 0) {
		printError("Socket bind error");
		close(_sock);
		return false;
	}

	if (listen(_sock, 1) < 0) {
		printError("Socket listen error");
		close(_sock);
		return false;
	}

	if (gethostname(hostname, sizeof(hostname)) != 0) {
		printError("hostname lookup");
		close(_sock);
		return false;
	}

	char mess[512];
	snprintf(mess, sizeof(mess),
		"INFO: To connect to this xvcServer instance, use: TCP:%s:%d\n\n",
		hostname, _port);
	printInfo(mess);

	return true;
}

bool XVC_server::close_connection()
{
	if (_sock != -1)
		close(_sock);
	_sock = -1;
	return true;
}

void XVC_server::thread_listen()
{
	fd_set conn;
	int maxfd = 0;

	FD_ZERO(&conn);
	FD_SET(_sock, &conn);

	maxfd = _sock;

	while (!_must_stop) {
		fd_set read = conn, except = conn;
		int fd;

		struct timeval tv;
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		if (select(maxfd + 1, &read, 0, &except, &tv) < 0) {
			printError("select");
			break;
		}

		for (fd = 0; fd <= maxfd; ++fd) {
			if (FD_ISSET(fd, &read)) {
				if (fd == _sock) {
					int newfd;
					socklen_t nsize = sizeof(_sock_addr);

					newfd = accept(_sock, (struct sockaddr*) &_sock_addr,
						&nsize);

					printf("connection accepted - fd %d\n", newfd);
					if (newfd < 0) {
						throw std::runtime_error("accept");
					} else {
						printInfo("setting TCP_NODELAY to 1\n");
						int flag = 1;
						int optResult = setsockopt(newfd, IPPROTO_TCP,
								TCP_NODELAY, (char *)&flag, sizeof(int));
						if (optResult < 0)
							throw std::runtime_error("TCP_NODELAY error");
						if (newfd > maxfd) {
							maxfd = newfd;
						}
						FD_SET(newfd, &conn);
					}
				} else {
					int ret = handle_data(fd);
					if (ret != 0) {
						printInfo("connection closed - fd " + std::to_string(fd));
						close(fd);
						FD_CLR(fd, &conn);
						if (ret == 3)
							throw std::runtime_error("communication failure");
					}
				}
			} else if (FD_ISSET(fd, &except)) {
				printWarn("connection aborted - fd " + std::to_string(fd));
				close(fd);
				FD_CLR(fd, &conn);
				if (fd == _sock)
					break;
			}
		}
	}
	_is_stopped = true;
}

bool XVC_server::listen_loop()
{
	_is_stopped = false;
	_must_stop = false;
	_thread = new std::thread(&XVC_server::thread_listen, this);
	printInfo("Press to quit");
	getchar();
	_must_stop = true;
	close_connection();
	while (!_is_stopped){}
	delete _thread;

	return true;
}

int XVC_server::sread(int fd, void *target, int len)
{
	unsigned char *t = (unsigned char *)target;
	while (len) {
		int r = read(fd, t, len);
		if (r == 0){  // connection closed
			return 2;
		} else if (r < 0) {
			char err[256];
			snprintf(err, 256, "Read error (%d) %d %s\n", r,
				errno, strerror(errno));
			printError(err);
			return 3;
		}

		t += r;
		len -= r;
	}
	return 1;
}

int XVC_server::handle_data(int fd)
{
	char xvcInfo[32];
	int ret;

	do {
		char cmd[16];
		memset(cmd, 0, 16);

		if ((ret = sread(fd, cmd, 2)) != 1) {
			return ret;
		}

		/* getinfo */
		if (memcmp(cmd, "ge", 2) == 0) {
			if ((ret = sread(fd, cmd, 6)) != 1)
				return ret;
			snprintf(xvcInfo, sizeof(xvcInfo),
				"xvcServer_v1.0:%u\n", _buffer_size);
			if (send(fd, xvcInfo, strlen(xvcInfo), 0) !=
				(ssize_t) strlen(xvcInfo)) {
				perror("write");
				return 1;
			}
			if (_verbose) {
				printInfo(std::to_string((int)time(NULL)) +
							" : Received command: 'getinfo'");
				printInfo("\t Replied with " + string(xvcInfo));
			}
			break;
		/* settck */
		} else if (memcmp(cmd, "se", 2) == 0) {
			if ((ret = sread(fd, cmd, 9)) != 1)
				return ret;
			memcpy(_result, cmd + 5, 4);
			if (write(fd, _result, 4) != 4) {
				printError("write");
				return 1;
			}
			uint32_t clk_period =
				(static_cast<uint32_t>(_result[0]) <<  0) |
				(static_cast<uint32_t>(_result[1]) <<  8) |
				(static_cast<uint32_t>(_result[2]) << 16) |
				(static_cast<uint32_t>(_result[3]) << 24);

			_jtag->setClkFreq(static_cast<uint32_t>(1e9/clk_period));

			if (_verbose) {
				printInfo(std::to_string((int)time(NULL)) +
						" : Received command: 'settck'");
				printf("\t Replied with '%.*s'\n\n", 4,
					   cmd + 5);
			}
			break;
		} else if (memcmp(cmd, "de", 2) == 0) {	 // DEBUG CODE
			if ((ret = sread(fd, cmd, 3)) != 1)
				return ret;
			printf("%u : Received command: 'debug'\n",
				   (int)time(NULL));
			break;
		} else if (memcmp(cmd, "of", 2) == 0) {	 // DEBUG CODE
			if ((ret = sread(fd, cmd, 1)) != 1)
				return ret;
			printf("%u : Received command: 'off'\n",
				   (int)time(NULL));
			break;
		} else if (memcmp(cmd, "sh", 2) == 0) {
			if ((ret = sread(fd, cmd, 4)) != 1)
				return ret;
			if (_verbose) {
				printInfo(std::to_string((int)time(NULL)) +
						" : Received command: 'shift'");
			}
		} else {
			printError("invalid cmd '" + string(cmd) + "'");
			return 1;
		}

		/* Handling for -> "shift:<num bits><tms vector><tdi vector>" */
		uint32_t len, nr_bytes;
		/* 1. len */
		if ((ret = sread(fd, &len, 4)) != 1) {
			printError("reading length failed");
			return ret;
		}

		/* 2. convert len (in bits) to nr_bytes (in bytes) */
		nr_bytes = (len + 7) / 8;
		/* check buffer size */
		if (nr_bytes * 2 > _buffer_size) {
			printError("buffer size exceeded");
			return 1;
		}

		/* 3. receive 2 x nr_bytes (TMS + TDI) */
		if ((ret = sread(fd, _tmstdi, nr_bytes * 2)) != 1) {
			printError("reading data failed");
			return ret;
		}
		memset(_result, 0, nr_bytes);

		if (_verbose) {
			printInfo("\tNumber of Bits  : " + std::to_string(len));
			printInfo("\tNumber of Bytes : " + std::to_string(nr_bytes));
			printInfo("\n");
		}

		// Due to a weird bug(??) xilinx impacts goes through another
		// "capture_ir"/"capture_dr" cycle after reading IR/DR which
		// unfortunately sets IR to the read-out IR value.
		// Just ignore these transactions.
		// ref: https://github.com/tmbinc/xvcd/blob/ftdi/src/xvcd.c#L265
		if (!((_state == Jtag::EXIT1_IR && len == 5 && _tmstdi[0] == 0x17) ||
				(_state == Jtag::EXIT1_DR && len == 4 && _tmstdi[0] == 0x6b))) {
			// update state using tms sequence
			set_state(_tmstdi, len);
			_jtag->writeTMSTDI(_tmstdi, _tmstdi + nr_bytes, _result, len);
		}

		/* send received TDO sequence */
		if (send(fd, _result, nr_bytes, 0) != nr_bytes) {
			printError("write");
			return 1;
		}
	} while (1);
	return 0;
}

/* loops over tms_seq, extracts bit by bit values and update
 * jtag "virtual" state accordingly.
 */
Jtag::tapState_t XVC_server::set_state(const uint8_t *tms_seq, uint32_t len)
{
	for (uint32_t i = 0; i < len; i++) {
		uint8_t tms = !!(tms_seq[i >> 3] & (1 << (i & 0x07)));
		switch (_state) {
			case Jtag::TEST_LOGIC_RESET:
				_state = (tms) ? Jtag::TEST_LOGIC_RESET : Jtag::RUN_TEST_IDLE;
				break;
			case Jtag::RUN_TEST_IDLE:
				_state = (tms) ? Jtag::SELECT_DR_SCAN : Jtag::RUN_TEST_IDLE;
				break;
			case Jtag::SELECT_DR_SCAN:
				_state = (tms) ? Jtag::SELECT_IR_SCAN: Jtag::CAPTURE_DR;
				break;
			case Jtag::CAPTURE_DR:
				_state = (tms) ? Jtag::EXIT1_DR : Jtag::SHIFT_DR;
				break;
			case Jtag::SHIFT_DR:
				_state = (tms) ? Jtag::EXIT1_DR : Jtag::CAPTURE_DR;
				break;
			case Jtag::EXIT1_DR:
				_state = (tms) ? Jtag::UPDATE_DR : Jtag::PAUSE_DR;
				break;
			case Jtag::PAUSE_DR:
				_state = (tms) ? Jtag::EXIT2_DR : Jtag::PAUSE_DR;
				break;
			case Jtag::EXIT2_DR:
				_state = (tms) ? Jtag::UPDATE_DR : Jtag::SHIFT_DR;
				break;
			case Jtag::UPDATE_DR:
			case Jtag::UPDATE_IR:
				_state = (tms) ? Jtag::SELECT_DR_SCAN : Jtag::RUN_TEST_IDLE;
				break;

			case Jtag::SELECT_IR_SCAN:
				_state = (tms) ? Jtag::TEST_LOGIC_RESET : Jtag::CAPTURE_IR;
				break;
			case Jtag::CAPTURE_IR:
				_state = (tms) ? Jtag::EXIT1_IR : Jtag::SHIFT_IR;
				break;
			case Jtag::SHIFT_IR:
				_state = (tms) ? Jtag::EXIT1_IR : Jtag::CAPTURE_IR;
				break;
			case Jtag::EXIT1_IR:
				_state = (tms) ? Jtag::UPDATE_IR : Jtag::PAUSE_IR;
				break;
			case Jtag::PAUSE_IR:
				_state = (tms) ? Jtag::EXIT2_IR : Jtag::PAUSE_IR;
				break;
			case Jtag::EXIT2_IR:
				_state = (tms) ? Jtag::UPDATE_IR : Jtag::SHIFT_IR;
				break;
			default:
				/* pass */
				break;
		}
	}

	return _state;
}
