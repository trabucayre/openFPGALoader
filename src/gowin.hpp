// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef GOWIN_HPP_
#define GOWIN_HPP_

#include <stdint.h>
#include <iostream>
#include <string>
#include <vector>

#include "device.hpp"
#include "fsparser.hpp"
#include "jtag.hpp"
#include "jedParser.hpp"

class Gowin: public Device {
	public:
		Gowin(Jtag *jtag, std::string filename, const std::string &file_type,
				Device::prog_type_t prg_type,
				bool verify, int8_t verbose);
		~Gowin();
		int idCode() override;
		void reset() override;
		void program(unsigned int offset) override;
		void programFlash();

	private:
		bool wr_rd(uint8_t cmd, uint8_t *tx, int tx_len,
				uint8_t *rx, int rx_len, bool verbose = false);
		bool EnableCfg();
		bool DisableCfg();
		bool pollFlag(uint32_t mask, uint32_t value);
		bool eraseSRAM();
		bool eraseFLASH();
		bool flashSRAM(uint8_t *data, int length);
		bool flashFLASH(uint8_t *data, int length);
		void displayReadReg(uint32_t dev);
		uint32_t readStatusReg();
		uint32_t readUserCode();
		FsParser *_fs;
		bool is_gw1n1;
};
#endif  // GOWIN_HPP_
