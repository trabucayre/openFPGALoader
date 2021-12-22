// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_ICE40_HPP_
#define SRC_ICE40_HPP_

#include <string>

#include "device.hpp"
#include "ftdispi.hpp"
#include "spiInterface.hpp"

class Ice40: public Device, SPIInterface {
	public:
		Ice40(FtdiSpi *spi, const std::string &filename,
			const std::string &file_type,
			uint16_t rst_pin, uint16_t done_pin,
			bool verify, int8_t verbose);
		~Ice40();

		void program(unsigned int offset, bool unprotect_flash) override;
		bool dumpFlash(uint32_t base_addr, uint32_t len);
		bool protect_flash(uint32_t len) override;
		bool unprotect_flash() override;
		/* not supported in SPI Active mode */
		int idCode() override {return 0;}
		void reset() override;

		int spi_put(uint8_t cmd, uint8_t *tx, uint8_t *rx,
				uint32_t len) {
			(void)cmd; (void)tx; (void)rx; (void)len;
			return 0;
		}
		int spi_put(uint8_t *tx, uint8_t *rx, uint32_t len) {
			(void)tx; (void)rx; (void)len;
			return 0;
		}
		int spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
				uint32_t timeout, bool verbose = false) {
			(void)cmd; (void)mask; (void)cond; (void)timeout; (void) verbose;
			return 0;
		}

	protected:
		/*!
		 * \brief prepare SPI flash access
		 */
		bool prepare_flash_access() override;
		/*!
		 * \brief end of SPI flash access
		 */
		bool post_flash_access() override;

	private:
		FtdiSpi *_spi;
		uint16_t _rst_pin;
		uint16_t _done_pin;
};

#endif  // SRC_ICE40_HPP_
