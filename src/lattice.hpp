// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_LATTICE_HPP_
#define SRC_LATTICE_HPP_

#include <stdint.h>
#include <iostream>
#include <string>
#include <vector>

#include "jtag.hpp"
#include "device.hpp"
#include "jedParser.hpp"
#include "feaparser.hpp"
#include "latticeBitParser.hpp"
#include "spiInterface.hpp"

class Lattice: public Device, SPIInterface {
	public:
		Lattice(Jtag *jtag, std::string filename, const std::string &file_type,
			Device::prog_type_t prg_type, std::string flash_sector, bool verify,
			int8_t verbose);
		int idCode() override;
		int userCode();
		void reset() override {}
		void program(unsigned int offset, bool unprotect_flash) override;
		bool program_mem();
		bool program_flash(unsigned int offset, bool unprotect_flash);
		bool Verify(std::vector<std::string> data, bool unlock = false,
				uint32_t flash_area = 0);
		bool dumpFlash(uint32_t base_addr, uint32_t len) override {
			return SPIInterface::dump(base_addr, len);
		}

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

		/* spi interface */
		int spi_put(uint8_t cmd, uint8_t *tx, uint8_t *rx,
		uint32_t len) override;
		int spi_put(uint8_t *tx, uint8_t *rx, uint32_t len) override;
		int spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
				uint32_t timeout, bool verbose = false) override;

	private:
		enum lattice_family_t {
			MACHXO2_FAMILY = 0,
			MACHXO3_FAMILY = 1,
			MACHXO3D_FAMILY = 2,
			ECP5_FAMILY = 3,
			NEXUS_FAMILY = 4,
			UNKNOWN_FAMILY = 999
		};

		lattice_family_t _fpga_family;

		bool program_intFlash(JedParser& _jed);
		bool program_extFlash(unsigned int offset, bool unprotect_flash);
		bool wr_rd(uint8_t cmd, uint8_t *tx, int tx_len,
				uint8_t *rx, int rx_len, bool verbose = false);
		/*!
		 * \brief move device to SPI access
		 */
		bool prepare_flash_access() override;
		/*!
		 * \brief end of device to SPI access
		 *        reload btistream from flash
		 */
		bool post_flash_access() override;
		/*!
		 * \brief erase SRAM
		 */
		bool clearSRAM();
		void unlock();
		bool EnableISC(uint8_t flash_mode);
		bool DisableISC();
		bool EnableCfgIf();
		bool DisableCfg();
		bool pollBusyFlag(bool verbose = false);
		bool flashEraseAll();
		bool flashErase(uint32_t mask);
		bool flashProg(uint32_t start_addr, const std::string &name,
				std::vector<std::string> data);
		bool checkStatus(uint32_t val, uint32_t mask);
		void displayReadReg(uint32_t dev);
		uint32_t readStatusReg();
		uint64_t readFeaturesRow();
		bool writeFeaturesRow(uint64_t features, bool verify);
		uint16_t readFeabits();
		bool writeFeabits(uint16_t feabits, bool verify);
		bool writeProgramDone();
		bool loadConfiguration();

		/* test */
		bool checkID();

		/*********************** MODS FOR MacXO3D *****************************/
		enum lattice_flash_sector_t {
			LATTICE_FLASH_UNDEFINED = 0,
			LATTICE_FLASH_CFG0,
			LATTICE_FLASH_CFG1,
			LATTICE_FLASH_UFM0,
			LATTICE_FLASH_UFM1,
			LATTICE_FLASH_UFM2,
			LATTICE_FLASH_UFM3,
			LATTICE_FLASH_FEA,
			LATTICE_FLASH_PKEY,
			LATTICE_FLASH_AKEY,
			LATTICE_FLASH_CSEC,
			LATTICE_FLASH_USEC
		};

		lattice_flash_sector_t _flash_sector;
		bool programFeatureRow_MachXO3D(uint8_t* feature_row);
		bool programFeabits_MachXO3D(uint32_t feabits);
		bool programPubKey_MachXO3D(uint8_t* pubkey);

		bool program_intFlash_MachXO3D(JedParser& _jed);
		bool program_fea_MachXO3D();
		bool program_pubkey_MachXO3D();
};
#endif  // SRC_LATTICE_HPP_
