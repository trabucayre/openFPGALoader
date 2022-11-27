// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_ICEVWIRELESS_HPP_
#define SRC_ICEVWIRELESS_HPP_

#include <stdint.h>

#include <string>
#include <vector>

#include "device.hpp"
#include "uart_ll.hpp"

/*!
 * \file iceVWireless
 * \class IceV_Wireless
 * \brief iceV Wireless protocol implementation
 * \author Gwenhael Goavec-Merou
 */
class IceV_Wireless {
	public:
		/*!
		 * \brief constructor
		 * \param[in] device: /dev/xxx
		 * \param[in] filename: bitstream file name
		 * \param[in] prg_type: write / load
		 */
		IceV_Wireless(const std::string &device,
			const std::string &filename, Device::prog_type_t prg_type);

		~IceV_Wireless();

		/* iceV Wireless commands */
		enum cmd_lst {
			READ_REG    =  0,
			WRITE_REG   =  1,
			READ_VBAT   =  2,
			SEND_CRED   =  3,
			/* 4: SEND_CRED password */
			READ_INFO   =  5,
			LOAD_CFG    =  6,
			PSRAM_INIT  = 10,
			PSRAM_READ  = 11,
			PSRAM_WRITE = 12,
			PRG_SPIFFS  = 14,
			PRG_RAM     = 15
		};

		/*!
		 * \brief read and display ESP32-C3 ADC (battery voltage)
		 * \return false is something is wrong
		 */
		bool read_vbat();

		/*!
		 * \brief read and display firmware version and IP address
		 * \return false is something is wrong
		 */
		bool read_info();

		/*!
		 * \brief read the specified register
		 * \param[in] reg: register address
		 * \return false is something is wrong
		 */
		bool read_reg(uint32_t reg);

		/*!
		 * \brief write the specified register
		 * \param[in] reg: register address
		 * \param[in] data: value to write
		 * \return false is something is wrong
		 */
		bool write_reg(uint32_t reg, uint32_t data);

		/*!
		 * \brief write wireless SSID or pass
		 * \param[in] reg: 0 -> SSID, 1 -> password
		 * \param[in] value: string to send
		 * \return false if something went wrong
		 */
		bool send_cred(uint8_t reg, std::string value);

		/*!
		 * \brief send content of specified file to SPIFFS or ice40 RAM
		 * \param[in] cmd: destination code (14 -> SPIFFS, 15 -> RAM)
		 * \param[in] filename: bitstream path
		 * \return false if something went wrong
		 */
		bool send_file(uint8_t cmd, const std::string &filename);
		bool load_cfg(uint32_t reg);

		/*!
		 * \brief send bitstream to the device (SPIFFS or RAM)
		 */
		void program() {send_file(_mode, _filename);}

	private:
		/*!
		 * \brief write command + payload, followed by a read_tokens call
		 * \param[in] cmd: command
		 * \param[in] reg: payload (uint32_t)
		 * \param[in] regsize: payload len
		 * \param[out] rx: iceV Wireless answer (vector)
		 * \return error/status code
		 */
		int wr_rd(uint8_t cmd, uint32_t reg, uint32_t regsize,
				std::vector<std::string> *rx);

		/*!
		 * \brief write command + payload, followed by a read_data call
		 * \param[in] cmd: command
		 * \param[in] reg: payload (uint32_t)
		 * \param[in] regsize: payload len
		 * \param[out] rx: iceV Wireless answer (scalar)
		 * \return error/status code
		 */
		int wr_rd(uint8_t cmd, uint32_t reg, uint32_t regsize,
				std::string *rx);

		/*!
		 * \brief write command + payload, followed by a read_data call
		 * \param[in] cmd: command
		 * \param[in] reg: payload (string)
		 * \param[in] regsize: payload len
		 * \param[out] rx: iceV Wireless answer (scalar)
		 * \return error/status code
		 */
		int wr_rd(uint8_t cmd, const std::string &reg, size_t regsize,
			std::string *rx);

		/*!
		 * \brief build and write a sequence (MAGIC + size + payload)
		 * \param[in] cmd: command
		 * \param[in] reg: payload (char *)
		 * \param[in] regsize: payload len
		 * \return false if issue with UART, true otherwise
		 */
		bool write_cmd(uint8_t cmd, const char *reg, uint32_t regsize);

		/*!
		 * \brief build and write a sequence (MAGIC + size + payload)
		 * \param[in] cmd: command
		 * \param[in] reg: payload (uint32_t)
		 * \param[in] regsize: payload len
		 * \return false if issue with UART, true otherwise
		 */
		bool write_cmd(uint8_t cmd, uint32_t reg, size_t regsize);

		/*!
		 * \brief read full ice4VWireless message, split sequence
		 *        returns error and fill vector<string> with answer
		 * \param[out] rx: ice4VWireless splitted answer
		 * \return status code
		 */
		int read_tokens(std::vector<std::string> *rx);

		/*!
		 * \brief similar to read_tokens but fill string with first
		 *        answer part
		 * \param[out] rx: ice4VWireless answer
		 * \return status code
		 */
		int read_data(std::string *rx);

		Uart_ll uart;          /*! lowlevel uart access */
		std::string _filename; /*! bitstream name       */
		cmd_lst _mode;         /*! SPIFFS or RAM        */
};

#endif  // SRC_ICEVWIRELESS_HPP_
