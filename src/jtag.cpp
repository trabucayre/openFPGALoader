// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <unistd.h>
#include <vector>
#include <string>

#include "anlogicCable.hpp"
#include "ch552_jtag.hpp"
#include "display.hpp"
#include "jtag.hpp"
#include "ftdiJtagBitbang.hpp"
#include "ftdiJtagMPSSE.hpp"
#ifdef ENABLE_LIBGPIOD
#include "libgpiodJtagBitbang.hpp"
#endif
#ifdef ENABLE_JETSONNANOGPIO
#include "jetsonNanoJtagBitbang.hpp"
#endif
#include "jlink.hpp"
#ifdef ENABLE_CMSISDAP
#include "cmsisDAP.hpp"
#endif
#include "dirtyJtag.hpp"
#include "ch347jtag.hpp"
#include "part.hpp"
#ifdef ENABLE_REMOTEBITBANG
#include "remoteBitbang_client.hpp"
#endif
#include "usbBlaster.hpp"
#ifdef ENABLE_XVC
#include "xvc_client.hpp"
#endif

using namespace std;

#define DEBUG 0

#if DEBUG
#define display(...) \
	do { if (_verbose) fprintf(stdout, __VA_ARGS__);}while(0)
#else
#define display(...) do {}while(0)
#endif

/*
 * AD0 -> TCK
 * AD1 -> TDI
 * AD2 -> TD0
 * AD3 -> TMS
 */

/* Rmq:
 * pour TMS: l'envoi de n necessite de mettre n-1 comme longueur
 *           mais le bit n+1 est utilise pour l'etat suivant le dernier
 *           front. Donc il faut envoyer 6bits ([5:0]) pertinents pour
 *           utiliser le bit 6 comme etat apres la commande,
 *           le bit 7 corresponds a l'etat de TDI (donc si on fait 7 cycles
 *           l'etat de TDI va donner l'etat de TMS...)
 * transfert/lecture: le dernier bit de IR ou DR doit etre envoye en
 *           meme temps que le TMS qui fait sortir de l'etat donc il faut
 *           pour n bits a transferer :
 *           - envoyer 8bits * (n/8)-1
 *           - envoyer les 7 bits du dernier octet;
 *           - envoyer le dernier avec 0x4B ou 0x6B
 */

Jtag::Jtag(const cable_t &cable, const jtag_pins_conf_t *pin_conf,
			const string &dev,
			const string &serial, uint32_t clkHZ, int8_t verbose,
			const string &ip_adr, int port,
			const bool invert_read_edge, const string &firmware_path):
			_verbose(verbose > 1),
			_state(UNKNOWN),
			_board_name("nope"), device_index(0),
			_txff(nullptr)
{
	switch (cable.type) {
	case MODE_ANLOGICCABLE:
		_jtag = new AnlogicCable(clkHZ);
		break;
	case MODE_FTDI_BITBANG:
		if (pin_conf == NULL)
			throw std::exception();
		_jtag = new FtdiJtagBitBang(cable, pin_conf, dev, serial, clkHZ, verbose);
		break;
	case MODE_FTDI_SERIAL:
		_jtag = new FtdiJtagMPSSE(cable, dev, serial, clkHZ,
				invert_read_edge, verbose);
		break;
	case MODE_CH552_JTAG:
		_jtag = new CH552_jtag(cable, dev, serial, clkHZ, verbose);
		break;
	case MODE_CH347:
		_jtag = new CH347Jtag(clkHZ, verbose);
		break;
	case MODE_DIRTYJTAG:
		_jtag = new DirtyJtag(clkHZ, verbose);
		break;
	case MODE_JLINK:
		_jtag = new Jlink(clkHZ, verbose, cable.vid, cable.pid);
		break;
	case MODE_USBBLASTER:
		_jtag = new UsbBlaster(cable, firmware_path, verbose);
		break;
	case MODE_CMSISDAP:
#ifdef ENABLE_CMSISDAP
		_jtag = new CmsisDAP(cable, cable.config.index, verbose);
		break;
#else
		std::cerr << "Jtag: support for cmsisdap was not enabled at compile time" << std::endl;
		throw std::exception();
#endif
	case MODE_XVC_CLIENT:
#ifdef ENABLE_XVC
		_jtag = new XVC_client(ip_adr, port, clkHZ, verbose);
		break;
#else
		std::cerr << "Jtag: support for xvc-client was not enabled at compile time" << std::endl;
		throw std::exception();
#endif
#ifdef ENABLE_LIBGPIOD
	case MODE_LIBGPIOD_BITBANG:
		_jtag = new LibgpiodJtagBitbang(pin_conf, dev, clkHZ, verbose);
		break;
#endif
#ifdef ENABLE_JETSONNANOGPIO
	case MODE_JETSONNANO_BITBANG:
		_jtag = new JetsonNanoJtagBitbang(pin_conf, dev, clkHZ, verbose);
		break;
#endif
#ifdef ENABLE_REMOTEBITBANG
	case MODE_REMOTEBITBANG:
		_jtag = new RemoteBitbang_client(ip_adr, port, verbose);
		break;
#endif
	default:
		std::cerr << "Jtag: unknown cable type" << std::endl;
		throw std::exception();
	}
	
	create_paths();

	detectChain(5);
}

