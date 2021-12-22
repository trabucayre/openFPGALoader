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

		void program(unsigned int offset, bool unprotect_flash) override;
		int idCode() override;
		void reset() override;

		/* spi interface */
		/*!
		 * \brief protect SPI flash blocks
		 */
		bool protect_flash(uint32_t len) override {
			return SPIInterface::protect_flash(len);
		}

		/*!
		 * \brief protect SPI flash blocks
		 */
		bool unprotect_flash() override {
			return SPIInterface::unprotect_flash();
		}

		/*!
		 * \brief dump len byte from base_addr from SPI flash
		 * \param[in] base_addr: start offset
		 * \param[in] len: dump len
		 * \return false if something wrong
		 */
		virtual bool dumpFlash(uint32_t base_addr, uint32_t len) override {
			return SPIInterface::dump(base_addr, len);
		}

		int spi_put(uint8_t cmd, uint8_t *tx, uint8_t *rx,
			uint32_t len) override;
		int spi_put(uint8_t *tx, uint8_t *rx, uint32_t len) override;
		int spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
			uint32_t timeout, bool verbose=false) override;

	protected:
		/*!
		 * \brief move device to SPI access
		 */
		virtual bool prepare_flash_access() override;
		/*!
		 * \brief end of device to SPI access
		 */
		virtual bool post_flash_access() override {reset(); return true;}

	private:
		SVF_jtag _svf;
};

#endif  // SRC_ANLOGIC_HPP_
