// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_POFPARSER_HPP_
#define SRC_POFPARSER_HPP_

#include <stdint.h>

#include <string>
#include <map>

#include "configBitstreamParser.hpp"

#define ARRAY2INT16(_array_) ( \
	(static_cast<uint16_t>(_array_[0] & 0x00ff) << 0) | \
	(static_cast<uint16_t>(_array_[1] & 0x00ff) << 8))

#define ARRAY2INT32(_array_) ( \
	(static_cast<uint16_t>(_array_[0] & 0x00ff) <<  0) | \
	(static_cast<uint16_t>(_array_[1] & 0x00ff) <<  8) | \
	(static_cast<uint16_t>(_array_[2] & 0x00ff) << 16) | \
	(static_cast<uint16_t>(_array_[3] & 0x00ff) << 24))

/*!
 * \file pofParser.hpp
 * \class POFParser
 * \brief basic implementation for intel/altera POF format
 * \author Gwenhael Goavec-Merou
 */
class POFParser: public ConfigBitstreamParser {
	public:
		POFParser(const std::string &filename, bool verbose = false);
		~POFParser();

		int parse() override;

		/**
		 * \brief return pointer to cfg data section when name is provided
		 *        when "" -> return full cfg_data
		 * \return a pointer
		 */
		uint8_t *getData(const std::string &section_name);

		/**
		 * \brief return length (bits) to a cfg data section when name is
		 *        provided or full cfg_data length when ""
		 * \return size in bits
		 */
		int getLength(const std::string &section_name);

		/**
         * \brief display header informations
         */
        void displayHeader() override;


	private:
		/* packet 0x1A content */
		typedef struct {
			uint8_t flag;         // 1 Byte before section name
			std::string section;  // UFM/CFM/ICB
			uint32_t offset;      // start offset in packet 17 area
			uint8_t *data;        // memory pointer
			uint32_t len;         // area length (bits)
		} memory_section_t;

		std::map<std::string, memory_section_t> mem_section;

		/*!
		 * \brief parse a section 0x1A (list of sections)
		 * \param[in] flag: 16Bits flag
		 * \param[in] pos : 32bits _raw_data's offset
		 * \param[in] size: 32bits content's size
		 */
		void parseFlag26(uint16_t flag, uint32_t pos,
			uint32_t size, const std::string &payload);

		/*!
		 * \brief parse a section (flag + pos + size)
		 * \param[in] flag: 16Bits flag
		 * \param[in] pos : 32bits _raw_data's offset
		 * \param[in] size: 32bits content's size
		 * \return size
		 */
		uint32_t parseSection(uint16_t flag, uint32_t pos, uint32_t size);
};
#endif  // SRC_POFPARSER_HPP_
