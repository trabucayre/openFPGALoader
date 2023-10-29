// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020-2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 * Copyright (C) 2022 Jean Biemar <jb@altaneos.com>
 *
 * Jetson nano bitbang driver added by Jean Biemar <jb@altaneos.com> in 2022
 */

#include "jetsonNanoJtagBitbang.hpp"

#include <stdio.h>
#include <string.h>

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <stdexcept>

#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/mman.h>

#include "display.hpp"

#define DEBUG 1

#ifdef DEBUG
#define display(...) \
	do { \
		if (_verbose) fprintf(stdout, __VA_ARGS__); \
	}while(0)
#else
#define display(...) do {}while(0)
#endif

/* Tegra X1 SoC Technical Reference Manual, version 1.3
 *
 * See Chapter 9 "Multi-Purpose I/O Pins", section 9.13 "GPIO Registers"
 * (table 32: GPIO Register Address Map)
 *
 * The GPIO hardware shares PinMux with up to 4 Special Function I/O per
 * pin, and only one of those five functions (SFIO plus GPIO) can be routed to
 * a pin at a time, using the PixMux.
 *
 * In turn, the PinMux outputs signals to Pads using Pad Control Groups. Pad
 * control groups control things like "drive strength" and "slew rate," and
 * need to be reset after deep sleep. Also, different pads have different
 * voltage tolerance. Pads marked "CZ" can be configured to be 3.3V tolerant
 * and driving; and pads marked "DD" can be 3.3V tolerant when in open-drain
 * mode (only.)
 *
 * The CNF register selects GPIO or SFIO, so setting it to 1 forces the GPIO
 * function. This is convenient for those who have a different pinmux at boot.
 */

#define GPIO_SET_BIT(REG, BIT) 		REG |= 1UL << BIT
#define GPIO_CLEAR_BIT(REG, BIT) 	REG &= ~(1UL << BIT)

JetsonNanoJtagBitbang::JetsonNanoJtagBitbang(
		const jtag_pins_conf_t *pin_conf,
		const std::string &dev, __attribute__((unused)) uint32_t clkHZ,
		int8_t verbose):_verbose(verbose>1);
{
	uint32_t tms_port_reg, tck_port_reg, tdi_port_reg, tdo_port_reg;

	_tck_pin = pin_conf->tck_pin;
	_tms_pin = pin_conf->tms_pin;
	_tdi_pin = pin_conf->tdi_pin;
	_tdo_pin = pin_conf->tdo_pin;

	display("Jetson Nano jtag bitbang driver, tck_pin=%d, tms_pin=%d, tdi_pin=%d, tdo_pin=%d\n",
		_tck_pin, _tms_pin, _tdi_pin, _tdo_pin);

	/* Validate pins */
	int pins[] = {_tck_pin, _tms_pin, _tdi_pin, _tdo_pin};
	for (uint32_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
		if (pins[i] < 0 || pins[i] >= 1000) {
			display("Pin %d is outside of valid range\n", pins[i]);
			throw std::runtime_error("A pin is outside of valid range\n");
		}

		for (uint32_t j = i + 1; j < sizeof(pins) / sizeof(pins[0]); j++) {
			if (pins[i] == pins[j]) {
				display("Two or more pins are assigned to the same pin number %d\n", pins[i]);
				throw std::runtime_error("Two or more pins are assigned to the same pin number\n");
			}
		}
	}

	/* Get ports */
	tms_port_reg = GPIO_PORT_BASE + ((_tms_pin/8)/GPIO_PORT_SHORT_OFFSET_SIZE*GPIO_PORT_LARGE_OFFSET) + ((_tms_pin/8)%GPIO_PORT_SHORT_OFFSET_SIZE*GPIO_PORT_SHORT_OFFSET);
	tck_port_reg = GPIO_PORT_BASE + ((_tck_pin/8)/GPIO_PORT_SHORT_OFFSET_SIZE*GPIO_PORT_LARGE_OFFSET) + ((_tck_pin/8)%GPIO_PORT_SHORT_OFFSET_SIZE*GPIO_PORT_SHORT_OFFSET);
	tdi_port_reg = GPIO_PORT_BASE + ((_tdi_pin/8)/GPIO_PORT_SHORT_OFFSET_SIZE*GPIO_PORT_LARGE_OFFSET) + ((_tdi_pin/8)%GPIO_PORT_SHORT_OFFSET_SIZE*GPIO_PORT_SHORT_OFFSET);
	tdo_port_reg = GPIO_PORT_BASE + ((_tdo_pin/8)/GPIO_PORT_SHORT_OFFSET_SIZE*GPIO_PORT_LARGE_OFFSET) + ((_tdo_pin/8)%GPIO_PORT_SHORT_OFFSET_SIZE*GPIO_PORT_SHORT_OFFSET);

	/* Get pin */
	_tms_pin %= 8;
	_tck_pin %= 8;
	_tdi_pin %= 8;
	_tdo_pin %= 8;

	display("reg:pin details: TMS %x:%i, TCK %x:%i, TDI %x:%i, TDO %x:%i\n", tms_port_reg, _tms_pin, tck_port_reg, _tck_pin, tdi_port_reg, _tdi_pin, tdo_port_reg, _tdo_pin);

	_curr_tdi = 0;
	_curr_tck = 0;
	_curr_tms = 1;

	// FIXME: I'm unsure how this value should be set.
	// Maybe experiment, or think through what it should be.
	_clkHZ = 5000000;

	//  read physical memory (needs root)
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
		display("Can't open /dev/mem. Error %x\n", errno);
		throw std::runtime_error("No access to /dev/mem\n");
    }
    //  map a particular physical address into our address space
    int pagesize = getpagesize();
    int pagemask = pagesize-1;

    //  This page will actually contain all the GPIO controllers, because they are co-located
    void *base = mmap(0, pagesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, ((tms_port_reg | tck_port_reg | tdi_port_reg | tdo_port_reg) & ~pagemask));
    if (base == NULL) {
		display("mmap return error\n");
		throw std::runtime_error("mmap error\n");
    }

	_tms_port = (gpio_t volatile *)((char *)base + (tms_port_reg & pagemask));
	_tck_port = (gpio_t volatile *)((char *)base + (tck_port_reg & pagemask));
	_tdi_port = (gpio_t volatile *)((char *)base + (tdi_port_reg & pagemask));
	_tdo_port = (gpio_t volatile *)((char *)base + (tdo_port_reg & pagemask));

	// for _tms_port : GPIO OUT
	GPIO_SET_BIT(_tms_port->CNF, _tms_pin);
	GPIO_SET_BIT(_tms_port->OE, _tms_pin);
	GPIO_SET_BIT(_tms_port->OUT, _tms_pin);

	// for _tck_port : GPIO OUT
	GPIO_SET_BIT(_tck_port->CNF, _tck_pin);
	GPIO_SET_BIT(_tck_port->OE, _tck_pin);
	GPIO_CLEAR_BIT(_tck_port->OUT, _tck_pin);

	// for _tdi_port : GPIO OUT
	GPIO_SET_BIT(_tdi_port->CNF, _tdi_pin);
	GPIO_SET_BIT(_tdi_port->OE, _tdi_pin);
	GPIO_CLEAR_BIT(_tdi_port->OUT, _tdi_pin);

	// for _tdo_port : GPIO IN
	GPIO_SET_BIT(_tdo_port->CNF, _tdo_pin);
	GPIO_CLEAR_BIT(_tdo_port->OE , _tdo_pin);
	GPIO_CLEAR_BIT(_tdo_port->IN, _tdo_pin);
}

