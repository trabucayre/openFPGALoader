// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include "xvc_server.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#include "ftdiJtagMPSSE.hpp"
#ifdef ENABLE_LIBGPIOD
#include "libgpiodJtagBitbang.hpp"
#endif
#ifdef ENABLE_USBBLASTER
#include "usbBlaster.hpp"
#endif
#ifdef ENABLE_CH347
#include "ch347jtag.hpp"
#endif
#ifdef ENABLE_DIRTYJTAG
#include "dirtyJtag.hpp"
#endif
#include "cable.hpp"
#include "display.hpp"


XVC_server::XVC_server(int port, const cable_t & cable,
	const jtag_pins_conf_t * pin_conf, std::string dev,
	const std::string & serial, uint32_t clkHZ, int8_t verbose,
	const std::string & ip_adr, const bool invert_read_edge,
	const std::string & firmware_path):_verbose(verbose > 0),
			_jtag(NULL), _port(port), _sock(INVALID_SOCKET),
			_is_stopped(false), _must_stop(false),
			_buffer_size(1048576), _state(Jtag::RUN_TEST_IDLE)
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
#ifdef ENABLE_USBBLASTER
	case MODE_USBBLASTER:
		_jtag = new UsbBlaster(cable, firmware_path, verbose);
		break;
#endif
#ifdef ENABLE_CH347
	case MODE_CH347:
		_jtag = new CH347Jtag(clkHZ, verbose, cable.vid, cable.pid,
				cable.bus_addr, cable.device_addr);
		break;
#endif
#ifdef ENABLE_DIRTYJTAG
	case MODE_DIRTYJTAG:
		_jtag = new DirtyJtag(clkHZ, verbose, cable.vid, cable.pid);
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
	case MODE_JLINK:
		_jtag = new Jlink(clkHZ, _verbose);
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
	xvc_socket_cleanup();
	if (_jtag)
		delete _jtag;
	free(_tmstdi);
	free(_result);
}

bool XVC_server::open_connection()
{
	char hostname[256];

	if (!xvc_socket_init()) {
		printError("Socket layer initialization failure");
		return false;
	}

	memset(&_sock_addr, '\0', sizeof(_sock_addr));
	_sock_addr.sin_family = AF_INET;
	_sock_addr.sin_port = htons(_port);
	_sock_addr.sin_addr.s_addr = INADDR_ANY;

	_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_sock == INVALID_SOCKET) {
		printError("Socket creation error");
		return false;
	}

	int i = 1;
	setsockopt(_sock, SOL_SOCKET, SO_REUSEADDR,
			reinterpret_cast<const char *>(&i), sizeof i);

	if (::bind(_sock, (struct sockaddr*) &_sock_addr, sizeof(_sock_addr))
			== SOCKET_ERROR) {
		printError("Socket bind error: " + xvc_last_socket_error());
		xvc_close_socket(_sock);
		return false;
	}

	if (listen(_sock, 1) == SOCKET_ERROR) {
		printError("Socket listen error: " + xvc_last_socket_error());
		xvc_close_socket(_sock);
		return false;
	}

	if (gethostname(hostname, sizeof(hostname)) != 0) {
		printError("hostname lookup");
		xvc_close_socket(_sock);
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
	/* Only close the listening socket here: thread_listen() may still be
	 * servicing already-accepted client connections at this point, so
	 * Winsock itself (xvc_socket_cleanup()) is only released once the
	 * thread has fully stopped, see ~XVC_server(). */
	if (_sock != INVALID_SOCKET)
		xvc_close_socket(_sock);
	_sock = INVALID_SOCKET;
	return true;
}

