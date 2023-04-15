// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef DISPLAY_HPP_
#define DISPLAY_HPP_

#include <iostream>
#include <string>

void printError(const std::string &err, bool eol = true);
void printWarn(const std::string &warn, bool eol = true);
void printInfo(const std::string &info, bool eol = true);
void printSuccess(const std::string &success, bool eol = true);

#endif  // DISPLAY_HPP_
