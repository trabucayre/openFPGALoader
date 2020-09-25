#include <iostream>
#include <stdexcept>

#include "jtag.hpp"
#include "bitparser.hpp"
#include "configBitstreamParser.hpp"
#include "mcsParser.hpp"
#include "spiFlash.hpp"
#include "rawParser.hpp"

#include "display.hpp"
#include "xilinx.hpp"
#include "part.hpp"
#include "progressBar.hpp"

Xilinx::Xilinx(Jtag *jtag, const std::string &filename, bool verbose):
	Device(jtag, filename, verbose)
{
	if (_filename != ""){
		if (_file_extension == "bit")
			_mode = Device::MEM_MODE;
		else
			_mode = Device::SPI_MODE;
	}
}
Xilinx::~Xilinx() {}

#define USER1	0x02
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
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(10000*12);

	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2000);

	_jtag->shiftIR(BYPASS, 6);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2000);
}

int Xilinx::idCode()
{
	unsigned char tx_data[4]= {0x00, 0x00, 0x00, 0x00};
	unsigned char rx_data[4];
	_jtag->go_test_logic_reset();
	_jtag->shiftIR(IDCODE, 6);
	_jtag->shiftDR(tx_data, rx_data, 32);
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
			BitParser bitfile(_filename, true, _verbose);
			bitfile.parse();
			program_mem(bitfile);
			break;
	}
}

void Xilinx::program_spi(unsigned int offset)
{
	// DATA_DIR is defined at compile time.
	std::string bitname = DATA_DIR "/openFPGALoader/spiOverJtag_";
	bitname += fpga_list[idCode()].model + ".bit";

	/* first: load spi over jtag */
	BitParser bitfile(bitname, true, _verbose);
	bitfile.parse();
	program_mem(bitfile);

	/* last: read file and erase/flash spi flash */
	ConfigBitstreamParser *_bit;
	if (_file_extension == "mcs")
		_bit = new McsParser(_filename, false, _verbose);
	else {
		if (offset == 0) {
			printError("Error: can't write raw data at the beginning of the flash");
			throw std::exception();
		}
		_bit = new RawParser(_filename, false);
	}

	int err = _bit->parse();
	printInfo("Parse file ", false);
	if (err == EXIT_FAILURE) {
		printError("FAIL");
		return;
	} else {
		printSuccess("DONE");
	}

	SPIFlash spiFlash(this, _verbose);
	spiFlash.reset();
	spiFlash.read_id();
	spiFlash.read_status_reg();
	spiFlash.erase_and_prog(offset, _bit->getData(), _bit->getLength()/8);
	delete _bit;
}

void Xilinx::program_mem(BitParser &bitfile)
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
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
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
	_jtag->set_state(Jtag::SELECT_DR_SCAN);
	/*
	 * 12: Enter the SHIFT-DR state.                      X     0   2
	 */
	_jtag->set_state(Jtag::SHIFT_DR);
	/*
	 * 13: Shift in the FPGA bitstream. Bitn (MSB)
	 *     is the first bit in the bitstream(2).    bit1...bitn 0  (bits in bitstream)-1
	 * 14: Shift in the last bit of the bitstream.
	 *     Bit0 (LSB) shifts on the transition to       bit0    1   1
	 *     EXIT1-DR.
	 */
	/* GGM: TODO */
	int byte_length = bitfile.getLength() / 8;
	uint8_t *data = bitfile.getData();
	int tx_len, tx_end;
	int burst_len = byte_length / 100;

	ProgressBar progress("Flash SRAM", byte_length, 50);

	for (int i=0; i < byte_length; i+=burst_len) {
		if (i + burst_len > byte_length) {
			tx_len = (byte_length - i) * 8;
			tx_end = 1;
		} else {
			tx_len = burst_len * 8;
			tx_end = 0;
		}
		_jtag->read_write(data+i, NULL, tx_len, tx_end);
		_jtag->flush();
		progress.display(i);
	}
	progress.done();
	/*
	 * 15: Enter UPDATE-DR state.                         X     1   1
	 */
	_jtag->set_state(Jtag::UPDATE_DR);
	/*
	 * 16: Move into RTI state.                           X     0   1
	 */
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	/*
	 * 17: Enter the SELECT-IR state.                     X     1   2
	 * 18: Move to the SHIFT-IR state.                    X     0   2
	 * 19: Start loading the JSTART instruction 
	 *     (optional). The JSTART instruction           01100   0   5
	 *     initializes the startup sequence.
	 * 20: Load the last bit of the JSTART instruction.   0     1   1
	 * 21: Move to the UPDATE-IR state.                   X     1   1
	 */
	_jtag->shiftIR(JSTART, 6, Jtag::UPDATE_IR);
	/*
	 * 22: Move to the RTI state and clock the
	 *     startup sequence by applying a minimum         X     0   2000
	 *     of 2000 clock cycles to the TCK.
	 */
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2000);
	/*
	 * 23: Move to the TLR state. The device is
	 * now functional.                                    X     1   3
	 */
	_jtag->go_test_logic_reset();
}

