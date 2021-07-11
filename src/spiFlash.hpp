// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SPIFLASH_HPP
#define SPIFLASH_HPP

#include "spiInterface.hpp"

class SPIFlash {
	public:
		SPIFlash(SPIInterface *spi, int8_t verbose);
		/* power */
		virtual void power_up();
		virtual void power_down();
		void reset();
		/* protection */
		int write_enable();
		int write_disable();
		int disable_protection();
		/*!
		 * \brief unlock all sectors: specific to
		 * Microchip SST26VF032B / SST26VF032BA
		 * \return false if unlock fail
		 */
		bool global_unlock();
		/* erase */
		int bulk_erase();
		int sector_erase(int addr);
		int sectors_erase(int base_addr, int len);
		/* write */
		int write_page(int addr, uint8_t *data, int len);
		/* read */
		int read(int base_addr, uint8_t *data, int len);
		/*!
		 * \brief read len Byte starting at base_addr and store
		 *        into filename
		 * \param[in] filename: file name
		 * \param[in] base_addr: starting address in flash memory
		 * \param[in] len: length (in Byte)
		 * \param[in] rd_burst: size of packet to read
		 * \return false if read fails or filename can't be open, true otherwise
		 */
		bool dump(const std::string &filename, const int &base_addr,
				const int &len, int rd_burst = 0);
		/* combo flash + erase */
		int erase_and_prog(int base_addr, uint8_t *data, int len);
		/*!
		 * \brief check if area base_addr to base_addr + len match
		 *        data content
		 * \param[in] base_addr: base address to read
		 * \param[in] data: theorical area content
		 * \param[in] len: length (in Byte) to area and data
		 * \param[in] rd_burst: size of packet to read
		 * \return false if read fails or content didn't match, true otherwise
		 */
		bool verify(const int &base_addr, const uint8_t *data,
				const int &len, int rd_burst = 0);
		/* display/info */
		uint8_t read_status_reg();
		virtual void read_id();
		uint16_t readNonVolatileCfgReg();
		uint16_t readVolatileCfgReg();
	protected:
		SPIInterface *_spi;
		int8_t _verbose;
		uint32_t _jedec_id; /**< CHIP ID */
};

#endif
