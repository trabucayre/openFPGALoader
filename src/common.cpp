// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2023 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */


#include "common.hpp"

#include <string>
#include <cstdlib>

/*!
 * \brief return shell environment variable value
 * \param[in] key: variable name
 * \return variable value or ""
 */
const std::string get_shell_env_var(const char* key,
		const char *def_val) noexcept {
	const char* ret = std::getenv(key);
	return std::string(ret ? ret : def_val);
}
