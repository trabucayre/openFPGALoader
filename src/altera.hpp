// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_ALTERA_HPP_
#define SRC_ALTERA_HPP_

#include <string>

#include "device.hpp"
#include "jtag.hpp"
#include "rawParser.hpp"
#include "spiInterface.hpp"
#include "svf_jtag.hpp"

class Altera: public Device, SPIInterface {
	public:
		Altera(Jtag *jtag, const std::string &filename,
				const std::string &file_type,
				Device::prog_type_t prg_type,
				const std::string &device_package,
				bool verify, int8_t verbose);
		~Altera();

		void programMem(RawParser &_bit);
		void program(unsigned int offset, bool unprotect_flash) override;
		/*!
		 * \brief read len Byte starting at base_addr and store
		 *        into filename
		 * \param[in] filename: file name
		 * \param[in] base_addr: starting address in flash memory
		 * \param[in] len: length (in Byte)
		 * \return false if read fails or filename can't be open, true otherwise
		 */
		bool dumpFlash(uint32_t base_addr, uint32_t len) override {
			return SPIInterface::dump(base_addr, len);
		}

		int idCode() override;
		void reset() override;

		/*************************/
		/*     spi interface     */
		/*************************/

		/*!
		 * \brief protect SPI flash blocks
		 */
		bool protect_flash(uint32_t len) override {
			return SPIInterface::protect_flash(len);
		}
		/*!
		 * \brief unprotect SPI flash blocks
		 */
		bool unprotect_flash() override {
			return SPIInterface::unprotect_flash();
		}

		int spi_put(uint8_t cmd, uint8_t *tx, uint8_t *rx,
				uint32_t len) override;
		int spi_put(uint8_t *tx, uint8_t *rx, uint32_t len) override;
		int spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
				uint32_t timeout, bool verbose = false) override;

	protected:
		bool prepare_flash_access() override {return load_bridge();}
		bool post_flash_access() override {reset(); return true;}

	private:
		/*!
		 * \brief with intel devices SPI flash direct access is not possible
		 * 		so a bridge must be loaded in RAM to access flash
		 * 	\return false if missing device mode, true otherwise
		 */
		bool load_bridge();
		/* virtual JTAG access */
		/*!
		 * \brief virtual IR: send USER0 IR followed, in DR, by
		 *         address (_vir_addr) in a burst of _vir_length
		 * \param[in] reg: data to send in shiftDR mode with addr
		 */
		void shiftVIR(uint32_t reg);
		/*!
		 * \brief virtual IR: send USER1 IR followed by
		 *         data in DR with an optional read
		 * \param[in] tx: data to send in shiftDR mode
		 * \param[in] rx: data to read in shiftDR mode
		 * \param[in] len: len of tx & rx
		 * \param[in] end_state: next state at the end of xfer
		 */
		void shiftVDR(uint8_t * tx, uint8_t * rx, uint32_t len,
				int end_state = Jtag::UPDATE_DR, bool debug = false);

		SVF_jtag _svf;
		std::string _device_package;
		uint32_t _vir_addr; /**< addr affected to virtual jtag */
		uint32_t _vir_length; /**< length of virtual jtag IR */
};

#endif  // SRC_ALTERA_HPP_