JetsonNanoJtagBitbang::~JetsonNanoJtagBitbang()
{
	GPIO_CLEAR_BIT(_tms_port->OE, _tms_pin);
	GPIO_CLEAR_BIT(_tck_port->OE, _tck_pin);
	GPIO_CLEAR_BIT(_tdi_port->OE, _tdi_pin);

	GPIO_CLEAR_BIT(_tms_port->CNF, _tms_pin);
	GPIO_CLEAR_BIT(_tck_port->CNF, _tck_pin);
	GPIO_CLEAR_BIT(_tdi_port->CNF, _tdi_pin);
	GPIO_CLEAR_BIT(_tdo_port->CNF, _tdo_pin);
}

int JetsonNanoJtagBitbang::update_pins(int tck, int tms, int tdi)
{
	if (tdi != _curr_tdi) {
		if(tdi)
			GPIO_SET_BIT(_tdi_port->OUT, _tdi_pin);
		else
			GPIO_CLEAR_BIT(_tdi_port->OUT, _tdi_pin);
	}

	if (tms != _curr_tms) {
		if(tms)
			GPIO_SET_BIT(_tms_port->OUT, _tms_pin);
		else
			GPIO_CLEAR_BIT(_tms_port->OUT, _tms_pin);
	}

	if (tck != _curr_tck) {
		if(tck)
			GPIO_SET_BIT(_tck_port->OUT, _tck_pin);
		else
			GPIO_CLEAR_BIT(_tck_port->OUT, _tck_pin);
	}

	_curr_tdi = tdi;
	_curr_tms = tms;
	_curr_tck = tck;

	return 0;
}

int JetsonNanoJtagBitbang::read_tdo()
{
	return (_tdo_port->IN>>_tdo_pin) & 0x01;
}

int JetsonNanoJtagBitbang::setClkFreq(__attribute__((unused)) uint32_t clkHZ)
{
	// FIXME: The assumption is that calling the gpiod_line_set_value
	// routine will limit the clock frequency to lower than what is specified.
	// This needs to be verified, and possibly artificial delays should be added.
	return 0;
}

int JetsonNanoJtagBitbang::writeTMS(uint8_t *tms_buf, uint32_t len,
		__attribute__((unused)) bool flush_buffer,
		__attribute__((unused)) const uint8_t tdi)
{
	int tms;

	if (len == 0) // nothing -> stop
		return len;

	for (uint32_t i = 0; i < len; i++) {
		tms = ((tms_buf[i >> 3] & (1 << (i & 7))) ? 1 : 0);

		update_pins(0, tms, 0);
		update_pins(1, tms, 0);
	}

	update_pins(0, tms, 0);

	return len;
}

int JetsonNanoJtagBitbang::writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end)
{
	int tms = _curr_tms;
	int tdi = _curr_tdi;

	if (rx)
		memset(rx, 0, len / 8);

	for (uint32_t i = 0; i < len; i++) {
		if (end && (i == len - 1))
			tms = 1;

		if (tx)
			tdi = (tx[i >> 3] & (1 << (i & 7))) ? 1 : 0;

		update_pins(0, tms, tdi);
		update_pins(1, tms, tdi);

		if (rx) {
			if (read_tdo() > 0)
				rx[i >> 3] |= 1 << (i & 7);
		}
	}

	update_pins(0, tms, tdi);

	return len;
}

int JetsonNanoJtagBitbang::toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len)
{
	for (uint32_t i = 0; i < clk_len; i++) {
		update_pins(0, tms, tdi);
		update_pins(1, tms, tdi);
	}

	update_pins(0, tms, tdi);

	return clk_len;
}
