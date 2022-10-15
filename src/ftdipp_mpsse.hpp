// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef _FTDIPP_MPSSE_H
#define _FTDIPP_MPSSE_H
#include <ftdi.h>
#include <string>

#include "cable.hpp"

class FTDIpp_MPSSE {
	public:
		FTDIpp_MPSSE(const cable_t &cable, const std::string &dev,
			const std::string &serial, uint32_t clkHZ, int8_t verbose = 0);
		~FTDIpp_MPSSE();

		int init(unsigned char latency, unsigned char bitmask_mode,
			unsigned char mode);
		int setClkFreq(uint32_t clkHZ);
		uint32_t getClkFreq() { return _clkHZ;}

		int vid() {return _vid;}
		int pid() {return _pid;}

		/* access gpio */
		/* read gpio */
		uint16_t gpio_get();
		uint8_t gpio_get(bool low_pins);
		/* update selected gpio */
		bool gpio_set(uint16_t gpio);
		bool gpio_set(uint8_t gpio, bool low_pins);
		bool gpio_clear(uint16_t gpio);
		bool gpio_clear(uint8_t gpio, bool low_pins);
		/* full access */
		bool gpio_write(uint16_t gpio);
		bool gpio_write(uint8_t gpio, bool low_pins);
		/* gpio direction */
		void gpio_set_dir(uint8_t dir, bool low_pins);
		void gpio_set_dir(uint16_t dir);
		/* configure as input low/high pins */
		void gpio_set_input(uint8_t gpio, bool low_pins);
		/* configure as input pins */
		void gpio_set_input(uint16_t gpio);
		/* configure as output low/high pins */
		void gpio_set_output(uint8_t gpio, bool low_pins);
		/* configure as output pins */
		void gpio_set_output(uint16_t gpio);

	protected:
		void open_device(const std::string &serial, unsigned int baudrate);
		void ftdi_usb_close_internal();
		int close_device();
		int mpsse_write();
		int mpsse_read(unsigned char *rx_buff, int len);
		int mpsse_store(unsigned char c);
		int mpsse_store(unsigned char *c, int len);
		int mpsse_get_buffer_size() {return _buffer_size;}
		unsigned int udevstufftoint(const char *udevstring, int base);
		bool search_with_dev(const std::string &device);
		int8_t _verbose;
		mpsse_bit_config _cable;
		int _vid;
		int _pid;
		int _index;
	private:
		uint8_t _bus;
		uint8_t _addr;
		char _product[64];
		unsigned char _interface;
		/* gpio */
		bool __gpio_write(bool low_pins);
	protected:
		uint32_t _clkHZ;
		struct ftdi_context *_ftdi;
		int _buffer_size;
		int _num;
		unsigned char *_buffer;
		uint8_t _iproduct[200];
};

#endif
