#include <iostream>
#include <stdexcept>

#include "ftdijtag.hpp"
#include "bitparser.hpp"

#include "xilinx.hpp"

Xilinx::Xilinx(FtdiJtag *jtag, enum prog_mode mode, std::string filename):Device(jtag, mode, filename),
	_bitfile(filename)
	{
	if (_mode == Device::SPI_MODE) {
		throw std::runtime_error("SPI flash is not supported on xilinx devices");
	}
	if (_mode != Device::NONE_MODE){
		_bitfile.parse();
	}
}
Xilinx::~Xilinx() {}

#define CFG_IN   0x05
#define USERCODE   0x08
#define IDCODE     0x09
#define ISC_ENABLE 0x10
#define JPROGRAM 0x0B
#define JSTART   0x0C
#define JSHUTDOWN 0x0D
#define ISC_DISABLE 0x16
#define BYPASS   0x3f

void Xilinx::reset()
{
	unsigned char instr;
	/*unsigned char reset_seq[] = {
			0xFF, 0xFF, 0xFF, 0xFF, // dummy word
	 		0xAA, 0x99, 0x55, 0x66, // sync word
	 		0x20, 0x00, 0x00, 0x00, // type1 noop
			//0x30, 0x02, 0x00, 0x01, 
	    	//0x00, 0x00, 0x00, 0x00,
		 	0x30, 0x00, 0x80, 0x01, // type1 write 1 word to CMD
		  	0x00, 0x00, 0x00, 0x0F, // iprog cmd
			0x20, 0x00, 0x00, 0x00}; // noop*/
	unsigned char reset_seq[] = {
			0xFF, 0xFF, 0xFF, 0xFF, // dummy word
	 		0x55, 0x99, 0xAA, 0x66, // sync word
	 		0x04, 0x00, 0x00, 0x00, // type1 noop
			0x0C, 0x40, 0x00, 0x80, 
	    	0x00, 0x00, 0x00, 0x00,
		 	0x0C, 0x00, 0x01, 0x80, // type1 write 1 word to CMD
		  	0x00, 0x00, 0x00, 0xf0, // iprog cmd
			0x04, 0x00, 0x00, 0x00}; // noop*/
	instr = JSHUTDOWN;
	_jtag->shiftIR(&instr, NULL, 6);
	_jtag->toggleClk(16);
	instr = CFG_IN;
	_jtag->shiftIR(&instr, NULL, 6);
	for (int i =0; i < 4*8; i++) {
		printf("%x\n", reset_seq[i]);
	}
	_jtag->shiftDR(reset_seq, NULL, 4*8*8, FtdiJtag::UPDATE_DR );
	_jtag->set_state(FtdiJtag::RUN_TEST_IDLE);
	instr = JSTART;
	_jtag->shiftIR(&instr, NULL, 6, FtdiJtag::UPDATE_IR);
	//_jtag->toggleClk(32);
	//instr = BYPASS;
	//_jtag->shiftIR(&instr, NULL, 6);
	//_jtag->toggleClk(1);
	_jtag->set_state(FtdiJtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2000);
	_jtag->go_test_logic_reset();
}

int Xilinx::idCode()
{
	unsigned char tx_data = IDCODE;
	unsigned char rx_data[4];
	_jtag->go_test_logic_reset();
	_jtag->shiftIR(&tx_data, NULL, 6);
	_jtag->shiftDR(NULL, rx_data, 32);
	return ((rx_data[0] & 0x000000ff) |
		((rx_data[1] << 8) & 0x0000ff00) |
		((rx_data[2] << 16) & 0x00ff0000) |
		((rx_data[3] << 24) & 0xff000000));
}

void Xilinx::flow_enable()
{
	unsigned char data;
	data = ISC_ENABLE;
	_jtag->shiftIR(&data, NULL, 6);
	//data[0]=0x0;
	//jtag->shiftDR(data,0,5);
	//io->cycleTCK(tck_len);
}


void Xilinx::flow_disable()
{
	  unsigned char data;

	data = ISC_DISABLE;
	_jtag->shiftIR(&data, NULL, 6);
	//io->cycleTCK(tck_len);
	//jtag->shiftIR(&BYPASS);
	//data[0]=0x0;
	//jtag->shiftDR(data,0,1);
	//io->cycleTCK(1);
}


