#include "altera.hpp"

#include <string.h>

#include <string>

#include "jtag.hpp"
#include "device.hpp"
#include "epcq.hpp"
#include "progressBar.hpp"
#include "rawParser.hpp"

#define IDCODE 6
#define BYPASS 0x3FF
#define IRLENGTH 10
// DATA_DIR is defined at compile time.
#define BIT_FOR_FLASH (DATA_DIR "/openFPGALoader/test_sfl.svf")

Altera::Altera(Jtag *jtag, const std::string &filename,
	const std::string &file_type, bool verify, int8_t verbose):
	Device(jtag, filename, file_type, verify, verbose), _svf(_jtag, _verbose)
{
	if (!_file_extension.empty()) {
		if (_file_extension == "svf" || _file_extension == "rbf")
			_mode = Device::MEM_MODE;
		else
			_mode = Device::SPI_MODE;
	}
}
Altera::~Altera()
{}
void Altera::reset()
{
	/* PULSE_NCONFIG */
	unsigned char tx_buff[2] = {0x01, 0x00};
	_jtag->set_state(Jtag::TEST_LOGIC_RESET);
	_jtag->shiftIR(tx_buff, NULL, IRLENGTH);
	_jtag->toggleClk(1);
	_jtag->set_state(Jtag::TEST_LOGIC_RESET);
}

void Altera::programMem()
{
	RawParser _bit(_filename, false);
	_bit.parse();
	int byte_length = _bit.getLength()/8;
	uint8_t *data = _bit.getData();

	uint32_t clk_period = 1e9/(float)_jtag->getClkFreq();

	unsigned char cmd[2];
	unsigned char tx[864/8], rx[864/8];

	memset(tx, 0, 864/8);
	/* enddr idle
	 * endir irpause
	 * state idle
	 */
	/* ir 0x02 IRLENGTH */
	*reinterpret_cast<uint16_t *>(cmd) = 0x02;
	_jtag->shiftIR(cmd, NULL, IRLENGTH, Jtag::PAUSE_IR);
	/* RUNTEST IDLE 12000 TCK ENDSTATE IDLE; */
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000000/clk_period);
	/* write */
	ProgressBar progress("Flash SRAM", byte_length, 50, _quiet);

	int xfer_len = 512;
	int tx_len;
	int tx_end;

	for (int i=0; i < byte_length; i+=xfer_len) {
		if (i + xfer_len > byte_length) {  // last packet with some size
			tx_len = (byte_length - i) * 8;
			tx_end = Jtag::EXIT1_DR;
		} else {
			tx_len = xfer_len * 8;
			tx_end = Jtag::SHIFT_DR;
		}
		_jtag->shiftDR(data+i, NULL, tx_len, tx_end);
		progress.display(i);
	}
	progress.done();

	/* reboot */
	/* SIR 10 TDI (004); */
	*reinterpret_cast<uint16_t *>(cmd) = 0x04;
	_jtag->shiftIR(cmd, NULL, IRLENGTH, Jtag::PAUSE_IR);
	/* RUNTEST 60 TCK; */
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(5000/clk_period);
	/*
	 * SDR 864 TDI
	 * (000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)
	 * TDO (00000000000000000000
	 *     0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000080000000000000000000000000000000000000000)
	 *     MASK (00000000000000000000000000000000000000000000000000
	 *         0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000080000000000000000000000000000000000000000);
	 */
	_jtag->shiftDR(tx, rx, 864, Jtag::RUN_TEST_IDLE);
	/* TBD -> something to check */
	/* SIR 10 TDI (003); */
	*reinterpret_cast<uint16_t *>(cmd) = 0x003;
	_jtag->shiftIR(cmd, NULL, IRLENGTH, Jtag::PAUSE_IR);
	/* RUNTEST 49152 TCK; */
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(4099645/clk_period);
	/* RUNTEST 512 TCK; */
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(512);
	 /* SIR 10 TDI (3FF); */
	*reinterpret_cast<uint16_t *>(cmd) = BYPASS;
	_jtag->shiftIR(cmd, NULL, IRLENGTH, Jtag::PAUSE_IR);

	/* RUNTEST 12000 TCK; */
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000000/clk_period);
	/* -> idle */
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
}

void Altera::program(unsigned int offset)
{
	if (_mode == Device::NONE_MODE)
		return;
	/* in all case we consider svf is mandatory
	 * MEM_MODE : svf file provided for constructor
	 *            is the bitstream to use
	 * SPI_MODE : svf file provided is bridge to have
	 *            access to the SPI flash
	 */
	/* mem mode -> svf */
	if (_mode == Device::MEM_MODE) {
		if (_file_extension == "svf")
			_svf.parse(_filename);
		else
			programMem();
	} else if (_mode == Device::SPI_MODE) {
		/* GGM: TODO: fix this issue */
		EPCQ epcq(0x403, 0x6010/*_jtag->vid(), _jtag->pid()*/, 2, 6000000);
		_svf.parse(BIT_FOR_FLASH);
		epcq.program(offset, _filename, (_file_extension == "rpd")? true:false);

		if (_verify)
			printWarn("writing verification not supported");
		reset();
	}
}
int Altera::idCode()
{
	unsigned char tx_data[4] = {IDCODE};
	unsigned char rx_data[4];
	_jtag->go_test_logic_reset();
	_jtag->shiftIR(tx_data, NULL, IRLENGTH);
	memset(tx_data, 0, 4);
	_jtag->shiftDR(tx_data, rx_data, 32);
	return ((rx_data[0] & 0x000000ff) |
		((rx_data[1] << 8) & 0x0000ff00) |
		((rx_data[2] << 16) & 0x00ff0000) |
		((rx_data[3] << 24) & 0xff000000));
}
