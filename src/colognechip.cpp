// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2021 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 * Copyright (C) 2021 Cologne Chip AG <support@colognechip.com>
 */

#include "colognechip.hpp"

#define JTAG_CONFIGURE  0x06
#define JTAG_SPI_BYPASS 0x05
#define SLEEP_US 500

CologneChip::CologneChip(FtdiSpi *spi, const std::string &filename,
	const std::string &file_type, Device::prog_type_t prg_type,
	uint16_t rstn_pin, uint16_t done_pin, uint16_t failn_pin, uint16_t oen_pin,
	bool verify, int8_t verbose) :
	Device(NULL, filename, file_type, verify, verbose), _rstn_pin(rstn_pin),
		_done_pin(done_pin), _failn_pin(failn_pin), _oen_pin(oen_pin)
{
	_spi = spi;
	_spi->gpio_set_input(_done_pin | _failn_pin);
	_spi->gpio_set_output(_rstn_pin | _oen_pin);

	if (prg_type == Device::WR_SRAM) {
		_mode = Device::MEM_MODE;
	} else {
		_mode = Device::FLASH_MODE;
	}
}

CologneChip::CologneChip(Jtag* jtag, const std::string &filename,
	const std::string &file_type, Device::prog_type_t prg_type,
	const std::string &board_name, const std::string &cable_name,
	bool verify, int8_t verbose) :
	Device(jtag, filename, file_type, verify, verbose)
{
	/* check which cable/board we're using in order to select pin definitions */
	std::string spi_board_name;
	if (board_name != "-") {
		spi_board_name = std::regex_replace(board_name, std::regex("jtag"), "spi");
	} else if (cable_name == "gatemate_pgm") {
		spi_board_name = "gatemate_pgm_spi";
	}

	target_board_t *spi_board = &(board_list[spi_board_name]);
	cable_t *spi_cable = &(cable_list[spi_board->cable_name]);

	/* pin configurations valid for both evaluation board and programer */
	_rstn_pin  = spi_board->reset_pin;
	_done_pin  = spi_board->done_pin;
	_failn_pin = DBUS6;
	_oen_pin   = spi_board->oe_pin;

	/* cast _jtag->_jtag from JtagInterface to FtdiJtagMPSSE to access GPIO */
	_ftdi_jtag = reinterpret_cast<FtdiJtagMPSSE *>(_jtag->_jtag);

	_ftdi_jtag->gpio_set_input(_done_pin | _failn_pin);
	_ftdi_jtag->gpio_set_output(_rstn_pin | _oen_pin);

	if (prg_type == Device::WR_SRAM) {
		_mode = Device::MEM_MODE;
	} else {
		_mode = Device::FLASH_MODE;
	}
}

/**
 * Enable outputs and hold FPGA in active hardware reset for SLEEP_US.
 */
void CologneChip::reset()
{
	if (_spi) {
		_spi->gpio_clear(_rstn_pin | _oen_pin);
		usleep(SLEEP_US);
		_spi->gpio_set(_rstn_pin);
	} else if (_ftdi_jtag) {
		_ftdi_jtag->gpio_clear(_rstn_pin | _oen_pin);
		usleep(SLEEP_US);
		_ftdi_jtag->gpio_set(_rstn_pin);
	}
}

/**
 * Obtain CFG_DONE and ~CFG_FAILED signals. Configuration is successfull iff
 * CFG_DONE=true and ~CFG_FAILED=false.
 */
bool CologneChip::cfgDone()
{
	uint16_t status = 0;
	if (_spi) {
		status = _spi->gpio_get(true);
	} else if (_ftdi_jtag) {
		status = _ftdi_jtag->gpio_get(true);
	}
	bool done = (status & _done_pin) > 0;
	bool fail = (status & _failn_pin) == 0;
	return (done && !fail);
}

/**
 * Prints information if configuration was successfull.
 */
void CologneChip::waitCfgDone()
{
	uint32_t timeout = 1000;

	printInfo("Wait for CFG_DONE ", false);
	do {
		timeout--;
		usleep(SLEEP_US);
	} while (!cfgDone() && timeout > 0);
	if (timeout == 0) {
		printError("FAIL");
	} else {
		printSuccess("DONE");
	}
}

/**
 * Dump flash contents to file. Works in both SPI and JTAG-SPI-bypass mode.
 */
