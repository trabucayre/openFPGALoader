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

#ifndef PROGRESSBARE_HPP
#define PROGRESSBARE_HPP
#include <time.h>
#include <iostream>

class ProgressBar {
	public:
		ProgressBar(std::string mess, int maxValue, int progressLen,
				bool quiet = false);
		void display(int value, char force = 0);
		void done();
		void fail();
	private:
		std::string _mess;
		int _maxValue;
		int _progressLen;
		clock_t last_time; //records the time of last progress bar update 
		bool _quiet;
		bool _first;
};

#endif
