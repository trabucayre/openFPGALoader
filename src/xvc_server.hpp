// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 * based on:
 * Taisen proposition
 * https://github.com/tom01h/xvc-pico
 * https://github.com/Xilinx/XilinxVirtualCable
 */

#ifndef SRC_XVC_SERVER_HPP_
#define SRC_XVC_SERVER_HPP_

#include <netinet/in.h>

#include <string>
#include <thread>

#include "jtag.hpp"

using namespace std;

class XVC_server {
	public:
		XVC_server(int port, const cable_t &cable, const jtag_pins_conf_t *pin_conf,
			string dev, const string &serial, uint32_t clkHZ, int8_t verbose,
			const string &ip_adr,
			const bool invert_read_edge, const string &firmware_path);
		~XVC_server();

		/*!
		 * \brief open a server socket
		 * \return true when success, false otherwise
		 */
		bool open_connection();
		/*!
		 * \brief close server socket
		 * \return true when success, false otherwise
		 */
		bool close_connection();
		/*!
		 * \brief start server loop
		 * \return true when success, false otherwise
		 */
		bool listen_loop();

		/*!
		 * \brief update jtag virtual state by following the tms sequence
		 * \param tms_seq: tms sequence lsb first
		 * \param len: number of tms bit to apply
		 * \return jtag state after applying sequence
		 */
		Jtag::tapState_t set_state(const uint8_t *tms_seq, uint32_t len);
		/*!
		 * \brief return actual jtag state
		 * \return jtag state
		 */
		Jtag::tapState_t get_state() { return _state;}

	private:
		/*!
		 * \brief thread dedicated for wait for incoming connection
		 */
		void thread_listen();
		/*!
		 * \brief parser and dispatcher for XVC transactions
		 * \return 2 when connection is closed, 1 when transactions fails,
		 *         0 otherwise
		 */
		int handle_data(int fd);
		/*!
		 * \brief read len bytes from client
		 * \param fd: socket descriptor
		 * \param target: buffer
		 * \param len: number of bytes to read
		 * \return 3 when failure, 2 when connection closed, 1 otherwise
		 */
		int sread(int fd, void *target, int len);

	    int _verbose;          /*!< verbose level */
		JtagInterface *_jtag;  /*!< jtag interface */
		int _port;             /*!< network port */
		int _sock;             /*!< server socket descriptor */
		struct sockaddr_in _sock_addr;
		std::thread *_thread;  /*!< connection thread */
		bool _is_stopped;      /*!< true when thread is stopped */
		bool _must_stop;       /*!< true to stop thread */
		uint32_t _buffer_size; /*!< buffer max capacity TDI+TMS */
		uint8_t *_tmstdi;      /*!< TDI/TMS from client */
		uint8_t *_result;      /*!< buffer for server -> client */
		Jtag::tapState_t _state; /*!< actual jtag state */
};

#endif  // SRC_XVC_SERVER_HPP_
