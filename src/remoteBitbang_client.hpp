// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2023 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_REMOTEBITBANG_CLIENT_HPP_
#define SRC_REMOTEBITBANG_CLIENT_HPP_

#include <string>

#include "jtagInterface.hpp"

/*!
 * \brief Remote Bitbang Protocol client side driver
 */
class RemoteBitbang_client: public JtagInterface {
	public:
		/*!
		 * \brief constructor: open device 
		 * \param[in] ip_addr: server IP addr
		 * \param[in] port   : server port
		 * \param[in] verbose: verbose level -1 quiet, 0 normal,
		 * 								1 verbose, 2 debug
		 */
		RemoteBitbang_client(const std::string &ip_addr, int port,
				int8_t verbose);

		~RemoteBitbang_client();

		// jtagInterface requirement
		/*!
		 * \brief configure probe clk frequency
		 * \param[in] clkHZ: frequency in Hertz
		 * \return <= 0 if something wrong, clkHZ otherwise
		 */
		int setClkFreq(uint32_t clkHz) override;

		/*!
		 * \brief store a len tms bits in a buffer. send is only done if
		 *   flush_buffer
		 * \param[in] tms: serie of tms state
		 * \param[in] len: number of tms bits
		 * \param[in] flush_buffer: force buffer to be send or not
		 * \return <= 0 if something wrong, len otherwise
		 */
		int writeTMS(const uint8_t *tms, uint32_t len, bool flush_buffer) override;

		/*!
		 * \brief write and read len bits with optional tms set to 1 if end
		 * \param[in] tx: serie of tdi state to send 
		 * \param[out] rx: buffer to store tdo bits from device
		 * \param[in] len: number of bit to read/write
		 * \param[in] end: if true tms is set to one with the last tdi bit
		 * \return <= 0 if something wrong, len otherwise
		 */
		int writeTDI(const uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;

		/*!
		 * \brief send a serie of clock cycle with constant TMS and TDI
		 * \param[in] tms: tms state
		 * \param[in] tdi: tdi state
		 * \param[in] clk_len: number of clock cycle
		 * \return <= 0 if something wrong, clk_len otherwise
		 */
		int toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len) override;

		/*!
		 * \brief flush internal buffer
		 * \return <=0 if something fail, > 0 otherwise
		 */
		int flush() override;

		/*
		 * unused
		 */
		int get_buffer_size() override { return _buffer_size;}
		bool isFull() override { return _buffer_size == _num_bytes;}

	private:
		/*!
		 * \brief create a TCP socket and connect to
		 *        server at ip_addr
		 * \param[in] ip_addr: server IP
		 * \return false if socket creation or connection fails
		 */
		bool open_connection(const std::string &ip_addr);

		/*!
		 * \brief sent one instruction (ASCII format) and read when requested
		 * \param[in] instr: ascii instruction
		 * \param[in] rx: server answer buffer (may be NULL)
		 * \return -1 when error, 0 when disconnected or tx_size/rx_size
		 */
		ssize_t xfer_pkt(uint8_t instr, uint8_t *rx);

		/*!
		 * \brief lowlevel write: write internal buffer (ASCII format)
		 *        and read one char when tdo is not NULL.
		 * \param[out]: tdo: TDO read pointer (may be null)
		 * \return false when failure
		 */
		bool ll_write(uint8_t *tdo);

		uint8_t *_xfer_buf;    /*!< tx buffer */
		uint32_t _num_bytes;   /*!< number of bits stored */
		uint32_t _last_tms;    /*!< last known TMS state */
		uint32_t _last_tdi;    /*!< last known TDI state */

		uint32_t _buffer_size; /*!< buffer max capacity */
		int _sock;             /*!< socket */
		int _port;             /*!< target port */
};
#endif  // SRC_REMOTEBITBANG_CLIENT_HPP_
