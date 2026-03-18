// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019-2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 * Copyright (C) 2022 phdussud
 *
 */

#include "svf_jtag.hpp"

#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <vector>

#include "jtag.hpp"


void SVF_jtag::split_str(std::string const &str, std::vector<std::string> &vparse)
{
	std::string token;
	std::istringstream tokenStream(str);
	while (getline(tokenStream, token, ' '))
		vparse.push_back(token);
}

void SVF_jtag::clear_XYR(svf_XYR &t)
{
	t.len = 0;
	t.tdo.clear();
	t.tdi.clear();
	t.mask.clear();
	t.smask.clear();
}

static unsigned char *parse_hex(std::string const &in, size_t byte_length,
		bool default_value)
{
	unsigned char *txbuf = new unsigned char[byte_length];
	char c;
	ssize_t last_iter = in.size() - (2 * byte_length);
	for (ssize_t i = (in.size() - 1), pos = 0; i >= last_iter; i--, pos++) {
		if (i < 0) {
			c = default_value ? 0x0f : 0x00;
		} else {
			if (in[i] <= '9')
				c = 0x0f & (in[i] - '0');
			else
				c = 0x0f & (in[i] - 'A' + 10);
		}
		if (!(pos & 1))
			txbuf[pos / 2] = c;
		else
			txbuf[pos / 2] |= c << 4;
	}
	return txbuf;
}

/* pas clair:
* si length != previous length : tout est remis a zero
* tdi, mask et smask sont memorises. Si pas present c'est la memoire
* qui est utilise
* tdo si absent on s'en fout
* TODO: ameliorer l'analyse des chaines de caracteres
*/
void SVF_jtag::parse_XYR(std::vector<std::string> const &vstr, svf_XYR &t)
{
	if (_verbose)
		std::cout << std::endl;
	int mode = 0;
	std::string s;
	std::string full_line;
	full_line.reserve(1276);
	int write_data = -1;

	if (vstr[0][0] == 'S')
		write_data = ((vstr[0][1] == 'I') ? 0 : 1);

	uint32_t new_length = stoul(vstr[1]);
	if (new_length != t.len) {
		clear_XYR(t);
	}
	t.len = new_length;
	t.tdo.clear();
	if (t.len == 0)
		return;
	for (size_t pos = 2; pos < vstr.size(); pos++) {
		s = vstr[pos];

		if (!s.compare("TDO")) {
			mode = 1;
			continue;
		} else if (!s.compare("TDI")) {
			mode = 2;
			continue;
		} else if (!s.compare("MASK")) {
			mode = 3;
			continue;
		} else if (!s.compare("SMASK")) {
			mode = 4;
			continue;
		}

		if (s.front() == '(')
			s = s.substr(1);
		if (s.front() == '\t')
			s = s.substr(1);
		if (s.back() == ')')
			s = s.substr(0, s.size() - 1);

		/* faut analyser et convertir le string ici
		 * quand s.back() == ')'
		 */

		full_line += s;
		s.clear();

		if (vstr[pos].back() == ')') {
			switch (mode) {
			case 1:
				t.tdo.clear();
				t.tdo = full_line;
				break;
			case 2:
				t.tdi = full_line;
				break;
			case 3:
				t.mask.clear();
				t.mask = full_line;
				break;
			case 4:
				t.smask.clear();
				t.smask = full_line;
				break;
			}
			full_line.clear();
		}
	}
	if (write_data != -1) {
		size_t byte_len = (t.len + 7) / 8;
		if (byte_len == 0)
			return;
		unsigned char *write_buffer = parse_hex(t.tdi, byte_len, 0);
		if (!t.smask.empty()) {
			unsigned char *smaskbuff = parse_hex(t.smask, byte_len, 0);
			for (unsigned int b = 0; b < byte_len; b++) {
				write_buffer[b] &= smaskbuff[b];
			}
			delete[] smaskbuff;
		}
		unsigned char *read_buffer = NULL;
		if (!t.tdo.empty()) {
			read_buffer = new unsigned char[byte_len];
			read_buffer[byte_len - 1] = 0;  // clear the last byte which may not be full;
		} else {
			read_buffer = NULL;
		}
		if (write_data == 0)
			_jtag->shiftIR(write_buffer, read_buffer, t.len, _endir);
		else
			_jtag->shiftDR(write_buffer, read_buffer, t.len, _enddr);
		if (!t.tdo.empty()) {
			unsigned char *tdobuf = parse_hex(t.tdo, byte_len, 0);
			unsigned char *maskbuf = parse_hex(t.mask, byte_len, t.mask.empty() ? 1 : 0);
			for (size_t i = 0; i < byte_len; i++) {
				if ((read_buffer[i] ^ tdobuf[i]) & maskbuf[i]) {
					std::cerr << "TDO value ";
					for (int j = byte_len - 1; j >= 0; j--) {
						std::cerr << std::uppercase << std::hex << int(read_buffer[j]);
					}
					std::cerr << " isn't the one expected: " << std::uppercase << t.tdo << std::endl;
					delete[] tdobuf;
					delete[] maskbuf;
					throw std::exception();
				}
			}
			delete[] tdobuf;
			delete[] maskbuf;
		}
		delete[] write_buffer;
		delete[] read_buffer;
	}
}

