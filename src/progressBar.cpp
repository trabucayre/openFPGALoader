// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include <string>
#include "progressBar.hpp"
#include "display.hpp"

ProgressBar::ProgressBar(std::string mess, int maxValue, int progressLen,
		bool quiet): _mess(mess), _maxValue(maxValue),
		_progressLen(progressLen), _quiet(quiet), _first(true)
{
	last_time = std::chrono::system_clock::now();
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

	std::chrono::time_point<std::chrono::system_clock> this_time;
	this_time = std::chrono::system_clock::now();
	std::chrono::duration<double> diff = this_time - last_time;

	if (!force && diff.count() < 1)
	{
		return;
	}
	last_time = this_time;
	float percent = ((float)value * 100.0f)/(float)_maxValue;
	float nbEq = (percent * (float) _progressLen)/100.0f;

	printInfo("\r" + _mess + ": [", false);
	for (int z=0; z < nbEq; z++) {
		fputc('=', stdout);
	}
	fprintf(stdout, "%*s", (int)(_progressLen-nbEq), "");
	char perc_str[11];
	snprintf(perc_str, sizeof(perc_str), "] %3.2f%%", percent);
	printInfo(perc_str, false);
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
		printError("Fail");
	} else {
		display(_maxValue, true);
		printError("\nFail");
	}
}
