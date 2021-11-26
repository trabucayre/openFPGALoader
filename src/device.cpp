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
	/* extension overwritten by user */
	if (!file_type.empty()) {
		_file_extension = file_type;
		/* check if extension and some specific type */
	} else if (!filename.empty()) {
		size_t offset = filename.find_last_of(".");
		/* no extension => consider raw */
		if (offset == string::npos) {
			_file_extension = "raw";
		/* compressed file ? */
		} else if  (_file_extension.substr(0, 2) == "gz") {
			size_t offset2 = filename.find_last_of(".", offset - 1);
			/* no more extension -> error */
			if (offset2 == string::npos) {
				char mess[256];
				snprintf(mess, sizeof(mess), "\nfile %s is compressed\n"
						"but can't determine real type\n"
						"please add correct extension or use --file-type",
						filename.c_str());
				throw std::runtime_error(mess);
			} else { /* extract sub extension */
				_file_extension = _filename.substr(offset2 + 1,
						offset - offset2 - 1);
			}
		}
	}

	_jtag = jtag;
	if (verbose > 0)
		cout << "File type : " << _file_extension << endl;
}

Device::~Device() {}

void Device::reset()
{
	throw std::runtime_error("Not implemented");
}

