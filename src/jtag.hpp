// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_JTAG_HPP_
#define SRC_JTAG_HPP_

#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "board.hpp"
#include "cable.hpp"
#include "jtagInterface.hpp"
#include "part.hpp"

class Jtag {
 public:
	Jtag(const cable_t &cable, const jtag_pins_conf_t *pin_conf,
		const std::string &dev,
		const std::string &serial, uint32_t clkHZ, int8_t verbose,
		const std::string &ip_adr, int port,
		const bool invert_read_edge = false,
		const std::string &firmware_path = "",
		const std::map<uint32_t, misc_device> &user_misc_devs = {});
	~Jtag();

	/* maybe to update */
	int setClkFreq(uint32_t clkHZ) { return _jtag->setClkFreq(clkHZ);}
	uint32_t getClkFreq() { return _jtag->getClkFreq();}

	/*!
	 * Return constant to describe if read is on rising or falling TCK edge
	 */
	JtagInterface::tck_edge_t getReadEdge() { return _jtag->getReadEdge();}
	/*!
	 * configure TCK edge used for read
	 */
	void setReadEdge(JtagInterface::tck_edge_t rd_edge) {
		_jtag->setReadEdge(rd_edge);
	}
	/*!
	 * Return constant to describe if write is on rising or falling TCK edge
	 */
	JtagInterface::tck_edge_t getWriteEdge() { return _jtag->getWriteEdge();}
	/*!
	 * configure TCK edge used for write
	 */
	void setWriteEdge(JtagInterface::tck_edge_t wr_edge) {
		_jtag->setWriteEdge(wr_edge);
	}

	/*!
	 * \brief scan JTAG chain to obtain IDCODE. Fill
	 *        a vector with all idcode and another
	 *        vector with irlength
	 * \return number of devices found
	 */
	int detectChain(unsigned int max_dev);

	/*!
	 * \brief return list of devices in the chain
	 * \return list of devices
	 */
	std::vector<uint32_t> get_devices_list() {return _devices_list;}

	/*!
	 * \brief return device index in list
	 * \return device index
	 */
	int get_device_index() {return device_index;}

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
	int device_select(unsigned index);
	/*!
	 * \brief inject a device into list at the begin
	 * \param[in] device_id: idcode
	 * \param[in] irlength: device irlength
	 * \return false if fails
	 */
	bool insert_first(uint32_t device_id, uint16_t irlength);

	/*!
	 * \brief return a pointer to the transport subclass
	 * \return a pointer instance of JtagInterface
	 */
	JtagInterface *get_ll_class() {return _jtag;}

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
		UNKNOWN = 16,
	};

	int shiftIR(unsigned char *tdi, unsigned char *tdo, int irlen,
		tapState_t end_state = RUN_TEST_IDLE);
	int shiftIR(unsigned char tdi, int irlen,
		tapState_t end_state = RUN_TEST_IDLE);
	int shiftDR(const uint8_t *tdi, unsigned char *tdo, int drlen,
		tapState_t end_state = RUN_TEST_IDLE);
	int read_write(const uint8_t *tdi, unsigned char *tdo, int len, char last);

	void toggleClk(int nb);
	void go_test_logic_reset();
	void set_state(tapState_t newState, const uint8_t tdi = 1);
	int flushTMS(bool flush_buffer = false);
	void flush() {flushTMS(); _jtag->flush();}
	void setTMS(unsigned char tms);

	const char *getStateName(tapState_t s);

	/* utilities */
	void setVerbose(int8_t verbose){_verbose = verbose;}

	JtagInterface *_jtag;

 private:
	/*!
	 * \brief search in fpga_list and misc_dev_list for a device with idcode
	 *        if found insert idcode and irlength in _devices_list and
	 *        _irlength_list
	 * \param[in] idcode: device idcode
	 * \return false if not found, true otherwise
	 */
	bool search_and_insert_device_with_idcode(uint32_t idcode);
	bool _verbose;
	tapState_t _state;
	int _tms_buffer_size;
	int _num_tms;
	unsigned char *_tms_buffer;
	std::string _board_name;
	const std::map<uint32_t, misc_device>& _user_misc_devs;

	int device_index; /*!< index for targeted FPGA */

	// Assume passive devices in JTAG chain are switched to BYPASS mode,
	// thus each device requeres 1 bit during SHIFT-DR
	unsigned _dr_bits_before, _dr_bits_after;
	std::vector<uint8_t> _dr_bits;

	// For the above we need for add BYPASS commands for each passive device
	unsigned _ir_bits_before, _ir_bits_after;
	std::vector<uint8_t> _ir_bits;

	std::vector<uint32_t> _devices_list; /*!< ordered list of devices idcode */
	std::vector<int16_t> _irlength_list; /*!< ordered list of irlength */
	uint8_t _curr_tdi;
};
#endif  // SRC_JTAG_HPP_
