#ifndef FTDIJTAG_H
#define FTDIJTAG_H
#include <ftdi.h>
#include <iostream>
#include <vector>

#include "ftdipp_mpsse.hpp"

class FtdiJtag : public FTDIpp_MPSSE {
 public:
	//FtdiJtag(std::string board_name, int vid, int pid, unsigned char interface, uint32_t clkHZ);
	FtdiJtag(FTDIpp_MPSSE::mpsse_bit_config &cable, std::string dev,
		unsigned char interface, uint32_t clkHZ, bool verbose = false);
	FtdiJtag(FTDIpp_MPSSE::mpsse_bit_config &cable, unsigned char interface, uint32_t clkHZ,
		bool verbose);
	~FtdiJtag();

	int detectChain(std::vector<int> &devices, int max_dev);

	int shiftIR(unsigned char *tdi, unsigned char *tdo, int irlen, int end_state = RUN_TEST_IDLE);
	int shiftIR(unsigned char tdi, int irlen, int end_state = RUN_TEST_IDLE);
	int shiftDR(unsigned char *tdi, unsigned char *tdo, int drlen, int end_state = RUN_TEST_IDLE);
	int read_write(unsigned char *tdi, unsigned char *tdo, int len, char last);

	void toggleClk(int nb);
	void go_test_logic_reset();
	void set_state(int newState);
	int flushTMS(bool flush_buffer = false);
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
	void setVerbose(bool verbose){_verbose=verbose;}

 private:
	int _state;
	int _tms_buffer_size;
	int _num_tms;
	unsigned char *_tms_buffer;
	std::string _board_name;
	bool _verbose;
};
#endif
