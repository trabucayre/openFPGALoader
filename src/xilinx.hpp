// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_XILINX_HPP_
#define SRC_XILINX_HPP_

#include <map>
#include <string>
#include <vector>

#include "configBitstreamParser.hpp"
#include "device.hpp"
#include "jedParser.hpp"
#include "jtag.hpp"
#include "spiInterface.hpp"

class Xilinx: public Device, SPIInterface {
	public:
		Xilinx(Jtag *jtag, const std::string &filename,
				const std::string &secondary_filename,
				const std::string &file_type,
				Device::prog_type_t prg_type,
				const std::string &device_package,
				const std::string &spiOverJtagPath,
				const std::string &target_flash,
				bool verify, int8_t verbose,
				bool skip_load_bridge, bool skip_reset,
				bool read_dna, bool read_xadc);
		~Xilinx();

		void program(unsigned int offset, bool unprotect_flash) override;
		void program_spi(ConfigBitstreamParser * bit, unsigned int offset,
				bool unprotect_flash);
		void program_mem(ConfigBitstreamParser *bitfile);
		bool dumpFlash(uint32_t base_addr, uint32_t len) override;

		bool read_register(const std::string reg_name) override {
			displayRegister(reg_name, dumpRegister(reg_name));
			return true;
		}
		/*!
		 * \brief display register content (bits/slices)
		 * \in reg_name: register name
		 * \in reg_val: register value
		 */
		void displayRegister(const std::string reg_name, const uint32_t reg_val);
		/*!
		 * \brief read register value from the devices
		 * \in reg_name: register name
		 * \return register value
		 */
		uint32_t dumpRegister(const std::string reg_name);

		/*!
		 * \brief display SPI flash ID and status register
		 */
		bool detect_flash() override;
		/*!
		 * \brief protect SPI flash blocks
		 */
		bool protect_flash(uint32_t len) override;
		/*!
		 * \brief unprotect SPI flash blocks
		 */
		bool unprotect_flash() override;
		/*!
		 * \brief configure Quad mode for SPI Flash
		 */
		bool set_quad_bit(bool set_quad) override;
		/*!
		 * \brief erase SPI flash blocks
		 */
		bool bulk_erase_flash() override;

		uint32_t idCode() override;
		void reset() override;

		/* -------------- */
		/* xc3s management */
		/* -------------- */

		/*!
		 * \brief load SRAM (enable ISC, load
		 *        and disable ISC
		 * \return false if something wrong
		 */
		bool xc3s_flow_program(ConfigBitstreamParser *bit);

		/* ------------------- */
		/* xc95/xc3s managment */
		/* ------------------- */

		/*!
		 * \brief enable ISC mode
		 */
		void flow_enable();
		/*!
		 * \brief disable ISC mode
		 */
		void flow_disable();

		/* -------------- */
		/* xc95 managment */
		/* -------------- */

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
		 * \brief erase full internal flash (optionally verify)
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
		int spi_put(uint8_t cmd, const uint8_t *tx, uint8_t *rx,
				uint32_t len) override;
		int spi_put(const uint8_t *tx, uint8_t *rx, uint32_t len) override;
		int spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
				uint32_t timeout, bool verbose = false) override;

		/* SpiOverJtag v2 specifics methods */
		int spi_put_v2(uint8_t cmd, const uint8_t *tx, uint8_t *rx,
				uint32_t len);

	protected:
		/*!
		 * \brief prepare SPI flash access (need to have bridge in RAM)
		 */
		bool prepare_flash_access() override;
		/*!
		 * \brief end of SPI flash access
		 */
		bool post_flash_access() override;

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
			KINTEXUS_FAMILY,
			KINTEXUSP_FAMILY,
			ZYNQ_FAMILY,
			ZYNQMP_FAMILY,
			XCF_FAMILY,
			ARTIXUSP_FAMILY,
			VIRTEXUS_FAMILY,
			VIRTEXUSP_FAMILY,
			UNKNOWN_FAMILY  = 999
		};

		xilinx_family_t _fpga_family; /**< used to store current family */

		/*!
		 * \brief xilinx ZynqMP Ultrascale+ specific initialization
		 * \param[in] family name
		 * \return true if device has been correctly initialized
		 */
		bool zynqmp_init(const std::string &family);

		/*!
		 * \brief with xilinx devices SPI flash direct access is not possible
		 * 		so a bridge must be loaded in RAM to access flash
		 * 	\return false if missing device mode, true otherwise
		 */
		bool load_bridge();

		/*!
		 * \brief read SpiOverJtag version to select between v1 and v2
		 * \return 2.0 for v2 or 1.0 for v1
		 */
		float get_spiOverJtag_version();

		enum xilinx_flash_chip_t {
			PRIMARY_FLASH = 0x1,
			SECONDARY_FLASH = 0x2
		};

		/* XADC */
		unsigned int xadc_read(uint16_t addr);
		void xadc_write(uint16_t addr, uint16_t data);
		unsigned int xadc_single(uint16_t ch);

		/* DNA */
		uint64_t fuse_dna_read(void);

		/*!
		 * \brief Starting from UltraScale, Xilinx devices can support dual
		 *        QSPI flash configuration, with two different flash chips
		 *        on the board. Target the selected one via the bridge by
		 *        chaging the USER instruction to use.
		 */
		void select_flash_chip(xilinx_flash_chip_t flash_chip);

		std::string _device_package;
		std::string _spiOverJtagPath; /**< spiOverJtag explicit path */
		int _xc95_line_len; /**< xc95 only: number of col by flash line */
		uint16_t _cpld_nb_row; /**< number of flash rows */
		uint16_t _cpld_nb_col; /**< number of cols in a row */
		uint16_t _cpld_addr_size; /**< number of addr bits */
		char _cpld_base_name[8]; /**< cpld name (without package size) */
		int _irlen; /**< IR bit length */
		std::map<std::string, std::vector<uint8_t>> _ircode_map; /**< bscan instructions based on model */
		std::string _secondary_filename; /* path to the secondary flash file (SPIx8) */
		std::string _secondary_file_extension; /* file type for the secondary flash file */
		int _flash_chips; /* bitfield to select the target in boards with two flash chips */
		std::string _user_instruction; /* which USER bscan instruction to interface with SPI */
		bool _soj_is_v2; /* SpiOverJtag version (1.0 or 2.0) */
		uint32_t _jtag_chain_len; /* Jtag Chain Length */
};

#endif  // SRC_XILINX_HPP_
