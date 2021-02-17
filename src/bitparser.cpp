#include "bitparser.hpp"
#include "display.hpp"
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
	verbose), _reverseOrder(reverseOrder)
{
}
BitParser::~BitParser() 
{
}

int BitParser::parseField()
{
	int ret = 1;
	short length;
	string tmp(64, ' ');
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
	_fd.read(&tmp[0], sizeof(uint8_t)*length);

	switch (type) {
		case 'a': /* design name:userid:synthesize tool version */
			prev_pos = 0;
			pos = tmp.find(";");
			_hdr["design_name"] = tmp.substr(prev_pos, pos);
			prev_pos = pos+1;

			pos = tmp.find(";", prev_pos);
			prev_pos = tmp.find("=", prev_pos) + 1;
			_hdr["userID"] = tmp.substr(prev_pos, pos-prev_pos);
			prev_pos = pos+1;

			prev_pos = tmp.find("=", prev_pos) + 1;
			_hdr["toolVersion"] = tmp.substr(prev_pos);
			break;
		case 'b': /* FPGA model */
			_hdr["part_name"] = tmp;
			break;
		case 'c': /* buildDate */
			_hdr["date"] = tmp;
			break;
		case 'd': /* buildHour */
			_hdr["hour"] = tmp;
			break;
		case 'e': /* file size */
			_bit_length = 0;
			for (int i = 0; i < 4; i++) {
				_bit_length <<= 8;
				_bit_length |= 0xff & tmp[i];
			}
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
		cout << "bitstream header infos" << endl;
		for (auto it = _hdr.begin(); it != _hdr.end(); it++) {
				printInfo((*it).first + ": ", false);
				printSuccess((*it).second);
		}
		cout << endl;
	}

	/* rest of the file is data to send */
	_fd.read((char *)&_bit_data[0], sizeof(uint8_t) * _bit_length);
	if (_fd.gcount() != _bit_length) {
		printError("Error: data read different to asked length ", false);
		printError(to_string(_fd.gcount()) + " " + to_string(_bit_length));
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