void XVC_server::thread_listen()
{
	/* Accepted client connections.
	 * NOTE: on POSIX a socket fd is a small sequential integer, so the
	 * original code just scanned every value between 0 and maxfd. On
	 * Windows a SOCKET is an opaque kernel handle (not a small sequential
	 * index), so that scan is invalid there. Keeping an explicit list of
	 * active client sockets works identically -- and is simpler -- on
	 * both POSIX and Windows, so it is used unconditionally. */
	std::vector<SOCKET> clients;

	try {
		while (!_must_stop) {
			fd_set conn;
			FD_ZERO(&conn);
			FD_SET(_sock, &conn);
			SOCKET maxfd = _sock;
			for (SOCKET c : clients) {
				FD_SET(c, &conn);
				if (c > maxfd)
					maxfd = c;
			}

			fd_set read = conn, except = conn;
			struct timeval tv;
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			/* nfds is ignored by Winsock's select(), only fd_set content
			 * matters there; on POSIX it must be maxfd + 1. */
#if defined(_WIN32) || defined(_WIN64)
			int select_ret = select(0, &read, NULL, &except, &tv);
#else
			int select_ret = select(static_cast<int>(maxfd) + 1, &read,
					NULL, &except, &tv);
#endif
			if (select_ret == SOCKET_ERROR) {
				printError("select: " + xvc_last_socket_error());
				break;
			}

			/* new incoming connection */
			if (FD_ISSET(_sock, &read)) {
				socklen_t nsize = sizeof(_sock_addr);
				SOCKET newfd = accept(_sock, (struct sockaddr*) &_sock_addr,
						&nsize);

				if (newfd == INVALID_SOCKET) {
					/* Transient accept() failures (e.g. the peer
					 * resets the connection between select() and
					 * accept()) must not take down the whole
					 * listening loop -- log and keep serving other
					 * connections / waiting for the next one. */
					printError("accept: " + xvc_last_socket_error());
				} else {
					printInfo("connection accepted - fd " +
							std::to_string(static_cast<intptr_t>(newfd)));
					printInfo("setting TCP_NODELAY to 1\n");
					int flag = 1;
					int optResult = setsockopt(newfd, IPPROTO_TCP,
							TCP_NODELAY, reinterpret_cast<const char *>(&flag),
							sizeof(int));
					if (optResult == SOCKET_ERROR) {
						printError("TCP_NODELAY: " + xvc_last_socket_error());
						xvc_close_socket(newfd);
					} else {
						clients.push_back(newfd);
					}
				}
			}

			/* service existing connections */
			for (auto it = clients.begin(); it != clients.end();) {
				SOCKET fd = *it;
				bool drop = false;

				if (FD_ISSET(fd, &read)) {
					int ret = handle_data(fd);
					if (ret != 0) {
						printInfo("connection closed - fd " +
								std::to_string(static_cast<intptr_t>(fd)));
						xvc_close_socket(fd);
						drop = true;
						/* NOTE: a communication failure (ret == 3) with
						 * THIS client must not take down the listening
						 * loop for every future connection -- it used
						 * to throw here, which propagated past the
						 * while() below straight into the catch block,
						 * silently ending thread_listen() for good:
						 * the process kept running (main thread still
						 * blocked on getchar()) but nothing ever
						 * accept()ed a new connection again, so any
						 * later client would connect at the TCP level
						 * and then hang forever waiting for a reply
						 * that could never come. Just drop this one
						 * client and keep serving. */
					}
				} else if (FD_ISSET(fd, &except)) {
					printWarn("connection aborted - fd " +
							std::to_string(static_cast<intptr_t>(fd)));
					xvc_close_socket(fd);
					drop = true;
				}

				it = drop ? clients.erase(it) : std::next(it);
			}
		}
	} catch (const std::runtime_error& e) {
		std::cerr << "thread exiting with error: " << e.what() << std::endl;
	}

	for (SOCKET c : clients)
		xvc_close_socket(c);
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
	_thread->join();
	delete _thread;

	return true;
}

int XVC_server::sread(SOCKET fd, void *target, int len)
{
	unsigned char *t = (unsigned char *)target;
	while (len) {
		long r = xvc_recv(fd, t, len);
		if (r == 0){  // connection closed
			return 2;
		} else if (r < 0) {
			printError("Read error (" + std::to_string(r) + ") " +
					xvc_last_socket_error());
			return 3;
		}

		t += r;
		len -= r;
	}
	return 1;
}

