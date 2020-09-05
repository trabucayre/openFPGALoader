/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
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
		 * \return EXIT_SUCCESS is file is fully read, EXIT_FAILURE otherwhise
		 */
		int parse() override;

	private:
		bool _reverseOrder; /*!< tail if byte must be reversed */
};

#endif  // SRC_RAWPARSER_HPP_

