/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
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

#include <string.h>
#include "anlogic.hpp"
#include "jtag.hpp"
#include "device.hpp"

#define IDCODE 6
#define IRLENGTH 8

Anlogic::Anlogic(Jtag *jtag, const std::string &filename, bool verbose):
	Device(jtag, filename, verbose), _svf(_jtag, _verbose)
{
	if (_filename != "") {
		if (_file_extension == "svf")
			_mode = Device::MEM_MODE;
		else
			throw std::exception();
	}
}
Anlogic::~Anlogic()
{}
void Anlogic::reset()
{
}

void Anlogic::program(unsigned int offset)
{
	if (_mode == Device::NONE_MODE)
		return;
	/* in all case we consider svf is mandatory
	 * MEM_MODE : svf file provided for constructor
	 *            is the bitstream to use
	 * SPI_MODE : svf file provided is bridge to have
	 *            access to the SPI flash
	 */
	/* mem mode -> svf */
	if (_mode == Device::MEM_MODE) {
		_svf.parse(_filename);
	}
}
int Anlogic::idCode()
{
	unsigned char tx_data[4] = {IDCODE};
	unsigned char rx_data[4];
	_jtag->go_test_logic_reset();
	_jtag->shiftIR(tx_data, NULL, IRLENGTH);
	memset(tx_data, 0, 4);
	_jtag->shiftDR(tx_data, rx_data, 32);
	return ((rx_data[0] & 0x000000ff) |
		((rx_data[1] << 8) & 0x0000ff00) |
		((rx_data[2] << 16) & 0x00ff0000) |
		((rx_data[3] << 24) & 0xff000000));
}