Jtag::~Jtag()
{
	delete _jtag;
}

constexpr static Jtag::tapState_t edges[16][2] = {
    {Jtag::RUN_TEST_IDLE, Jtag::TEST_LOGIC_RESET}, // TEST_LOGIC_RESET
    {Jtag::RUN_TEST_IDLE, Jtag::SELECT_DR_SCAN}, // RUN_TEST_IDLE
    
    {Jtag::CAPTURE_DR, Jtag::SELECT_IR_SCAN}, // SELECT_DR_SCAN
    {Jtag::SHIFT_DR, Jtag::EXIT1_DR}, // CAPTURE_DR
    {Jtag::SHIFT_DR, Jtag::EXIT1_DR}, // SHIFT_DR
    {Jtag::PAUSE_DR, Jtag::UPDATE_DR}, // EXIT1_DR
    {Jtag::PAUSE_DR, Jtag::EXIT2_DR}, // PAUSE_DR
    {Jtag::SHIFT_DR, Jtag::UPDATE_DR}, // EXIT2_DR
    {Jtag::RUN_TEST_IDLE, Jtag::SELECT_DR_SCAN}, // UPDATE_DR
    
    {Jtag::CAPTURE_IR, Jtag::TEST_LOGIC_RESET}, // SELECT_IR_SCAN
    {Jtag::SHIFT_IR, Jtag::EXIT1_IR}, // CAPTURE_IR
    {Jtag::SHIFT_IR, Jtag::EXIT1_IR}, // SHIFT_IR
    {Jtag::PAUSE_IR, Jtag::UPDATE_IR}, // EXIT1_IR
    {Jtag::PAUSE_IR, Jtag::EXIT2_IR}, // PAUSE_IR
    {Jtag::SHIFT_IR, Jtag::UPDATE_IR}, // EXIT2_IR
    {Jtag::RUN_TEST_IDLE, Jtag::SELECT_DR_SCAN}, // UPDATE_IR, same as UPDATE_DR
};

void Jtag::dive(unsigned origin, unsigned prev, const Path &p) {
	for (unsigned i = 0; i < 2; ++i) {
		unsigned x = edges[prev][i];
		if (paths[origin][x].len <= p.len) continue;
		Path pn;
		pn.len = p.len + 1;
		pn.tms_bits = p.tms_bits | i << p.len;
		
		paths[origin][x] = pn;
		dive(origin, x, pn);
	}
}

constexpr static const char *getStateName[16] = {
    "TEST_LOGIC_RESET",
    "RUN_TEST_IDLE",
    "SELECT_DR_SCAN",
    "CAPTURE_DR",
    "SHIFT_DR",
    "EXIT1_DR",
    "PAUSE_DR",
    "EXIT2_DR",
    "UPDATE_DR",
    "SELECT_IR_SCAN",
    "CAPTURE_IR",
    "SHIFT_IR",
    "EXIT1_IR",
    "PAUSE_IR",
    "EXIT2_IR",
    "UPDATE_IR",
};

void Jtag::print_path(tapState_t from, tapState_t to) {
	uint8_t path = paths[from][to].tms_bits;
	printf ("Start: %s[%d]\n", getStateName[from], from);
	tapState_t x = from;
	for (unsigned i = 0; i < paths[from][to].len; ++i) {
		unsigned step = path & 1;
		path >>= 1;
		x = edges[x][step];
		printf("\t%d:TMS=%d: -> %s[%d]\n", i + 1, step, getStateName[x], x);
	}
	printf("Should arrive at %s[%d]\n", getStateName[x], x);
}

