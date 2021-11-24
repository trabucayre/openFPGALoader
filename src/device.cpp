// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <iostream>
#include <stdexcept>

#include "device.hpp"

using namespace std;

Device::Device(Jtag *jtag, string filename, const string &file_type,
		bool verify, int8_t verbose):
		_filename(filename),
		_file_extension(filename.substr(filename.find_last_of(".") +1)),
		_mode(NONE_MODE), _verify(verify), _verbose(verbose > 0),
		_quiet(verbose < 0)
{
	if (!file_type.empty())
		_file_extension = file_type;
	else if (!filename.empty() && (filename.find_last_of(".")) == string::npos)
		_file_extension = "raw";

	_jtag = jtag;
	if (verbose > 0)
		cout << "File type : " << _file_extension << endl;
}

Device::~Device() {}

void Device::reset()
{
	throw std::runtime_error("Not implemented");
}

