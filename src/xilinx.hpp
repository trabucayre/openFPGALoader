// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef XILINX_HPP
#define XILINX_HPP

#include <string>

#include "configBitstreamParser.hpp"
#include "device.hpp"
#include "jtag.hpp"
#include "spiInterface.hpp"

class Xilinx: public Device, SPIInterface {
	public:
		Xilinx(Jtag *jtag, const std::string &filename,
				const std::string &file_type,
				Device::prog_type_t prg_type,
				const std::string &device_package,
				bool verify, int8_t verbose);
		~Xilinx();

		void program(unsigned int offset, bool unprotect_flash) override;
		void program_spi(ConfigBitstreamParser * bit, unsigned int offset,
				bool unprotect_flash);
		void program_mem(ConfigBitstreamParser *bitfile);
		bool dumpFlash(uint32_t base_addr, uint32_t len) override;

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

		int idCode() override;
		void reset() override;

		/* -------------- */
		/* xc95 managment */
		/* -------------- */

		/*!
		 * \brief enable ISC mode
		 */
		void flow_enable();
		/*!
		 * \brief disable ISC mode
		 */
		void flow_disable();
		/*!
		 * \brief erase internal flash
		 * \return false if something wrong
		 */
		bool flow_erase();
		/*!
		 * \brief program internal flash (enable ISC, erase flash,
		 *        program and disable ISC
		 * \return false if something wrong
		 */
		bool flow_program(JedParser *jed);

		/*!
		 * \brief fill a buffer with internal flash content
		 * \return buffer filled
		 */
		std::string flow_read();

		/* ------------------- */
		/* XCF JTAG Flash PROM */
		/* ------------------- */
		void xcf_flow_enable(uint8_t mode = 0x37);
		void xcf_flow_disable();
		bool xcf_flow_erase();
		bool xcf_program(ConfigBitstreamParser *bitfile);
		std::string xcf_read();

		/* -------------------- */
		/* XC2C (CoolRunner II) */
		/* -------------------- */
		/*!
		 * \brief configure instance using model name and idcode
		 * \param[in] idcode: targeted device idcode
		 */
		void xc2c_init(uint32_t idcode);
		/*!
		 * \brief reset device, force read configuration
		 */
		void xc2c_flow_reinit();
		/*!
		 * \brief erase full internal flash (optionnally verify)
		 * \return false if erase fails, true otherwise
		 */
		bool xc2c_flow_erase();
		/*!
		 * \brief read full internal flash
		 * \return flash configuration data
		 */
		std::string xc2c_flow_read();
		/*!
		 * \brief write program to the flash (erase before, optional read after)
		 * \param[in] jed: bitstream instance
		 * \return false when erase or verify fails
		 */
		bool xc2c_flow_program(JedParser *jed);

		/* spi interface */
		int spi_put(uint8_t cmd, uint8_t *tx, uint8_t *rx,
				uint32_t len) override;
		int spi_put(uint8_t *tx, uint8_t *rx, uint32_t len) override;
		int spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
				uint32_t timeout, bool verbose = false) override;

	protected:
		/*!
		 * \brief prepare SPI flash access (need to have bridge in RAM)
		 */
		virtual bool prepare_flash_access() override {return load_bridge();}
		/*!
		 * \brief end of SPI flash access
		 */
		virtual bool post_flash_access() override {reset(); return true;}

	private:
		/* list of xilinx family devices */
		enum xilinx_family_t {
			XC95_FAMILY     = 0,
			XC2C_FAMILY,
			SPARTAN3_FAMILY,
			SPARTAN6_FAMILY,
			SPARTAN7_FAMILY,
			ARTIX_FAMILY,
			KINTEX_FAMILY,
			ZYNQ_FAMILY,
			XCF_FAMILY,
			UNKNOWN_FAMILY  = 999
		};

		xilinx_family_t _fpga_family; /**< used to store current family */

		/*!
		 * \brief with xilinx devices SPI flash direct access is not possible
		 * 		so a bridge must be loaded in RAM to access flash
		 * 	\return false if missing device mode, true otherwise
		 */
		bool load_bridge();
		std::string _device_package;
		int _xc95_line_len; /**< xc95 only: number of col by flash line */
		uint16_t _cpld_nb_row; /**< number of flash rows */
		uint16_t _cpld_nb_col; /**< number of cols in a row */
		uint16_t _cpld_addr_size; /**< number of addr bits */
		char _cpld_base_name[7]; /**< cpld name (without package size) */
};

#endif
