// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <libusb.h>
#include <stdio.h>
#include <string.h>

#include <cerrno>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "display.hpp"
#ifdef USE_LIBFTDI
#include "ftdipp_mpsse.hpp"
#endif
#include "fx2_ll.hpp"
#include "usbBlaster.hpp"


#define DO_READ  (1 << 6)
#define DO_WRITE (0 << 6)
#define DO_RDWR  (1 << 6)
#define DO_SHIFT (1 << 7)
#define DO_BITBB (0 << 7)
#define DEFAULT ((1<<2) | (1<<3) | (1 << 5))

UsbBlaster::UsbBlaster(const cable_t &cable, const std::string &firmware_path,
		int8_t verbose):
			_verbose(verbose > 1), _nb_bit(0),
			_curr_tdi(0), _curr_tms(0), _buffer_size(64),
			_passive_serial_mode(false)
{
	if (cable.pid == 0x6001 || cable.pid == 0x6002 || cable.pid == 0x6003)
#ifdef ENABLE_USB_BLASTERI
		ll_driver = new UsbBlasterI(cable.pid);
#else
		throw std::runtime_error("usb-blasterI: Not build");
#endif
	else if (cable.pid == 0x6810 || cable.pid == 0x6010)
#ifdef ENABLE_USB_BLASTERII
		ll_driver = new UsbBlasterII(firmware_path);
#else
		throw std::runtime_error("usb-blasterII: Not build");
#endif
	else
		throw std::runtime_error("usb-blaster: unknown VID/PID");

	_tck_pin = (1 << 0);
	_tms_pin = (1 << 1);
	_tdi_pin = (1 << 4);

	_in_buf = (unsigned char *)malloc(sizeof(unsigned char) * _buffer_size);

	memset(_in_buf, 0, _buffer_size);
}

UsbBlaster::~UsbBlaster()
{
	if (!_passive_serial_mode) {
		if (_nb_bit + 1 > _buffer_size) {
			if (flush() < 0)
				printError("UsbBlaster DTOR: failed to flush before reset");
		}
		_in_buf[_nb_bit++] = 0;
	}
	if (flush() < 0)
		printError("UsbBlaster DTOR: failed to flush");
	free(_in_buf);
	delete ll_driver;
}

int UsbBlaster::setClkFreq(uint32_t clkHZ)
{
	return ll_driver->setClkFreq(clkHZ);
}

uint32_t UsbBlaster::getClkFreq()
{
	return ll_driver->getClkFreq();
}

int UsbBlaster::writeTMS(const uint8_t *tms, uint32_t len, bool flush_buffer,
		const uint8_t tdi)
{
	int ret;

	/* nothing to send
	 * but maybe need to flush internal buffer
	 */
	if (len == 0) {
		if (flush_buffer) {
			return flush();
		}
		return 0;
	}
	_curr_tdi = tdi ? _tdi_pin : 0;

	/* check for at least one bit space in buffer */
	if (_nb_bit+2 > _buffer_size) {
		ret = flush();
		if (ret < 0)
			return ret;
	}

	/* fill buffer to reduce USB transaction */
	for (uint32_t i = 0; i < len; i++) {
		_curr_tms = ((tms[i >> 3] & (1 << (i & 0x07)))? _tms_pin : 0);
		uint8_t val = DEFAULT | DO_WRITE | DO_BITBB | _curr_tdi | _curr_tms;
		_in_buf[_nb_bit++] = val;
		_in_buf[_nb_bit++] = val | _tck_pin;

		if (_nb_bit + 2 > _buffer_size) {
			ret = flush();
			if (ret < 0)
				return ret;
		}
	}
	_in_buf[_nb_bit++] = DEFAULT | DO_WRITE | DO_BITBB | _curr_tdi | _curr_tms;

	/* security check: try to flush buffer */
	if (flush_buffer) {
		ret = flush();
		if (ret < 0)
			return ret;
	}

	return len;
}

