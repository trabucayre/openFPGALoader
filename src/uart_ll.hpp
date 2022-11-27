// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_UART_LL_HPP_
#define SRC_UART_LL_HPP_

#include <stdint.h>
#include <termios.h>

#include <string>

/*!
 * \file uart_ll
 * \class Uart_ll
 * \brief low level implementation for UART protocol
 * \author Gwenhael Goavec-Merou
 */
class Uart_ll {
	public:
		/*!
		 * \brief constructor
		 * \param[in] filename: /dev/ttyxx path
		 * \param[in] clkHz: baudrate (in Hz)
		 * \param[in] byteSize: transaction size (5 to 8)
		 * \param[in] stopBits: 1 or 2 stop bit
		 * \param[in] firmware_path: firmware to load
		 */

		Uart_ll(const std::string &filename, uint32_t clkHz,
			uint8_t byteSize, bool stopBits);
		~Uart_ll();

		int setClkFreq(uint32_t clkHz);
		uint32_t getClkFreq();

		/*!
		 * \brief send buffer content
		 * \param[in] data: buffer
		 * \param[in] size: number of char to send
		 * \return size if ok, < 0 otherwise
		 */
		int write(const unsigned char *data, int size);
		/*!
		 * \brief read maxsize char from device
		 * \param[in] buf: buffer to fill
		 * \param[in] maxsize: number of char to read
		 * \return read size if ok, < 0 otherwise
		 */
		int read(std::string *buf, int maxsize);
		/*!
		 * \brief read char from device until receiving end char
		 * \param[in] buf: buffer to fill
		 * \param[in] end: number of char to read
		 * \return read size if ok, < 0 otherwise
		 */
		int read_until(std::string *buf, uint8_t end = '\n');

		/*!
		 * \brief flush by reading maxsize char from device
		 * \param[in] maxsize: number of char to read
		 * \return true if ok, false otherwise
		 */
		bool flush(int maxsize=64);
	private:
		/*!
		 * \brief serial port configuration
		 * \return -1 if something fails, 0 otherwise
		 */
		int port_configure(void);
		/*!
		 * \brief convert a frequency to the baudrate (BXXX)
		 * \param[in] clkHz: clock frequency
		 * \return the corresponding baudrate value
		 */
		speed_t freq_to_baud(uint32_t clkHz);
		/*!
		 * \brief convert the baudrate (BXXX) to the corresponding frequency
		 * \param[in] baud: a speed_t (BXXX) value
		 * \return the corresponding frequency (Hz)
		 */
		uint32_t baud_to_freq(speed_t baud);

		struct termios _prev_termios; /*! original serial port configuration */
		struct termios _curr_termios; /*! current serial port configuration  */
		speed_t _baudrate;            /*! current baudrate                   */
		uint32_t _clkHz;              /*! current clk frequency              */
		int _serial;                  /*! device file descriptor             */
		uint32_t _byteSize;           /*! transfer size                      */
		bool _stopBits;
};
#endif  // SRC_UART_LL_HPP_
