// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <libusb.h>

#include <iostream>
#include <map>
#include <vector>
#include <stdio.h>
#include <string.h>
#include <string>

#include "anlogicCable.hpp"
#include "ch552_jtag.hpp"
#include "display.hpp"
#include "jtag.hpp"
#include "ftdipp_mpsse.hpp"
#include "ftdiJtagBitbang.hpp"
#include "ftdiJtagMPSSE.hpp"
#ifdef ENABLE_CMSISDAP
#include "cmsisDAP.hpp"
#endif
#include "dirtyJtag.hpp"
#include "part.hpp"
#include "usbBlaster.hpp"

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

Jtag::Jtag(cable_t &cable, const jtag_pins_conf_t *pin_conf, string dev,
			const string &serial, uint32_t clkHZ, int8_t verbose,
			const string &firmware_path):
			_verbose(verbose),
			_state(RUN_TEST_IDLE),
			_tms_buffer_size(128), _num_tms(0),
			_board_name("nope"), device_index(0)
{
	init_internal(cable, dev, serial, pin_conf, clkHZ, firmware_path);
	detectChain(5);
}

Jtag::~Jtag()
{
	free(_tms_buffer);
	delete _jtag;
}

void Jtag::init_internal(cable_t &cable, const string &dev, const string &serial,
	const jtag_pins_conf_t *pin_conf, uint32_t clkHZ, const string &firmware_path)
{
	switch (cable.type) {
	case MODE_ANLOGICCABLE:
		_jtag = new AnlogicCable(clkHZ, _verbose);
		break;
	case MODE_FTDI_BITBANG:
		if (pin_conf == NULL)
			throw std::exception();
		_jtag = new FtdiJtagBitBang(cable.config, pin_conf, dev, serial, clkHZ, _verbose);
		break;
	case MODE_FTDI_SERIAL:
		_jtag = new FtdiJtagMPSSE(cable.config, dev, serial, clkHZ, _verbose);
		break;
	case MODE_CH552_JTAG:
		_jtag = new CH552_jtag(cable.config, dev, serial, clkHZ, _verbose);
		break;
	case MODE_DIRTYJTAG:
		_jtag = new DirtyJtag(clkHZ, _verbose);
		break;
	case MODE_USBBLASTER:
		_jtag = new UsbBlaster(cable.config.vid, cable.config.pid,
				firmware_path, _verbose);
		break;
#ifdef ENABLE_CMSISDAP
	case MODE_CMSISDAP:
		_jtag = new CmsisDAP(cable.config.vid, cable.config.pid, _verbose);
		break;
#endif
	default:
		std::cerr << "Jtag: unknown cable type" << std::endl;
		throw std::exception();
	}

	_tms_buffer = (unsigned char *)malloc(sizeof(unsigned char) * _tms_buffer_size);
	memset(_tms_buffer, 0, _tms_buffer_size);
}

int Jtag::detectChain(int max_dev)
{
	unsigned char rx_buff[4];
	/* WA for CH552/tangNano: write is always mandatory */
	unsigned char tx_buff[4] = {0xff, 0xff, 0xff, 0xff};
	unsigned int tmp;

	/* cleanup */
	_devices_list.clear();
	_irlength_list.clear();

	go_test_logic_reset();
	set_state(SHIFT_DR);

	for (int i = 0; i < max_dev; i++) {
		read_write(tx_buff, rx_buff, 32, (i == max_dev-1)?1:0);
		tmp = 0;
		for (int ii=0; ii < 4; ii++)
			tmp |= (rx_buff[ii] << (8*ii));
		if (tmp != 0 && tmp != 0xffffffff) {
			/* ckeck highest nibble to prevent confusion between Cologne Chip
			 * GateMate and Efinix Trion T4/T8 devices
			 */
			if (tmp != 0x20000001)
				tmp &= 0x0fffffff;
			else
				tmp &= 0xffffffff;
			_devices_list.insert(_devices_list.begin(), tmp);

			/* search for irlength in fpga_list or misc_dev_list */
			uint16_t irlength = -1;
			auto dev = fpga_list.find(tmp);
			if (dev == fpga_list.end()) {
				auto misc = misc_dev_list.find(tmp);
				if (misc != misc_dev_list.end()) {
					irlength = misc->second.irlength;
				} else {
					char error[256];
					snprintf(error, 256, "Unknown device with IDCODE: 0x%08x",
							tmp);
					throw std::runtime_error(error);
				}
			} else {
				irlength = dev->second.irlength;
			}

			_irlength_list.insert(_irlength_list.begin(), irlength);
		}
	}
	go_test_logic_reset();
	flushTMS(true);
	return _devices_list.size();
}