void Xilinx::program()
{
	std::cout << "load program" << std::endl;
	unsigned char tx_buf, rx_buf;
	/*            comment                                TDI   TMS TCK
	 * 1: On power-up, place a logic 1 on the TMS,
	 *    and clock the TCK five times. This ensures      X     1   5
	 *    starting in the TLR (Test-Logic-Reset) state.
	 */
	_jtag->go_test_logic_reset();
	/*
	 * 2: Move into the RTI state.                        X     0   1
	 * 3: Move into the SELECT-IR state.                  X     1   2
	 * 4: Enter the SHIFT-IR state.                       X     0   2
	 * 5: Start loading the JPROGRAM instruction,     01011(4)  0   5
	 *    LSB first:
	 * 6: Load the MSB of the JPROGRAM instruction
	 *    when exiting SHIFT-IR, as defined in the        0     1   1
	 *    IEEE standard.
	 * 7: Place a logic 1 on the TMS and clock the
	 *    TCK five times. This ensures starting in        X     1   5
	 *    the TLR (Test-Logic-Reset) state.
	 */
	tx_buf = JPROGRAM;
	_jtag->shiftIR(&tx_buf, NULL, 6/*, FtdiJtag::TEST_LOGIC_RESET*/);
	/* test */
	tx_buf = BYPASS;
	do {
		_jtag->shiftIR(&tx_buf, &rx_buf, 6);
	} while (!(rx_buf &0x01));
	/*
	 * 8: Move into the RTI state.                        X     0   10,000(1)
	 */
	_jtag->set_state(FtdiJtag::RUN_TEST_IDLE);
	_jtag->toggleClk(10000*12);
	/*
	 * 9: Start loading the CFG_IN instruction,
	 *    LSB first:                                    00101   0   5
	 * 10: Load the MSB of CFG_IN instruction when
	 *     exiting SHIFT-IR, as defined in the            0     1   1
	 *     IEEE standard.
	 */
	tx_buf = CFG_IN;
	_jtag->shiftIR(&tx_buf, NULL, 6);
	/*
	 * 11: Enter the SELECT-DR state.                     X     1   2
	 */
	_jtag->set_state(FtdiJtag::SELECT_DR_SCAN);
	/*
	 * 12: Enter the SHIFT-DR state.                      X     0   2
	 */
	_jtag->set_state(FtdiJtag::SHIFT_DR);
	/*
	 * 13: Shift in the FPGA bitstream. Bitn (MSB)
	 *     is the first bit in the bitstream(2).    bit1...bitn 0  (bits in bitstream)-1
	 * 14: Shift in the last bit of the bitstream.
	 *     Bit0 (LSB) shifts on the transition to       bit0    1   1
	 *     EXIT1-DR.
	 */
	/* GGM: TODO */
	_jtag->shiftDR(_bitfile.getData(), NULL, 8*_bitfile.getLength());
	/*
	 * 15: Enter UPDATE-DR state.                         X     1   1
	 */
	_jtag->set_state(FtdiJtag::UPDATE_DR);
	/*
	 * 16: Move into RTI state.                           X     0   1
	 */
	_jtag->set_state(FtdiJtag::RUN_TEST_IDLE);
	/*
	 * 17: Enter the SELECT-IR state.                     X     1   2
	 * 18: Move to the SHIFT-IR state.                    X     0   2
	 * 19: Start loading the JSTART instruction 
	 *     (optional). The JSTART instruction           01100   0   5
	 *     initializes the startup sequence.
	 * 20: Load the last bit of the JSTART instruction.   0     1   1
	 * 21: Move to the UPDATE-IR state.                   X     1   1
	 */
	tx_buf = JSTART;
	_jtag->shiftIR(&tx_buf, NULL, 6, FtdiJtag::UPDATE_IR);
	/*
	 * 22: Move to the RTI state and clock the
	 *     startup sequence by applying a minimum         X     0   2000
	 *     of 2000 clock cycles to the TCK.
	 */
	_jtag->set_state(FtdiJtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2000);
	/*
	 * 23: Move to the TLR state. The device is
	 * now functional.                                    X     1   3
	 */
	_jtag->go_test_logic_reset();
}
