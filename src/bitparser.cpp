#include "bitparser.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#ifndef _WIN32
#include <arpa/inet.h>
#else
//for ntohs
#include <winsock2.h>
#endif

using namespace std;

#define display(...) \
	do { if (_verbose) fprintf(stdout, __VA_ARGS__);} while(0)

BitParser::BitParser(const string &filename, bool reverseOrder, bool verbose):
	ConfigBitstreamParser(filename, ConfigBitstreamParser::BIN_MODE,
	verbose), fieldA(), part_name(), date(), hour(),
	design_name(), userID(), toolVersion(), _reverseOrder(reverseOrder)
{
}
BitParser::~BitParser() 
{
}

int BitParser::parseField()
{
	int ret = 1;
	short length;
	char tmp[64];
	int pos, prev_pos;

	/* type */
	uint8_t type;
	_fd.read((char *)&type, sizeof(uint8_t));

	if (type != 'e') {
		_fd.read((char*)&length, sizeof(uint16_t));
		length = ntohs(length);
	} else {
		length = 4;
	}
	_fd.read(tmp, sizeof(uint8_t)*length);
	if (_verbose) {
		for (int i = 0; i < length; i++)
			printf("%c", tmp[i]);
		printf("\n");
	}
	switch (type) {
		case 'a': /* design name:userid:synthesize tool version */
			fieldA=(tmp);
			prev_pos = 0;
			pos = fieldA.find(";");
			design_name = fieldA.substr(prev_pos, pos);
			display("%d %d %s\n", prev_pos, pos, design_name.c_str());
			prev_pos = pos+1;

			pos = fieldA.find(";", prev_pos);
			userID = fieldA.substr(prev_pos, pos-prev_pos);
			display("%d %d %s\n", prev_pos, pos, userID.c_str());
			prev_pos = pos+1;

			//pos = fieldA.find(";", prev_pos);
			toolVersion = fieldA.substr(prev_pos);
			display("%d %d %s\n", prev_pos, pos, toolVersion.c_str());
			break;
		case 'b': /* FPGA model */
			part_name = (tmp);
			break;
		case 'c': /* buildDate */
			date = (tmp);
			break;
		case 'd': /* buildHour */
			hour = (tmp);
			break;
		case 'e': /* file size */
			_bit_length = 0;
			for (int i = 0; i < 4; i++) {
				display("%x %x\n", 0xff & tmp[i], _bit_length);
				_bit_length <<= 8;
				_bit_length |= 0xff & tmp[i];
			}
			display("    %x\n", _bit_length);
			ret = 0;

			break;
	}
	return ret;

}
int BitParser::parse()
{
	uint16_t length;
	display("parser\n\n");

	/* Field 1 : misc header */
	_fd.read((char*)&length, sizeof(uint16_t));
	length = ntohs(length);
	_fd.seekg(length, _fd.cur);

	_fd.read((char*)&length, sizeof(uint16_t));
	length = ntohs(length);

	/* process all field */
	do {} while (parseField());

	if (_verbose) {
		display("results\n\n");

		cout << "fieldA      : " << fieldA << endl;
		cout << "            : " << design_name << ";" << userID << ";" << toolVersion << endl;
		cout << "part name   : " << part_name << endl;
		cout << "date        : " << date << endl;
		cout << "hour        : " << hour << endl;
		cout << "file length : " << _bit_length << endl;
	}

	/* rest of the file is data to send */
	int pos = _fd.tellg();
	display("%d %d\n", pos, _bit_length);
	_fd.read((char *)&_bit_data[0], sizeof(uint8_t) * _bit_length);
	if (_fd.gcount() != _bit_length) {
		cerr << "Error: data read different to asked length ";
		cerr << to_string(_fd.gcount()) << " " << to_string(_bit_length) << endl;
		return -1;
	}

	if (_reverseOrder) {
		for (int i = 0; i < _bit_length; i++) {
			_bit_data[i] = reverseByte(_bit_data[i]);
		}
	}

	/* convert size to bit */
	_bit_length *= 8;

	return 0;
}
