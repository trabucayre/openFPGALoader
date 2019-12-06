/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <vector>

#include "fsparser.hpp"
#include "display.hpp"

using namespace std;

FsParser::FsParser(string filename, bool reverseByte, bool verbose):
			ConfigBitstreamParser(filename, ConfigBitstreamParser::ASCII_MODE,
			verbose), _reverseByte(reverseByte), _toolVersion(), _partNumber(),
			_devicePackage(), _backgroundProgramming(false), _checksum(),
			_crcCheck(false), _compress(false), _encryption(false),
			_securityBit(false), _jtagAsRegularIO(false), _date()
{
}
FsParser::~FsParser()
{
}

int FsParser::parseHeader()
{
	int ret = 1;
	string buffer;

	while (1){
		std::getline(_fd, buffer, '\n');
		if (buffer[0] != '/')
			break;
		buffer = buffer.substr(2);
		size_t pos = buffer.find(':');
		if (pos == string::npos)
			continue;
		string v1, v2;
		v1 = buffer.substr(0, pos);
		v2 = buffer.substr(pos+2);  // ':' + ' '

		if (v1 == "GOWIN Version")
			_toolVersion = v2;
		if (v1 == "Part Number")
			_partNumber = v2;
		if (v1 == "Device-package")
			_devicePackage = v2;
		if (v1 == "BackgroundProgramming")
			_backgroundProgramming = ((v2 == "OFF")?false:true);
		if (v1 == "CheckSum")
			sscanf(v2.c_str(), "0x%04hx", &_checksum);
		if (v1 == "CRCCheck")
			_crcCheck = ((v2 == "OFF")?false:true);
		if (v1 == "Compress")
			_compress = ((v2 == "OFF")?false:true);
		if (v1 == "Encryption")
			_encryption = ((v2 == "OFF")?false:true);
		if (v1 == "SecurityBit")
			_securityBit = ((v2 == "OFF")?false:true);
		if (v1 == "JTAGAsRegularIO")
			_jtagAsRegularIO = ((v2 == "OFF")?false:true);
		if (v1 == "Created Time")
			_date = v2;
	}
	if (_verbose) {
		printInfo("tool version:           " +  _toolVersion);
		printInfo("Part number:            " + _partNumber);
		printInfo("Device package:         " + _devicePackage);
		printInfo("Background programming: " +
				string((_backgroundProgramming)?"ON":"OFF"));
		printInfo("Checksum:               " + _checksum);
		printInfo("CRC check:              " + string((_crcCheck)?"ON":"OFF"));
		printInfo("Compression:            " + string((_compress)?"ON":"OFF"));
		printInfo("Encryption:             " + string((_encryption)?"ON":"OFF"));
		printInfo("Security bit:           " + string((_securityBit)?"ON":"OFF"));
		printInfo("Jtag as regular IO:     " + string((_jtagAsRegularIO)?"ON":"OFF"));
		printInfo("Creation date:          " + _date);
	}

	return ret;
}

int FsParser::parse()
{
	uint8_t data;
	string buffer, tmp;

	printInfo("Parse " + _filename + ": ", false);

	parseHeader();
	_fd.seekg(0, _fd.beg);

	while (1) {
		std::getline(_fd, buffer, '\n');
		if (buffer.size() == 0)
			break;
		if (buffer[0] == '/')
			continue;
		tmp += buffer;
	}

	_bit_length = tmp.size();

	/* Fs file format is MSB first
	 * so if reverseByte = false bit 0 -> 7, 1 -> 6,
	 * if true 0 -> 0, 1 -> 1
	 */

	for (int i = 0; i < _bit_length; i+=8) {
		data = 0;
		for (int ii = 0; ii < 8; ii++) {
			uint8_t val = (tmp[i+ii] == '1'?1:0);
			if (_reverseByte)
				data |= val << ii;
			else
				data |= val << (7-ii);
		}
		_bit_data += data;
	}

	printSuccess("Done");

	return 0;
}
