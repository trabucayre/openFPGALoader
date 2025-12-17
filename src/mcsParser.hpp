// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019-2025 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef MCSPARSER_HPP
#define MCSPARSER_HPP

#include <string>
#include <vector>
#include <memory>

#include "configBitstreamParser.hpp"
#include "spiFlash.hpp"

class McsParser: public ConfigBitstreamParser {
	public:
		McsParser(const std::string &filename, bool reverseOrder, bool verbose);
		int parse() override;

		size_t getRecordCount() const noexcept { return _records.size(); }
		const FlashDataSection &getRecord(size_t record_id) const { return _records.at(record_id); }
		const std::vector<uint8_t> &getRecordData(size_t record_id) const { return _records.at(record_id).getRecord(); }
		uint32_t getRecordBaseAddr(size_t record_id) const { return _records.at(record_id).getStartAddr(); }
		size_t getRecordLength(size_t record_id) const { return _records.at(record_id).getLength(); }
		const std::vector<FlashDataSection> &getRecords() const noexcept { return _records; }

	private:
		int _base_addr;
		bool _reverseOrder;
		std::vector<FlashDataSection> _records;
};

#endif

