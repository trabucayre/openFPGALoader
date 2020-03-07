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

#include <libusb.h>

#include <iostream>
#include <map>
#include <vector>
#include <stdio.h>
#include <string.h>
#include <string>

#include "display.hpp"
#include "jtag.hpp"
#include "ftdipp_mpsse.hpp"
#include "ftdiJtagBitbang.hpp"
#include "ftdiJtagMPSSE.hpp"

using namespace std;

#define DEBUG 0

#ifdef DEBUG
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
			uint32_t clkHZ, bool verbose):
			_verbose(verbose),
			_state(RUN_TEST_IDLE),
			_tms_buffer_size(128), _num_tms(0),
			_board_name("nope")
{
	init_internal(cable, pin_conf, clkHZ);
}

Jtag::Jtag(cable_t &cable, const jtag_pins_conf_t *pin_conf,
			uint32_t clkHZ, bool verbose):
		   _verbose(verbose),
		   _state(RUN_TEST_IDLE),
		   _tms_buffer_size(128), _num_tms(0),
		   _board_name("nope")
{
	init_internal(cable, pin_conf, clkHZ);
}

Jtag::~Jtag()
{
	free(_tms_buffer);
	delete _jtag;
}

void Jtag::init_internal(cable_t &cable, const jtag_pins_conf_t *pin_conf,
	uint32_t clkHZ)
{
	if (cable.type == MODE_FTDI_BITBANG) {
		if (pin_conf == NULL)
			throw std::exception();
		_jtag = new FtdiJtagBitBang(cable.config, pin_conf, clkHZ, _verbose);
	} else {
		_jtag = new FtdiJtagMPSSE(cable.config, clkHZ, _verbose);
	}

	_tms_buffer = (unsigned char *)malloc(sizeof(unsigned char) * _tms_buffer_size);
	bzero(_tms_buffer, _tms_buffer_size);
}

int Jtag::detectChain(vector<int> &devices, int max_dev)
{
	unsigned char rx_buff[4];
	/* WA for CH552/tangNano: write is always mandatory */
	unsigned char tx_buff[4] = {0xff, 0xff, 0xff, 0xff};
	unsigned int tmp;

	devices.clear();
	go_test_logic_reset();
	set_state(SHIFT_DR);

	for (int i = 0; i < max_dev; i++) {
		read_write(tx_buff, rx_buff, 32, (i == max_dev-1)?1:0);
		tmp = 0;
		for (int ii=0; ii < 4; ii++)
			tmp |= (rx_buff[ii] << (8*ii));
		if (tmp != 0 && tmp != 0xffffffff)
			devices.push_back(tmp);
	}
	go_test_logic_reset();
	return devices.size();
}

void Jtag::setTMS(unsigned char tms)
{
	display("%s %x %d %d\n", __func__, tms, _num_tms, (_num_tms >> 3));
	if (_num_tms+1 == _tms_buffer_size * 8)
		flushTMS();
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
	if (_num_tms != 0) {
		display("%s: %d %x\n", __func__, _num_tms, _tms_buffer[0]);

		_jtag->storeTMS(_tms_buffer, _num_tms);

		/* reset buffer and number of bits */
		bzero(_tms_buffer, _tms_buffer_size);
		_num_tms = 0;
	}
	if (flush_buffer)
		return _jtag->writeTMS(NULL);
	return 0;
}

void Jtag::go_test_logic_reset()
{
	/* idenpendly to current state 5 clk with TMS high is enough */
	for (int i = 0; i < 6; i++)
		setTMS(0x01);
	flushTMS(true);
	_state = TEST_LOGIC_RESET;
}

/* GGM: faut tenir plus compte de la taille de la fifo interne
 *      du FT2232 pour maximiser l'envoi au lieu de faire de petits envoies
 */
