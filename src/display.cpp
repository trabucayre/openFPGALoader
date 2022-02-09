// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
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
#define KBLUL "\x1B[94m"
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
		std::cout << KBLUL << info << "\e[0m" << std::flush;
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
