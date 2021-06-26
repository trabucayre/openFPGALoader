// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_ICE40_HPP_
#define SRC_ICE40_HPP_

#include <string>

#include "device.hpp"
#include "ftdispi.hpp"

class Ice40: public Device {
	public:
		Ice40(FtdiSpi *spi, const std::string &filename,
			const std::string &file_type,
			uint16_t rst_pin, uint16_t done_pin,
			bool verify, int8_t verbose);
		~Ice40();

		void program(unsigned int offset = 0) override;
		bool dumpFlash(const std::string &filename,
			uint32_t base_addr, uint32_t len);
		/* not supported in SPI Active mode */
		int idCode() override {return 0;}
		void reset() override;

	private:
		FtdiSpi *_spi;
		uint16_t _rst_pin;
		uint16_t _done_pin;
};

#endif  // SRC_ICE40_HPP_
