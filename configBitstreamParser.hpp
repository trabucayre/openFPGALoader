#ifndef CONFIGBITSTREAMPARSER_H
#define CONFIGBITSTREAMPARSER_H

#include <iostream>
#include <fstream>
#include <stdint.h>

class ConfigBitstreamParser {
	public:
		ConfigBitstreamParser(std::string filename, int mode = ASCII_MODE,
			bool verbose = false);
		virtual ~ConfigBitstreamParser();
		virtual int parse() = 0;
		uint8_t *getData() {return (uint8_t*)_bit_data.c_str();}
		int getLength() {return _bit_length;}
	
		enum {
			ASCII_MODE = 0,
			BIN_MODE = std::ifstream::binary
		};

	protected:
		std::string _filename;
		int _bit_length;
		int _file_size;
		bool _verbose;
		std::ifstream _fd;
		std::string _bit_data;
};

#endif
