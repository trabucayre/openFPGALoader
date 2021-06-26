// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <vector>

#include "jtag.hpp"

#include "svf_jtag.hpp"

using namespace std;

void SVF_jtag::split_str(string const &str, vector<string> &vparse)
{
	string token;
	std::istringstream tokenStream(str);
	while (std::getline(tokenStream, token, ' '))
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


/* pas clair:
 * si length = 0 : tout est remis a zero
 * tdi, mask et smask sont memorises. Si pas present c'est la memoire
 * qui est utilise
 * tdo si absent on s'en fout
 * TODO: faut prendre en compte smask, mask and tdo
 *       ameliorer l'analyse des chaines de caracteres
 */
void SVF_jtag::parse_XYR(vector<string> const &vstr, svf_XYR &t)
{
	if (_verbose) cout << endl;
	int mode = 0;
	string s;
	//string tdi;
	string full_line;
	full_line.reserve(1276);
	int write_data = -1;

	if (vstr[0][0] == 'S')
		write_data = ((vstr[0][1] == 'I') ? 0 : 1);

	t.len = stoul(vstr[1]);
	if (t.len == 0) {
		clear_XYR(t);
		return;
	}

	for (long unsigned int pos=2; pos < vstr.size(); pos++) {
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
			s = s.substr(0, s.size()-1);

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
				t.mask= full_line;
				break;
			case 4:
				t.smask.clear();
				t.smask= full_line;
				break;
			}
			full_line.clear();
		}
	}
	if (write_data != -1) {
		string txbuf;
		int len = t.tdi.size() / 2 + ((t.tdi.size() % 2)? 1 : 0);
		txbuf.resize(len);
		char c;
		for (int i = t.tdi.size()-1, pos = 0; i >= 0; i--, pos++) {
			if (t.tdi[i] <= '9')
				c = 0x0f & (t.tdi[i] - '0'); 
			else
				c = 0x0f & (t.tdi[i] - 'A' + 10);

			txbuf[pos/2] |= ((0x0F & c) << ((4*(pos & 1))));
		}

		if (write_data == 0)
			_jtag->shiftIR((unsigned char *)txbuf.c_str(), NULL, t.len, _endir);
		else
			_jtag->shiftDR((unsigned char *)txbuf.c_str(), NULL, t.len, _enddr);
	}
}

/* Implementation partielle de la spec */
void SVF_jtag::parse_runtest(vector<string> const &vstr)
{
	int pos = 1;
	int nb_iter = 0;
	int run_state = -1;
	int end_state = -1;
// 0 => RUNTEST
// 1 => Ca depend
	if (vstr[pos][0] > '9') { 
		run_state = fsm_state[vstr[1]];
		pos++;
	}
	nb_iter = atoi(vstr[pos].c_str()); // duree mais attention ca peut etre un xxeyy
	pos++;
	pos++; // clk currently don't care
	if (!vstr[pos].compare("ENDSTATE")) {
		pos++;
		end_state = fsm_state[vstr[pos]];
	}

	if (run_state != -1) {
		_run_state = run_state;
	}
	if (end_state != -1) {
		_end_state = end_state;
	}
	else if (run_state != -1)
		_end_state = run_state;
	_jtag->set_state(_run_state);
	_jtag->toggleClk(nb_iter);
	_jtag->set_state(_end_state);
}

