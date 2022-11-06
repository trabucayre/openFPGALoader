// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019-2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_SVF_JTAG_HPP_
#define SRC_SVF_JTAG_HPP_
#include <iostream>
#include <string>
#include <vector>
#include <map>

#include "jtag.hpp"
using namespace std;

class SVF_jtag {
 public:
	SVF_jtag(Jtag *jtag, bool verbose);
	~SVF_jtag();
	void parse(string filename);
	void setVerbose(bool verbose) {_verbose = verbose;}

	private:
	typedef struct {
		uint32_t len;
		string tdo;
		string tdi;
		string mask;
		string smask;
	} svf_XYR;

		void split_str(string const &str, vector<string> &vparse);
		void clear_XYR(svf_XYR &t);
		void parse_XYR(vector<string> const &vstr/*, svf_stat &svfs*/, svf_XYR &t);
		void parse_runtest(vector<string> const &vstr);
		void handle_instruction(vector<string> const &vstr);

	map <string, uint8_t> fsm_state = {
		{"RESET", 0},
		{"IDLE", 1},
		{"DRSELECT", 2},
		{"DRCAPTURE", 3},
		{"DRSHIFT", 4},
		{"DREXIT1", 5},
		{"DRPAUSE", 6},
		{"DREXIT2", 7},
		{"DRUPDATE", 8},
		{"IRSELECT", 9},
		{"IRCAPTURE", 10},
		{"IRSHIFT", 11},
		{"IREXIT1", 12},
		{"IRPAUSE", 13},
		{"IREXIT2", 14},
		{"IRUPDATE", 15}
	};

	Jtag *_jtag;
	bool _verbose;

	uint32_t _freq_hz;
	int _enddr;
	int _endir;
	int _run_state;
	int _end_state;
	svf_XYR hdr;
	svf_XYR hir;
	svf_XYR sdr;
	svf_XYR sir;
	svf_XYR tdr;
	svf_XYR tir;
};
#endif  // SRC_SVF_JTAG_HPP_