int Jtag::read_write(unsigned char *tdi, unsigned char *tdo, int len, char last)
{
	/* 3 possible case :
	 *  - n * 8bits to send -> use byte command
	 *  - less than 8bits   -> use bit command
	 *  - last bit to send  -> sent in conjunction with TMS
	 */
	int tx_buff_size = _jtag->get_buffer_size();
	int real_len = (last) ? len - 1 : len;	// if its a buffer in a big send send len
						// else supress last bit -> with TMS
	int nb_byte = real_len >> 3;	// number of byte to send
	int nb_bit = (real_len & 0x07);	// residual bits
	int xfer = tx_buff_size;

	unsigned char *rx_ptr = (unsigned char *)tdo;
	unsigned char *tx_ptr = (unsigned char *)tdi;

	flushTMS(true);

	display("%s len : %d %d %d %d %d\n", __func__, len, real_len, nb_byte,
		nb_bit, tx_buff_size);
	while (nb_byte > xfer) {
		display("%s %d %d\n", __func__, nb_byte, xfer);

		if (xfer != _jtag->storeTDI(tx_ptr, xfer, tdo != NULL)) {
			printError("%s: Fail to store tdi\n", __func__);
			return -1;
		}
		if (0 > _jtag->writeTDI(rx_ptr, xfer * 8)) {
			printError("%s: Write errror\n", __func__);
			return -1;
		}
		if (tdi)
			tx_ptr += xfer;
		if (tdo)
			rx_ptr += xfer;
		nb_byte -= xfer;
	}


	/* 1/ send serie of byte */
	if (nb_byte > 0) {
		display("%s read/write %d byte\n", __func__, nb_byte);
		if (nb_byte != _jtag->storeTDI(tx_ptr, nb_byte, tdo != NULL)) {
			printError("%s: Fail to store tdi\n", __func__);
			return -1;
		}
		if (0 > _jtag->writeTDI(((tdo)?rx_ptr:NULL), nb_byte * 8)) {
			printError("%s: Write errror\n", __func__);
			return -1;
		}
		if (tdi)
			tx_ptr += nb_byte;
		if (tdo)
			rx_ptr += nb_byte;
	}

	unsigned char last_bit = (tdi) ? *tx_ptr : 0;

	if (nb_bit != 0) {
		display("%s read/write %d bit\n", __func__, nb_bit);
		if (nb_bit != _jtag->storeTDI(last_bit, nb_bit, tdo != NULL)) {
			printError("%s: Fail to store tdi\n", __func__);
			return -1;
		}
		if (0 > _jtag->writeTDI((tdo)?rx_ptr:NULL, nb_bit)) {
			printError("%s: Write errror\n", __func__);
			return -1;
		}

		if (tdo) {
			/* realign we have read nb_bit 
			 * since LSB add bit by the left and shift
			 * we need to complete shift
			 */
			*rx_ptr >>= (8 - nb_bit);
			display("%s %x\n", __func__, *rx_ptr);
		}
	}

	/* display : must be dropped */
	if (_verbose && tdo) {
		display("\n");
		for (int i = (len / 8) - 1; i >= 0; i--)
			display("%x ", (unsigned char)tdo[i]);
		display("\n");
	}

	if (last == 1) {
		uint8_t c;
		last_bit = (tdi)? (*tx_ptr & (1 << nb_bit)) : 0;

		display("%s move to EXIT1_xx and send last bit %x\n", __func__, (last_bit?0x81:0x01));

		c=1;
		_jtag->storeTMS(&c, 1, (last_bit)?1:0, tdo != NULL);
		_jtag->writeTMS((tdo)?&c:NULL, 1);
		if (tdo) {
			/* in this case for 1 one it's always bit 7 */
			*rx_ptr |= ((c & 0x80) << (7 - nb_bit));
			display("%s %x\n", __func__, c);
		}
		_state = (_state == SHIFT_DR) ? EXIT1_DR : EXIT1_IR;
	}

	return 0;
}

void Jtag::toggleClk(int nb)
{
	unsigned char c = (TEST_LOGIC_RESET == _state) ? 1 : 0;
	for (int i = 0; i < nb; i++)
		setTMS(c);
	flushTMS(true);
}

int Jtag::shiftDR(unsigned char *tdi, unsigned char *tdo, int drlen, int end_state)
{
	set_state(SHIFT_DR);
	// force transmit tms state
	flushTMS(true);
	// currently don't care about multiple device in the chain
	read_write(tdi, tdo, drlen, 1);// 1 since only one device

	set_state(end_state);
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
	set_state(SHIFT_IR);
	flushTMS(true);
	// currently don't care about multiple device in the chain

	display("%s: envoi ircode\n", __func__);
	read_write(tdi, tdo, irlen, 1);  // 1 since only one device

	set_state(end_state);
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
	flushTMS();
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
