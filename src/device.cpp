#include <iostream>
#include <stdexcept>

#include "device.hpp"

using namespace std;

Device::Device(Jtag *jtag, string filename, int8_t verbose):
		_filename(filename),
		_file_extension(filename.substr(filename.find_last_of(".") +1)),
		_mode(NONE_MODE), _verbose(verbose > 0), _quiet(verbose < 0)
{
	_jtag = jtag;
	if (verbose > 0)
		cout << "File type : " << _file_extension << endl;
}

Device::~Device() {}

void Device::reset()
{
	throw std::runtime_error("Not implemented");
}

