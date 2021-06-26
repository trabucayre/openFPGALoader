// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef FSPARSER_HPP_
#define FSPARSER_HPP_

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "configBitstreamParser.hpp"

class FsParser: public ConfigBitstreamParser {
	public:
		FsParser(const std::string &filename, bool reverseByte, bool verbose);
		~FsParser();
		int parse() override;

		uint16_t checksum() {return _checksum;}

	private:
		int parseHeader();
		/**
		 * \brief convert an binary string representation to the corresponding
		 * value
		 *
		 * \param[in] bits: '1' or '0' buffer
		 * \param[in] len: array length (up to 64)
		 * \return converted value
		 */
		uint64_t bitToVal(const char *bits, int len);

		bool _reverseByte; /*!< direct or reverse bit */
		uint16_t _end_header; /*!< last header line */
		uint16_t _checksum; /*!< locally computed data checksum */
		uint8_t _8Zero; /*!< in compress mode, used to replace 8 * 0x00 */
		uint8_t _4Zero; /*!< in compress mode, used to replace 8 * 0x00 */
		uint8_t _2Zero; /*!< in compress mode, used to replace 8 * 0x00 */
		uint32_t _idcode; /*!< device idcode */
		bool _compressed; /*!< compress mode or not */
		std::vector<std::string> _lstRawData; /* cfg + EBR data buffer */
};

#endif  // FSPARSER_HPP_
