// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Greg Davill <greg.davill@gmail.com>
 */

#if defined (_WIN64) || defined (_WIN32)
#include <string>

class PathHelper{
    public:
        static std::string absolutePath(std::string input_path);
};
#endif
