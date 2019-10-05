#include <iostream>
#include <stdexcept>

#include "ftdijtag.hpp"
#include "bitparser.hpp"
#include "mcsParser.hpp"
#include "spiFlash.hpp"

#include "xilinx.hpp"
#include "part.hpp"

Xilinx::Xilinx(FtdiJtag *jtag, std::string filename):Device(jtag, filename)
	{
	if (_filename != ""){
		if (_file_extension == "bit")
			_mode = Device::MEM_MODE;
		else
			_mode = Device::SPI_MODE;
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
	_jtag->shiftIR(JSHUTDOWN, 6);
	_jtag->shiftIR(JPROGRAM, 6);
	_jtag->set_state(FtdiJtag::RUN_TEST_IDLE);
	_jtag->toggleClk(10000*12);

	_jtag->set_state(FtdiJtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2000);

	_jtag->shiftIR(BYPASS, 6);
	_jtag->set_state(FtdiJtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2000);

}

int Xilinx::idCode()
{
	unsigned char rx_data[4];
	_jtag->go_test_logic_reset();
	_jtag->shiftIR(IDCODE, 6);
	_jtag->shiftDR(NULL, rx_data, 32);
	return ((rx_data[0] & 0x000000ff) |
		((rx_data[1] << 8) & 0x0000ff00) |
		((rx_data[2] << 16) & 0x00ff0000) |
		((rx_data[3] << 24) & 0xff000000));
}

void Xilinx::program(unsigned int offset)
{
	switch (_mode) {
		case Device::NONE_MODE:
			return;
			break;
		case Device::SPI_MODE:
			program_spi(offset);
			reset();
			break;
		case Device::MEM_MODE:
			BitParser bitfile(_filename);
			bitfile.parse();
			program_mem(bitfile, offset);
			break;
	}
}

void Xilinx::program_spi(unsigned int offset)
{
	std::string bitname = "/usr/local/share/cycloader/spiOverJtag_";
	bitname += fpga_list[idCode()].family + ".bit";

	/* first: load spi over jtag */
	BitParser bitfile(bitname);
	bitfile.parse();
	program_mem(bitfile, offset);

	/* last: read file and erase/flash spi flash */
	McsParser mcs(_filename);
	mcs.parse();
	SPIFlash spiFlash(_jtag);
	spiFlash.erase_and_prog(offset, mcs.getData(), mcs.getLength());
}

void Xilinx::program_mem(BitParser &bitfile, unsigned int offset)
{
	if (_filename == "") return;
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
	_jtag->shiftIR(JPROGRAM, 6);
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
	_jtag->shiftIR(CFG_IN, 6);
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
	_jtag->shiftDR(bitfile.getData(), NULL, 8*bitfile.getLength());
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
	_jtag->shiftIR(JSTART, 6, FtdiJtag::UPDATE_IR);
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
