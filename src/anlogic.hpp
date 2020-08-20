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

#ifndef SRC_ANLOGIC_HPP_
#define SRC_ANLOGIC_HPP_

#include <string>

#include "bitparser.hpp"
#include "device.hpp"
#include "jtag.hpp"
#include "svf_jtag.hpp"

class Anlogic: public Device {
	public:
		Anlogic(Jtag *jtag, const std::string &filename, bool verbose);
		~Anlogic();

		void program(unsigned int offset = 0) override;
		int idCode() override;
		void reset() override;
	private:
		SVF_jtag _svf;
};

#endif  // SRC_ANLOGIC_HPP_
