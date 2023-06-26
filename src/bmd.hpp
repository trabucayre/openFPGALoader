// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2021 Uwe Bonnes bon@elektron.ikp.physik.tu-darmstadt.de
 */

#ifndef SRC_BMD_HPP_
#define SRC_BMD_HPP_

#include <libusb.h>

#include "jtagInterface.hpp"

/*!
 * \file Bmd.hpp
 * \class Bmd
 * \brief concrete class between jtag implementation and blackmagic debug probe remote protocoll
 * \author Uwe Bonnes
 */

class Bmd : public JtagInterface {
public:
	Bmd(std::string dev,
		const std::string &serial, uint32_t clkHZ, bool verbose);
	~Bmd(void);
	int setClkFreq(uint32_t clkHZ) override;
	uint32_t getClkFreq() { return _clkHZ;}
	/* TMS */
	int writeTMS(uint8_t *tms, uint32_t len, bool flush_buffer) override;
	/* TDI */
	int writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;
	/* clk */
	int toggleClk(uint8_t tms, uint8_t tdo, uint32_t clk_len) override;
	int get_buffer_size() override { return 0;}
	bool isFull() override {return false;}
	int flush() override { return 0;};
private:
	bool _verbose;				/**< display more message */
	bool platform_buffer_write(const void *const data, const size_t length);
	int platform_buffer_read(void *data, const size_t length);
	char *unhexify(void *buf, const char *hex, size_t size);
	void DEBUG_WIRE(const void *const data, const size_t length);
	void remote_v0_jtag_tdi_tdo_seq(uint8_t *data_out, bool final_tms, const uint8_t *data_in, size_t clock_cycles);
	uint64_t remote_hex_string_to_num(const uint32_t max, const char *const str);
protected:
	uint32_t _clkHZ;
};
#endif // SRC_BMD_HPP_
