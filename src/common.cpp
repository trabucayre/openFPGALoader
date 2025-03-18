// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2023 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */


#include "common.hpp"

#include <cstdlib>
#include <string>
#include <vector>

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

/*!
 * \brief convert a string, separate by delim to a vector
 * \param[in] in: string to split
 * \param[in] delim: split caracter
 * \return vector a substring
 */
const std::vector<std::string> splitString(const std::string& in,
		const char delim) noexcept {
	std::vector<std::string> tokens;
	size_t start = 0, end = 0;

	while ((end = in.find(delim, start)) != std::string::npos) {
		tokens.push_back(in.substr(start, end - start));
		start = end + 1;
	}

	tokens.push_back(in.substr(start)); // Add the last token
	return tokens;
}