uint16_t Jtag::device_select(uint16_t index)
{
	if (index > (uint16_t) _devices_list.size())
		return -1;
	device_index = index;
	return device_index;
}

void Jtag::setTMS(unsigned char tms)
{
	display("%s %x %d %d\n", __func__, tms, _num_tms, (_num_tms >> 3));
	if (_num_tms+1 == _tms_buffer_size * 8)
		flushTMS(false);
	if (tms != 0)
		_tms_buffer[_num_tms>>3] |= (0x1) << (_num_tms & 0x7);
	_num_tms++;
}

/* reconstruct byte sent to TMS pins
 * - use up to 6 bits
 * -since next bit after length is use to
 *  fix TMS state after sent we copy last bit
 *  to bit after next
 * -bit 7 is TDI state for each clk cycles
 */

int Jtag::flushTMS(bool flush_buffer)
{
	int ret = 0;
	if (_num_tms != 0) {
		display("%s: %d %x\n", __func__, _num_tms, _tms_buffer[0]);

		ret = _jtag->writeTMS(_tms_buffer, _num_tms, flush_buffer);

		/* reset buffer and number of bits */
		memset(_tms_buffer, 0, _tms_buffer_size);
		_num_tms = 0;
	} else if (flush_buffer) {
		_jtag->flush();
	}
	return ret;
}

void Jtag::go_test_logic_reset()
{
	/* idenpendly to current state 5 clk with TMS high is enough */
	for (int i = 0; i < 6; i++)
		setTMS(0x01);
	flushTMS(false);
	_state = TEST_LOGIC_RESET;
}

int Jtag::read_write(unsigned char *tdi, unsigned char *tdo, int len, char last)
{
	flushTMS(false);
	_jtag->writeTDI(tdi, tdo, len, last);
	if (last == 1)
		_state = (_state == SHIFT_DR) ? EXIT1_DR : EXIT1_IR;
	return 0;
}

void Jtag::toggleClk(int nb)
{
	unsigned char c = (TEST_LOGIC_RESET == _state) ? 1 : 0;
	flushTMS(false);
	if (_jtag->toggleClk(c, 0, nb) >= 0)
		return;
	throw std::exception();
	return;
}

int Jtag::shiftDR(unsigned char *tdi, unsigned char *tdo, int drlen, int end_state)
{
	/* get number of devices in the JTAG chain
	 * after the selected one
	 */
	int bits_after = device_index;

	/* if current state not shift DR
	 * move to this state
	 */
	if (_state != SHIFT_DR) {
		set_state(SHIFT_DR);
		flushTMS(false);  // force transmit tms state

		/* get number of devices, in the JTAG chain,
		 * before the selected one
		 */
		int bits_before = _devices_list.size() - device_index - 1;

		/* if not 0 send enough bits
		 */
		if (bits_before > 0) {
			int n = (bits_before + 7) / 8;
			uint8_t tx[n];
			memset(tx, 0xff, n);
			read_write(tx, NULL, bits_before, 0);
		}
	}

	/* write tdi (and read tdo) to the selected device
	 * end (ie TMS high) is used only when current device
	 * is the last of the chain and a state change must
	 * be done
	 */
	read_write(tdi, tdo, drlen, bits_after == 0 && end_state != SHIFT_DR);

	/* if it's asked to move in FSM */
	if (end_state != SHIFT_DR) {
		/* if current device is not the last */
		if (bits_after > 0) {
			int n = (bits_after + 7) / 8;
			uint8_t tx[n];
			memset(tx, 0xff, n);
			read_write(tx, NULL, bits_after, 1);  // its the last force
			                                      // tms high with last bit
		}

		/* move to end_state */
		set_state(end_state);
	}
	return 0;
}

