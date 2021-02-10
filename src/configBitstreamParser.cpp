#include <iostream>
#include <stdint.h>
#include <strings.h>

#include "configBitstreamParser.hpp"

using namespace std;

ConfigBitstreamParser::ConfigBitstreamParser(const string &filename, int mode,
			bool verbose):
			_filename(filename), _bit_length(0),
			_file_size(0), _verbose(verbose), _fd(filename,
			ifstream::in | (ios_base::openmode)mode), _bit_data(), _raw_data(), _hdr()
{
	if (!_fd.is_open()) {
		cerr << "Error: fail to open " << _filename << endl;
		throw std::exception();
	}
	_fd.seekg(0, _fd.end);
	_file_size = _fd.tellg();
	_fd.seekg(0, _fd.beg);

	_raw_data.resize(_file_size);
	_bit_data.reserve(_file_size);
}

ConfigBitstreamParser::~ConfigBitstreamParser()
{
	_fd.close();
}

string ConfigBitstreamParser::getHeaderVal(string key)
{
	auto val = _hdr.find(key);
	if (val == _hdr.end())
		throw std::runtime_error("Error key " + key + " not found");
	return val->second;
}

uint8_t ConfigBitstreamParser::reverseByte(uint8_t src)
{
	uint8_t dst = 0;
	for (int i=0; i < 8; i++) {
		dst = (dst << 1) | (src & 0x01);
		src >>= 1;
	}
	return dst;
}
