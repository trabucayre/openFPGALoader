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

#include <unistd.h>

#include <iostream>
#include <string>

#include "display.hpp"

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

void printError(std::string err, bool eol)
{
	if (isatty(STDERR_FILENO))
		std::cerr << KRED << err << "\e[0m";
	else
		std::cerr << err;
	std::cerr << std::flush;
	if (eol)
		std::cerr << std::endl;
}

void printWarn(std::string warn, bool eol)
{
	if (isatty(STDOUT_FILENO))
		std::cout << KYEL << warn << "\e[0m" << std::flush;
	else
		std::cout << warn;
	std::cout << std::flush;
	if (eol)
		std::cout << std::endl;
}

void printInfo(std::string info, bool eol)
{
	if (isatty(STDOUT_FILENO))
		std::cout << KBLU << info << "\e[0m" << std::flush;
	else
		std::cout << info;
	std::cout << std::flush;
	if (eol)
		std::cout << std::endl;
}

void printSuccess(std::string success, bool eol)
{
	if (isatty(STDOUT_FILENO))
		std::cout << KGRN << success << "\e[0m" << std::flush;
	else
		std::cout << success;
	std::cout << std::flush;
	if (eol)
		std::cout << std::endl;
}
