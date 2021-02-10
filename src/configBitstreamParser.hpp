#ifndef CONFIGBITSTREAMPARSER_H
#define CONFIGBITSTREAMPARSER_H

#include <iostream>
#include <fstream>
#include <stdint.h>
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

	protected:
		std::string _filename;
		int _bit_length;
		int _file_size;
		bool _verbose;
		std::ifstream _fd;
		std::string _bit_data;
		std::string _raw_data; /**< unprocessed file content */
		std::map<std::string, std::string> _hdr;
};

#endif
