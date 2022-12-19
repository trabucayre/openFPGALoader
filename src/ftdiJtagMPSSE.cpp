// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <libusb.h>
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <map>
#include <vector>
#include <stdexcept>
#include <string>

#include "display.hpp"
#include "ftdiJtagMPSSE.hpp"
#include "ftdipp_mpsse.hpp"

using namespace std;

#define DEBUG 0

#ifdef DEBUG
#define display(...) \
	do { \
		if (_verbose) fprintf(stdout, __VA_ARGS__); \
	}while(0)
#else
#define display(...) do {}while(0)
#endif

FtdiJtagMPSSE::FtdiJtagMPSSE(const cable_t &cable,
			string dev, const string &serial, uint32_t clkHZ,
			bool invert_read_edge, int8_t verbose):
			FTDIpp_MPSSE(cable, dev, serial, clkHZ, verbose), _ch552WA(false),
			_write_mode(MPSSE_WRITE_NEG),  // always write on neg edge
			_read_mode(0),
			_invert_read_edge(invert_read_edge), // false: pos, true: neg
			_tdo_pos(0)
{
	init_internal(cable.config);
}

FtdiJtagMPSSE::~FtdiJtagMPSSE()
{
	int read;
	/* Before shutdown, we must wait until everything is shifted out
	 * Do this by temporary enabling loopback mode, write something
	 * and wait until we can read it back
	 */
	static unsigned char tbuf[16] = { SET_BITS_LOW, 0xff, 0x00,
		SET_BITS_HIGH, 0xff, 0x00,
		LOOPBACK_START,
		static_cast<unsigned char>(MPSSE_DO_READ | _read_mode |
		MPSSE_DO_WRITE | _write_mode | MPSSE_LSB),
		0x04, 0x00,
		0xaa, 0x55, 0x00, 0xff, 0xaa,
		LOOPBACK_END
	};
	mpsse_store(tbuf, 16);
	read = mpsse_read(tbuf, 5);
	if (read != 5)
		fprintf(stderr,
			"Loopback failed, expect problems on later runs %d\n", read);
}

void FtdiJtagMPSSE::init_internal(const mpsse_bit_config &cable)
{
	display("iProduct : %s\n", _iproduct);

	if (!strncmp((const char *)_iproduct, "Sipeed-Debug", 12)) {
		_ch552WA = true;
	}

	display("%x\n", cable.bit_low_val);
	display("%x\n", cable.bit_low_dir);
	display("%x\n", cable.bit_high_val);
	display("%x\n", cable.bit_high_dir);

	if (init(5, 0xfb, BITMODE_MPSSE) != 0)
		throw std::runtime_error("low level FTDI init failed");
	config_edge();

	_curr_tms = (cable.bit_low_val >> 3) & 0x01;
	_curr_tdi = (cable.bit_low_val >> 1) & 0x01;
}

int FtdiJtagMPSSE::setClkFreq(uint32_t clkHZ) {

	int ret = FTDIpp_MPSSE::setClkFreq(clkHZ);
	config_edge();
	return ret;
}

void FtdiJtagMPSSE::config_edge()
{
	/* at high (>15MHz) with digilent cable (arty)
	 * opposite edges must be used.
	 * Not required with classic FT2232 but user selectable
	 */
	if (_invert_read_edge || (FTDIpp_MPSSE::getClkFreq() >= 15000000 &&
			!strncmp((const char *)_iproduct, "Digilent USB Device", 19))) {
		_read_mode = MPSSE_READ_NEG;
	} else {
		_read_mode = 0;
	}
}

