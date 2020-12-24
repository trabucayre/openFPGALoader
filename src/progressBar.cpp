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
#include "time.h"
#include "progressBar.hpp"
#include "display.hpp"

ProgressBar::ProgressBar(std::string mess, int maxValue, int progressLen):
		_mess(mess), _maxValue(maxValue), _progressLen(progressLen)
{
}
static time_t last_time; 
void ProgressBar::display(int value, char force)
{
	clock_t this_time = clock();
	if (!force && ((((float)(this_time - last_time))/CLOCKS_PER_SEC) < 0.2))
	{
		return;
	}
	last_time = this_time;
	float percent = ((float)value * 100.0f)/(float)_maxValue;
	float nbEq = (percent * (float) _progressLen)/100.0f;

	//fprintf(stderr, "\r%s: [", _mess.c_str());
	printInfo("\r" + _mess + ": [", false);
	for (int z=0; z < nbEq; z++) {
		fputc('=', stderr);
	}
	fprintf(stderr, "%*s", (int)(_progressLen-nbEq), "");
	//fprintf(stderr, "] %3.2f%%", percent);
	printInfo("] " + std::to_string(percent) + "%", false);
}
void ProgressBar::done()
{
	display(_maxValue, true);
	//fprintf(stderr, "\nDone\n");
	printSuccess("\nDone");
}
void ProgressBar::fail()
{
	display(_maxValue, true);
	//fprintf(stderr, "\nDone\n");
	printError("\nFail");
}