int UsbBlaster::writeTDI(const uint8_t *tx, uint8_t *rx, uint32_t len, bool end)
{
	/* nothing to do */
	if (len == 0)
		return 0;
	const uint32_t real_len = len - (end ? 1 : 0);
	/* DO_SHIFT mode is only allowed when tms is low
	 * otherwise everything must be written bit per bit
	 */
	uint32_t nb_byte = (_curr_tms == 0) ? real_len >> 3 : 0;
	uint32_t nb_bit = (_curr_tms == 0) ? (real_len & 0x07) : real_len;
	const uint8_t mode = (rx != NULL) ? DO_RDWR : DO_WRITE;

	const uint8_t *tx_ptr = tx;
	uint8_t *rx_ptr = rx;
	int ret;

	/* security: send residual
	 * it's possible since all functions do a flush at the end
	 * okay it's maybe less efficient
	 */
	_in_buf[_nb_bit++] = DEFAULT | DO_BITBB | DO_WRITE | _curr_tdi | _curr_tms;
	if ((ret = flush()) < 0) {
		printError("UsbBlaster::writeTDI: Failed to flush before write "
			"with error " + std::to_string(ret));
		return ret;
	}

	if (nb_byte != 0) {
		uint8_t mask = DO_SHIFT | mode;

		while (nb_byte != 0) {
			uint32_t tx_len = nb_byte;
			if (tx_len > static_cast<uint32_t>(_buffer_size) - 1)
				tx_len = _buffer_size - 1;
			/* if not enough space flush */
			if (_nb_bit + tx_len + 1 > _buffer_size) {
				const int num_read = _nb_bit -1;
				if (writeByte((rx)? rx_ptr:NULL, num_read) < 0)
					return -EXIT_FAILURE;
				if (rx)
					rx_ptr += num_read;
			}
			_in_buf[_nb_bit++] = mask | (tx_len & 0x3f);
			if (tx) {
				memcpy(&_in_buf[_nb_bit], tx_ptr, tx_len);
				_curr_tdi = (tx_ptr[tx_len - 1] & 0x80) ? _tdi_pin : 0;
				tx_ptr += tx_len;
			} else {
				memset(&_in_buf[_nb_bit], _curr_tdi ? 0xff : 0, tx_len);
			}
			_nb_bit += tx_len;

			nb_byte -= tx_len;
		}

		if (_nb_bit != 0) {
			int num_read = _nb_bit-1;
			if (writeByte((rx)? rx_ptr:NULL, num_read) < 0)
				return -EXIT_FAILURE;
			if (rx)
				rx_ptr += num_read;
		}
	}

	if (nb_bit != 0) {
		const uint8_t mask = DEFAULT | DO_BITBB | _curr_tms;
		/* limit iters to a multiple of 8 with _buffer_size respect
		 * this allows to have byte aligned when tms = 1 for
		 * sake of simplicity (avoids rebuilding the rx buffer)
		 */
		const uint32_t max_bits = (_buffer_size / 2) & ~0x07;
		uint32_t xfer_len = nb_bit;
		/* Here the buffer is known as empty:
		 * flushed at the beginning and nothing updated if
		 * nb_byte = 0
		 * cleaned by writeByte if nb_byte > 0
		 */
		while (xfer_len > 0) {
			const uint32_t xfer = (xfer_len > max_bits) ? max_bits: xfer_len;
			for (uint32_t i = 0; i < xfer; i++) {
				if (tx) {
					_curr_tdi =
						(tx_ptr[i >> 3] & (1 << (i & 0x07))) ? _tdi_pin : 0;
				}
				_in_buf[_nb_bit++] = mask | _curr_tdi;
				_in_buf[_nb_bit++] = mask | _curr_tdi | mode | _tck_pin;
			}

			/* the buffer is filled as much as possible -> it's possible
			 * to write it
			 */
			if (writeBit((rx)? rx_ptr : NULL, xfer) < 0)
				return -EXIT_FAILURE;
			xfer_len -= xfer;
			/* Update rx/tx ptr */
			if (rx)
				rx_ptr += xfer >> 3;
			if (tx)
				tx_ptr += xfer >> 3;
		}
		/* reworks nb_bit with a value similar to when nb_byte > 0
		 * ie keep bit index in the last Byte
		 */
		nb_bit &= 0x07;
		/* realign bits when a Byte is partially read */
		if (rx && nb_bit != 0)
			*rx_ptr >>= 8 - nb_bit;
	}

	/* set TMS high */
	if (end) {
		_curr_tms = _tms_pin;
		if (tx)
			_curr_tdi = (*tx_ptr & (1 << nb_bit)) == 0 ? 0 : _tdi_pin;
		const uint8_t mask = DEFAULT | DO_BITBB | _curr_tms | _curr_tdi;
		_in_buf[_nb_bit++] = mask;
		_in_buf[_nb_bit++] = mask | mode | _tck_pin;
		uint8_t tmp = 0;
		if (writeBit((rx)? &tmp : NULL, 1) < 0)
			return -EXIT_FAILURE;
		if (rx) {
			if (nb_bit == 0)
				*rx_ptr = 0;
			*rx_ptr |= (tmp & 0x80) >> (7 - nb_bit);
		}
		_in_buf[_nb_bit++] = mask;
		if (writeBit(NULL, 0) < 0)
			return -EXIT_FAILURE;
	}

	return len;
}