int XVC_server::handle_data(SOCKET fd)
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
			if (xvc_send(fd, xvcInfo, strlen(xvcInfo)) !=
				(long) strlen(xvcInfo)) {
				printError("write: " + xvc_last_socket_error());
				return 1;
			}
			if (_verbose) {
				printInfo(std::to_string((int)time(NULL)) +
							" : Received command: 'getinfo'");
				printInfo("\t Replied with " + std::string(xvcInfo));
			}
			break;
		/* settck */
		} else if (memcmp(cmd, "se", 2) == 0) {
			if ((ret = sread(fd, cmd, 9)) != 1)
				return ret;
			memcpy(_result, cmd + 5, 4);
			if (xvc_send(fd, _result, 4) != 4) {
				printError("write: " + xvc_last_socket_error());
				return 1;
			}
			uint32_t clk_period =
				(static_cast<uint32_t>(_result[0]) <<  0) |
				(static_cast<uint32_t>(_result[1]) <<  8) |
				(static_cast<uint32_t>(_result[2]) << 16) |
				(static_cast<uint32_t>(_result[3]) << 24);

			uint32_t requested_freq = static_cast<uint32_t>(1e9/clk_period);
			uint32_t safe_max = _jtag->maxSafeXvcFreq();
			uint32_t actual_freq = (safe_max != 0 && requested_freq > safe_max)
				? safe_max : requested_freq;
			if (_verbose && actual_freq != requested_freq) {
				printInfo("settck: capping requested " +
						std::to_string(requested_freq) + "Hz to " +
						std::to_string(actual_freq) +
						"Hz (cable safety limit)");
			}
			_jtag->setClkFreq(actual_freq);

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
			printError("invalid cmd '" + std::string(cmd) + "'");
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
			if (_jtag->hasNativeTMSTDI()) {
				if (!_jtag->writeTMSTDI(_tmstdi, _tmstdi + nr_bytes,
						_result, len)) {
					printError("writeTMSTDI failed");
					return 1;
				}
			} else {
				if (!generic_writeTMSTDI(_tmstdi, _tmstdi + nr_bytes,
						_result, len)) {
					printError("generic_writeTMSTDI failed");
					return 1;
				}
			}
		}

		/* send received TDO sequence */
		if (xvc_send(fd, _result, nr_bytes) != (long) nr_bytes) {
			printError("write: " + xvc_last_socket_error());
			return 1;
		}
	} while (1);
	return 0;
}

/* loops over tms_seq, extracts bit by bit values and update
 * jtag "virtual" state accordingly.
 */
/* Extract `nbits` bits starting at bit offset `src_bit_offset` in `src`
 * (lsb first) into a freshly, tightly-packed buffer `dst` (starting at
 * bit 0). Used to build the small per-run buffers writeTDI()/writeTMS()
 * expect out of the middle of the much larger XVC (tms, tdi) vectors. */
static void xvc_pack_bits(const uint8_t *src, uint32_t src_bit_offset,
		uint8_t *dst, uint32_t nbits)
{
	for (uint32_t i = 0; i < nbits; i++) {
		uint32_t sbit = src_bit_offset + i;
		bool bit = (src[sbit >> 3] >> (sbit & 7)) & 1;
		if (bit)
			dst[i >> 3] |= (1 << (i & 7));
		else
			dst[i >> 3] &= ~(1 << (i & 7));
	}
}

/* Reverse of xvc_pack_bits(): copy `nbits` tightly-packed bits from `src`
 * (starting at bit 0) into `dst` starting at bit offset `dst_bit_offset`
 * (lsb first). Used to place a run's TDO capture back into the XVC
 * reply buffer at the right bit position. */
static void xvc_unpack_bits(const uint8_t *src, uint32_t nbits,
		uint8_t *dst, uint32_t dst_bit_offset)
{
	for (uint32_t i = 0; i < nbits; i++) {
		uint32_t dbit = dst_bit_offset + i;
		bool bit = (src[i >> 3] >> (i & 7)) & 1;
		if (bit)
			dst[dbit >> 3] |= (1 << (dbit & 7));
		else
			dst[dbit >> 3] &= ~(1 << (dbit & 7));
	}
}

