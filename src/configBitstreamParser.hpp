// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef CONFIGBITSTREAMPARSER_H
#define CONFIGBITSTREAMPARSER_H

#include <stdint.h>

#include <iostream>
#include <fstream>
#include <string>
#include <map>

class ConfigBitstreamParser {
	public:
		ConfigBitstreamParser(const std::string &filename, int mode = ASCII_MODE,
			bool verbose = false);
		virtual ~ConfigBitstreamParser();
		virtual int parse() = 0;
		uint8_t *getData() {return (uint8_t*)_bit_data.c_str();}
		int getLength() {return _bit_length;}

		/**
		 * \brief display header informations
		 */
		virtual void displayHeader();

		/**
		 * \brief get header list of keys/values
		 * \return list of couple keys/values
		 */
		std::map<std::string, std::string> getHeader() { return _hdr; }

		/**
		 * \brief get value in header list
		 * \param[in] key: key value
		 * \return associated value for the key
		 */
		std::string getHeaderVal(std::string key);

		enum {
			ASCII_MODE = 0,
			BIN_MODE = std::ifstream::binary
		};

		static uint8_t reverseByte(uint8_t src);

	private:
		/**
		 * \brief decompress bitstream in gzip format
		 * \param[in] source: raw compressed data
		 * \param[out] dest: raw uncompressed data
		 * \return false if openFPGALoader is build without zlib or
		 *              if uncompress fails
		 */
		bool decompress_bitstream(std::string source, std::string *dest);

	protected:
		std::string _filename;
		int _bit_length;
		int _file_size;
		bool _verbose;
		std::string _bit_data;
		std::string _raw_data; /**< unprocessed file content */
		std::map<std::string, std::string> _hdr;
};

#endif