int UsbBlaster::toggleClk(uint8_t /*tms*/, uint8_t /*tdi*/, uint32_t clk_len)
{
	int ret;
	int xfer_len = clk_len;

	int mask = DO_SHIFT | DO_WRITE;

	/* try to use shift mode but only when
	 * xfer > 1Byte and tms is low
	 */
	if (_curr_tms == 0 && xfer_len >= 8) {
		if (_nb_bit == _buffer_size)
			if ((ret = flush()) < 0) {
				printError("UsbBlaster::toggleClk failed to flush "
					"with error " + std::to_string(ret));
				return ret;
			}

		_in_buf[_nb_bit++] = DEFAULT | DO_WRITE | DO_BITBB |
			_curr_tdi | _curr_tms;
		if ((ret = flush()) < 0) {
			printError("UsbBlaster::toggleClk failed to flush "
				"with error " + std::to_string(ret));
			return ret;
		}

		/* fill a byte with all 1 or all 0 */
		uint8_t content = (_curr_tdi) ? 0xff : 0;

		while (xfer_len >= 8) {
			uint32_t tx_len = (xfer_len >> 3);
			if (tx_len > static_cast<uint32_t>(_buffer_size) - 1)
				tx_len = _buffer_size - 1;
			/* if not enough space flush */
			if (_nb_bit + tx_len + 1 > _buffer_size)
				if (flush() < 0)
					return -EXIT_FAILURE;
			_in_buf[_nb_bit++] = mask | static_cast<uint8_t>(tx_len);
			for (uint32_t i = 0; i < tx_len; i++)
				_in_buf[_nb_bit++] = content;
			xfer_len -= (tx_len << 3);
		}
	}

	mask = DEFAULT | DO_BITBB | DO_WRITE | _curr_tms | _curr_tdi;
	while (xfer_len > 0) {
		if (_nb_bit + 2 > _buffer_size)
			if (flush() < 0)
				return -EXIT_FAILURE;
		_in_buf[_nb_bit++] = mask;
		_in_buf[_nb_bit++] = mask | _tck_pin;

		xfer_len--;
	}

	/* flush */
	if (_nb_bit == _buffer_size)
		if ((ret = flush()) < 0) {
			printError("UsbBlaster::toggleClk failed to force flush "
				"with error " + std::to_string(ret));
			return ret;
		}
	_in_buf[_nb_bit++] = mask;
	if ((ret = flush()) < 0) {
		printError("UsbBlaster::toggleClk failed for the final flush "
			"with error " + std::to_string(ret));
		return ret;
	}

	return clk_len;
}

int UsbBlaster::flush()
{
	return write(false, 0);
}

