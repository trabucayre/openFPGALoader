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

		void program(unsigned int offset = 0) override;
		void program_spi(ConfigBitstreamParser * bit, unsigned int offset = 0);
		void program_mem(ConfigBitstreamParser *bitfile);
		bool dumpFlash(const std::string &filename,
			uint32_t base_addr, uint32_t len) override;
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
		bool flow_program();

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

		/* spi interface */
		int spi_put(uint8_t cmd, uint8_t *tx, uint8_t *rx,
				uint32_t len) override;
		int spi_put(uint8_t *tx, uint8_t *rx, uint32_t len) override;
		int spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
				uint32_t timeout, bool verbose = false) override;

	private:
		/* list of xilinx family devices */
		enum xilinx_family_t {
			XC95_FAMILY     = 0,
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
};

#endif
