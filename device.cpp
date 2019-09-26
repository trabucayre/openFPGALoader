#include <iostream>
#include <stdexcept>

#include "device.hpp"

using namespace std;

Device::Device(FtdiJtag *jtag, enum prog_mode mode, string filename):
		_filename(filename), _mode(mode)
{
	_jtag = jtag;
}

int Device::idCode()
{
	return 0;
}

void Device::program()
{
	throw std::runtime_error("Not implemented");
}
void Device::reset()
{
	throw std::runtime_error("Not implemented");
}

