// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Greg Davill <greg.davill@gmail.com>
 */

#if defined (_WIN64) || defined (_WIN32)
#include "pathHelper.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <array>
#include <regex>

std::string PathHelper::absolutePath(std::string input_path) {

    /* Attempt to execute cygpath */
    std::string cmd = std::string("cygpath -m " + input_path);

    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        /* If cygpath fails to run, return original path */
        return input_path;
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    /* Trim trailing newline */
    static const std::regex tws{"[[:space:]]*$", std::regex_constants::extended};
    return std::regex_replace(result, tws, "");
}

#endif