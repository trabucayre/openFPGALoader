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

ProgressBar::ProgressBar(std::string mess, int maxValue, int progressLen):
		_mess(mess), _maxValue(maxValue), _progressLen(progressLen)
{
}

void ProgressBar::display(int value)
{
	float percent = ((float)value * 100.0f)/(float)_maxValue;
	float nbEq = (percent * (float) _progressLen)/100.0f;

	fprintf(stderr, "\r%s: [", _mess.c_str());
	for (int z=0; z < nbEq; z++) {
		fputc('=', stderr);
	}
	fprintf(stderr, "%*s", (int)(_progressLen-nbEq), "");
	fprintf(stderr, "] %3.2f%%", percent);
}
void ProgressBar::done()
{
	display(_maxValue);
	fprintf(stderr, "\nDone\n");
}