bool CologneChip::dumpFlash(const std::string &filename, uint32_t base_addr,
	uint32_t len)
{
	if (_spi) {
		/* enable output and hold reset */
		_spi->gpio_clear(_rstn_pin | _oen_pin);
	} else if (_ftdi_jtag) {
		/* enable output and disable reset */
		_ftdi_jtag->gpio_clear(_oen_pin);
		_ftdi_jtag->gpio_set(_rstn_pin);
	}

	/* prepare SPI access */
	printInfo("Read Flash ", false);
	try {
		SPIFlash *flash;
		if (_spi) {
			flash = new SPIFlash(reinterpret_cast<SPIInterface *>(_spi), false,
					_verbose);
		} else if (_ftdi_jtag) {
			flash = new SPIFlash(this, false, _verbose);
		}
		flash->reset();
		flash->power_up();
		flash->dump(filename, base_addr, len);
	} catch (std::exception &e) {
		printError("Fail");
		printError(std::string(e.what()));
		return false;
	}

	if (_spi) {
		/* disable output and release reset */
		_spi->gpio_set(_rstn_pin | _oen_pin);
	} else if (_ftdi_jtag) {
		/* disable output */
		_ftdi_jtag->gpio_set(_oen_pin);
	}
	usleep(SLEEP_US);

	return true;
}

/**
 * Parse bitstream from *.bit or *.cfg and program FPGA in SPI or JTAG mode
 * or write configuration to external flash via SPI or JTAG-SPI-bypass.
 */
void CologneChip::program(unsigned int offset, bool unprotect_flash)
{
	ConfigBitstreamParser *cfg;
	if (_file_extension == "cfg") {
		cfg = new CologneChipCfgParser(_filename);
	} else if (_file_extension == "bit") {
		cfg = new RawParser(_filename, false);
	} else { /* unknown type: */
		if (_mode == Device::FLASH_MODE) {
			cfg = new RawParser(_filename, false);
		} else {
			throw std::runtime_error("incompatible file format");
		}
	}

	cfg->parse();

	uint8_t *data = cfg->getData();
	int length = cfg->getLength() / 8;

	switch (_mode) {
		case Device::FLASH_MODE:
			if (_jtag != NULL) {
				programJTAG_flash(offset, data, length, unprotect_flash);
			} else if (_jtag == NULL) {
				programSPI_flash(offset, data, length, unprotect_flash);
			}
			break;
		case Device::MEM_MODE:
			if (_jtag != NULL) {
				programJTAG_sram(data, length);
			} else if (_jtag == NULL) {
				programSPI_sram(data, length);
			}
			break;
	}
}

/**
 * Write configuration into FPGA latches via SPI after active reset.
 * CFG_MD[3:0] must be set to 0x40 (SPI passive).
 */
void CologneChip::programSPI_sram(uint8_t *data, int length)
{
	/* hold device in reset for a moment */
	reset();

	uint8_t *recv = new uint8_t[length];
	_spi->gpio_set(_rstn_pin);
	_spi->spi_put(data, recv, length); // TODO _spi->spi_put(data, null, length) does not work?

	waitCfgDone();

	_spi->gpio_set(_oen_pin);
	delete [] recv;
}

/**
 * Write configuration to flash via SPI while FPGA is in active reset. When
 * done, release reset to start FPGA in active SPI mode (load from flash).
 * CFG_MD[3:0] must be set to 0x00 (SPI active).
 */
void CologneChip::programSPI_flash(unsigned int offset, uint8_t *data,
		int length, bool unprotect_flash)
{
	/* hold device in reset during flash write access */
	_spi->gpio_clear(_rstn_pin | _oen_pin);
	usleep(SLEEP_US);

	SPIFlash flash(reinterpret_cast<SPIInterface *>(_spi), unprotect_flash,
			_verbose);
	flash.reset();
	flash.power_up();

	printf("%02x\n", flash.read_status_reg());
	flash.read_id();
	flash.erase_and_prog(offset, data, length);

	/* verify write if required */
	if (_verify)
		flash.verify(offset, data, length);

	_spi->gpio_set(_rstn_pin);
	usleep(SLEEP_US);

	waitCfgDone();

	_spi->gpio_set(_oen_pin);
}

/**
 * Write configuration into FPGA latches via JTAG after active reset.
 * CFG_MD[3:0] must be set to 0xF0 (JTAG).
 */
void CologneChip::programJTAG_sram(uint8_t *data, int length)
{
	/* hold device in reset for a moment */
	reset();

	_jtag->set_state(Jtag::RUN_TEST_IDLE);

	uint8_t tmp[1024];
	int size = 1024;

	_jtag->shiftIR(JTAG_CONFIGURE, 6, Jtag::SELECT_DR_SCAN);

	ProgressBar progress("Load SRAM via JTAG", length, 50, _quiet);

	for (int i = 0; i < length; i += size) {
		if (length < i + size)
			size = length-i;

		for (int ii = 0; ii < size; ii++)
			tmp[ii] = data[i+ii];

		_jtag->shiftDR(tmp, NULL, size*8, Jtag::SHIFT_DR);
		progress.display(i);
	}

	progress.done();
	_jtag->set_state(Jtag::RUN_TEST_IDLE);

	waitCfgDone();

	_ftdi_jtag->gpio_set(_oen_pin);
}

