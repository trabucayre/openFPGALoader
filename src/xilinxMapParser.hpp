// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2021 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */
#ifndef SRC_XILINXMAPPARSER_HPP_
#define SRC_XILINXMAPPARSER_HPP_

#include <stdint.h>

#include <string>
#include <vector>

#include "configBitstreamParser.hpp"
#include "jedParser.hpp"

/*!
 * \file xilinxMapParser.hpp
 * \class XilinxMapParser (based on "XILINX PROGRAMMER QUALIFICATION SPECIFICATION ")
 * \brief xilinx map parser. Used to place jed content into flash memory order
 * \author Gwenhael Goavec-Merou
 */

class XilinxMapParser: public ConfigBitstreamParser {
	public:
		/*!
		 * \brief XilinxMapParser constructor
		 * \param[in] filename: absolute map file path
		 * \param[in] num_row: device x size
		 * \param[in] num_col: device y size (with usercode and done/sec rows)
		 * \param[in] jed: input configuration data
		 * \param[in] usercode: usercode to place in usercode area
		 * \param[in] verbose: verbose level
		 */
		XilinxMapParser(const std::string &filename,
				const uint16_t num_row, const uint16_t num_col,
				JedParser *jed,
				const uint32_t usercode, bool verbose = false);

		/*!
		 * \brief parse map file to construct _map_table and call jedApplyMap
		 * \return false for unknown code, true otherwise
		 */
		int parse() override;

		/*!
		 * \brief return 2 dimension configuration data reorganized
		 *        according to map file
		 * \return configuration data array
		 */
		std::vector<std::string> cfg_data() { return _cfg_data;}

	private:
		/*!
		 * \brief build _cfg_data by using cfg_data content.
		 * \return false for unknown code in map, true otherwise
		 */
		bool jedApplyMap();

		enum {
			BIT_ZERO  = -1,  /* empty bit with only blank before ie '\t' */
			BIT_ONE   = -2,  /* empty bit with non blank before */
		};

		std::vector<std::vector<int>> _map_table; /**< map array */
		JedParser *_jed; /**< raw jed content */
		uint16_t _num_row; /**< bitstream number of row */
		uint16_t _num_col; /**< bitsteam number of col */
		uint32_t _usercode; /**< usercode to add into corresponding area */
		std::vector<std::string> _cfg_data; /**< fuse array after built */
};

#endif  // SRC_XILINXMAPPARSER_HPP_
