// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef JTAG_H
#define JTAG_H
#include <ftdi.h>
#include <iostream>
#include <string>
#include <vector>

#include "board.hpp"
#include "cable.hpp"
#include "ftdipp_mpsse.hpp"
#include "jtagInterface.hpp"

class Jtag {
 public:
	Jtag(cable_t &cable, const jtag_pins_conf_t *pin_conf, std::string dev,
		const std::string &serial, uint32_t clkHZ, int8_t verbose = 0,
		const std::string &firmware_path="");
	~Jtag();

	/* maybe to update */
	int setClkFreq(uint32_t clkHZ) { return _jtag->setClkFreq(clkHZ);}
	uint32_t getClkFreq() { return _jtag->getClkFreq();}

	/*!
	 * \brief scan JTAG chain to obtain IDCODE. Fill
	 *        a vector with all idcode and another
	 *        vector with irlength
	 * \return number of devices found
	 */
	int detectChain(int max_dev);

	/*!
	 * \brief return list of devices in the chain
	 * \return list of devices
	 */
	std::vector<int> get_devices_list() {return _devices_list;}

	/*!
	 * \brief return current selected device idcode
	 * \return device idcode
	 */
	uint32_t get_target_device_id() {return _devices_list[device_index];}

	/*!
	 * \brief set index for targeted FPGA
	 * \param[in] index: index in the chain
	 * \return -1 if index is out of bound, index otherwise
	 */
	uint16_t device_select(uint16_t index);

	int shiftIR(unsigned char *tdi, unsigned char *tdo, int irlen,
		int end_state = RUN_TEST_IDLE);
	int shiftIR(unsigned char tdi, int irlen,
		int end_state = RUN_TEST_IDLE);
	int shiftDR(unsigned char *tdi, unsigned char *tdo, int drlen,
		int end_state = RUN_TEST_IDLE);
	int read_write(unsigned char *tdi, unsigned char *tdo, int len, char last);

	void toggleClk(int nb);
	void go_test_logic_reset();
	void set_state(int newState);
	int flushTMS(bool flush_buffer = false);
	void flush() {flushTMS(); _jtag->flush();}
	void setTMS(unsigned char tms);

	enum tapState_t {
		TEST_LOGIC_RESET = 0,
		RUN_TEST_IDLE = 1,
		SELECT_DR_SCAN = 2,
		CAPTURE_DR = 3,
		SHIFT_DR = 4,
		EXIT1_DR = 5,
		PAUSE_DR = 6,
		EXIT2_DR = 7,
		UPDATE_DR = 8,
		SELECT_IR_SCAN = 9,
		CAPTURE_IR = 10,
		SHIFT_IR = 11,
		EXIT1_IR = 12,
		PAUSE_IR = 13,
		EXIT2_IR = 14,
		UPDATE_IR = 15,
		UNKNOWN = 999
	};
	const char *getStateName(tapState_t s);

	/* utilities */
	void setVerbose(int8_t verbose){_verbose = verbose;}

	JtagInterface *_jtag;

 private:
	void init_internal(cable_t &cable, const std::string &dev, const std::string &serial,
		const jtag_pins_conf_t *pin_conf, uint32_t clkHZ,
		const std::string &firmware_path);
	int8_t _verbose;
	int _state;
	int _tms_buffer_size;
	int _num_tms;
	unsigned char *_tms_buffer;
	std::string _board_name;

	int device_index; /*!< index for targeted FPGA */
	std::vector<int32_t> _devices_list; /*!< ordered list of devices idcode */
	std::vector<int16_t> _irlength_list; /*!< ordered list of irlength */
};
#endif
