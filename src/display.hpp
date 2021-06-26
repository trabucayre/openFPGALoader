// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef DISPLAY_HPP_
#define DISPLAY_HPP_

#include <iostream>
#include <string>

void printError(std::string err, bool eol = true);
void printWarn(std::string warn, bool eol = true);
void printInfo(std::string info, bool eol = true);
void printSuccess(std::string success, bool eol = true);

#endif  // DISPLAY_HPP_
