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

#include <stdio.h>
#include <stdlib.h>
#include "progressBar.hpp"
#include "display.hpp"

ProgressBar::ProgressBar(std::string mess, int maxValue, int progressLen,
		bool quiet): _mess(mess), _maxValue(maxValue),
		_progressLen(progressLen), _quiet(quiet), _first(true)
{
	last_time = clock();
}
void ProgressBar::display(int value, char force)
{
	if (_quiet) {
		if (_first) {
			printInfo(_mess + ": ", false);
			_first = false;
		}
		return;
	}

	clock_t this_time = clock();
	if (!force && ((((float)(this_time - last_time))/CLOCKS_PER_SEC) < 0.2))
	{
		return;
	}
	last_time = this_time;
	float percent = ((float)value * 100.0f)/(float)_maxValue;
	float nbEq = (percent * (float) _progressLen)/100.0f;

	printInfo("\r" + _mess + ": [", false);
	for (int z=0; z < nbEq; z++) {
		fputc('=', stderr);
	}
	fprintf(stderr, "%*s", (int)(_progressLen-nbEq), "");
	printInfo("] " + std::to_string(percent) + "%", false);
}
void ProgressBar::done()
{
	if (_quiet) {
		printSuccess("Done");
	} else {
		display(_maxValue, true);
		printSuccess("\nDone");
	}
}
void ProgressBar::fail()
{
	if (_quiet) {
		printError("\nFail");
	} else {
		display(_maxValue, true);
		printError("\nFail");
	}
}