void Jtag::create_paths() {
	uint8_t max_len = 0;
	for (unsigned i = 0; i < 16; ++i) {
		for (unsigned j = 0; j < 16; ++j) {
			paths[i][j].len = 0xff;
			paths[i][j].tms_bits = 0;
		}
		Path p = {0, 0};
		paths[i][i] = p;
		dive(i, i, p);
		for (unsigned j = 0; j < 16; ++j) {
			max_len = std::max(max_len, paths[i][j].len);
		}
	}
#if 0
	printf("max tms sequence is %d\n", max_len);
	print_path(TEST_LOGIC_RESET, RUN_TEST_IDLE);
	print_path(RUN_TEST_IDLE, SHIFT_IR);
	print_path(RUN_TEST_IDLE, SHIFT_DR);
	print_path(SHIFT_IR, RUN_TEST_IDLE);
	print_path(SHIFT_DR, PAUSE_DR);
	print_path(PAUSE_DR, SHIFT_DR);
	print_path(SHIFT_IR, PAUSE_IR);
	print_path(PAUSE_IR, SHIFT_DR);
	print_path(PAUSE_IR, RUN_TEST_IDLE);
#endif
}

int Jtag::detectChain(int max_dev)
{
	char message[256];
	unsigned char rx_buff[4];
	/* WA for CH552/tangNano: write is always mandatory */
	unsigned char tx_buff[4] = {0xff, 0xff, 0xff, 0xff};
	unsigned int tmp;

	/* cleanup */
	_devices_list.clear();
	_irlength_list.clear();
	_ir_bits_before = _ir_bits_after = _dr_bits_before = _dr_bits_after = 0;
	go_test_logic_reset();
	set_state(SHIFT_DR);

	if (_verbose)
		printInfo("Raw IDCODE:");

	for (int i = 0; i < max_dev; i++) {
		read_write(tx_buff, rx_buff, 32, (i == max_dev-1)?1:0);
		tmp = 0;
		for (int ii=0; ii < 4; ii++)
			tmp |= (rx_buff[ii] << (8*ii));

		if (_verbose) {
			snprintf(message, sizeof(message), "- %d -> 0x%08x", i, tmp);
			printInfo(message);
		}

		/* search IDCODE in fpga_list and misc_dev_list
		 * since most device have idcode with high nibble masked
		 * we start to search sub IDCODE
		 * if IDCODE has no match: try the same with version unmasked
		 */
		if (tmp != 0 && tmp != 0xffffffff) {
			bool found = false;
			/* ckeck highest nibble to prevent confusion between Cologne Chip
			 * GateMate and Efinix Trion T4/T8 devices
			 */
			if (tmp != 0x20000001)
				found = search_and_insert_device_with_idcode(tmp & 0x0fffffff);
			if (!found) /* if masked not found -> search for full */
				found = search_and_insert_device_with_idcode(tmp);

			if (!found) {
				uint16_t mfg = IDCODE2MANUFACTURERID(tmp);
				uint8_t part = IDCODE2PART(tmp);
				uint8_t vers = IDCODE2VERS(tmp);

				char error[1024];
				snprintf(error, sizeof(error),
						"Unknown device with IDCODE: 0x%08x"
						" (manufacturer: 0x%03x (%s),"
						" part: 0x%02x vers: 0x%x", tmp,
						mfg, list_manufacturer[mfg].c_str(), part, vers);
				throw std::runtime_error(error);
			}
		}
	}
	go_test_logic_reset();
	return _devices_list.size();
}

bool Jtag::search_and_insert_device_with_idcode(uint32_t idcode)
{
	int irlength = -1;
	auto dev = fpga_list.find(idcode);
	if (dev != fpga_list.end())
		irlength = dev->second.irlength;
	if (irlength == -1) {
		auto misc = misc_dev_list.find(idcode);
		if (misc != misc_dev_list.end())
			irlength = misc->second.irlength;
	}
	if (irlength == -1)
		return false;

	return insert_first(idcode, irlength);
}

bool Jtag::insert_first(uint32_t device_id, uint16_t irlength)
{
	_devices_list.insert(_devices_list.begin(), device_id);
	_irlength_list.insert(_irlength_list.begin(), irlength);

	return true;
}