int UsbBlaster::setPassiveSerialPins(bool dclk, bool nconfig, bool data0)
{
	uint8_t value = DEFAULT | DO_WRITE | DO_BITBB;
	if (dclk)
		value |= _tck_pin;
	if (nconfig)
		value |= _tms_pin;
	if (data0)
		value |= _tdi_pin;

	return ll_driver->write(&value, 1, NULL, 0);
}

bool UsbBlaster::readConfDone()
{
	uint8_t value = DEFAULT | DO_READ | DO_BITBB | _tms_pin;
	uint8_t input = 0;
	int ret = ll_driver->write(&value, 1, &input, 1);
	if (ret <= 0)
		throw std::runtime_error("Failed to read CONF_DONE");

	return (input & 0x01) != 0;
}

int UsbBlaster::shiftPassiveSerial(const uint8_t *data, uint32_t length)
{
	uint8_t buffer[64];
	uint32_t offset = 0;

	while (offset < length) {
		uint32_t chunk = length - offset;
		if (chunk > 63)
			chunk = 63;

		buffer[0] = DO_SHIFT | DO_WRITE | static_cast<uint8_t>(chunk);
		memcpy(&buffer[1], &data[offset], chunk);
		int ret = ll_driver->write(buffer, static_cast<int>(chunk + 1), NULL, 0);
		if (ret != static_cast<int>(chunk + 1))
			return -1;
		offset += chunk;
	}

	return length;
}

int UsbBlaster::clockPassiveSerial(uint32_t cycles)
{
	uint8_t buffer[64];
	uint8_t value = DEFAULT | DO_WRITE | DO_BITBB | _tms_pin;
	uint32_t remaining = cycles;

	while (remaining != 0) {
		uint32_t chunk = remaining;
		if (chunk > 31)
			chunk = 31;

		for (uint32_t i = 0; i < chunk; i++) {
			buffer[2 * i] = value;
			buffer[2 * i + 1] = value | _tck_pin;
		}
		buffer[2 * chunk] = value;

		int length = static_cast<int>(2 * chunk + 1);
		if (ll_driver->write(buffer, length, NULL, 0) != length)
			return -1;
		remaining -= chunk;
	}

	return static_cast<int>(cycles);
}

void UsbBlaster::programPassiveSerial(const uint8_t *data, uint32_t length)
{
	if (data == NULL || length == 0)
		throw std::runtime_error("Passive serial bitstream is empty");
	if (flush() < 0)
		throw std::runtime_error("Failed to flush USB-Blaster");

	/*
	 * The normal destructor command drives TMS low, which is nCONFIG in
	 * passive serial mode and would immediately reset the configured FPGA.
	 */
	_passive_serial_mode = true;

	try {
		if (setPassiveSerialPins(false, false, false) != 1)
			throw std::runtime_error("Failed to assert nCONFIG");
		std::this_thread::sleep_for(std::chrono::microseconds(5));

		if (setPassiveSerialPins(false, true, false) != 1)
			throw std::runtime_error("Failed to release nCONFIG");

		/*
		 * nSTATUS is not available. Assume device will be ready after a small delay.
		 */
		std::this_thread::sleep_for(std::chrono::microseconds(2000));
		if (readConfDone())
			throw std::runtime_error("CONF_DONE did not clear after nCONFIG");

		if (shiftPassiveSerial(data, length) != static_cast<int>(length))
			throw std::runtime_error("Failed to write passive serial data");

		if (!readConfDone())
			throw std::runtime_error("CONF_DONE is low after passive serial "
				"programming");

		/* Complete device initialization after CONF_DONE is asserted. */
		if (clockPassiveSerial(10) != 10)
			throw std::runtime_error("Failed to write passive serial startup clocks");
	} catch (...) {
		setPassiveSerialPins(false, true, false);
		throw;
	}

	if (setPassiveSerialPins(false, true, false) != 1)
		throw std::runtime_error("Failed to leave passive serial pins idle");
}

/* simply call write and return buffer
 */