/* Implementation partielle de la spec */
void SVF_jtag::parse_runtest(std::vector<std::string> const &vstr)
{
	unsigned int pos = 1;
	int nb_iter = 0;
	int run_state = -1;
	int end_state = -1;
	double min_duration = -1;
	// 0 => RUNTEST
	// 1 => Ca depend
	if (isalpha(vstr[pos][0])) {
		run_state = fsm_state[vstr[1]];
		pos++;
	}
	if (!vstr[pos + 1].compare("SEC")) {
		min_duration = atof(vstr[pos].c_str());
		pos++;
		pos++;
	} else {
		nb_iter = atoi(vstr[pos].c_str());
		pos++;
		pos++;  // run_clk field, ignored.
		if (((pos + 1) < vstr.size()) &&
			(!vstr[pos + 1].compare("SEC"))) {
			min_duration = atof(vstr[pos].c_str());
			pos++;
			pos++;
		}
	}
	auto res = find(begin(vstr) + pos, end(vstr), "ENDSTATE");
	if (res != end(vstr)) {
		res++;
		end_state = fsm_state[*res];
	}
	if (run_state != -1) {
		_run_state = (Jtag::tapState_t)run_state;
	}
	if (end_state != -1) {
		_end_state = (Jtag::tapState_t)end_state;
	} else if (run_state != -1) {
		_end_state = (Jtag::tapState_t)run_state;
	}
	_jtag->set_state(_run_state);
	_jtag->toggleClk(nb_iter);
	_jtag->flush();
	if (min_duration > 0)
	{
		usleep((useconds_t)(min_duration * 1.0E6));
	}
	_jtag->set_state(_end_state);
	}

