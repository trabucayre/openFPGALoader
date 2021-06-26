// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2021 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_IHEXPARSER_HPP_
#define SRC_IHEXPARSER_HPP_

#include <string>
#include <vector>

#include "configBitstreamParser.hpp"

/*!
 * \file ihexParser
 * \brief IhexParser
 * \brief class used to read a (i)hex data file
 * \author Gwenhael Goavec-Merou
 */
class IhexParser: public ConfigBitstreamParser {
	public:
		/*!
		 * \brief constructor
		 * \param[in] filename: file path
		 * \param[in] reverseOrder: MSB or LSB storage
		 * \param[in] verbose: verbosity level
		 */
		IhexParser(const std::string &filename, bool reverseOrder, bool verbose);
		/*!
		 * \brief read full content of the file, fill sections
		 */
		int parse() override;

		/*!
		 * structure to store file content by section
		 */
		typedef struct {
			uint16_t addr;
			uint16_t length;
			std::vector<uint8_t> line_data;
		} data_line_t;

		/*!
		 * \brief return list of sections
		 */
		std::vector<data_line_t> getDataArray() {return _array_content;}

	private:
		int _base_addr;
		bool _reverseOrder;
		std::vector<uint32_t> _data;
		std::vector<data_line_t> _array_content; /**< list of sections */
};

#endif  // SRC_IHEXPARSER_HPP_

