// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020-2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 * Copyright (C) 2022 Jean Biemar <jb@altaneos.com>
 *
 * Jetson nano bitbang driver added by Jean Biemar <jb@altaneos.com> in 2022
 */

#ifndef JETSONNANOBITBANG_H
#define JETSONNANOBITBANG_H

#include <string>

#include "board.hpp"
#include "jtagInterface.hpp"

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
#define GPIO_PORT_BASE				0x6000d000 // Port A
#define GPIO_PORT_MAX				0x6000d708 // Port EE
#define GPIO_PORT_SHORT_OFFSET_SIZE	4
#define GPIO_PORT_SHORT_OFFSET		0x00000004
#define GPIO_PORT_LARGE_OFFSET		0x00000100

/*!
 * \file JetsonNanoJtagBitbang.hpp
 * \class JetsonNanoJtagBitbang
 * \brief concrete class between jtag implementation and gpio bitbang
 * \author Jean Biemar
 */

class JetsonNanoJtagBitbang : public JtagInterface {
 public:
	JetsonNanoJtagBitbang(const jtag_pins_conf_t *pin_conf,
		const std::string &dev, uint32_t clkHZ, int8_t verbose);
	virtual ~JetsonNanoJtagBitbang();

	int setClkFreq(uint32_t clkHZ) override;
	int writeTMS(uint8_t *tms_buf, uint32_t len, bool flush_buffer) override;
	int writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;
	int toggleClk(uint8_t tms, uint8_t tdo, uint32_t clk_len) override;

	int get_buffer_size() override { return 0; }
	bool isFull() override { return false; }
	int flush() override { return 0; }

 private:
	//  layout based on the definitions above
	//  Each GPIO controller has four ports, each port controls 8 pins, each
	//  register is interleaved for the four ports, so
	//  REGX: port0, port1, port2, port3
	//  REGY: port0, port1, port2, port3
	typedef struct {
		uint32_t CNF;
		uint32_t _padding1[3];
		uint32_t OE;
		uint32_t _padding2[3];
		uint32_t OUT;
		uint32_t _padding3[3];
		uint32_t IN;
		uint32_t _padding4[3];
		uint32_t INT_STA;
		uint32_t _padding5[3];
		uint32_t INT_ENB;
		uint32_t _padding6[3];
		uint32_t INT_LVL;
		uint32_t _padding7[3];
		uint32_t INT_CLR;
		uint32_t _padding8[3];
	} gpio_t;

	int update_pins(int tck, int tms, int tdi);
	int read_tdo();

	bool _verbose;

	int _tck_pin;
	int _tms_pin;
	int _tdo_pin;
	int _tdi_pin;

	gpio_t volatile *_tms_port, *_tck_port, *_tdo_port, *_tdi_port;

	int _curr_tms;
	int _curr_tdi;
	int _curr_tck;
};
#endif
