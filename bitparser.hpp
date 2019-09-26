#ifndef BITPARSER_H
#define BITPARSER_H

#include <iostream>

class BitParser {
	public:
		BitParser(std::string filename);
		~BitParser();
		int parse();
		unsigned char *getData();
		int getLength() {return file_length;}

	private:
		int parseField(unsigned char type, FILE *fd);
		unsigned char reverseByte(unsigned char c);
		std::string fieldA;
		std::string part_name;
		std::string date;
		std::string hour;
		std::string design_name;
		std::string userID;
		std::string toolVersion;
		int file_length;
		unsigned char *bit_data;
		std::string _filename;
};

#endif