int FtdiJtagMPSSE::writeTMS(uint8_t *tms, uint32_t len, bool flush_buffer)
{
	(void) flush_buffer;
	display("%s %d %d\n", __func__, len, (len/8)+1);

	if (len == 0)
		return 0;

	int xfer = len;
	int iter = _buffer_size / 3;
	int offset = 0, pos = 0;

	uint8_t buf[3]= {static_cast<unsigned char>(MPSSE_WRITE_TMS | MPSSE_LSB |
						MPSSE_BITMODE | _write_mode),
						0, 0};
	while (xfer > 0) {
		int bit_to_send = (xfer > 6) ? 6 : xfer;
		buf[1] = bit_to_send-1;
		buf[2] = 0x80;

		for (int i = 0; i < bit_to_send; i++, offset++) {
			buf[2] |=
			(((tms[offset >> 3] & (1 << (offset & 0x07))) ? 1 : 0) << i);
		}
		pos+=3;

		mpsse_store(buf, 3);
		if (pos == iter * 3) {
			pos = 0;
			if (mpsse_write() < 0)
				printf("writeTMS: error\n");

			if (_ch552WA) {
				uint8_t c[len/8+1];
				int ret = ftdi_read_data(_ftdi, c, len/8+1);
				if (ret != 0) {
					printf("ret : %d\n", ret);
				}
			}
		}
		xfer -= bit_to_send;
	}
	if (flush_buffer)
		mpsse_write();
	if (_ch552WA) {
		uint8_t c[len/8+1];
		ftdi_read_data(_ftdi, c, len/8+1);
	}

	return len;
}

/* need a WA for ch552 */
int FtdiJtagMPSSE::toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len)
{
	(void) tdi;
	int ret;
	uint32_t len = clk_len;

	/* clk ouput without data xfer is only supported
	 * with 2232H, 4242H & 232H
	 */

	if (_ftdi->type == TYPE_2232H || _ftdi->type == TYPE_4232H ||
				_ftdi->type == TYPE_232H) {
		uint8_t buf[] = {static_cast<uint8_t>(0x8f), 0, 0};
		while (len) {
			unsigned int chunk = len;
			if (chunk > 0x10000 * 8)
				chunk = 0x10000 * 8;
			if (chunk  > 8) {
				unsigned cycles8 = chunk / 8;
				len -= cycles8 * 8;
				cycles8 --;
				buf[1] = ((cycles8)		) & 0xff;
				buf[2] = ((cycles8) >> 8) & 0xff;
				mpsse_store(buf, 3);
			}
			if (len && len < 9) {
				buf[0] = 0x8E;
				buf[1] = len - 1;
				mpsse_store(buf, 2);
				len = 0;
			}
		}
		ret = clk_len;
	} else {
		int byteLen = (len+7)/8;
		uint8_t buf_tms[byteLen];
		memset(buf_tms, (tms) ? 0xff : 0x00, byteLen);
		ret = writeTMS(buf_tms, len, false);
	}

	return ret;
}

int FtdiJtagMPSSE::flush()
{
	return mpsse_write();
}