bool XVC_server::generic_writeTMSTDI(const uint8_t *tms, const uint8_t *tdi,
		uint8_t *tdo, uint32_t len)
{
	if (len == 0)
		return true;

	uint32_t nbytes = (len + 7) / 8;
	/* scratch buffer, reused to pack either a TDI run or a TMS run */
	std::vector<uint8_t> run_buf(nbytes, 0);
	std::vector<uint8_t> run_tdo(nbytes, 0);

	uint32_t i = 0;
	while (i < len) {
		bool bit_tms = (tms[i >> 3] >> (i & 7)) & 1;

		if (!bit_tms) {
			/* "shift" run: bits with tms==0 (shifting through the
			 * currently selected data register) followed by, at
			 * most, a single tms==1 bit that both ends the run and
			 * exits the shift state -- exactly what writeTDI()'s
			 * "end" parameter does on its last bit. */
			uint32_t j = i;
			while (j < len && !((tms[j >> 3] >> (j & 7)) & 1))
				j++;
			uint32_t run_len = j - i;
			bool end = false;
			if (j < len) {
				run_len++;  // absorb the single trailing tms==1 bit
				end = true;
			}

			/* NOTE: an earlier version of this function "primed" the
			 * wire with an explicit tms=0 bit through writeTMS()
			 * before calling writeTDI() with the rest, to work
			 * around cables (e.g. UsbBlaster) whose writeTDI() only
			 * uses its fast byte-oriented path when it already
			 * believes tms=0 is on the wire. That priming bit had
			 * no TDO capture (writeTMS() returns none), which is
			 * only safe if that bit is a throwaway state-entry
			 * artifact -- true for some XVC clients' message
			 * framing, but NOT guaranteed in general: a client that
			 * sends TAP navigation and the data shift as two
			 * separate "shift:" messages (as openFPGALoader's own
			 * xvc-client does) can have a shift-run whose very
			 * first bit is genuine payload (e.g. bit 0 of a 32-bit
			 * IDCODE read) -- priming silently corrupted exactly
			 * that bit. Fixed properly instead in
			 * UsbBlaster::writeTDI() itself, which now handles a
			 * stale internal TMS-tracking state safely (falls back
			 * to bit-banging, still capturing TDO, instead of
			 * silently dropping data) -- so this layer no longer
			 * needs to work around it at all. */
			xvc_pack_bits(tdi, i, run_buf.data(), run_len);
			int ret = _jtag->writeTDI(run_buf.data(), run_tdo.data(),
					run_len, end);
			if (ret < 0 || static_cast<uint32_t>(ret) != run_len) {
				printError("generic_writeTMSTDI: writeTDI failed");
				return false;
			}
			xvc_unpack_bits(run_tdo.data(), run_len, tdo, i);

			i += run_len;
		} else {
			/* pure TAP navigation run: tms varies, tdi is not
			 * meaningfully sampled by real hardware here since the
			 * TAP is not in a SHIFT-DR/IR state; the XVC reply bits
			 * for this run are simply left at 0 (already the case,
			 * _result is memset to 0 by the caller), matching what
			 * other minimal XVC servers do outside of shift runs. */
			uint32_t j = i;
			while (j < len && ((tms[j >> 3] >> (j & 7)) & 1))
				j++;
			uint32_t run_len = j - i;
			bool tdi_bit = (tdi[i >> 3] >> (i & 7)) & 1;

			xvc_pack_bits(tms, i, run_buf.data(), run_len);
			int ret = _jtag->writeTMS(run_buf.data(), run_len, false,
					tdi_bit);
			if (ret < 0 || static_cast<uint32_t>(ret) != run_len) {
				printError("generic_writeTMSTDI: writeTMS failed");
				return false;
			}

			i += run_len;
		}
	}

	/* Some cables (e.g. UsbBlaster) buffer writeTMS()/writeTDI() output
	 * internally and only push it over USB at specific points (buffer
	 * full, or the start of the next writeTDI()). Without an explicit
	 * flush here, whatever was queued by the last run of this XVC
	 * "shift" transaction could still be sitting unsent when we reply
	 * to Vivado -- leaving the real TAP state on the board out of sync
	 * with what Vivado believes it just set. Force it out now. */
	int ret = _jtag->flush();
	if (ret < 0) {
		printError("generic_writeTMSTDI: flush failed");
		return false;
	}

	return true;
}

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
