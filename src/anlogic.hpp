// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_ANLOGIC_HPP_
#define SRC_ANLOGIC_HPP_

#include <string>

#include "bitparser.hpp"
#include "device.hpp"
#include "jtag.hpp"
#include "spiInterface.hpp"
#include "svf_jtag.hpp"

class Anlogic: public Device, SPIInterface {
	public:
		Anlogic(Jtag *jtag, const std::string &filename,
			const std::string &file_type,
			Device::prog_type_t prg_type, bool verify, int8_t verbose);
		~Anlogic();

		void program(unsigned int offset = 0) override;
		int idCode() override;
		void reset() override;

		/* spi interface */
		int spi_put(uint8_t cmd, uint8_t *tx, uint8_t *rx,
			uint32_t len) override;
		int spi_put(uint8_t *tx, uint8_t *rx, uint32_t len) override;
		int spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
			uint32_t timeout, bool verbose=false) override;
	private:
		SVF_jtag _svf;
};

#endif  // SRC_ANLOGIC_HPP_
