// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_RAWPARSER_HPP_
#define SRC_RAWPARSER_HPP_

#include <string>

#include "configBitstreamParser.hpp"

/*!
 * \file rawParser
 * \class RawParser
 * \brief class used to read a raw data file 
 * \author Gwenhael Goavec-Merou
 */
class RawParser: public ConfigBitstreamParser {
	public:
		/*!
		 * \brief constructor
		 * \param[in] filename: raw file to read
		 * \param[in] reverseOrder: reverse each byte (LSB -> MSB, MSB -> LSB)
		 */
		RawParser(const std::string &filename, bool reverseOrder);
		/*!
		 * \brief read full content of the file, fill the buffer
		 * \return EXIT_SUCCESS is file is fully read, EXIT_FAILURE otherwise
		 */
		int parse() override;

	private:
		bool _reverseOrder; /*!< tail if byte must be reversed */
};

#endif  // SRC_RAWPARSER_HPP_

