// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2021 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_DFUFILEPARSER_HPP_
#define SRC_DFUFILEPARSER_HPP_

#include <string>

#include "configBitstreamParser.hpp"

/*!
 * \file dfuFileParser
 * \class DFUFileParser
 * \brief class used to read a DFU data file 
 * \author Gwenhael Goavec-Merou
 */
class DFUFileParser: public ConfigBitstreamParser {
	public:
		/*!
		 * \brief constructor
		 * \param[in] filename: raw file to read
		 */
		DFUFileParser(const std::string &filename, bool verbose);
		/*!
		 * \brief read full content of the file, fill the buffer
		 * \return EXIT_SUCCESS is file is fully read, EXIT_FAILURE otherwise
		 */
		int parse() override;
		/*!
		 * \brief read DFU suffix content of the file, fill _hdr structure
		 * \return EXIT_SUCCESS if suffix is fully read, EXIT_FAILURE otherwise
		 */
		int parseHeader();
		/*!
		 * \brief return vendor id associated
		 * \return _idVendor
		 */
		uint16_t vendorID() {return _idVendor;}
		/*!
		 * \brief return product id associated
		 * \return _idProduct
		 */
		uint16_t productID() {return _idProduct;}

	private:
		/*!
		 * \brief information in the suffix file part
		 */
		uint16_t _bcdDFU; /**< specification number */
		uint16_t _idVendor; /**< device VID */
		uint16_t _idProduct; /**< device PID */
		uint16_t _bcdDevice; /**< release number */
		uint32_t _dwCRC; /**< data CRC */
		uint8_t _bLength; /**< DFU suffix length */
};

#endif  // SRC_DFUFILEPARSER_HPP_