void SVF_jtag::handle_instruction(std::vector<std::string> const &vstr)
{
	if (!vstr[0].compare("FREQUENCY")) {
		_freq_hz = atof(vstr[1].c_str());
		if (_verbose) {
			std::cout << "frequency value " << vstr[1] << " unit " << vstr[2];
			std::cout << _freq_hz << std::endl;
		}
		_jtag->setClkFreq(_freq_hz);
	} else if (!vstr[0].compare("TRST")) {
		if (_verbose) std::cout << "trst value : " << vstr[1] << std::endl;
	} else if (!vstr[0].compare("ENDDR")) {
		if (_verbose) std::cout << "enddr value : " << vstr[1] << std::endl;
		_enddr = (Jtag::tapState_t)fsm_state[vstr[1]];
	} else if (!vstr[0].compare("ENDIR")) {
		if (_verbose) std::cout << "endir value : " << vstr[1] << std::endl;
		_endir = (Jtag::tapState_t)fsm_state[vstr[1]];
	} else if (!vstr[0].compare("STATE")) {
		if (_verbose) std::cout << "state value : " << vstr[1] << std::endl;
		_jtag->set_state((Jtag::tapState_t)fsm_state[vstr[1]]);
	} else if (!vstr[0].compare("RUNTEST")) {
		parse_runtest(vstr);
	} else if (!vstr[0].compare("HIR")) {
		parse_XYR(vstr, hir);
		if (hir.len > 0) {
			std::cerr << "HIR length supported is only 0 " << std::endl;
		}
		if (_verbose) {
			std::cout << "HIR" << std::endl;
			std::cout << "\tlen   : " << hir.len << std::endl;
			std::cout << "\ttdo   : " << hir.tdo.size()*4 << std::endl;
			std::cout << "\ttdi   : " << hir.tdi.size()*4 << std::endl;
			std::cout << "\tmask  : " << hir.mask.size()*4 << std::endl;
			std::cout << "\tsmask : " << hir.smask.size()*4 << std::endl;
		}
	} else if (!vstr[0].compare("HDR")) {
		parse_XYR(vstr, hdr);
		if (hdr.len > 0) {
			std::cerr << "HDR length supported is only 0" << std::endl;
		}
		if (_verbose) {
			std::cout << "HDR" << std::endl;
			std::cout << "\tlen   : " << hdr.len << std::endl;
			std::cout << "\ttdo   : " << hdr.tdo.size()*4 << std::endl;
			std::cout << "\ttdi   : " << hdr.tdi.size()*4 << std::endl;
			std::cout << "\tmask  : " << hdr.mask.size()*4 << std::endl;
			std::cout << "\tsmask : " << hdr.smask.size()*4 << std::endl;
		}
	} else if (!vstr[0].compare("SIR")) {
		parse_XYR(vstr, sir);
		if (_verbose) {
			for (auto &&t : vstr)
				std::cout << t << " ";
			std::cout << std::endl;
			std::cout << "\tlen   : " << sir.len << std::endl;
			std::cout << "\ttdo   : " << sir.tdo.size()*4 << std::endl;
			std::cout << "\ttdi   : " << sir.tdi.size()*4 << std::endl;
			std::cout << "\tmask  : " << sir.mask.size()*4 << std::endl;
			std::cout << "\tsmask : " << sir.smask.size()*4 << std::endl;
		}
	} else if (!vstr[0].compare("SDR")) {
		parse_XYR(vstr, sdr);
		if (_verbose) {
			std::cout << "SDR" << std::endl;
			std::cout << "\tlen   : " << sdr.len << std::endl;
			std::cout << "\ttdo   : " << sdr.tdo.size()*4 << std::endl;
			std::cout << "\ttdi   : " << sdr.tdi.size()*4 << std::endl;
			std::cout << "\tmask  : " << sdr.mask.size()*4 << std::endl;
			std::cout << "\tsmask : " << sdr.smask.size()*4 << std::endl;
		}
	} else if (!vstr[0].compare("TDR")) {
		parse_XYR(vstr, tdr);
		if (tdr.len > 0) {
			std::cerr << "TDR length supported is only 0" << std::endl;
		}
		if (_verbose) {
			std::cout << "TDR" << std::endl;
			std::cout << "\tlen   : " << tdr.len << std::endl;
			std::cout << "\ttdo   : " << tdr.tdo.size() * 4 << std::endl;
			std::cout << "\ttdi   : " << tdr.tdi.size() * 4 << std::endl;
			std::cout << "\tmask  : " << tdr.mask.size() * 4 << std::endl;
			std::cout << "\tsmask : " << tdr.smask.size() * 4 << std::endl;
		}
	} else if (!vstr[0].compare("TIR")) {
		parse_XYR(vstr, tir);
		if (tir.len > 0) {
			std::cerr << "TIR length supported is only 0" << std::endl;
		}
		if (_verbose) {
			std::cout << "TIR" << std::endl;
			std::cout << "\tlen   : " << tir.len << std::endl;
			std::cout << "\ttdo   : " << tir.tdo.size() * 4 << std::endl;
			std::cout << "\ttdi   : " << tir.tdi.size() * 4 << std::endl;
			std::cout << "\tmask  : " << tir.mask.size() * 4 << std::endl;
			std::cout << "\tsmask : " << tir.smask.size() * 4 << std::endl;
		}
	} else {
		std::cout << "error: unhandled instruction " << vstr[0] << std::endl;
		throw std::exception();
	}
}

SVF_jtag::SVF_jtag(Jtag *jtag, bool verbose):_verbose(verbose), _freq_hz(0),
	_enddr((Jtag::tapState_t)fsm_state["IDLE"]), _endir((Jtag::tapState_t)fsm_state["IDLE"]),
	_run_state((Jtag::tapState_t)fsm_state["IDLE"]), _end_state((Jtag::tapState_t)fsm_state["IDLE"])

{
	_jtag = jtag;
	_jtag->go_test_logic_reset();
}

SVF_jtag::~SVF_jtag() {}

bool is_space(char x) {
	return !!isspace(x);
}
/* Read SVF file line by line
 * concat continuous lines
 * and pass instruction to handle_instruction
 */
void SVF_jtag::parse(std::string filename)
{
	std::string str;
	std::vector<std::string> vstr;
	bool is_complete;
	std::ifstream fs;

	fs.open(filename);
	if (!fs.is_open()) {
		std::cerr << "Error opening svf file " << filename << std::endl;
		return;
	}
	unsigned int lineno = 0;
	try	{
		while (getline(fs, str)) {
			/* sanity check: DOS CR */
			if (str.back() == '\r')
				str.pop_back();
			lineno++;
			is_complete = false;
			if (str[0] == '!')  // comment
				continue;
			if (str.back() == ';') {
				str.pop_back();
				is_complete = true;
			}
			replace_if(begin(str), end(str), is_space, ' ');
			split_str(str, vstr);
			if (is_complete) {
				if (_verbose) {
					if (vstr[0].compare("HDR") && vstr[0].compare("HIR")
						&& vstr[0].compare("SDR") && vstr[0].compare("SIR")) {
						for (auto &&word : vstr)
							std::cout << word << " ";
						std::cout << std::endl;
					}
				}
				handle_instruction(vstr);
				vstr.clear();
			}
		}
	}
	catch (std::exception &e)
	{
		std::cerr << "Cannot proceed because of error(s) at line " << lineno << std::endl;
		throw;
	}

	std::cout << "end of SVF file" << std::endl;
}