void SVF_jtag::handle_instruction(vector<string> const &vstr)
{
	if (!vstr[0].compare("FREQUENCY")) {
		_freq_hz = atof(vstr[1].c_str());
		if (_verbose) {
			cout << "frequence valeur " << vstr[1] << " unite " << vstr[2];
			cout << _freq_hz << endl;
		}
		_jtag->setClkFreq(_freq_hz);
	} else if (!vstr[0].compare("TRST")) {
		if (_verbose) cout << "trst value : " << vstr[1] << endl;
	} else if (!vstr[0].compare("ENDDR")) {
		if (_verbose) cout << "enddr value : " << vstr[1] << endl;
		_enddr = fsm_state[vstr[1]];
	} else if (!vstr[0].compare("ENDIR")) {
		if (_verbose) cout << "endir value : " << vstr[1] << endl;
		_endir = fsm_state[vstr[1]];
	} else if (!vstr[0].compare("STATE")) {
		if (_verbose) cout << "state value : " << vstr[1] << endl;
		_jtag->set_state(fsm_state[vstr[1]]);
	} else if (!vstr[0].compare("RUNTEST")) {
		parse_runtest(vstr);
	} else if (!vstr[0].compare("HIR")) {
		parse_XYR(vstr, hir);
		if (_verbose) {
			cout << "HIR" << endl;
			cout << "\tlen   : " << hir.len << endl;
			cout << "\ttdo   : " << hir.tdo.size()*4 << endl;
			cout << "\ttdi   : " << hir.tdi.size()*4 << endl;
			cout << "\tmask  : " << hir.mask.size()*4 << endl;
			cout << "\tsmask : " << hir.smask.size()*4 << endl;
		}
	} else if (!vstr[0].compare("HDR")) {
		parse_XYR(vstr, hdr);
		if (_verbose) {
			cout << "HDR" << endl;
			cout << "\tlen   : " << hdr.len << endl;
			cout << "\ttdo   : " << hdr.tdo.size()*4 << endl;
			cout << "\ttdi   : " << hdr.tdi.size()*4 << endl;
			cout << "\tmask  : " << hdr.mask.size()*4 << endl;
			cout << "\tsmask : " << hdr.smask.size()*4 << endl;
		}
	} else if (!vstr[0].compare("SIR")) {
		parse_XYR(vstr, sir);
		if (_verbose) {
			for (auto &&t: vstr)
			 	cout << t << " ";
			cout << endl;
			cout << "\tlen   : " << sir.len << endl;
			cout << "\ttdo   : " << sir.tdo.size()*4 << endl;
			cout << "\ttdi   : " << sir.tdi.size()*4 << endl;
			cout << "\tmask  : " << sir.mask.size()*4 << endl;
			cout << "\tsmask : " << sir.smask.size()*4 << endl;
		}
	} else if (!vstr[0].compare("SDR")) {
		parse_XYR(vstr, sdr);
		if (_verbose) {
			cout << "SDR" << endl;
			cout << "\tlen   : " << sdr.len << endl;
			cout << "\ttdo   : " << sdr.tdo.size()*4 << endl;
			cout << "\ttdi   : " << sdr.tdi.size()*4 << endl;
			cout << "\tmask  : " << sdr.mask.size()*4 << endl;
			cout << "\tsmask : " << sdr.smask.size()*4 << endl;
		}
	} else {
		cout << "error: unhandled instruction : " << vstr[0] << endl;
	}
}

SVF_jtag::SVF_jtag(Jtag *jtag, bool verbose):_verbose(verbose), _freq_hz(0),
	_enddr(fsm_state["IDLE"]), _endir(fsm_state["IDLE"]),
	_run_state(fsm_state["IDLE"]), _end_state(fsm_state["IDLE"])

{
	_jtag = jtag;
	_jtag->go_test_logic_reset();
}

SVF_jtag::~SVF_jtag() {}

/* Read SVF file line by line
 * concat continuous lines
 * and pass instruction to handle_instruction
 */
void SVF_jtag::parse(string filename) 
{
	string str;
	vector<string> vstr;
	bool is_complete;
	ifstream fs;

	fs.open(filename);
	if (!fs.is_open()) {
		cerr << "error to opening svf file " << filename << endl;
		return;
	}
	
	while (getline(fs, str)) {
		/* sanity check: DOS CR */
		if (str.back() == '\r')
			str.pop_back();

		is_complete = false;
		if (str[0] == '!') // comment
			continue;
		if (str.back() == ';') {
			str.pop_back();
			is_complete = true;
		}

		split_str(str, vstr);
		if (is_complete) {
			if (_verbose) {
				if (vstr[0].compare("HDR") && vstr[0].compare("HIR")
					&& vstr[0].compare("SDR") && vstr[0].compare("SIR")) {
					for (auto &&word: vstr)
						cout << word << " ";
					cout << endl;
				}
			}
			handle_instruction(vstr);
			vstr.clear();
		}
	}

	cout << "end of flash" << endl;

}