int Jtag::shiftIR(unsigned char tdi, int irlen, int end_state)
{
	if (irlen > 8) {
		cerr << "Error: this method this direct char don't support more than 1 byte" << endl;
		return -1;
	}
	return shiftIR(&tdi, NULL, irlen, end_state);
}

int Jtag::shiftIR(unsigned char *tdi, unsigned char *tdo, int irlen, int end_state)
{
	display("%s: avant shiftIR\n", __func__);
	int bypass_after = 0;

	/* if not in SHIFT IR move to this state */
	if (_state != SHIFT_IR) {
		set_state(SHIFT_IR);
		/* force flush */
		flushTMS(false);

		/* send serie of bypass instructions
		 * final size depends on number of device
		 * before targeted and irlength of each one
		 */
		int bypass_before = 0;
		for (unsigned int i = device_index + 1; i < _devices_list.size(); i++)
			bypass_before += _irlength_list[i];
		/* same for device after targeted device
		 */
		for (int i = 0; i < device_index; i++)
			bypass_after += _irlength_list[i];

		/* if > 0 send bits */
		if (bypass_before > 0) {
			int n = (bypass_before + 7) / 8;
			uint8_t tx[n];
			memset(tx, 0xff, n);
			read_write(tx, NULL, bypass_before, 0);
		}
	}

	display("%s: envoi ircode\n", __func__);

	/* write tdi (and read tdo) to the selected device
	 * end (ie TMS high) is used only when current device
	 * is the last of the chain and a state change must
	 * be done
	 */
	read_write(tdi, tdo, irlen, bypass_after == 0 && end_state != SHIFT_IR);

	/* it's asked to move out of SHIFT IR state */
	if (end_state != SHIFT_IR) {
		/* again if devices after fill '1' */
		if (bypass_after > 0) {
			int n = (bypass_after + 7) / 8;
			uint8_t tx[n];
			memset(tx, 0xff, n);
			read_write(tx, NULL, bypass_after, 1);
		}
		/* move to the requested state */
		set_state(end_state);
	}

	return 0;
}

