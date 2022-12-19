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
#include <string>

#include "display.hpp"
#include "ch552_jtag.hpp"
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

CH552_jtag::CH552_jtag(const cable_t &cable,
			string dev, const string &serial, uint32_t clkHZ, uint8_t verbose):
			FTDIpp_MPSSE(cable, dev, serial, clkHZ, verbose), _to_read(0)
{
	init_internal(cable.config);
}

CH552_jtag::~CH552_jtag()
{
	int read;
	/* Before shutdown, we must wait until everything is shifted out
	 * Do this by temporary enabling loopback mode, write something
	 * and wait until we can read it back
	 */
	static unsigned char tbuf[16] = { SET_BITS_LOW, 0xff, 0x00,
		SET_BITS_HIGH, 0xff, 0x00,
		LOOPBACK_START,
		static_cast<unsigned char>(MPSSE_DO_READ |
		MPSSE_DO_WRITE | MPSSE_WRITE_NEG | MPSSE_LSB),
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

void CH552_jtag::init_internal(const mpsse_bit_config &cable)
{
	display("iProduct : %s\n", _iproduct);

	display("%x\n", cable.bit_low_val);
	display("%x\n", cable.bit_low_dir);
	display("%x\n", cable.bit_high_val);
	display("%x\n", cable.bit_high_dir);

	init(5, 0xfb, BITMODE_MPSSE);
	ftdi_set_event_char(_ftdi, 0, 0);
	ftdi_set_error_char(_ftdi, 0, 0);
	ftdi_set_latency_timer(_ftdi, 5);
#if (FTDI_VERSION < 105)
	ftdi_usb_purge_rx_buffer(_ftdi);
	ftdi_usb_purge_tx_buffer(_ftdi);
#else
	ftdi_tciflush(_ftdi);
	ftdi_tcoflush(_ftdi);
#endif
}

int CH552_jtag::setClkFreq(uint32_t clkHZ) {

	int ret = FTDIpp_MPSSE::setClkFreq(clkHZ);
	return ret;
}

int CH552_jtag::writeTMS(uint8_t *tms, uint32_t len, bool flush_buffer)
{
	(void) flush_buffer;
	display("%s %d %d\n", __func__, len, (len/8)+1);

	if (len == 0)
		return 0;

	uint32_t xfer = len;
	uint32_t iter = (_buffer_size -8) / 4;
	iter = (_buffer_size / 3);
	uint32_t offset = 0, pos = 0;
	flush_buffer = true;

	uint8_t buf[3]= {static_cast<unsigned char>(MPSSE_WRITE_TMS | MPSSE_LSB |
						MPSSE_BITMODE | MPSSE_WRITE_NEG|
						MPSSE_DO_READ),
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
		_to_read++;

		mpsse_store(buf, 3);
		if (pos >= iter) {
			uint8_t tmp[_to_read];
			pos = 0;
			if (-1 == mpsse_read(tmp, _to_read))
				printError("writeTMS: Fail to read/write");
			_to_read = 0;
		}
		xfer -= bit_to_send;
	}

	if (flush_buffer) {
		if (_to_read > 0) {
			uint8_t tmp[_to_read];
			if (mpsse_read(tmp, _to_read) == -1)
				printError("writeTMS: fail to flush");
			_to_read = 0;
		}
		if (_num > 0)
			if (mpsse_write() == -1)
				printError("writeTMS: fail to flush in write mode");
	}

	return len;
}

int CH552_jtag::toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len)
{
	(void) tdi;

	int byteLen = (clk_len+7)/8;
	uint8_t buf_tms[byteLen];

	memset(buf_tms, (tms) ? 0xff : 0x00, byteLen);
	return writeTMS(buf_tms, clk_len, false);
}

int CH552_jtag::flush()
{
	int ret;
	if (_to_read == 0) {
		ret = mpsse_write();
		if (ret == -1)
			printError("flush: fails to write");
	} else {
		uint8_t tmp[_to_read];
		ret = mpsse_read(tmp, _to_read);
		if (ret == -1)
			printError("flush: fails to read/write");
		_to_read = 0;
	}
	return ret;
}

int CH552_jtag::writeTDI(uint8_t *tdi, uint8_t *tdo, uint32_t len, bool last)
{
	bool rd_mode = (tdo) ? true : false;
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
	int xfer = tx_buff_size - 7; // 2 byte for opcode and size 2 time
	unsigned char *rx_ptr = (unsigned char *)tdo;
	unsigned char *tx_ptr = (unsigned char *)tdi;
	unsigned char tx_buf[3] = {(unsigned char)(MPSSE_LSB |
						((tdi) ? (MPSSE_DO_WRITE | MPSSE_WRITE_NEG) : 0) |
						((rd_mode) ? (MPSSE_DO_READ) : 0)),
						static_cast<unsigned char>((xfer - 1) & 0xff),       // low
						static_cast<unsigned char>((((xfer - 1) >> 8) & 0xff))}; // high
	unsigned char def_cmd = tx_buf[0];
	unsigned char rd_cmd = def_cmd | (MPSSE_DO_READ);
	/* read (and write) 1Byte */
	uint8_t oneshot_buf[3] = {rd_cmd, 0, 0};

	if (_to_read != 0) {
		uint8_t tmp_[_to_read];
		if (mpsse_read(tmp_, _to_read) == -1)
			printError("writeTDI: fails to flush read");
		_to_read = 0;
	}

	display("%s len : %d %d %d %d\n", __func__, len, real_len, nb_byte,
		nb_bit);

	if (_num != 0 && ((nb_byte + _num + 3) > _buffer_size))
		if (mpsse_write() == -1)
			printError("writeTDI: fails to flush write");

	if ((nb_byte * 8) + nb_bit != real_len) {
		printf("pas cool\n");
		throw std::exception();
	}

	/* first case: 1 full byte (and maybe up to 7bit) to send
	 *             direct write (and read)
	 */
	if (nb_byte == 1) {
		uint8_t tmp_;
		mpsse_store(oneshot_buf, 3);
		if (tdi) {
			mpsse_store(tx_ptr, 1);
			tx_ptr++;
		}
		if (mpsse_read(&tmp_, 1) == -1)
			printError("writeTDI: fails to read/write with nb_byte == 1");
		if (rd_mode) {
			*rx_ptr = tmp_;
			rx_ptr++;
		}
		nb_byte--;
	}

	while (nb_byte != 0) {
		uint8_t tmp_;
		int xfer_len = (nb_byte > xfer) ? xfer : nb_byte;
		if (!rd_mode) {
			xfer_len--;
		}
		if (xfer_len != 0) {
			tx_buf[0] = def_cmd;
			tx_buf[1] = (((xfer_len - 1)     ) & 0xff);  // low
			tx_buf[2] = (((xfer_len - 1) >> 8) & 0xff);  // high
			mpsse_store(tx_buf, 3);
			if (tdi) {
				mpsse_store(tx_ptr, xfer_len);
				tx_ptr += xfer_len;
			}
			if (rd_mode) {
				if (mpsse_read(rx_ptr, xfer_len) == -1)
					printError("writeTDI: fails to read with nb_byte > 1");
				rx_ptr += xfer_len;
			} else {
				mpsse_store(oneshot_buf, 3);
				if (tdi) {
					mpsse_store(tx_ptr, 1);
					tx_ptr++;
				}
				if (mpsse_read(&tmp_, 1) == -1)
					printError("writeTDI: fails to read/write with nb_byte > 1");
			}
		} else {
			tx_buf[0] = rd_cmd;
			tx_buf[1] = 0;
			tx_buf[2] = 0;
			mpsse_store(tx_buf, 3);
			if (tdi) {
				mpsse_store(tx_ptr, 1);
				tx_ptr++;
			}
			if (mpsse_read(&tmp_, 1) == -1)
				printError("writeTDI: fails to read/write with nb_byte == 0");
			if (rd_mode) {
				*rx_ptr = tmp_;
				rx_ptr++;
			}
		}
		if (!rd_mode)
			xfer_len++;
		nb_byte -= xfer_len;
	}

	unsigned char last_bit = (tdi) ? *tx_ptr : 0;

	/* next: serie of bit to send: unconditionally write AND read
	 */
	if (nb_bit != 0) {
		display("%s read/write %d bit\n", __func__, nb_bit);
		tx_buf[0] = rd_cmd | MPSSE_BITMODE;
		tx_buf[1] = nb_bit - 1;
		mpsse_store(tx_buf, 2);
		if (tdi) {
			display("%s last_bit %x size %d\n", __func__, last_bit, nb_bit-1);
			mpsse_store(last_bit);
		}
		uint8_t tmp_;
		if (mpsse_read(&tmp_, 1) == -1)
			printError("writeTDI: fails to read/write serie of bits");
		if (rd_mode) {
			*rx_ptr = tmp_;
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
		last_bit = (tdi)? (*tx_ptr & (1 << nb_bit)) : 0;

		display("%s move to EXIT1_xx and send last bit %x\n", __func__, (last_bit?0x81:0x01));
		/* write the last bit in conjunction with TMS */
		tx_buf[0] = MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE | MPSSE_WRITE_NEG |
					(MPSSE_DO_READ);
		tx_buf[1] = 0x0;  // send 1bit
		tx_buf[2] = ((last_bit) ? 0x81 : 0x01);  // we know in TMS tdi is bit 7
							// and to move to EXIT_XR TMS = 1
		mpsse_store(tx_buf, 3);
		uint8_t c;
		if (mpsse_read(&c, 1) == -1)
			printError("writeTDI: fails to read/write last transaction");
		if (rd_mode) {
			/* in this case for 1 one it's always bit 7 */
			*rx_ptr |= ((c & 0x80) << (7 - nb_bit));
		}
	}

	return 0;
}