int FtdiJtagMPSSE::writeTDI(uint8_t *tdi, uint8_t *tdo, uint32_t len, bool last)
{
	/* 3 possible case :
	 *  - n * 8bits to send -> use byte command
	 *  - less than 8bits   -> use bit command
	 *  - last bit to send  -> sent in conjunction with TMS
	 */
	int tx_buff_size = mpsse_get_buffer_size();
	int real_len = (last) ? len - 1 : len;  // if its a buffer in a big send send len
						// else suppress last bit -> with TMS
	int nb_byte = real_len >> 3;    // number of byte to send
	int nb_bit = (real_len & 0x07); // residual bits
	int xfer = tx_buff_size - 3;
	unsigned char c[xfer];
	unsigned char *rx_ptr = (unsigned char *)tdo;
	unsigned char *tx_ptr = (unsigned char *)tdi;
	unsigned char tx_buf[3] = {(unsigned char)(MPSSE_LSB |
						((tdi) ? (MPSSE_DO_WRITE | _write_mode) : 0) |
						((tdo) ? (MPSSE_DO_READ | _read_mode) : 0)),
						static_cast<unsigned char>((xfer - 1) & 0xff),       // low
						static_cast<unsigned char>((((xfer - 1) >> 8) & 0xff))}; // high

	display("%s len : %d %d %d %d\n", __func__, len, real_len, nb_byte,
		nb_bit);

	if ((nb_byte + _num + 3) > _buffer_size)
		mpsse_write();

	if ((nb_byte * 8) + nb_bit != real_len) {
		printf("pas cool\n");
		throw std::exception();
	}

	/* if only one full byte use BITMODE to reduce
	 * transaction size
	 */
	if (nb_byte == 1 && nb_bit == 0) {
		nb_byte = 0;
		nb_bit = 8;
	}

	while (nb_byte != 0) {
		int xfer_len = (nb_byte > xfer) ? xfer : nb_byte;
		tx_buf[1] = (((xfer_len - 1)     ) & 0xff);  // low
		tx_buf[2] = (((xfer_len - 1) >> 8) & 0xff);  // high
		mpsse_store(tx_buf, 3);
		if (tdi) {
			mpsse_store(tx_ptr, xfer_len);
			tx_ptr += xfer_len;
		}
		if (tdo) {
			mpsse_read(rx_ptr, xfer_len);
			rx_ptr += xfer_len;
		} else if (_ch552WA) {
			mpsse_write();
			ftdi_read_data(_ftdi, c, xfer_len);
		} else if (!last) {
			mpsse_write();
		}
		nb_byte -= xfer_len;
	}

	unsigned char last_bit = (tdi) ? *tx_ptr : 0;
	bool double_write = true;

	if (nb_bit != 0) {
		display("%s read/write %d bit\n", __func__, nb_bit);
		tx_buf[0] |= MPSSE_BITMODE;
		tx_buf[1] = nb_bit - 1;
		mpsse_store(tx_buf, 2);
		if (tdi) {
			display("%s last_bit %x size %d\n", __func__, last_bit, nb_bit-1);
			mpsse_store(last_bit);
		}
		if (tdo && !last) {
			mpsse_read(rx_ptr, 1);
			double_write = false;
			/* realign we have read nb_bit
			 * since LSB add bit by the left and shift
			 * we need to complete shift
			 */
			*rx_ptr >>= (8 - nb_bit);
			display("%s %x\n", __func__, *rx_ptr);
		} else if (_ch552WA) {
			if (tdo) {
				mpsse_read(rx_ptr, 1);
				double_write = false;
				*rx_ptr >>= (8 - nb_bit);
			} else {
				mpsse_write();
				ftdi_read_data(_ftdi, c, nb_bit);
			}
		} else if (!last) {
			mpsse_write();
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
		last_bit = (tdi)? (*tx_ptr & (1 << nb_bit)) : 0;

		display("%s move to EXIT1_xx and send last bit %x\n", __func__, (last_bit?0x81:0x01));
		/* write the last bit in conjunction with TMS */
		tx_buf[0] = MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE | _write_mode |
					((tdo) ? (MPSSE_DO_READ | _read_mode) : 0);
		tx_buf[1] = 0x0;  // send 1bit
		tx_buf[2] = ((last_bit) ? 0x81 : 0x01);  // we know in TMS tdi is bit 7
							// and to move to EXIT_XR TMS = 1
		mpsse_store(tx_buf, 3);
		if (tdo) {
			unsigned char c[2];
			int index = 0;
			mpsse_read(c, 1 + ((double_write)?1:0));
			if (double_write) {
				*rx_ptr = c[index] >> (8-nb_bit);
				index++;
			}
			/* in this case for 1 one it's always bit 7 */
			*rx_ptr |= ((c[index] & 0x80) << (7 - nb_bit));
		} else if (_ch552WA) {
			mpsse_write();
			ftdi_read_data(_ftdi, c, 1);
		} else {
			mpsse_write();
		}
	}

	return 0;
}

int32_t FtdiJtagMPSSE::update_tms_buff(uint8_t *buffer, uint8_t bit,
		uint32_t offset, uint8_t tdi, uint8_t *tdo, bool end)
{
	int32_t ret;
	if (_verbose)
		printf("%s %d %02x %d\n", __func__, offset, buffer[0], end);
	if (!end) {
		uint8_t bit_shift = (1 << (offset));
		if (bit)
			buffer[0] |= bit_shift;
		else
			buffer[0] &= ~bit_shift;
		offset++;
	}
	if (offset == 6 || end) {
		if (tdi)
			buffer[0] |= (0x01 << 7);
		else
			buffer[0] &= ~(0x01 << 7);
		uint8_t mp[3] = {
			static_cast<unsigned char>(MPSSE_WRITE_TMS | MPSSE_LSB |
										MPSSE_BITMODE | _write_mode |
										MPSSE_DO_READ | _read_mode
										),
			static_cast<uint8_t>(offset - 1),
			buffer[0]
		};
		// force a write
		if (_verbose)
			printf("\t%02x %02d %02x\n", mp[0], mp[1], mp[2]);
		if ((ret = mpsse_store(mp, 3)) < 0)
			return ret;
		uint8_t tdo_tmp;
		if ((ret = mpsse_read(&tdo_tmp, 1)) < 0)
			return ret;
		update_tdo_buff(&tdo_tmp, tdo, offset);
		offset = 0;
		buffer[0] = 0;
	}
	return offset;
}

uint32_t FtdiJtagMPSSE::update_tdo_buff(uint8_t *buffer, uint8_t *tdo, uint32_t len)
{
	if (_verbose) {
		printError("update tdo " + std::to_string(_tdo_pos) + " " + std::to_string(len) + " ", false);
		uint32_t tt = (len + 7) / 8;
		for (uint32_t i = 0; i < tt; i++)
			printf("%02x ", buffer[i]);
	}
	for (uint32_t i = 0; i < len; i++, _tdo_pos++) {
		uint8_t bit = (buffer[i >> 3] >> (i & 0x07)) & 0x01;
		uint8_t mask = 1 << (_tdo_pos & 0x07);
		if (bit)
			tdo[_tdo_pos >> 3] |= mask;
		else
			tdo[_tdo_pos >> 3] &= ~mask;
	}
	if (_verbose)
		printf("\n");
	return _tdo_pos;
}

bool FtdiJtagMPSSE::writeTMSTDI(const uint8_t *tms, const uint8_t *tdi,
		uint8_t *tdo, uint32_t len)
{
	int32_t ret;
	uint32_t max_len = 1024;
	uint8_t mode = 0;         // current state: 0 none, 1 TDI, 2 TMS
	uint8_t tdi_buf[max_len]; // buffer to store TDI sequence
	uint8_t tms_tmp = 0;      // buffer to store TMS sequence (limited to 6bits per cmd)
	uint8_t tdo_tmp[max_len]; // local TDO sequence
	uint32_t buff_len = 0;    // current bits stored
	memset(tdi_buf, 0, max_len);
	memset(tdo_tmp, 0, max_len);
	_tdo_pos = 0; // current bits read

	if (_verbose)
		printSuccess("begin: " + std::to_string(len));

	for (uint32_t buf_pos = 0; buf_pos < len; buf_pos++) {
		/* extract bit from TMS and TDI sequence */
		uint8_t tms_bit = (tms[buf_pos >> 3] >> (buf_pos & 0x07) & 0x01);
		uint8_t tdi_bit = (tdi[buf_pos >> 3] >> (buf_pos & 0x07) & 0x01);

		if (_verbose) {
			char mess[256];
			snprintf(mess, 256, "tms %d -> %d tdi %d -> %d mode %d %d/%d (%d)",
					_curr_tms, tms_bit, _curr_tdi, tdi_bit, mode, buf_pos, len, buff_len);
			printInfo(mess);
		}

		/* possible case:
		 * tdi & tms not changed    -> write tdi
		 * tdi change but tms not   -> write tdi
		 * tdi idem but tms changed -> write tms
		 * tdi & tms changed        -> serie of write tms
		 * but finally:
		 * if tms is not changed -> write tdi
		 * otherwise             -> write tms
		 */
		/* tms unchanged -> try to use TDI/TDO transaction */
		if (tms_bit == _curr_tms) {
			/* a tms transaction exist but not flushed or full
			 * if tdi is not changed -> update tms sequence
			 */
			if (mode == 2 && buff_len != 0 && tdi_bit == _curr_tdi) {
				ret = update_tms_buff(&tms_tmp, tms_bit, buff_len,
						tdi_bit, tdo);
				if (ret < 0)
					return false;
				else
					buff_len = ret;
			} else {
				/* tdi sequence
				 * first flush tms sequence if required */
				if (mode != 1 && buff_len != 0) {
					ret = update_tms_buff(&tms_tmp, 0, buff_len, _curr_tdi,
						tdo, true);
					if (ret < 0)
						return false;
					else
						buff_len = ret;
				}
				/* update tdi buffer with one more bit */
				uint8_t mask = (1 << (buff_len & 0x07));
				if (tdi_bit)
					tdi_buf[buff_len >> 3] |= mask;
				else
					tdi_buf[buff_len >> 3] &= ~mask;
				buff_len++;
				mode = 1;
			}
		/* TMS is changed -> TMS transaction */
		} else {
			/* flush */
			/* previous write TDI -> flush */
			if (mode == 1 && buff_len > 0) {
				bool is_end = false;
				/* tms 0 -> 1: it's handled by writeTDI:
				 * append bit to avoid another transaction */
				if (_curr_tms == 0 && tms_bit == 1) {
					uint8_t mask = (1 << (buff_len & 0x07));
					if (tdi_bit)
						tdi_buf[buff_len >> 3] |= mask;
					else
						tdi_buf[buff_len >> 3] &= ~mask;
					buff_len++;
					is_end = true;
				}
				writeTDI(tdi_buf, tdo_tmp, buff_len, is_end);
				update_tdo_buff(tdo_tmp, tdo, buff_len);
				memset(tdi_buf, 0, max_len);
				buff_len = 0;
				if (is_end) {
					_curr_tdi = tdi_bit;
					mode = 1;
					continue;
				}
			/* same state -> simply flush */
			} else if (tdi_bit != _curr_tdi && mode == 2 && buff_len > 0) {
				ret = update_tms_buff(&tms_tmp, 0, buff_len, _curr_tdi, tdo, true);
				if (ret < 0)
					return false;
				else
					buff_len = ret;
				tms_tmp = 0;
			}
			/* update */
			ret = update_tms_buff(&tms_tmp, tms_bit, buff_len, tdi_bit, tdo);
			if (ret < 0)
				return false;
			else
				buff_len = ret;
			mode = 2;
		}

		/* buffer full? */
		if (buff_len == 8*max_len && mode == 1) {
			writeTDI(tdi_buf, tdo_tmp, buff_len, false);
			update_tdo_buff(tdo_tmp, tdo, buff_len);
			memset(tdi_buf, 0, max_len);
			buff_len = 0;
		} else if (buff_len == 6 && mode == 2) {
			ret = update_tms_buff(&tms_tmp, 0, buff_len, _curr_tdi, tdo, true);
			if (ret < 0)
				return false;
			else
				buff_len = ret;
			tms_tmp = 0;
		}
		_curr_tdi = tdi_bit;
		_curr_tms = tms_bit;
	}

	 /* end -> force flush buffers */
	if (buff_len > 0) {
		switch (mode) {
		case 1:
			writeTDI(tdi_buf, tdo_tmp, buff_len, false);
			update_tdo_buff(tdo_tmp, tdo, buff_len);
			break;
		case 2:
			if (update_tms_buff(&tms_tmp, 0, buff_len, _curr_tdi,
						tdo, true) < 0)
				return false;
			break;
		}
	}
	if (_verbose) {
		printSuccess("end state: tdi " + std::to_string(_curr_tdi) +
				" tms " + std::to_string(_curr_tms));
	}

	return true;
}