int UsbBlaster::writeByte(uint8_t *tdo, int nb_byte)
{
	int ret = write(tdo != NULL, nb_byte);
	if (tdo && ret > 0)
		memcpy(tdo, _in_buf, nb_byte);
	return ret;
}

/* call write with a temporary buffer
 * if tdo reconstruct message
 */
int UsbBlaster::writeBit(uint8_t *tdo, int nb_bit)
{
	int ret = write(tdo != NULL, nb_bit);
	if (tdo && ret > 0) {
		/* need to reconstruct received word 
		 * since jtag is LSB first we need to shift right content by 1
		 * and add 0x80 (1 << 7) or 0
		 * the buffer may contains some tms bit, so start with i
		 * equal to fill exactly nb_bit bits
		 * */
		for (int i = 0, offset=0; i < nb_bit; i++, offset++) {
			tdo[offset >> 3] = (((_in_buf[i] & (1<<0)) ? 0x80 : 0x00) |
							(tdo[offset >> 3] >> 1));
		}
	}

	return ret;
}

int UsbBlaster::write(bool read, int rd_len)
{
	if (_nb_bit == 0)
		return 0;

	int ret = ll_driver->write(_in_buf, _nb_bit,
			(read)?_in_buf:NULL, rd_len);
	_nb_bit = 0;
	return ret;
}

#ifdef ENABLE_USB_BLASTERI
/*
 * USB Blash I specific implementation
 */

UsbBlasterI::UsbBlasterI(uint16_t pid)
{
	int ret;
	_ftdi = ftdi_new();
	if (_ftdi == NULL)
		throw std::runtime_error("open_device: failed to initialize ftdi");

	 ret = ftdi_usb_open(_ftdi, 0x09fb, pid);
	if (ret < 0) {
		fprintf(stderr, "unable to open ftdi device: %d (%s)\n",
			ret, ftdi_get_error_string(_ftdi));
		ftdi_free(_ftdi);
		throw std::exception();
	}

	ret = ftdi_usb_reset(_ftdi);
	if (ret < 0) {
		fprintf(stderr, "Error reset: %d (%s)\n",
			ret, ftdi_get_error_string(_ftdi));
		ftdi_usb_close(_ftdi);
		ftdi_free(_ftdi);
		throw std::exception();
	}

	ret = ftdi_set_latency_timer(_ftdi, 2);
	if (ret < 0) {
		fprintf(stderr, "Error set latency timer: %d (%s)\n",
			ret, ftdi_get_error_string(_ftdi));
		ftdi_usb_close(_ftdi);
		ftdi_free(_ftdi);
		throw std::exception();
	}

	/* Force flush internal FT245 internal buffer */
	uint8_t val = DEFAULT | DO_WRITE | DO_BITBB | (1 << 1);  // tms=1
	uint8_t tmp_buf[4096];
	for (uint32_t nb_bit = 0; nb_bit < 4096; nb_bit += 2) {
		tmp_buf[nb_bit    ] = val;
		tmp_buf[nb_bit + 1] = val | (1 << 0);  // tck set
	}

	ret = ftdi_write_data(_ftdi, tmp_buf, 4096);
	if (ret != 4096)
		printWarn("UsbBlaster: failed to flush FT245 internal queue");

	/* drop bytes left in read FIFO at power-up */
	int n;
	for (n = 0; n < 200; n++) {
		ret = ftdi_read_data(_ftdi, tmp_buf, 4096);
		if (ret <= 0)
			break;
	}
	if (ret < 0 || n == 200)
		printWarn("UsbBlaster: failed to flush FT245 FIFO");
}

UsbBlasterI::~UsbBlasterI()
{
	ftdi_usb_close(_ftdi);
	ftdi_free(_ftdi);
}

int UsbBlasterI::setClkFreq(uint32_t clkHZ)
{
	(void) clkHZ;
	printWarn("USB-BlasterI has a 24MHz fixed frequency");
	return 1;
}

uint32_t UsbBlasterI::getClkFreq()
{
	return static_cast<uint32_t>(24e6);
}

