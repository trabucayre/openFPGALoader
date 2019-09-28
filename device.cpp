#include <iostream>
#include <stdexcept>

#include "device.hpp"

using namespace std;

Device::Device(FtdiJtag *jtag, string filename):
		_filename(filename),
		_file_extension(filename.substr(filename.find_last_of(".") +1)),
		_mode(NONE_MODE)
{
	_jtag = jtag;
	cout << _file_extension << endl;
}

Device::~Device() {}

void Device::reset()
{
	throw std::runtime_error("Not implemented");
}