void Jtag::set_state(int newState)
{
	unsigned char tms;
	while (newState != _state) {
		display("_state : %16s(%02d) -> %s(%02d) ",
			getStateName((tapState_t)_state),
			_state,
			getStateName((tapState_t)newState), newState);
		switch (_state) {
		case TEST_LOGIC_RESET:
			if (newState == TEST_LOGIC_RESET) {
				tms = 1;
			} else {
				tms = 0;
				_state = RUN_TEST_IDLE;
			}
			break;
		case RUN_TEST_IDLE:
			if (newState == RUN_TEST_IDLE) {
				tms = 0;
			} else {
				tms = 1;
				_state = SELECT_DR_SCAN;
			}
			break;
		case SELECT_DR_SCAN:
			switch (newState) {
			case CAPTURE_DR:
			case SHIFT_DR:
			case EXIT1_DR:
			case PAUSE_DR:
			case EXIT2_DR:
			case UPDATE_DR:
				tms = 0;
				_state = CAPTURE_DR;
				break;
			default:
				tms = 1;
				_state = SELECT_IR_SCAN;
			}
			break;
		case SELECT_IR_SCAN:
			switch (newState) {
			case CAPTURE_IR:
			case SHIFT_IR:
			case EXIT1_IR:
			case PAUSE_IR:
			case EXIT2_IR:
			case UPDATE_IR:
				tms = 0;
				_state = CAPTURE_IR;
				break;
			default:
				tms = 1;
				_state = TEST_LOGIC_RESET;
			}
			break;
			/* DR column */
		case CAPTURE_DR:
			if (newState == SHIFT_DR) {
				tms = 0;
				_state = SHIFT_DR;
			} else {
				tms = 1;
				_state = EXIT1_DR;
			}
			break;
		case SHIFT_DR:
			if (newState == SHIFT_DR) {
				tms = 0;
			} else {
				tms = 1;
				_state = EXIT1_DR;
			}
			break;
		case EXIT1_DR:
			switch (newState) {
			case PAUSE_DR:
			case EXIT2_DR:
			case SHIFT_DR:
			case EXIT1_DR:
				tms = 0;
				_state = PAUSE_DR;
				break;
			default:
				tms = 1;
				_state = UPDATE_DR;
			}
			break;
		case PAUSE_DR:
			if (newState == PAUSE_DR) {
				tms = 0;
			} else {
				tms = 1;
				_state = EXIT2_DR;
			}
			break;
		case EXIT2_DR:
			switch (newState) {
			case SHIFT_DR:
			case EXIT1_DR:
			case PAUSE_DR:
				tms = 0;
				_state = SHIFT_DR;
				break;
			default:
				tms = 1;
				_state = UPDATE_DR;
			}
			break;
		case UPDATE_DR:
			if (newState == RUN_TEST_IDLE) {
				tms = 0;
				_state = RUN_TEST_IDLE;
			} else {
				tms = 1;
				_state = SELECT_DR_SCAN;
			}
			break;
			/* IR column */
		case CAPTURE_IR:
			if (newState == SHIFT_IR) {
				tms = 0;
				_state = SHIFT_IR;
			} else {
				tms = 1;
				_state = EXIT1_IR;
			}
			break;
		case SHIFT_IR:
			if (newState == SHIFT_IR) {
				tms = 0;
			} else {
				tms = 1;
				_state = EXIT1_IR;
			}
			break;
		case EXIT1_IR:
			switch (newState) {
			case PAUSE_IR:
			case EXIT2_IR:
			case SHIFT_IR:
			case EXIT1_IR:
				tms = 0;
				_state = PAUSE_IR;
				break;
			default:
				tms = 1;
				_state = UPDATE_IR;
			}
			break;
		case PAUSE_IR:
			if (newState == PAUSE_IR) {
				tms = 0;
			} else {
				tms = 1;
				_state = EXIT2_IR;
			}
			break;
		case EXIT2_IR:
			switch (newState) {
			case SHIFT_IR:
			case EXIT1_IR:
			case PAUSE_IR:
				tms = 0;
				_state = SHIFT_IR;
				break;
			default:
				tms = 1;
				_state = UPDATE_IR;
			}
			break;
		case UPDATE_IR:
			if (newState == RUN_TEST_IDLE) {
				tms = 0;
				_state = RUN_TEST_IDLE;
			} else {
				tms = 1;
				_state = SELECT_DR_SCAN;
			}
			break;
		}

		setTMS(tms);
		display("%d %d %d %x\n", tms, _num_tms-1, _state,
			_tms_buffer[(_num_tms-1) / 8]);
	}
	/* force write buffer */
	flushTMS(false);
}

const char *Jtag::getStateName(tapState_t s)
{
	switch (s) {
	case TEST_LOGIC_RESET:
		return "TEST_LOGIC_RESET";
	case RUN_TEST_IDLE:
		return "RUN_TEST_IDLE";
	case SELECT_DR_SCAN:
		return "SELECT_DR_SCAN";
	case CAPTURE_DR:
		return "CAPTURE_DR";
	case SHIFT_DR:
		return "SHIFT_DR";
	case EXIT1_DR:
		return "EXIT1_DR";
	case PAUSE_DR:
		return "PAUSE_DR";
	case EXIT2_DR:
		return "EXIT2_DR";
	case UPDATE_DR:
		return "UPDATE_DR";
	case SELECT_IR_SCAN:
		return "SELECT_IR_SCAN";
	case CAPTURE_IR:
		return "CAPTURE_IR";
	case SHIFT_IR:
		return "SHIFT_IR";
	case EXIT1_IR:
		return "EXIT1_IR";
	case PAUSE_IR:
		return "PAUSE_IR";
	case EXIT2_IR:
		return "EXIT2_IR";
	case UPDATE_IR:
		return "UPDATE_IR";
	default:
		return "Unknown";
	}
}
