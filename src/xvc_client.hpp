// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_XVC_CLIENT_HPP_
#define SRC_XVC_CLIENT_HPP_

#include <string>

#include "jtagInterface.hpp"

/*!
 * \brief Xilinx Virtual Server client side probe driver
 */
class XVC_client: public JtagInterface {
	public:
		/*!
		 * \brief constructor: open device 
		 * \param[in] ip_addr: server IP addr
		 * \param[in] clkHz: output clock frequency
		 * \param[in] verbose: verbose level -1 quiet, 0 normal,
		 * 								1 verbose, 2 debug
		 */
		XVC_client(const std::string &ip_addr, int port, uint32_t clkHz,
				int8_t verbose);

		~XVC_client();

		// jtagInterface requirement
		/*!
		 * \brief configure probe clk frequency
		 * \param[in] clkHZ: frequency in Hertz
		 * \return <= 0 if something wrong, clkHZ otherwise
		 */
		int setClkFreq(uint32_t clkHZ) override;

		/*!
		 * \brief store a len tms bits in a buffer. send is only done if
		 *   flush_buffer
		 * \param[in] tms: serie of tms state
		 * \param[in] len: number of tms bits
		 * \param[in] flush_buffer: force buffer to be send or not
		 * \return <= 0 if something wrong, len otherwise
		 */
		int writeTMS(uint8_t *tms, uint32_t len, bool flush_buffer) override;

		/*!
		 * \brief write and read len bits with optional tms set to 1 if end
		 * \param[in] tx: serie of tdi state to send 
		 * \param[out] rx: buffer to store tdo bits from device
		 * \param[in] len: number of bit to read/write
		 * \param[in] end: if true tms is set to one with the last tdi bit
		 * \return <= 0 if something wrong, len otherwise
		 */
		int writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;

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
		int get_buffer_size() override { return 2048;}
		bool isFull() override { return false;}

		std::string server_name() {return _server_name;}
		std::string server_version() {return _server_vers;}

	private:
		/*!
		 * \brief create a TCP socket and connect to
		 *        server at ip_addr
		 * \param[in] ip_addr: server IP
		 * \return false if socket creation or connection fails
		 */
		bool open_connection(const std::string &ip_addr);

		/*!
		 * \brief sent instruction followed by tx buffer and read answer
		 * \param[in] instr: ascii instruction
		 * \param[in] tx: data buffer to send
		 * \param[in] tx_size: tx size (in Byte)
		 * \param[in] rx: server answer buffer (may be NULL)
		 * \param[in] rx_size: rx size (in Byte) or (0 when unused)
		 * \return -1 when error, 0 when disconnected or tx_size/rx_size
		 */
		ssize_t xfer_pkt(const std::string &instr,
				const uint8_t *tx, uint32_t tx_size,
				uint8_t *rx, uint32_t rx_size);

		/*!
		 * \brief lowlevel write: EMU_CMD_HW_JTAGx implementation
		 * \param[out]: tdo: TDO read buffer (may be null)
		 * \return false when failure
		 */
		bool ll_write(uint8_t *tdo);

		bool _verbose; /*!< display informations */

		uint8_t *_xfer_buf; /*!> tx buffer */
		uint8_t *_tms;      /*!< TMS internal buffer */
		uint8_t *_tditdo;   /*!< TDI/TDO internal buffer */
		uint32_t _num_bits; /*!< number of bits stored */
		uint32_t _last_tms; /*!< last known TMS state */
		uint32_t _last_tdi; /*!< last known TDI state */

		uint32_t _buffer_size;
		std::string _server_name;
		std::string _server_vers;
		int _sock;
		int _port;                /*!< target port */
};
#endif  // SRC_XVC_CLIENT_HPP_
