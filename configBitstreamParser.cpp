#include <iostream>
#include <stdint.h>
#include <strings.h>

#include "configBitstreamParser.hpp"

using namespace std;

ConfigBitstreamParser::ConfigBitstreamParser(string filename, int mode):
			_filename(filename), _bit_length(0),
			_file_size(0), _fd(filename,
			ifstream::in | (ios_base::openmode)mode), _bit_data()
{
	if (!_fd.is_open()) {
		cerr << "Error: fail to open " << _filename << endl;
		throw std::exception();
	}
	_fd.seekg(0, _fd.end);
	_file_size = _fd.tellg();
	_fd.seekg(0, _fd.beg);

	_bit_data.reserve(_file_size);
}

ConfigBitstreamParser::~ConfigBitstreamParser()
{
	_fd.close();
}