int UsbBlasterI::write(uint8_t *wr_buf, int wr_len,
				uint8_t *rd_buf, int rd_len)
{
	int ret = 0;

	ret = ftdi_write_data(_ftdi, wr_buf, wr_len);
	if (ret != wr_len) {
		printf("problem %d written %d\n", ret, wr_len);
		return ret;
	}

	if (rd_buf) {
		int timeout = 100;
		int byte_read = 0;
		while (byte_read < rd_len && timeout != 0) {
			timeout--;
			ret = ftdi_read_data(_ftdi, rd_buf + byte_read, rd_len - byte_read);
			if (ret < 0) {
				printError("Read error: " + std::to_string(ret));
				return ret;
			}
			byte_read += ret;
		}

		/* compares to byte_read: both may be true at the same time
		 * avoid an error is this situation
		 */
		if (byte_read < rd_len) {
			printError("Error: timeout " + std::to_string(byte_read) +
				" " + std::to_string(rd_len));
			for (int i=0; i < byte_read; i++)
				printf("%02x ", rd_buf[i]);
			printf("\n");
			return -ETIME;
		}
	}
	return ret;
}
#endif

#ifdef ENABLE_USB_BLASTERII
/*
 * USB Blash II specific implementation
 */

UsbBlasterII::UsbBlasterII(const std::string &firmware_path)
{
	std::string fpath;
	uint8_t buf[6];
	if (firmware_path.empty() && strlen(BLASTERII_DIR) == 0) {
		printError("missing FX2 firmware");
		printError("use --probe-firmware with something");
		printError("like /opt/intelFPGA/VERSION/quartus/linux64/blaster_6810.hex");
		printError("Or use -DBLASTERII_PATH=/opt/intelFPGA/VERSION/quartus/linux64");
		throw std::runtime_error("usbBlasterII: missing firmware");
	}
	if (firmware_path.empty())
		fpath = BLASTERII_DIR "/blaster_6810.hex";
	else
		fpath = firmware_path;
	fx2 = new FX2_ll(0x09fb, 0x6810, 0x09fb, 0x6010, fpath);
	if (!fx2->read_ctrl(0x94, 0, buf, 5, 0x0000)) {
		delete fx2;
		throw std::runtime_error("Unable to read firmware version.");
	}
	buf[5] = '\0';
	printInfo("USB-Blaster II firmware version: " + std::string((char *)buf));
}

UsbBlasterII::~UsbBlasterII()
{
	delete fx2;
}

int UsbBlasterII::setClkFreq(uint32_t clkHZ)
{
	(void) clkHZ;
	printWarn("USB-BlasterII has a 24MHz fixed frequency");
	return 1;
}

uint32_t UsbBlasterII::getClkFreq()
{
	return static_cast<uint32_t>(24e6);
}

int UsbBlasterII::write(uint8_t *wr_buf, int wr_len,
				uint8_t *rd_buf, int rd_len)
{
	int ret = 0;

	ret = fx2->write(4, wr_buf, wr_len);
	if (ret != wr_len) {
		printf("problem %d written %d\n", ret, wr_len);
		return ret;
	}

	if (rd_buf) {
		uint8_t c = 0x5f;
		ret = fx2->write(4, &c, 1);
		if (ret != 1) {
			printf("problem %d written %d\n", ret, wr_len);
			return ret;
		}

		int timeout = 100;
		int byte_read = 0;
		while (byte_read < rd_len && timeout != 0) {
			timeout--;
			ret = fx2->read(8, rd_buf + byte_read, rd_len - byte_read);
			if (ret < 0) {
				printError("Read error: " + std::to_string(ret));
				return ret;
			}
			byte_read += ret;
		}

		if (timeout == 0) {
			printError("Error: timeout " + std::to_string(byte_read) +
				" " + std::to_string(rd_len));
			for (int i=0; i < byte_read; i++)
				printf("%02x ", rd_buf[i]);
			printf("\n");
			return -ETIME;
		}
	}
	return ret;
}
#endif
