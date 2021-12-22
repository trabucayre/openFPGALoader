// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_SPIINTERFACE_HPP_
#define SRC_SPIINTERFACE_HPP_

#include <iostream>
#include <vector>

/*!
 * \file SPIInterface.hpp
 * \class SPIInterface
 * \brief abstract class between spi implementation and converters
 * \author Gwenhael Goavec-Merou
 */

class SPIInterface {
 public:
	SPIInterface();
	SPIInterface(const std::string &filename, uint8_t verbose,
			uint32_t rd_burst, bool verify);
	virtual ~SPIInterface() {}

	bool protect_flash(uint32_t len);
	bool unprotect_flash();
	/*!
	 * \brief write len byte into flash starting at offset,
	 *        optionnally verify after write and unprotect
	 *        blocks if required and allowed
	 * \param[in] offset: offset into flash
	 * \param[in] data: data to write
	 * \param[in] len: byte len to write
	 * \param[in] verify: verify flash after write
	 * \param[in] unprotect_flash: unprotect blocks if allowed and required
	 * \param[in] rd_burst: read flash by rd_burst bytes
	 * \param[in] verbose: verbose level
	 * \return false when something fails
	 */
	bool write(uint32_t offset, uint8_t *data, uint32_t len,
		bool unprotect_flash);
	/*!
	 * \brief read flash offset byte starting at base_addr and
	 *        store into filename
	 * \param[in] filename: file to store
	 * \param[in] base_addr: offset into flash
	 * \param[in] len: byte len to read
	 * \param[in] rd_burst: read flash by rd_burst bytes
	 * \param[in] verbose: verbose level
	 * \return false when something fails
	 */
	bool dump(uint32_t base_addr, uint32_t len);

	/*!
	 * \brief send a command, followed by len byte.
	 * \param[in] cmd: command/opcode to send
	 * \param[in] tx: buffer to send
	 * \param[in] rx: buffer for read access
	 * \param[in] len: number of byte to send/receive (cmd not comprise)
	 *                 to send only a cmd set len to 0
	 * \return 0 when success
	 */
	virtual int spi_put(uint8_t cmd, uint8_t *tx, uint8_t *rx,
						uint32_t len) = 0;

	/*!
	 * \brief send a command, followed by len byte.
	 * \param[in] tx: buffer to send
	 * \param[in] rx: buffer for read access
	 * \param[in] len: number of byte to send/receive
	 * \return 0 when success
	 */
	virtual int spi_put(uint8_t *tx, uint8_t *rx, uint32_t len) = 0;

	/*!
	 * \brief wait until register content and mask match cond, or timeout
	 * \param[in] cmd: register to read
	 * \param[in] mask: mask used with read byte
	 * \param[in] cond: condition to wait
	 * \param[in] timeout: number of try before fail
	 * \return 0 when success, -ETIME when timeout occur
	 */
	virtual int spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
			uint32_t timeout, bool verbose = false) = 0;

 protected:
	/*!
	 * \brief prepare SPI flash access
	 */
	virtual bool prepare_flash_access() {return false;}
	/*!
	 * \brief end of SPI flash access
	 */
	virtual bool post_flash_access() {return false;}

	uint8_t _spif_verbose;
	uint32_t _spif_rd_burst;
	bool _spif_verify;
 private:
	std::string _spif_filename;

};
#endif  // SRC_SPIINTERFACE_HPP_
