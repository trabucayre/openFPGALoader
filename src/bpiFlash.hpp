// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2024 openFPGALoader contributors
 * BPI (Parallel NOR) Flash support via JTAG bridge
 */

#ifndef SRC_BPIFLASH_HPP_
#define SRC_BPIFLASH_HPP_

#include <cstdint>
#include <string>

#include "jtag.hpp"

/*!
 * \class BPIFlash
 * \brief Intel CFI parallel NOR flash programming via JTAG bridge
 */
class BPIFlash {
 public:
	BPIFlash(Jtag *jtag, int8_t verbose);
	~BPIFlash();

	/*!
	 * \brief Read device ID and manufacturer info
	 * \return true if device detected
	 */
	bool detect();

	/*!
	 * \brief Read flash content
	 * \param[out] data: buffer to store read data
	 * \param[in] addr: start address (word address)
	 * \param[in] len: number of bytes to read
	 * \return true on success
	 */
	bool read(uint8_t *data, uint32_t addr, uint32_t len);

	/*!
	 * \brief Write data to flash (handles erase internally)
	 * \param[in] addr: start address (word address)
	 * \param[in] data: data to write
	 * \param[in] len: number of bytes to write
	 * \return true on success
	 */
	bool write(uint32_t addr, const uint8_t *data, uint32_t len);

	/*!
	 * \brief Erase a block
	 * \param[in] addr: address within the block to erase
	 * \return true on success
	 */
	bool erase_block(uint32_t addr);

	/*!
	 * \brief Bulk erase entire flash
	 * \return true on success
	 */
	bool bulk_erase();

	/*!
	 * \brief Get flash capacity in bytes
	 */
	uint32_t capacity() const { return _capacity; }

	/*!
	 * \brief Get block size in bytes
	 */
	uint32_t block_size() const { return _block_size; }

 private:
	/* BPI bridge command codes (match bpiOverJtag_core.v) */
	static const uint8_t CMD_WRITE = 0x1;
	static const uint8_t CMD_READ  = 0x2;
	static const uint8_t CMD_NOP   = 0x3;

	/* Intel CFI flash commands */
	static const uint16_t FLASH_CMD_READ_ARRAY   = 0x00FF;
	static const uint16_t FLASH_CMD_READ_ID      = 0x0090;
	static const uint16_t FLASH_CMD_READ_CFI     = 0x0098;
	static const uint16_t FLASH_CMD_READ_STATUS  = 0x0070;
	static const uint16_t FLASH_CMD_CLEAR_STATUS = 0x0050;
	static const uint16_t FLASH_CMD_PROGRAM      = 0x0041;  /* Single-word program (MT28GU512AAA) */
	static const uint16_t FLASH_CMD_BUFFERED_PRG = 0x00E9;
	static const uint16_t FLASH_CMD_CONFIRM      = 0x00D0;
	static const uint16_t FLASH_CMD_BLOCK_ERASE  = 0x0020;
	static const uint16_t FLASH_CMD_UNLOCK_BLOCK = 0x0060;
	static const uint16_t FLASH_CMD_UNLOCK_CONF  = 0x00D0;

	/* Status register bits */
	static const uint16_t SR_READY      = 0x0080;
	static const uint16_t SR_ERASE_ERR  = 0x0020;
	static const uint16_t SR_PROG_ERR   = 0x0010;
	static const uint16_t SR_VPP_ERR    = 0x0008;
	static const uint16_t SR_BLOCK_LOCK = 0x0002;

	/*!
	 * \brief Read a 16-bit word from flash at word address
	 */
	uint16_t bpi_read(uint32_t word_addr);

	/*!
	 * \brief Write a 16-bit word to flash at word address
	 */
	void bpi_write(uint32_t word_addr, uint16_t data);

	/*!
	 * \brief Wait for operation to complete
	 * \return true if completed successfully
	 */
	bool wait_ready(uint32_t timeout_ms = 10000);

	/*!
	 * \brief Unlock a block for programming/erase
	 */
	bool unlock_block(uint32_t word_addr);

	Jtag *_jtag;
	int8_t _verbose;
	int _irlen;
	uint32_t _capacity;
	uint32_t _block_size;
	uint16_t _manufacturer_id;
	uint16_t _device_id;
};

#endif  // SRC_BPIFLASH_HPP_
