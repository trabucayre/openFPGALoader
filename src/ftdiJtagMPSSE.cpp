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

FtdiJtagMPSSE::FtdiJtagMPSSE(const FTDIpp_MPSSE::mpsse_bit_config &cable,
			string dev, const string &serial, uint32_t clkHZ,
			bool invert_read_edge, int8_t verbose):
			FTDIpp_MPSSE(cable, dev, serial, clkHZ, verbose), _ch552WA(false),
			_write_mode(MPSSE_WRITE_NEG),  // always write on neg edge
			_read_mode(0),
			_invert_read_edge(invert_read_edge) // false: pos, true: neg
{
	init_internal(cable);
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

void FtdiJtagMPSSE::init_internal(const FTDIpp_MPSSE::mpsse_bit_config &cable)
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
						// else supress last bit -> with TMS
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
