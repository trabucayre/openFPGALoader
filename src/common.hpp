// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2023 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_COMMON_HPP_
#define SRC_COMMON_HPP_

#include <string>

/*!
 * \brief return shell environment variable value
 * \param[in] key: variable name
 * \param[in] def_val: value to return when not found
 * \return variable value or def_val
 */
const std::string get_shell_env_var(const char* key,
	const char *def_val="") noexcept;

#endif  // SRC_COMMON_HPP_
