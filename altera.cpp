#include "altera.hpp"
#include "ftdijtag.hpp"
#include "device.hpp"
#include "epcq.hpp"

#define IDCODE 6
#define IRLENGTH 10
#define BIT_FOR_FLASH "/usr/local/share/openFPGALoader/test_sfl.svf"

Altera::Altera(FtdiJtag *jtag, std::string filename, bool verbose):
	Device(jtag, filename, verbose), _svf(_jtag, _verbose)
{
	if (_filename != "") {
		if (_file_extension == "svf")
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
	_jtag->set_state(FtdiJtag::TEST_LOGIC_RESET);
	_jtag->shiftIR(tx_buff, NULL, IRLENGTH);
	_jtag->toggleClk(1);
	_jtag->set_state(FtdiJtag::TEST_LOGIC_RESET);
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
		_svf.parse(_filename);
	} else if (_mode == Device::SPI_MODE) {
		/* GGM: TODO: fix this issue */
		EPCQ epcq(_jtag->vid(), _jtag->pid(), 2, 6000000);
		_svf.parse(BIT_FOR_FLASH);
		epcq.program(offset, _filename, (_file_extension == "rpd")? true:false);
		reset();
	}
}
int Altera::idCode()
{
	unsigned char tx_data = IDCODE;
	unsigned char rx_data[4];
	_jtag->go_test_logic_reset();
	_jtag->shiftIR(&tx_data, NULL, IRLENGTH);
	_jtag->shiftDR(NULL, rx_data, 32);
	return ((rx_data[0] & 0x000000ff) |
		((rx_data[1] << 8) & 0x0000ff00) |
		((rx_data[2] << 16) & 0x00ff0000) |
		((rx_data[3] << 24) & 0xff000000));
}
