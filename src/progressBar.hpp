// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef PROGRESSBARE_HPP
#define PROGRESSBARE_HPP
#include <iostream>
#include <chrono>

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
		//records the time of last progress bar update
		std::chrono::time_point<std::chrono::system_clock> last_time;
		bool _quiet;
		bool _first;
};

#endif