/*
 * jtag : jtag interface
 * cmd  : opcode for SPI flash
 * tx   : buffer to send
 * rx   : buffer to fill
 * len  : number of byte to send/receive (cmd not comprise)
 *        so to send only a cmd set len to 0 (or omit this param)
 */
int Xilinx::spi_put(uint8_t cmd,
			uint8_t *tx, uint8_t *rx, uint32_t len)
{
	int xfer_len = len + 1 + ((rx == NULL) ? 0 : 1);
	uint8_t jtx[xfer_len];
	jtx[0] = McsParser::reverseByte(cmd);
	/* uint8_t jtx[xfer_len] = {McsParser::reverseByte(cmd)}; */
	uint8_t jrx[xfer_len];
	if (tx != NULL) {
		for (uint32_t i=0; i < len; i++)
			jtx[i+1] = McsParser::reverseByte(tx[i]);
	}
	/* addr BSCAN user1 */
	_jtag->shiftIR(USER1, 6);
	/* send first already stored cmd,
	 * in the same time store each byte
	 * to next
	 */
	_jtag->shiftDR(jtx, (rx == NULL)? NULL: jrx, 8*xfer_len);

	if (rx != NULL) {
		for (uint32_t i=0; i < len; i++)
			rx[i] = McsParser::reverseByte(jrx[i+1] >> 1) | (jrx[i+2] & 0x01);
	}
	return 0;
}

int Xilinx::spi_put(uint8_t *tx, uint8_t *rx, uint32_t len)
{
	int xfer_len = len + ((rx == NULL) ? 0 : 1);
	uint8_t jtx[xfer_len];
	uint8_t jrx[xfer_len];
	if (tx != NULL) {
		for (uint32_t i=0; i < len; i++)
			jtx[i] = McsParser::reverseByte(tx[i]);
	}
	/* addr BSCAN user1 */
	_jtag->shiftIR(USER1, 6);
	/* send first already stored cmd,
	 * in the same time store each byte
	 * to next
	 */
	_jtag->shiftDR(jtx, (rx == NULL)? NULL: jrx, 8*xfer_len);

	if (rx != NULL) {
		for (uint32_t i=0; i < len; i++)
			rx[i] = McsParser::reverseByte(jrx[i] >> 1) | (jrx[i+1] & 0x01);
	}
	return 0;
}

int Xilinx::spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
			uint32_t timeout, bool verbose)
{
	uint8_t rx[2];
	uint8_t dummy[2];
	uint8_t tmp;
	uint8_t tx = McsParser::reverseByte(cmd);
	uint32_t count = 0;

	_jtag->shiftIR(USER1, 6, Jtag::UPDATE_IR);
	_jtag->set_state(Jtag::SHIFT_DR);
	_jtag->read_write(&tx, NULL, 8, 0);

	do {
		_jtag->read_write(dummy, rx, 8*2, 0);
		tmp = (McsParser::reverseByte(rx[0]>>1)) | (0x01 & rx[1]);
		count++;
		if (count == timeout){
			printf("timeout: %x %x %x\n", tmp, rx[0], rx[1]);
			break;
		}
		if (tmp & ~0x3) {
			printf("Error: rx %x %x %x\n", tmp, McsParser::reverseByte(rx[0]), rx[1]);
			count = timeout;
			break;
		}
		if (verbose) {
			printf("%x %x %x %u\n", tmp, mask, cond, count);
		}
	} while ((tmp & mask) != cond);
	_jtag->go_test_logic_reset();

	if (count == timeout) {
		printf("%x\n", tmp);
		std::cout << "wait: Error" << std::endl;
		return -ETIME;
	} else {
		return 0;
	}
}
