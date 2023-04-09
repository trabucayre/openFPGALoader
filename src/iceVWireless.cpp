// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include "iceVWireless.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <regex>

#include "display.hpp"
#include "uart_ll.hpp"
#include "rawParser.hpp"

IceV_Wireless::IceV_Wireless(const std::string &device,
			const std::string &filename, Device::prog_type_t prg_type)
	:uart(device, 9600, 8, false), _filename(filename)
{
	_mode = (prg_type == Device::WR_SRAM) ? PRG_RAM : PRG_SPIFFS;
	if (!uart.flush())
		throw std::runtime_error("Flush serial interface failed");
	if (!read_vbat())
		throw std::runtime_error("Fail to read vbat");
	if (!read_info())
		throw std::runtime_error("Fail to read info");
}

IceV_Wireless::~IceV_Wireless() {}

bool IceV_Wireless::write_cmd(uint8_t cmd,
		uint32_t reg, size_t regsize)
{
	char payload[4] = {
		static_cast<char>((reg >>  0) & 0xFF),
		static_cast<char>((reg >>  8) & 0xFF),
		static_cast<char>((reg >> 16) & 0xFF),
		static_cast<char>((reg >> 24) & 0xFF)};
	return write_cmd(cmd, payload, regsize);
}

bool IceV_Wireless::write_cmd(uint8_t cmd,
		const char *reg, uint32_t regsize)
{
	int kBuffSize = 4 + 4 + regsize;
	uint8_t buffer[kBuffSize];
	int pos = 0;
	memset(buffer, '\0', kBuffSize);
	/* cmd */
	buffer[pos++] = 0xE0 + (cmd & 0x0F);
	buffer[pos++] = 0xBE;
	buffer[pos++] = 0xFE;
	buffer[pos++] = 0xCA;
	/* register size */
	buffer[pos++] = (regsize >>  0) & 0xFF;
	buffer[pos++] = (regsize >>  8) & 0xFF;
	buffer[pos++] = (regsize >> 16) & 0xFF;
	buffer[pos++] = (regsize >> 24) & 0xFF;
	/* register/payload */
	memcpy(buffer+pos, reg, regsize);
	/* write buffer */
	if (uart.write(buffer, kBuffSize) != kBuffSize) {
		printError("Error: uart write failed");
		return false;
	}
	return true;
}

int IceV_Wireless::read_tokens(std::vector<std::string> *rx)
{
	std::string payload;
	/* read message from ESP32-C3 */
	int ret = uart.read_until(&payload, '\n');
	if (ret < 0)
		return 32;

	std::regex regex{R"([\s]+)"};  // split on space
	std::sregex_token_iterator it{payload.begin(), payload.end(), regex, -1};
	std::vector<std::string> words{it, {}};

	/* decode / check */
	uint16_t errorCode;
	bool found = false;
	for (size_t i = 0; i < words.size(); i++){
		if (words[i] == "RX") {
			if (i + 2 <= words.size()) {
				found = true;
				errorCode = stoul(words[i+1], nullptr, 16);
				rx->resize(words.size() - i - 2);
				std::copy(words.begin()+i+2, words.end(), rx->begin());
				break;
			}
		}
	}
	if (!found)
		return 64;
	return errorCode;
}

int IceV_Wireless::read_data(std::string *rx)
{
	std::vector<std::string> rxv;
	int errorCode = read_tokens(&rxv);
	if (errorCode != 0)
		return errorCode;

	rx->resize(rxv[0].size());
	std::copy(rxv[0].begin(), rxv[0].end(),
		rx->begin());
	return errorCode;
}

int IceV_Wireless::wr_rd(uint8_t cmd, uint32_t reg, uint32_t regsize,
		std::vector<std::string> *rx)
{
	write_cmd(cmd, reg, regsize);
	return read_tokens(rx);
}

int IceV_Wireless::wr_rd(uint8_t cmd, uint32_t reg, uint32_t regsize,
		std::string *rx)
{
	write_cmd(cmd, reg, regsize);
	return read_data(rx);
}

int IceV_Wireless::wr_rd(uint8_t cmd, const std::string &reg,
		size_t regsize, std::string *rx)
{
	write_cmd(cmd, reg.c_str(), regsize);
	return read_data(rx);
}

bool IceV_Wireless::read_vbat()
{
	std::string rx;
	uint32_t val = wr_rd(READ_VBAT, 0, 4, &rx);
	if (val != 0)
		return false;
	val = stoul(rx, nullptr, 16);
	printInfo("Vbat = " + std::to_string(val) + " mV");
	return true;
}

bool IceV_Wireless::read_info()
{
	std::vector<std::string> rx;
	uint32_t val = wr_rd(READ_INFO, 0, 4, &rx);
	if (val != 0)
		return false;
	printInfo("info: version: " + rx[0] + " ipaddr: " +
		rx[1]);

	return true;
}

// send a file for direct load to FPGA or write to SPIFFS
bool IceV_Wireless::send_file(uint8_t cmd, const std::string &filename)
{
	std::string rx;
	std::string tx;
	// open file

	printInfo("Open file " + filename + " ", false);
	RawParser _bit(filename, false);
	printSuccess("DONE");

	printInfo("Parse file ", false);
	if (_bit.parse() == EXIT_FAILURE) {
		printError("FAIL");
		throw std::runtime_error("Failed to parse bitstream");
	}
	printSuccess("DONE");
	uint32_t size = _bit.getLength() / 8;
	uint8_t *data = _bit.getData();
	tx.append(reinterpret_cast<char *>(data), size);

	// send to the C3 over usb

	uint32_t ret = wr_rd(cmd, tx, size, &rx);
	if (ret != 0) {
		printError("FAIL");
		throw std::runtime_error("Error " + std::to_string(ret));
	}
	printSuccess("DONE");

	return (ret == 0) ? true : false;
}

bool IceV_Wireless::read_reg(uint32_t reg)
{
	std::string rx;
	uint32_t val = wr_rd(READ_REG, reg, 4, &rx);
	if (val != 0)
		return false;
	printf("Read reg %u = %4x\n", reg, val);
	return true;
}

/* TODO: write reg */
bool IceV_Wireless::write_reg(uint32_t reg, uint32_t data)
{
	std::string rx;
	std::string tx;
	for (int i = 0; i < 4; i++)
		tx.append(1, static_cast<uint8_t>(reg >> (i * 8)));
	for (int i = 0; i < 4; i++)
		tx.append(i, static_cast<uint8_t>(data >> (i * 8)));
	uint32_t val = wr_rd(WRITE_REG, tx, 8, &rx);
	return (val == 0) ? true : false;
}

/* TODO: psram write */
/* TODO: psram read */
/* TODO: psram init */
/* TODO: send cred */
bool IceV_Wireless::send_cred(uint8_t cred_type, std::string value)
{
	std::string rep;
	value+='\0';
	/* 3 -> SSID
	 * 4 -> PASS
	 */
	uint32_t val = wr_rd((uint8_t)(SEND_CRED + (cred_type & 0x01)),
		value, static_cast<size_t>(value.size()), &rep);
	return val == 0;
}

// load config from SPIFFS (0: default, 1: spi pass)
bool IceV_Wireless::load_cfg(uint32_t reg)
{
	std::string rep;
	uint32_t val = wr_rd(LOAD_CFG, reg, 4, &rep);
	return val == 0;
}
