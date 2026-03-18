// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <sstream>
#include <stdexcept>

#include "configBitstreamParser.hpp"
#include "display.hpp"
#include "efinixHexParser.hpp"


EfinixHexParser::EfinixHexParser(const std::string &filename):
		ConfigBitstreamParser(filename, ConfigBitstreamParser::ASCII_MODE,
		false)
{}

int EfinixHexParser::parseHeader()
{
	std::string buffer;
	std::istringstream lineStream(_raw_data);
	int bytesRead = 0;
	std::string headerText;
	bool foundPaddedBits = false;
	
	while (std::getline(lineStream, buffer, '\n')) {
		bytesRead += buffer.size() + 1;
		
		if (buffer != "0A") {
			try {
				uint8_t byte = std::stol(buffer, nullptr, 16);
				headerText += (char)byte;
			} catch (...) {
			}
		} else {
			headerText += '\n';
			if (foundPaddedBits)
				break;
		}
		
		if (headerText.find("PADDED_BITS") != std::string::npos)
			foundPaddedBits = true;
	}
	
	size_t pos;
	if ((pos = headerText.find("Mode: ")) != std::string::npos) {
		size_t end = headerText.find('\n', pos);
		_hdr["mode"] = headerText.substr(pos + 6, end - pos - 6);
	}
	if ((pos = headerText.find("Width: ")) != std::string::npos) {
		size_t end = headerText.find('\n', pos);
		_hdr["width"] = headerText.substr(pos + 7, end - pos - 7);
	}
	if ((pos = headerText.find("Device: ")) != std::string::npos) {
		size_t end = headerText.find('\n', pos);
		_hdr["device"] = headerText.substr(pos + 8, end - pos - 8);
	}
	
	return bytesRead;
}

int EfinixHexParser::parse()
{
	std::string buffer;
	parseHeader();
	
	std::istringstream lineStream(_raw_data);

	while (std::getline(lineStream, buffer, '\n')) {
		_bit_data.push_back(std::stol(buffer, nullptr, 16));
	}
	_bit_length = _bit_data.size() * 8;

	return EXIT_SUCCESS;
}
