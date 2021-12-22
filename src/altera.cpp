// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include "altera.hpp"

#include <string.h>

#include <string>

#include "jtag.hpp"
#include "device.hpp"
#include "epcq.hpp"
#include "progressBar.hpp"
#include "rawParser.hpp"

#define IDCODE 6
#define USER0  0x0C
#define USER1  0x0E
#define BYPASS 0x3FF
#define IRLENGTH 10
// DATA_DIR is defined at compile time.
#define BIT_FOR_FLASH (DATA_DIR "/openFPGALoader/test_sfl.svf")

Altera::Altera(Jtag *jtag, const std::string &filename,
	const std::string &file_type, Device::prog_type_t prg_type,
	const std::string &device_package, bool verify, int8_t verbose):
	Device(jtag, filename, file_type, verify, verbose),
	SPIInterface(filename, verbose, 256, verify),
	_svf(_jtag, _verbose), _device_package(device_package),
	_vir_addr(0x1000), _vir_length(14)
{
	if (prg_type == Device::RD_FLASH) {
		_mode = Device::READ_MODE;
	} else {
		if (!_file_extension.empty()) {
			if (_file_extension == "svf") {
				_mode = Device::MEM_MODE;
			} else if (_file_extension == "rpd" ||
					_file_extension == "rbf") {
				if (prg_type == Device::WR_SRAM)
					_mode = Device::MEM_MODE;
				else
					_mode = Device::SPI_MODE;
			} else { // unknown type -> sanity check
				if (prg_type == Device::WR_SRAM) {
					printError("file has an unknown type:");
					printError("\tplease use rbf or svf file");
					printError("\tor use --write-flash with: ", false);
					printError("-b board_name or --fpga_part xxxx");
					std::runtime_error("Error: wrong file");
				} else {
					_mode = Device::SPI_MODE;
				}
			}
		}
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

void Altera::programMem(RawParser &_bit)
{
	int byte_length = _bit.getLength()/8;
	uint8_t *data = _bit.getData();

	uint32_t clk_period = 1e9/static_cast<float>(_jtag->getClkFreq());

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

bool Altera::load_bridge()
{
	if (_device_package.empty()) {
		printError("Can't program SPI flash: missing device-package information");
		return false;
	}

	// DATA_DIR is defined at compile time.
	std::string bitname = DATA_DIR "/openFPGALoader/spiOverJtag_";
#ifdef HAS_ZLIB
	bitname += _device_package + ".rbf.gz";
#else
	bitname += _device_package + ".rbf";
#endif

	std::cout << "use: " << bitname << std::endl;

	/* first: load spi over jtag */
	try {
		RawParser bridge(bitname, false);
		bridge.parse();
		programMem(bridge);
	} catch (std::exception &e) {
		printError(e.what());
		throw std::runtime_error(e.what());
	}

	return true;
}

void Altera::program(unsigned int offset, bool unprotect_flash)
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
		if (_file_extension == "svf") {
			_svf.parse(_filename);
		} else {
			RawParser _bit(_filename, false);
			_bit.parse();
			programMem(_bit);
		}
	} else if (_mode == Device::SPI_MODE) {
		// reverse only bitstream raw binaries data no
		bool reverseOrder = false;
		if (_file_extension == "rbf" || _file_extension == "rpd")
			reverseOrder = true;

		/* prepare data to write */
		uint8_t *data = NULL;
		int length = 0;

		RawParser bit(_filename, reverseOrder);
		try {
			bit.parse();
			data = bit.getData();
			length = bit.getLength() / 8;
		} catch (std::exception &e) {
			printError(e.what());
			throw std::runtime_error(e.what());
		}

		if (!SPIInterface::write(offset, data, length, unprotect_flash))
			throw std::runtime_error("Fail to write data");
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

/* SPI interface */

int Altera::spi_put(uint8_t cmd, uint8_t *tx, uint8_t *rx, uint32_t len)
{
	/* +1 because send first cmd + len byte + 1 for rx due to a delay of
	 * one bit
	 */
	int xfer_len = len + 1 + ((rx == NULL) ? 0 : 1);
	uint8_t jtx[xfer_len];
	uint8_t jrx[xfer_len];

	if (tx != NULL) {
		for (uint32_t i = 0; i < len; i++)
			jtx[i] = RawParser::reverseByte(tx[i]);
	}

	shiftVIR(RawParser::reverseByte(cmd));
	shiftVDR(jtx, (rx) ? jrx : NULL, 8 * xfer_len);

	if (rx) {
		for (uint32_t i = 0; i < len; i++) {
			rx[i] = RawParser::reverseByte(jrx[i+1] >> 1) | (jrx[i+2] & 0x01);
		}
	}

	return 0;
}
int Altera::spi_put(uint8_t *tx, uint8_t *rx, uint32_t len)
{
	return spi_put(tx[0], &tx[1], rx, len-1);
}

int Altera::spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
		uint32_t timeout, bool verbose)
{
	uint8_t rx[3];
	uint8_t tmp;
	uint32_t count = 0;
	bool first = true;

	shiftVIR(RawParser::reverseByte(cmd));
	do {
		if (first) {
			first = false;
			shiftVDR(NULL, rx, 24, Jtag::SHIFT_DR);
			tmp = RawParser::reverseByte(rx[1] >> 1) | (rx[2] & 0x01);
		} else {
			_jtag->shiftDR(NULL, rx, 16, Jtag::SHIFT_DR);
			tmp = RawParser::reverseByte(rx[0] >> 1) | (rx[1] & 0x01);
		}

		count++;
		if (count == timeout){
			printf("timeout: %x %x %x\n", tmp, rx[0], rx[1]);
			break;
		}

		if (verbose) {
			printf("%x %x %x %u\n", tmp, mask, cond, count);
		}
	} while ((tmp & mask) != cond);
	_jtag->set_state(Jtag::UPDATE_DR);

	if (count == timeout) {
		printf("%x\n", tmp);
		std::cout << "wait: Error" << std::endl;
		return -1;
	}
	return 0;
}

/* VIrtual Jtag Access */
void Altera::shiftVIR(uint32_t reg)
{
	uint32_t len = _vir_length;
	uint32_t mask = (1 << len) - 1;
	uint32_t tmp = (reg & mask) | _vir_addr;
	uint8_t *tx = (uint8_t *) & tmp;
	uint8_t tx_ir[2] = {USER1, 0};

	_jtag->set_state(Jtag::RUN_TEST_IDLE);

	_jtag->shiftIR(tx_ir, NULL, IRLENGTH, Jtag::UPDATE_IR);
	/* len + 1 + 1 => IRLENGTH + Slave ID + 1 (ASMI/SFL) */
	_jtag->shiftDR(tx, NULL, len/* + 2*/, Jtag::UPDATE_DR);
}

void Altera::shiftVDR(uint8_t * tx, uint8_t * rx, uint32_t len,
		int end_state, bool debug)
{
	(void) debug;
	uint8_t tx_ir[2] = {USER0, 0};
	_jtag->shiftIR(tx_ir, NULL, IRLENGTH, Jtag::UPDATE_IR);
	_jtag->shiftDR(tx, rx, len, end_state);
}
