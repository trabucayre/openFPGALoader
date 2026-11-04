// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Greg Davill <greg.davill@gmail.com>
 */

#if defined (_WIN64) || defined (_WIN32)
#include "pathHelper.hpp"

#include <filesystem>

std::string PathHelper::absolutePath(std::string input_path) {
    std::error_code ec;
    const auto absolute = std::filesystem::absolute(input_path, ec);
    if (ec) return input_path;
    return absolute.generic_string();
}
#endif