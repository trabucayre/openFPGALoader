// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <iostream>
#include <vector>
#include "spiInterface.hpp"
#include "spiFlash.hpp"

using namespace std;

class EPCQ: public SPIFlash {
 public:
 	EPCQ(SPIInterface *spi, int8_t verbose);
	~EPCQ();

	void read_id() override;

	//void program(unsigned int start_offset, string filename, bool reverse=true);
	//int erase_sector(char start_sector, char nb_sectors);
	//void dumpflash(char *dest_file, int size);

	/* not supported */
	virtual void power_up() override {}
	virtual void power_down() override {}

	private:
		unsigned char convertLSB(unsigned char src);
		//void wait_wel();
		//void wait_wip();
		//int do_write_enable();

		/* trash */
		void dumpJICFile(char *jic_file, char *out_file, size_t max_len);

		unsigned char _device_id;
		unsigned char _silicon_id;
};