/**
 * Write configuration to flash via JTAG-SPI-bypass. The FPGA will not start
 * as it is in JTAG mode with CFG_MD[3:0] set to 0xF0 (JTAG).
 */
void CologneChip::programJTAG_flash(unsigned int offset, uint8_t *data,
		int length, bool unprotect_flash)
{
	/* hold device in reset for a moment */
	reset();

	SPIFlash flash(this, unprotect_flash, _verbose);
	flash.reset();
	flash.power_up();

	printf("%02x\n", flash.read_status_reg());
	flash.read_id();
	flash.erase_and_prog(offset, data, length);

	/* verify write if required */
	if (_verify)
		flash.verify(offset, data, length);

	_ftdi_jtag->gpio_set(_oen_pin);
}

/**
 * Overrides spi_put() to access SPI components via JTAG-SPI-bypass.
 */
int CologneChip::spi_put(uint8_t cmd, uint8_t *tx, uint8_t *rx, uint32_t len)
{
	int xfer_len = len + 1;
	uint8_t jtx[xfer_len+2];
	uint8_t jrx[xfer_len+2];

	jtx[0] = ConfigBitstreamParser::reverseByte(cmd);

	if (tx != NULL) {
		for (uint32_t i=0; i < len; i++)
			jtx[i+1] = ConfigBitstreamParser::reverseByte(tx[i]);
	}

	_jtag->shiftIR(JTAG_SPI_BYPASS, 6, Jtag::SELECT_DR_SCAN);

	int test = (rx == NULL) ? 8*xfer_len+1 : 8*xfer_len+2;
	_jtag->shiftDR(jtx, (rx == NULL)? NULL: jrx, test, Jtag::SELECT_DR_SCAN);

	if (rx != NULL) {
		for (uint32_t i=0; i < len; i++) {
			uint8_t b0 = ConfigBitstreamParser::reverseByte(jrx[i+1]);
			uint8_t b1 = ConfigBitstreamParser::reverseByte(jrx[i+2]);
			rx[i] = (b0 << 1) | ((b1 >> 7) & 0x01);
		}
	}
	return 0;
}

/**
 * Overrides spi_put() to access SPI components via JTAG-SPI-bypass.
 */
int CologneChip::spi_put(uint8_t *tx, uint8_t *rx, uint32_t len)
{
	int xfer_len = len;
	uint8_t jtx[xfer_len+2];
	uint8_t jrx[xfer_len+2];

	if (tx != NULL) {
		for (uint32_t i=0; i < len; i++)
			jtx[i] = ConfigBitstreamParser::reverseByte(tx[i]);
	}

	_jtag->shiftIR(JTAG_SPI_BYPASS, 6, Jtag::SELECT_DR_SCAN);
	_jtag->shiftDR(jtx, (rx == NULL)? NULL: jrx, 8*xfer_len+1, Jtag::SELECT_DR_SCAN);

	if (rx != NULL) {
		for (uint32_t i=0; i < len; i++) {
			uint8_t b0 = ConfigBitstreamParser::reverseByte(jrx[i]);
			uint8_t b1 = ConfigBitstreamParser::reverseByte(jrx[i+1]);
			rx[i] = (b0 << 1) | ((b1 >> 7) & 0x01);
		}
	}
	return 0;
}

/**
 * Overrides spi_put() to access SPI components via JTAG-SPI-bypass.
 */
int CologneChip::spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
						  uint32_t timeout, bool verbose)
{
	uint8_t rx[2];
	uint8_t dummy[2];
	uint8_t tmp;
	uint8_t tx = ConfigBitstreamParser::reverseByte(cmd);
	uint32_t count = 0;

	_jtag->shiftIR(JTAG_SPI_BYPASS, 6, Jtag::SHIFT_DR);
	_jtag->read_write(&tx, NULL, 8, 0);

	do {
		if (count == 0) {
			_jtag->read_write(dummy, rx, 9, 0);
			uint8_t b0 = ConfigBitstreamParser::reverseByte(rx[0]);
			uint8_t b1 = ConfigBitstreamParser::reverseByte(rx[1]);
			tmp = (b0 << 1) | ((b1 >> 7) & 0x01);
		} else {
			_jtag->read_write(dummy, rx, 8, 0);
			tmp = ConfigBitstreamParser::reverseByte(rx[0]);
		}

		count++;
		if (count == timeout) {
			printf("timeout: %x %u\n", tmp, count);
			break;
		}

		if (verbose) {
			printf("%x %x %x %u\n", tmp, mask, cond, count);
		}
	} while ((tmp & mask) != cond);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	if (count == timeout) {
		printf("%x\n", tmp);
		std::cout << "wait: Error" << std::endl;
		return -ETIME;
	} else {
		return 0;
	}
}
