// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_EFINIXHEXPARSER_HPP_
#define SRC_EFINIXHEXPARSER_HPP_

#include <string>

#include "configBitstreamParser.hpp"

/*!
 * \file efinixHexParser
 * \class EfinixHexParser
 * \brief class used to read a raw data file 
 * \author Gwenhael Goavec-Merou
 */
class EfinixHexParser: public ConfigBitstreamParser {
	public:
		/*!
		 * \brief constructor
		 * \param[in] filename: raw file to read
		 * \param[in] reverseOrder: reverse each byte (LSB -> MSB, MSB -> LSB)
		 */
		EfinixHexParser(const std::string &filename, bool reverseOrder);
		/*!
		 * \brief read full content of the file, fill the buffer
		 * \return EXIT_SUCCESS is file is fully read, EXIT_FAILURE otherwhise
		 */
		int parse() override;

	private:
		bool _reverseOrder; /*!< tail if byte must be reversed */
};

#endif  // SRC_EFINIXHEXPARSER_HPP_