int Jtag::device_select(unsigned index)
{
	if (index > _devices_list.size())
		return -1;
	device_index = index;
	/* get number of devices, in the JTAG chain,
	 * before the selected one
	 */
	_dr_bits_before = _devices_list.size() - device_index - 1;
	/* get number of devices in the JTAG chain
	 * after the selected one
	 */
	_dr_bits_after = device_index;
	unsigned max_bits = std::max(_dr_bits_after, _dr_bits_before);
	/* when the device is not alone and not
	 * the first a serie of bypass must be
	 * send to complete send ir sequence
	 */
	_ir_bits_after = 0;
	for (int i = 0; i < device_index; ++i)
		_ir_bits_after += _irlength_list[i];

	max_bits = std::max(_ir_bits_after, max_bits);

	/* send serie of bypass instructions
	 * final size depends on number of device
	 * before targeted and irlength of each one
	 */
	_ir_bits_before = 0;
	for (unsigned i = device_index + 1; i < _devices_list.size(); ++i)
		_ir_bits_before += _irlength_list[i];
	max_bits = std::max(_ir_bits_before, max_bits);

	if (_txff) delete[] _txff;

	_txff = nullptr;

	unsigned size = (max_bits + 7) / 8;
	if (size) {
		_txff = new uint8_t[size];
		memset(_txff, 0xff, sizeof(size));
	}
	return device_index;
}

void Jtag::go_test_logic_reset()
{
	/* independently to current state 5 clk with TMS high is enough */
	uint8_t bits = 0xff;
	_jtag->writeTMS(&bits, 5, false);
	_state = TEST_LOGIC_RESET;
}

int Jtag::read_write(const uint8_t *tdi, unsigned char *tdo, int len, char last)
{
	_jtag->writeTDI(tdi, tdo, len, last);
	if (last == 1)
		_state = (_state == SHIFT_DR) ? EXIT1_DR : EXIT1_IR;
	return 0;
}

void Jtag::toggleClk(int nb)
{
	uint8_t c = (TEST_LOGIC_RESET == _state) ? 1 : 0;
	if (_jtag->toggleClk(c, 0, nb) >= 0)
		return;
	throw std::exception();
	return;
}

int Jtag::shiftDR(const uint8_t *tdi, unsigned char *tdo, int drlen, tapState_t end_state)
{
	/* if current state not shift DR
	 * move to this state
	 */
	if (_state != SHIFT_DR) {
		set_state(SHIFT_DR);

		if (_dr_bits_before)
			read_write(_txff, NULL, _dr_bits_before, false);
	}

	/* write tdi (and read tdo) to the selected device
	 * end (ie TMS high) is used only when current device
	 * is the last of the chain and a state change must
	 * be done
	 */
	read_write(tdi, tdo, drlen, _dr_bits_after == 0 && end_state != SHIFT_DR);

	/* if it's asked to move in FSM */
	if (end_state != SHIFT_DR) {
		/* if current device is not the last */
		if (_dr_bits_after)
			read_write(_txff, NULL, _dr_bits_after, true);  // its the last force
								   // tms high with last bit


		/* move to end_state */
		set_state(end_state);
	}
	return 0;
}

int Jtag::shiftIR(unsigned char tdi, int irlen, tapState_t end_state)
{
	if (irlen > 8) {
		cerr << "Error: this method this direct char don't support more than 1 byte" << endl;
		return -1;
	}
	return shiftIR(&tdi, NULL, irlen, end_state);
}

int Jtag::shiftIR(unsigned char *tdi, unsigned char *tdo, int irlen, tapState_t end_state)
{
	display("%s: avant shiftIR\n", __func__);

	/* if not in SHIFT IR move to this state */
	if (_state != SHIFT_IR) {
		set_state(SHIFT_IR);
		if (_ir_bits_before)
			read_write(_txff, NULL, _ir_bits_before, false);
	}

	display("%s: envoi ircode\n", __func__);

	/* write tdi (and read tdo) to the selected device
	 * end (ie TMS high) is used only when current device
	 * is the last of the chain and a state change must
	 * be done
	 */
	read_write(tdi, tdo, irlen, _ir_bits_after == 0 && end_state != SHIFT_IR);

	/* it's asked to move out of SHIFT IR state */
	if (end_state != SHIFT_IR) {
		/* again if devices after fill '1' */
		if (_ir_bits_after > 0)
			read_write(_txff, NULL, _ir_bits_after, true);
		/* move to the requested state */
		set_state(end_state);
	}

	return 0;
}

void Jtag::set_state(tapState_t newState)
{
	if (_state >= UNKNOWN || newState >= UNKNOWN) {
		throw std::exception();
	}
	_jtag->writeTMS(&paths[_state][newState].tms_bits, paths[_state][newState].len, false);
	_state = newState;
}
