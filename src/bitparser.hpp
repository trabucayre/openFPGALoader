#ifndef BITPARSER_H
#define BITPARSER_H

#include <iostream>
#include <fstream>

#include "configBitstreamParser.hpp"

class BitParser: public ConfigBitstreamParser {
	public:
		BitParser(const std::string &filename, bool verbose = false);
		~BitParser();
		int parse() override;

	private:
		int parseField();
		std::string fieldA;
		std::string part_name;
		std::string date;
		std::string hour;
		std::string design_name;
		std::string userID;
		std::string toolVersion;
};

#endif
