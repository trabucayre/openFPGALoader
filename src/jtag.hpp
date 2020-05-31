/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef JTAG_H
#define JTAG_H
#include <ftdi.h>
#include <iostream>
#include <string>
#include <vector>

#include "board.hpp"
#include "cable.hpp"
#include "ftdipp_mpsse.hpp"
#include "jtagInterface.hpp"

class Jtag {
 public:
	Jtag(cable_t &cable, const jtag_pins_conf_t *pin_conf, std::string dev,
		uint32_t clkHZ, bool verbose = false);
	Jtag(cable_t &cable, const jtag_pins_conf_t *pin_conf,
		uint32_t clkHZ, bool verbose);
	~Jtag();

	/* maybe to update */
	int setClkFreq(uint32_t clkHZ) { return _jtag->setClkFreq(clkHZ);}
	int setClkFreq(uint32_t clkHZ, char use_divide_by_5) {
		return _jtag->setClkFreq(clkHZ, use_divide_by_5);}

	int detectChain(std::vector<int> &devices, int max_dev);

	int shiftIR(unsigned char *tdi, unsigned char *tdo, int irlen,
		int end_state = RUN_TEST_IDLE);
	int shiftIR(unsigned char tdi, int irlen,
		int end_state = RUN_TEST_IDLE);
	int shiftDR(unsigned char *tdi, unsigned char *tdo, int drlen,
		int end_state = RUN_TEST_IDLE);
	int read_write(unsigned char *tdi, unsigned char *tdo, int len, char last);

	void toggleClk(int nb);
	void go_test_logic_reset();
	void set_state(int newState);
	int flushTMS(bool flush_buffer = false);
	void flush() {flushTMS(); _jtag->flush();}
	void setTMS(unsigned char tms);

	enum tapState_t {
		TEST_LOGIC_RESET = 0,
		RUN_TEST_IDLE = 1,
		SELECT_DR_SCAN = 2,
		CAPTURE_DR = 3,
		SHIFT_DR = 4,
		EXIT1_DR = 5,
		PAUSE_DR = 6,
		EXIT2_DR = 7,
		UPDATE_DR = 8,
		SELECT_IR_SCAN = 9,
		CAPTURE_IR = 10,
		SHIFT_IR = 11,
		EXIT1_IR = 12,
		PAUSE_IR = 13,
		EXIT2_IR = 14,
		UPDATE_IR = 15,
		UNKNOWN = 999
	};
	const char *getStateName(tapState_t s);

	/* utilities */
	void setVerbose(bool verbose){_verbose = verbose;}

 private:
	void init_internal(cable_t &cable, const std::string &dev,
		const jtag_pins_conf_t *pin_conf, uint32_t clkHZ);
	bool _verbose;
	int _state;
	int _tms_buffer_size;
	int _num_tms;
	unsigned char *_tms_buffer;
	std::string _board_name;
	JtagInterface *_jtag;
};
#endif
