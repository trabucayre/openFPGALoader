// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_SPIFLASH_HPP_
#define SRC_SPIFLASH_HPP_

#include <map>
#include <string>

#include "spiInterface.hpp"
#include "spiFlashdb.hpp"

class SPIFlash {
	public:
		SPIFlash(SPIInterface *spi, bool unprotect, int8_t verbose);
		/* power */
		virtual void power_up();
		virtual void power_down();
		virtual void reset();
		/* protection */
		int write_enable();
		int write_disable();
		/*!
		 * \brief disable protection for all sectors
		 * \return -1 if write enable or disabling failed
		 */
		int disable_protection();
		/*!
		 * \brief enable protection for selected blocks
		 * \param[in] protect_code: bp + tb combinaison
		 * \return -1 if write enable or enabling failed
		 */
		int enable_protection(uint8_t protect_code = 0x1c);
		/*!
		 * \brief enable protection for specified area
		 * \param[in] length: TODO
		 * \return -1 if write enable or enabling failed
		 */
		int enable_protection(uint32_t len);
		/*!
		 * \brief unlock all sectors: specific to
		 * Microchip SST26VF032B / SST26VF032BA
		 * \return false if unlock fail
		 */
		bool global_unlock();
		/* erase */
		int bulk_erase();
		/*!
		 * \brief erase one sector (4Kb)
		 */
		int sector_erase(int addr);
		/*!
		 * \brief erase one 32Kb block
		 */
		int block32_erase(int addr);
		/*!
		 * \brief erase one 64Kb block
		 */
		int block64_erase(int addr);
		/*!
		 * \brief erase n sectors starting at base_addr
		 */
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
		/* return status register value */
		uint8_t read_status_reg();
		/* display/info */
		void display_status_reg(uint8_t reg);
		void display_status_reg() {display_status_reg(read_status_reg());}
		virtual void read_id();
		uint16_t readNonVolatileCfgReg();
		uint16_t readVolatileCfgReg();

	protected:
		/*!
		 * \brief retrieve TB (Top/Bottom) bit from one register
		 *        (depends on flash)
		 * \return -1 if unknown register, 1 or 0 otherwise
		 */
		int8_t get_tb();

	public:
		/*!
		 * \brief convert block protect to len in byte
		 * \param[in] bp: block protection
		 * \return protect area in byte
		 */
		std::map<std::string, uint32_t> bp_to_len(uint8_t bp, uint8_t tb);

	protected:
		/*!
		 * \brief convert len (in byte) to corresponding block protect
		 * \param[in] len: len in byte
		 * \return bp code (based on chip bp[x] position)
		 */
		uint8_t len_to_bp(uint32_t len);

		SPIInterface *_spi;
		int8_t _verbose;
		uint32_t _jedec_id; /**< CHIP ID */
		flash_t *_flash_model; /**< detect flash model */
		bool _unprotect; /**< allows to unprotect memory before write */
};

#endif  // SRC_SPIFLASH_HPP_
