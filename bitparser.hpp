#ifndef BITPARSER_H
#define BITPARSER_H

#include <iostream>
#include <fstream>

#include "configBitstreamParser.hpp"

class BitParser: public ConfigBitstreamParser {
	public:
		BitParser(std::string filename);
		~BitParser();
		int parse();

	private:
		int parseField();
		unsigned char reverseByte(unsigned char c);
		std::string fieldA;
		std::string part_name;
		std::string date;
		std::string hour;
		std::string design_name;
		std::string userID;
		std::string toolVersion;
};

#endif
