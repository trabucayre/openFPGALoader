// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_EPCQ_HPP_
#define SRC_EPCQ_HPP_

#include <iostream>
#include <vector>
#include "spiInterface.hpp"
#include "spiFlash.hpp"

using namespace std;

class EPCQ: public SPIFlash {
 public:
	EPCQ(SPIInterface *spi, bool unprotect_flash, int8_t verbose);
	~EPCQ();

	void read_id() override;

	void reset() override;

	/* not supported */
	void power_up() override {}
	void power_down() override {}

	private:
		unsigned char convertLSB(unsigned char src);

		/* trash */
		void dumpJICFile(char *jic_file, char *out_file, size_t max_len);

		unsigned char _device_id;
		unsigned char _silicon_id;
};

#endif  // SRC_EPCQ_HPP_
