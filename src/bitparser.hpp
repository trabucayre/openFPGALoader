// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef BITPARSER_H
#define BITPARSER_H

#include <iostream>
#include <fstream>
#include <string>

#include "configBitstreamParser.hpp"

class BitParser: public ConfigBitstreamParser {
	public:
		BitParser(const std::string &filename, bool reverseOrder, bool verbose = false);
		~BitParser();
		int parse() override;

		/*!
		 * \brief Extract the device IDCODE from Xilinx configuration data.
		 * Checks the UG470 synchronization word at its defined offset, then
		 * searches for a Type 1 Write IDCODE packet. The following word
		 * contains the device IDCODE.
		 *
		 * \param[in] data Configuration data beginning with the UG470 preamble.
		 *                 Any .bit file header must already have been removed.
		 *                 For .bin this header is absent.
		 * \param[in] length of data in bytes.
		 * \param[in] is_reversed True if the bits are reversed within each byte.
		 * \return The device IDCODE, or 0 if the data is invalid or the required
		 *         synchronization word or Write IDCODE packet is not found.
		 */
		static uint32_t get_idcode(const uint8_t *data, uint32_t length,
			bool is_reversed);

	private:
		int parseHeader();
		bool _reverseOrder;
};

#endif
