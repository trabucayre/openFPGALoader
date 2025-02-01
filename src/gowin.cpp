// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <iostream>
#include <stdexcept>

#include "display.hpp"
#include "fsparser.hpp"
#include "gowin.hpp"
#include "jtag.hpp"
#include "progressBar.hpp"
#include "rawParser.hpp"
#include "spiFlash.hpp"

using namespace std;

#ifdef STATUS_TIMEOUT
// defined in the Windows headers included by libftdi.h
#undef STATUS_TIMEOUT
#endif

#define NOOP				0x02
#define ERASE_SRAM			0x05
#define READ_SRAM			0x03
#define XFER_DONE			0x09
#define READ_IDCODE			0x11
#define INIT_ADDR			0x12
#define READ_USERCODE		0x13
#define CONFIG_ENABLE		0x15
#define XFER_WRITE			0x17
#define CONFIG_DISABLE		0x3A
#define RELOAD				0x3C
#define STATUS_REGISTER		0x41
#  define STATUS_CRC_ERROR			(1 << 0)
#  define STATUS_BAD_COMMAND		(1 << 1)
#  define STATUS_ID_VERIFY_FAILED	(1 << 2)
#  define STATUS_TIMEOUT			(1 << 3)
#  define STATUS_MEMORY_ERASE		(1 << 5)
#  define STATUS_PREAMBLE			(1 << 6)
#  define STATUS_SYSTEM_EDIT_MODE	(1 << 7)
#  define STATUS_PRG_SPIFLASH_DIRECT (1 << 8)
#  define STATUS_NON_JTAG_CNF_ACTIVE (1 << 10)
#  define STATUS_BYPASS				(1 << 11)
#  define STATUS_GOWIN_VLD			(1 << 12)
#  define STATUS_DONE_FINAL			(1 << 13)
#  define STATUS_SECURITY_FINAL		(1 << 14)
#  define STATUS_READY				(1 << 15)
#  define STATUS_POR				(1 << 16)
#  define STATUS_FLASH_LOCK			(1 << 17)

#define EF_PROGRAM			0x71
#define EFLASH_ERASE		0x75
#define SWITCH_TO_MCU_JTAG		0x7a

/* BSCAN spi (external flash) (see below for details) */
/* most common pins def */
#define BSCAN_SPI_SCK           (1 << 1)
#define BSCAN_SPI_CS            (1 << 3)
#define BSCAN_SPI_DI            (1 << 5)
#define BSCAN_SPI_DO            (1 << 7)
#define BSCAN_SPI_MSK           (1 << 6)
/* GW1NSR-4C pins def */
#define BSCAN_GW1NSR_4C_SPI_SCK (1 << 7)
#define BSCAN_GW1NSR_4C_SPI_CS  (1 << 5)
#define BSCAN_GW1NSR_4C_SPI_DI  (1 << 3)
#define BSCAN_GW1NSR_4C_SPI_DO  (1 << 1)
#define BSCAN_GW1NSR_4C_SPI_MSK (1 << 0)

Gowin::Gowin(Jtag *jtag, const string filename, const string &file_type, std::string mcufw,
		Device::prog_type_t prg_type, bool external_flash,
		bool verify, int8_t verbose, const std::string& user_flash)
	: Device(jtag, filename, file_type, verify, verbose),
		SPIInterface(filename, verbose, 0, verify, false, false),
		_idcode(0), is_gw1n1(false), is_gw1n4(false), is_gw1n9(false),
		is_gw2a(false), is_gw5a(false),
		_external_flash(external_flash),
		_spi_sck(BSCAN_SPI_SCK), _spi_cs(BSCAN_SPI_CS),
		_spi_di(BSCAN_SPI_DI), _spi_do(BSCAN_SPI_DO),
		_spi_msk(BSCAN_SPI_MSK)
{
	detectFamily();

	_prev_wr_edge = _jtag->getWriteEdge();
	_prev_rd_edge = _jtag->getReadEdge();

	if (prg_type == Device::WR_FLASH)
		_mode = Device::FLASH_MODE;
	else
		_mode = Device::MEM_MODE;

	if (!_file_extension.empty() && prg_type != Device::RD_FLASH) {
		if (_file_extension == "fs") {
			try {
				_fs = std::unique_ptr<ConfigBitstreamParser>(new FsParser(_filename, _mode == Device::MEM_MODE, _verbose));
			} catch (std::exception &e) {
				throw std::runtime_error(e.what());
			}
		} else {
			/* non fs file is only allowed with external flash */
			if (!_external_flash)
				throw std::runtime_error("incompatible file format");
			try {
				_fs = std::unique_ptr<ConfigBitstreamParser>(new RawParser(_filename, false));
			} catch (std::exception &e) {
				throw std::runtime_error(e.what());
			}
		}

		printInfo("Parse file ", false);
		if (_fs->parse() == EXIT_FAILURE) {
			printError("FAIL");
			throw std::runtime_error("can't parse file");
		} else {
			printSuccess("DONE");
		}

		if (_verbose)
			_fs->displayHeader();

		/* for fs file check match with targeted device */
		if (_file_extension == "fs") {
			string idcode_str = _fs->getHeaderVal("idcode");
			uint32_t fs_idcode = std::stoul(idcode_str.c_str(), NULL, 16);
			if ((fs_idcode & 0x0fffffff) != _idcode) {
				char mess[256];
				snprintf(mess, 256, "mismatch between target's idcode and bitstream idcode\n"
					"\tbitstream has 0x%08X hardware requires 0x%08x", fs_idcode, _idcode);
				throw std::runtime_error(mess);
			}
		}
	}

	if (mcufw.size() > 0) {
		if (_idcode != 0x0100981b)
			throw std::runtime_error("Microcontroller firmware flashing only supported on GW1NSR-4C");

		_mcufw = std::unique_ptr<ConfigBitstreamParser>(new RawParser(mcufw, false));

		if (_mcufw->parse() == EXIT_FAILURE) {
			printError("FAIL");
			throw std::runtime_error("can't parse file");
		} else {
			printSuccess("DONE");
		}
	}

	if (user_flash.size() > 0) {
		if (!is_gw1n9)
			throw std::runtime_error("Unsupported FPGA model (only GW1N(R)-9(C) is supported at the moment)");
		if (mcufw.size() > 0)
			throw std::runtime_error("Microcontroller firmware and user flash can't be specified simultaneously");

		_userflash = std::unique_ptr<ConfigBitstreamParser>(new RawParser(user_flash, false));

		if (_userflash->parse() == EXIT_FAILURE) {
			printError("FAIL");
			throw std::runtime_error("can't parse file");
		} else {
			printSuccess("DONE");
		}
	}

	if (is_gw5a && _mode == Device::FLASH_MODE) {
		_jtag->setClkFreq(2500000);
		_jtag->set_state(Jtag::TEST_LOGIC_RESET);
		if (_verbose)
			displayReadReg("Before disable SPI mode", readStatusReg());
		disableCfg();
		send_command(0);  // BYPASS ?
		_jtag->set_state(Jtag::TEST_LOGIC_RESET);
		gw5a_disable_spi();
	}
}

bool Gowin::detectFamily()
{
	_idcode = _jtag->get_target_device_id();

	/* bscan spi external flash differ for GW1NSR-4C */
	if (_idcode == 0x0100981b) {
		_spi_sck = BSCAN_GW1NSR_4C_SPI_SCK;
		_spi_cs  = BSCAN_GW1NSR_4C_SPI_CS;
		_spi_di  = BSCAN_GW1NSR_4C_SPI_DI;
		_spi_do  = BSCAN_GW1NSR_4C_SPI_DO;
		_spi_msk = BSCAN_GW1NSR_4C_SPI_MSK;
	}

	/*
	 * GW2 series has no internal flash and uses new bitstream checksum
	 * algorithm that is not yet supported.
	 */
	switch (_idcode) {
		case 0x0900281B: /* GW1N-1 */
			is_gw1n1 = true;
			break;
		case 0x0100381B: /* GW1N-4B */
		case 0x0100681b: /* GW1NZ-1 */
			is_gw1n4 = true;
			break;
		case 0x0100481B: /* GW1N(R)-9, although documentation says otherwise */
			is_gw1n9 = true;
			break;
		case 0x0000081b: /* GW2A(R)-18(C) */
		case 0x0000281b: /* GW2A(R)-55(C) */
			_external_flash = true;
			/* FIXME: implement GW2 checksum calculation */
			skip_checksum = true;
			is_gw2a = true;
			break;
		case 0x0001081b: /* GW5AST-138 */
		case 0x0001481b: /* GW5AT-60 */
		case 0x0001181b: /* GW5AT-138 */
		case 0x0001281b: /* GW5A-25 */
			_external_flash = true;
			/* FIXME: implement GW5 checksum calculation */
			skip_checksum = true;
			is_gw5a = true;
			break;
	}

	return true;
}

bool Gowin::send_command(uint8_t cmd)
{
	_jtag->shiftIR(&cmd, nullptr, 8);
	_jtag->toggleClk(6);
	return true;
}

#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#define le32toh(x) OSSwapLittleToHostInt32(x)
#define htole32(x) OSSwapHostToLittleInt32(x)
#elif (defined(_WIN16) || defined(_WIN32) || defined(_WIN64)) || defined(__WINDOWS__) || defined(__wasm__)
	#if BYTE_ORDER == LITTLE_ENDIAN
		#if defined(_MSC_VER)
			#include <stdlib.h>
			#define htole32(x) (x)
			#define le32toh(x) (x)
		#elif defined(__GNUC__) || defined(__clang__)
			#define htole32(x) (x)
			#define le32toh(x) (x)
		#endif
	#endif
#endif

uint32_t Gowin::readReg32(uint8_t cmd)
{
	uint32_t reg = 0, tmp = 0xffffffffU;
	send_command(cmd);
	_jtag->shiftDR((uint8_t *)&tmp, (uint8_t *)&reg, 32);
	return le32toh(reg);
}

void Gowin::reset()
{
	send_command(RELOAD);
	send_command(NOOP);
}

void Gowin::programFlash()
{
	const uint8_t *data = NULL;
	int length = 0;
	if (_fs) {
		data = _fs->getData();
		length = _fs->getLength();
	}

	_jtag->setClkFreq(2500000);  // default for GOWIN, should use LoadingRate from file header

	send_command(CONFIG_DISABLE);
	send_command(0);
	_jtag->set_state(Jtag::TEST_LOGIC_RESET);
	uint32_t state = readStatusReg();
	if ((state & (STATUS_GOWIN_VLD | STATUS_POR)) == 0) {
		displayReadReg("Either GOWIN_VLD or POR should be set, aborting", state);
		return;
	}

	if (!eraseSRAM())
		return;

	if (!enableCfg())
		return;
	if (!eraseFLASH())
		return;
	if (!disableCfg())
		return;
	/* test status a faire */
	if (data) {
		if (!writeFLASH(0, data, length))
			return;
	}

	if (_mcufw) {
		const uint8_t *mcu_data = _mcufw->getData();
		int mcu_length = _mcufw->getLength();
		if (!writeFLASH(0x380, mcu_data, mcu_length))
			return;
	}

	if (_userflash) {
		const uint8_t *userflash_data = _userflash->getData();
		int userflash_length = _userflash->getLength();
		if (!writeFLASH(0x6D0, userflash_data, userflash_length, true))
			return;
	}

	if (_verify)
		printWarn("writing verification not supported");

	if (!disableCfg())
		return;
	send_command(RELOAD);
	send_command(NOOP);

	/* wait for reload */
	usleep(4*150*1000); // FIXME: adapt delay for each family/model

	/* check if file checksum == checksum in FPGA */
	/* don't try to read checksum in mcufw mode only */
	if (!skip_checksum && data)
		checkCRC();

	if (_verbose)
		displayReadReg("after program flash", readStatusReg());
}

void Gowin::programExtFlash(unsigned int offset, bool unprotect_flash)
{
	displayReadReg("after program flash", readStatusReg());

	if (!prepare_flash_access()) {
		throw std::runtime_error("Error: fail to prepare flash access");
	}

	SPIFlash spiFlash(this, unprotect_flash,
					  (_verbose ? 1 : (_quiet ? -1 : 0)));
	spiFlash.reset();
	spiFlash.read_id();
	spiFlash.display_status_reg(spiFlash.read_status_reg());
	const uint8_t *data = _fs->getData();
	int length = _fs->getLength();

	char mess[256];
	bool ret = true;

	if (spiFlash.erase_and_prog(offset, data, length / 8) != 0) {
		snprintf(mess, 256, "Error: write to flash failed");
		printError(mess);
		ret = false;
	}
	if (ret && _verify)
		if (!spiFlash.verify(offset, data, length / 8, 256)) {
			snprintf(mess, 256, "Error: flash vefication failed");
			printError(mess);
			ret = false;
		}

	if (!post_flash_access()) {
		snprintf(mess, 256, "Error: fail to disable flash access");
		printError(mess);
		ret = false;
	}

	if (!ret) {
		throw std::runtime_error(mess);
	}
}

void Gowin::programSRAM()
{
	if (_verbose) {
		displayReadReg("before program sram", readStatusReg());
	}
	/* Work around FPGA stuck in Bad Command status */
	if (is_gw5a) {  // 20231112: no more required but left
		reset();
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(1000000);
	}

	if (!eraseSRAM())
		return;

	/* GW5AST-138k WA. Temporary until found correct solution/sequence */
	if (is_gw5a && _idcode == 0x0001081b) {
		printf("double eraseSRAM\n");
		if (!eraseSRAM())
			return;
	}

	/* load bitstream in SRAM */
	if (!writeSRAM(_fs->getData(), _fs->getLength()))
		return;

	/* ocheck if file checksum == checksum in FPGA */
	if (!skip_checksum)
		checkCRC();
	if (_verbose)
		displayReadReg("after program sram", readStatusReg());
}

void Gowin::program(unsigned int offset, bool unprotect_flash)
{
	if (!_fs && !_mcufw)
		return;

	if (_mode == FLASH_MODE) {
		if (_external_flash)
			programExtFlash(offset, unprotect_flash);
		else
			programFlash();
	} else if (_mode == MEM_MODE) {
		programSRAM();
	}

	return;
}

void Gowin::checkCRC()
{
	uint32_t ucode = readUserCode();
	uint16_t checksum = static_cast<FsParser *>(_fs.get())->checksum();
	if (static_cast<uint16_t>(0xffff & ucode) == checksum)
		goto success;
	/* no match:
	 * user code register contains checksum or
	 * user_code when set_option -user_code
	 * is used, try to compare with this value
	 */
	try {
		string hdr = _fs->getHeaderVal("checkSum");
		if (!hdr.empty()) {
			if (ucode == strtol(hdr.c_str(), NULL, 16))
				goto success;
		}
	} catch (std::exception &e) {}
	char mess[256];
	snprintf(mess, 256, "Read: 0x%08x checksum: 0x%04x\n", ucode, checksum);
	printError("CRC check : FAIL");
	printError(mess);
	return;
success:
	printSuccess("CRC check: Success");
	return;
}

bool Gowin::enableCfg()
{
	send_command(CONFIG_ENABLE);
	return pollFlag(STATUS_SYSTEM_EDIT_MODE, STATUS_SYSTEM_EDIT_MODE);
}

bool Gowin::disableCfg()
{
	send_command(CONFIG_DISABLE);
	send_command(NOOP);
	return pollFlag(STATUS_SYSTEM_EDIT_MODE, 0);
}

uint32_t Gowin::idCode()
{
	return readReg32(READ_IDCODE);
}

uint32_t Gowin::readStatusReg()
{
	return readReg32(STATUS_REGISTER);
}

uint32_t Gowin::readUserCode()
{
	return readReg32(READ_USERCODE);
}

void Gowin::displayReadReg(const char *prefix, uint32_t reg)
{
	static const char *desc[19] = {
	    "CRC Error", "Bad Command", "ID Verify Failed", "Timeout",
	    "Reserved4", "Memory Erase", "Preamble", "System Edit Mode",
	    "Program SPI FLASH directly", "Reserved9",
		"Non-JTAG configuration is active", "Bypass",
	    "Gowin VLD", "Done Final", "Security Final", "Ready",
	    "POR", "FLASH lock", "FLASH2 lock",
	};

	static const char *gw5a_desc[32] = {
	    "CRC Error", "Bad Command", "ID Verify Failed", "Timeout",
	    "auto_boot_2nd_fail", "Memory Erase", "Preamble", "System Edit Mode",
	    "Program SPI FLASH directly", "auto_boot_1st_fail",
		"Non-JTAG configuration is active", "Bypass", "i2c_sram_f",
	    "Done Final", "Security Final", "encrypted_format",
	    "key_right", "sspi_mode", "CRC Comparison Done", "CRC Error",
		"ECC Error", "ECC Error Uncorrectable", "CMSER IDLE",
		"CPU Bus Width", "", "Retry time sync pattern detect", "",
		"Decompression Failed", "OTP Reading Done", "Init Done",
		"Wakeup Done", "Auto Erase",
	};
	/* 20-22 differ */
	static const char *gw5ast_desc[3] = {
		"Ser_Ecc_Corr", "Ser_Ecc_Uncorr", "Ser_Ecc_Runing",
	};

	/* Bits 26:25 */
	static const char *gw5a_sync_det_retry[4] = {
		"no retry",
		"retry one time",
		"retry two times",
		"no \"sync pattern\" is found after three times detection",
	};

	/* Bits 24:23 */
	static const char *gw5a_cpu_bus_width[4] = {
		"no BWD pattern is detected",
		"8-bit mode",
		"16-bit mode",
		"32-bit mode",
	};

	printf("%s: displayReadReg %08x\n", prefix, reg);

	if (is_gw5a) {
		uint8_t max_shift = (_idcode == 0x1081b) ? 24 : 32;
		for (unsigned i = 0, bm = 1; i < max_shift; ++i, bm <<= 1) {
			switch (i) {
				case 23:
					printf("\t[%d:%d] %s: %s\n", i + 1, i, gw5a_desc[i],
						gw5a_cpu_bus_width[(reg >> i)&0x3]);
					bm <<= 1;
					i++;
					break;
				case 25:
					printf("\t[%d:%d] %s: %s\n", i + 1, i, gw5a_desc[i],
						gw5a_sync_det_retry[(reg >> i)&0x3]);
					bm <<= 1;
					i++;
					break;
				default:
					if (reg & bm) {
						if (_idcode == 0x1081b && i >= 20)
							printf("\t   [%2d] %s\n", i, gw5ast_desc[i-20]);
						else
							printf("\t   [%2d] %s\n", i, gw5a_desc[i]);
					}
			}
		}
	} else {
		for (unsigned i = 0, bm = 1; i < 19; ++i, bm <<= 1) {
			if (reg & bm)
				printf("\t%s\n", desc[i]);
		}
	}
}

bool Gowin::pollFlag(uint32_t mask, uint32_t value)
{
	uint32_t status;
	int timeout = 0;
	do {
		status = readStatusReg();
		if (_verbose)
			printf("pollFlag: %x (%x)\n", status, status & mask);
		if (timeout == 100000000){
			printError("timeout");
			return false;
		}
		timeout++;
	} while ((status & mask) != value);

	return true;
}

inline uint32_t bswap_32(uint32_t x)
{
	return  ((x << 24) & 0xff000000) |
		((x <<  8) & 0x00ff0000) |
		((x >>  8) & 0x0000ff00) |
		((x >> 24) & 0x000000ff);
}

/* TN653 p. 17-21 */
bool Gowin::writeFLASH(uint32_t page, const uint8_t *data, int length, bool invert_bits)
{

#if 1
 uint8_t tx[4] = {0x4E, 0x31, 0x57, 0x47};
    uint8_t tmp[4];
    uint32_t addr;
    int nb_iter;
    int byte_length = length / 8;
    int buffer_length;
    uint8_t *buffer;
    int nb_xpage;
    uint8_t tt[39];
    memset(tt, 0, 39);

    _jtag->go_test_logic_reset();

    if (page == 0) {
        /* we have to send
         * bootcode a X=0, Y=0 (4Bytes)
         * 5 x 32 dummy bits
         * full bitstream
         */
        buffer_length = byte_length+(6*4);
        unsigned char bufvalues[]={
                                        0x47, 0x57, 0x31, 0x4E,
                                        0xff, 0xff , 0xff, 0xff,
                                        0xff, 0xff , 0xff, 0xff,
                                        0xff, 0xff , 0xff, 0xff,
                                        0xff, 0xff , 0xff, 0xff,
                                        0xff, 0xff , 0xff, 0xff};
        nb_xpage = buffer_length/256;
        if (nb_xpage * 256 != buffer_length) {
            nb_xpage++;
            buffer_length = nb_xpage * 256;
        }

        buffer = new uint8_t[buffer_length];
        /* fill theorical size with 0xff */
        memset(buffer, 0xff, buffer_length);
        /* fill first page with code */
        memcpy(buffer, bufvalues, 6*4);
        /* bitstream just after opcode */
        memcpy(buffer+6*4, data, byte_length);
    } else {
        buffer_length = byte_length;
        nb_xpage = buffer_length/256;
        if (nb_xpage * 256 != buffer_length) {
            nb_xpage++;
            buffer_length = nb_xpage * 256;
        }
        buffer = new uint8_t[buffer_length];
        memset(buffer, 0xff, buffer_length);
        memcpy(buffer, data, byte_length);
    }

 ProgressBar progress("write Flash", buffer_length, 50, _quiet);

    for (int i=0, xpage = 0; xpage < nb_xpage; i += (nb_iter * 4), xpage++) {
        send_command(CONFIG_ENABLE);
        send_command(EF_PROGRAM);
        if ((page + xpage) != 0)
            _jtag->toggleClk(312);
        addr = (page + xpage) << 6;
        tmp[3] = 0xff&(addr >> 24);
        tmp[2] = 0xff&(addr >> 16);
        tmp[1] = 0xff&(addr >> 8);
        tmp[0] = addr&0xff;
        _jtag->shiftDR(tmp, NULL, 32);
        _jtag->toggleClk(312);

        int xoffset = xpage * 256;  // each page containt 256Bytes
        if (xoffset + 256 > buffer_length)
            nb_iter = (buffer_length-xoffset) / 4;
        else
            nb_iter = 64;

        for (int ypage = 0; ypage < nb_iter; ypage++) {
            unsigned char *t = buffer+xoffset + 4*ypage;
            for (int x=0; x < 4; x++) {
                if (page == 0)
                    tx[3-x] = t[x];
                else
                    tx[x] = t[x];
            }

            if (invert_bits) {
            	for (int x = 0; x < 4; x++) {
            		tx[x] ^= 0xFF;
            	}
            }

            _jtag->shiftDR(tx, NULL, 32);

            if (!is_gw1n1)
                _jtag->toggleClk(40);
        }
        if (is_gw1n1) {
            //usleep(10*2400*2);
            uint8_t tt2[6008/8];
            memset(tt2, 0, 6008/8);
            _jtag->toggleClk(6008);
        }
        progress.display(i);
    }
    /* 2.2.6.6 */
    _jtag->set_state(Jtag::RUN_TEST_IDLE);

    progress.done();
    delete[] buffer;
    return true;

#else
	printInfo("Write FLASH ", false);
	if (_verbose)
		displayReadReg("before write flash", readStatusReg());

	_jtag->go_test_logic_reset();

	uint8_t xpage[256];
	length /= 8;
	ProgressBar progress("Writing to FLASH", length, 50, _quiet);
	for (int off = 0; off < length; off += 256) {
		int l = 256;
		if (length - off < l) {
			memset(xpage, 0xff, sizeof(xpage));
			l = length - off;
		}
		memcpy(xpage, &data[off], l);
		unsigned addr = off / 4 + page;
		if (addr) {
			sendClkUs(16);
		} else {
			// autoboot pattern
			static const uint8_t pat[4] = {'G', 'W', '1', 'N'};
			memcpy(xpage, pat, 4);
		}

		send_command(CONFIG_ENABLE);
		send_command(NOOP);
		send_command(EF_PROGRAM);

		unsigned w = htole32(addr);
		_jtag->shiftDR((uint8_t *)&w, nullptr, 32);
		sendClkUs(16);
		for (int y = 0; y < 64; ++y) {
			memcpy(&w, &xpage[y * 4], 4);
			w = bswap_32(w);
			_jtag->shiftDR((uint8_t *)&w, nullptr, 32);
			sendClkUs((is_gw1n1) ? 32 : 16);
		}
		sendClkUs((is_gw1n1) ? 2400 : 6);
		progress.display(off);
	}

	/*if (!disableCfg()) {
		printError("FAIL");
		return false;
	}
	if (_verbose)
		displayReadReg("after write flash #1", readStatusReg());
	send_command(RELOAD);
	send_command(NOOP);
	if (_verbose)
		displayReadReg("after write flash #2", readStatusReg());
	_jtag->flush();
	usleep(500*1000);
*/
	progress.done();
	/*if (_verbose)
		displayReadReg("after write flash", readStatusReg());
	if (readStatusReg() & STATUS_DONE_FINAL) {
		printSuccess("DONE");
		return true;
	} else {
		printSuccess("FAIL");
		return false;
	}*/
	return true;
#endif
}

bool Gowin::connectJtagToMCU()
{
	send_command(SWITCH_TO_MCU_JTAG);
	return true;
}

/* TN653 p. 9 */
bool Gowin::writeSRAM(const uint8_t *data, int length)
{
	printInfo("Load SRAM ", false);
	if (_verbose)
		displayReadReg("before write sram", readStatusReg());
	ProgressBar progress("Load SRAM", length, 50, _quiet);
	send_command(CONFIG_ENABLE); // config enable

	/* UG704 3.4.3 */
	send_command(INIT_ADDR); // address initialize

	/* 2.2.6.4 */
	send_command(XFER_WRITE); // transfer configuration data
	int remains = length;
	const uint8_t *ptr = data;
	static const unsigned pstep = 524288; // 0x80000, about 0.2 sec of bitstream at 2.5MHz
	while (remains) {
		int chunk = pstep;
		/* 2.2.6.5 */
		Jtag::tapState_t next = Jtag::SHIFT_DR;
		if (remains < chunk) {
			chunk = remains;
			/* 2.2.6.6 */
			next = Jtag::RUN_TEST_IDLE;
		}
		_jtag->shiftDR(ptr, NULL, chunk, next);
		ptr += chunk >> 3; // in bytes
		remains -= chunk;
		progress.display(length - remains);
	}
	progress.done();
	send_command(0x0a);
	uint32_t checksum = static_cast<FsParser *>(_fs.get())->checksum();
	checksum = htole32(checksum);
	_jtag->shiftDR((uint8_t *)&checksum, NULL, 32);
	send_command(0x08);

	send_command(CONFIG_DISABLE); // config disable
	send_command(NOOP); // noop

	uint32_t status_reg = readStatusReg();
	if (_verbose)
		displayReadReg("after write sram", status_reg);
	if (status_reg & STATUS_DONE_FINAL) {
		printSuccess("DONE");
		return true;
	} else {
		printSuccess("FAIL");
		return false;
	}
}

/* Erase SRAM:
 * TN653 p.14-17
 * UG290-2.7.1E p.53
 */
bool Gowin::eraseFLASH()
{
	printInfo("Erase FLASH ", false);
	ProgressBar progress("Erasing FLASH", 100, 50, _quiet);

	for (unsigned i = 0; i < 100; ++i) { // 100 attempts?
		if (_verbose)
			displayReadReg("before erase flash", readStatusReg());
		if (!enableCfg()) {
			printError("FAIL");
			progress.fail();
			return false;
		}
		send_command(EFLASH_ERASE);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);

		/* GW1N1 need 65 x 32bits
		 * others 1 x 32bits
		 */
		int nb_iter = (is_gw1n1)?65:1;
		uint8_t dummy[4] = {0, 0, 0, 0};
		for (int i = 0; i < nb_iter; ++i) {
			// keep following sequence as-is. it is _not_ _jtag->shiftDR().
			_jtag->shiftDR(dummy, NULL, 32);
		}

		/* TN653 specifies to wait for 160ms with
		 * there are no bit in status register to specify
		 * when this operation is done so we need to wait
		 */
		sendClkUs(150 * 1000);
		if (!disableCfg()) {
			printError("FAIL");
			progress.fail();
			return false;
		}
		_jtag->flush();
		usleep(500 * 1000);
		uint32_t state = readStatusReg();
		if (_verbose)
			displayReadReg("after erase flash", state);
		progress.display(i);
		if (!(state & STATUS_DONE_FINAL)) {
			break;
		}
	}
	if (readStatusReg() & STATUS_DONE_FINAL) {
		printError("FAIL");
		progress.fail();
		return false;
	} else {
		printSuccess("DONE");
		progress.done();
		return true;
	}
}

void Gowin::sendClkUs(unsigned us)
{
	uint64_t clocks = _jtag->getClkFreq();
	clocks *= us;
	clocks /= 1000000;
	_jtag->toggleClk(clocks);
}

/* Erase SRAM:
 * TN653 p.9-10, 14 and 31
 */
bool Gowin::eraseSRAM()
{
	printInfo("Erase SRAM ", false);
	uint32_t status = readStatusReg();
	if (_verbose)
		displayReadReg("before erase sram", status);

	// If flash is invalid, send extra cmd 0x3F before SRAM erase
	// This is required on GW5A-25
	bool auto_boot_2nd_fail = (status & 0x8) >> 3;
	if ((_idcode == 0x0001281B) && auto_boot_2nd_fail)
	{
		disableCfg();
		send_command(0x3F);
		send_command(NOOP);
	}

	if (!enableCfg()) {
		printError("FAIL");
		return false;
	}
	send_command(ERASE_SRAM);
	send_command(NOOP);

	/* TN653 specifies to wait for 4ms with
	 * clock generated but
	 * status register bit MEMORY_ERASE goes low when ERASE_SRAM
	 * is send and goes high after erase
	 * this check seems enough
	 */
	if (_idcode == 0x0001081b) // seems required for GW5AST...
		sendClkUs(10000);
	if (pollFlag(STATUS_MEMORY_ERASE, STATUS_MEMORY_ERASE)) {
		if (_verbose)
			displayReadReg("after erase sram", readStatusReg());
	} else {
		printError("FAIL");
		return false;
	}

	send_command(XFER_DONE);
	send_command(NOOP);
	if (!disableCfg()) {
		printError("FAIL");
		return false;
	}

	if (_mode == Device::FLASH_MODE) {
		uint32_t status_reg = readStatusReg();
		if (_verbose)
			displayReadReg("after erase sram", status_reg);
		if (status_reg & STATUS_DONE_FINAL) {
			printError("FAIL");
			return false;
		} else {
			printSuccess("DONE");
		}
	}
	return true;
}

inline void Gowin::spi_gowin_write(const uint8_t *wr, uint8_t *rd, unsigned len) {
	_jtag->shiftDR(wr, rd, len);
	_jtag->toggleClk(6);
}

/* SPI wrapper
 * extflash access may be done using specific mode or
 * boundary scan. But former is only available with mode=[11]
 * so use Bscan
 *
 * it's a bitbanging mode with:
 * Pins Name of SPI Flash | SCLK | CS  | DI  | DO  |
 * Bscan Chain[7:0]       | 7  6 | 5 4 | 3 2 | 1 0 |
 * (ctrl & data)          | 0    | 0   | 0   | 1   |
 * ctrl 0 -> out, 1 -> in
 * data 1 -> high, 0 -> low
 * but all byte must be bit reversal...
 */

int Gowin::spi_put(uint8_t cmd, const uint8_t *tx, uint8_t *rx, uint32_t len)
{
	if (is_gw5a)
		return spi_put_gw5a(cmd, tx, rx, len);

	uint8_t jrx[len+1], jtx[len+1];
	jtx[0] = cmd;
	if (tx)
		memcpy(jtx+1, tx, len);
	else
		memset(jtx+1, 0, len);
	int ret = spi_put(jtx, (rx)? jrx : NULL, len+1);
	if (rx)
		memcpy(rx, jrx + 1, len);
	return ret;
}

int Gowin::spi_put(const uint8_t *tx, uint8_t *rx, uint32_t len)
{
	if (is_gw5a) {
		uint8_t jrx[len];
		int ret = spi_put_gw5a(tx[0], (len > 1) ? &tx[1] : NULL,
				(rx) ? jrx : NULL, len - 1);
		// FIXME: first byte is never read (but in most call it's not an issue
		if (rx) {
			rx[0] = 0;
			memcpy(&rx[1], jrx, len - 1);
		}
		return ret;
	}

	if (is_gw2a) {
		uint8_t jtx[len];
		uint8_t jrx[len];
		if (rx)
			len++;
		if (tx != NULL) {
			for (uint32_t i = 0; i < len; i++)
				jtx[i] = FsParser::reverseByte(tx[i]);
		}
		bool ret = send_command(0x16);
		if (!ret)
			return -1;
		_jtag->set_state(Jtag::EXIT2_DR);
		_jtag->shiftDR(jtx, (rx)? jrx:NULL, 8*len);
		if (rx) {
			for (uint32_t i=0; i < len; i++) {
				rx[i] = FsParser::reverseByte(jrx[i]>>1) |
					(jrx[i+1]&0x01);
			}
		}
	} else {
		/* set CS/SCK/DI low */
		uint8_t t = _spi_msk | _spi_do;
		t &= ~_spi_cs;
		spi_gowin_write(&t, NULL, 8);
		_jtag->flush();

		/* send bit/bit full tx content (or set di to 0 when NULL) */
		for (unsigned l = 0; l < len; ++l) {
			if (rx)
				rx[l] = 0;
			for (uint8_t b = 0, bm = 0x80; b < 8; ++b, bm >>= 1) {
				uint8_t r;
				t = _spi_msk | _spi_do;
				if (tx != NULL && tx[l] & bm)
					t |= _spi_di;
				spi_gowin_write(&t, NULL, 8);
				t |= _spi_sck;
				spi_gowin_write(&t, (rx) ? &r : NULL, 8);
				_jtag->flush();
				/* if read reconstruct bytes */
				if (rx && (r & _spi_do))
					rx[l] |= bm;
			}
		}
		/* set CS and unset SCK (next xfer) */
		t &= ~_spi_sck;
		t |= _spi_cs;
		spi_gowin_write(&t, NULL, 8);
		_jtag->flush();
	}
	return 0;
}

int Gowin::spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
		uint32_t timeout, bool verbose)
{
	if (is_gw5a)
		return spi_wait_gw5a(cmd, mask, cond, timeout, verbose);

	uint8_t tmp;
	uint32_t count = 0;

	if (is_gw2a) {
		uint8_t rx[3];
		uint8_t tx[3];
		tx[0] = FsParser::reverseByte(cmd);

		do {
			bool ret = send_command(0x16);
			if (!ret)
				return -1;
			_jtag->set_state(Jtag::EXIT2_DR);
			_jtag->shiftDR(tx, rx, 8 * 3);

			tmp = (FsParser::reverseByte(rx[1]>>1)) | (0x01 & rx[2]);
			count++;
			if (count == timeout) {
				printf("timeout: %x %x %x\n", tmp, rx[0], rx[1]);
				break;
			}
			if (verbose) {
				printf("%x %x %x %u\n", tmp, mask, cond, count);
			}
		} while ((tmp & mask) != cond);
	} else {
		uint8_t t;

		/* set CS/SCK/DI low */
		t = _spi_msk | _spi_do;
		spi_gowin_write(&t, NULL, 8);

		/* send command bit/bit */
		for (uint8_t i = 0, bm = 0x80; i < 8; ++i, bm >>= 1) {
			t = _spi_msk | _spi_do;
			if ((cmd & bm) != 0)
				t |= _spi_di;
			spi_gowin_write(&t, NULL, 8);
			t |= _spi_sck;
			spi_gowin_write(&t, NULL, 8);
			_jtag->flush();
		}

		t = _spi_msk | _spi_do;
		do {
			tmp = 0;
			/* read status register bit/bit with di == 0 */
			for (uint8_t i = 0, bm = 0x80; i < 8; ++i, bm >>= 1) {
				uint8_t r;
				t &= ~_spi_sck;
				spi_gowin_write(&t, NULL, 8);
				t |= _spi_sck;
				spi_gowin_write(&t, &r, 8);
				_jtag->flush();
				if ((r & _spi_do) != 0)
					tmp |= bm;
			}

			count++;
			if (count == timeout) {
				printf("timeout: %x\n", tmp);
				break;
			}
			if (verbose)
				printf("%x %x %x %u\n", tmp, mask, cond, count);
		} while ((tmp & mask) != cond);

		/* set CS & unset SCK (next xfer) */
		t &= ~_spi_sck;
		t |= _spi_cs;
		spi_gowin_write(&t, NULL, 8);
		_jtag->flush();
	}

	if (count == timeout) {
		printf("%02x\n", tmp);
		std::cout << "wait: Error" << std::endl;
		return -ETIME;
	}

	return 0;
}

bool Gowin::dumpFlash(uint32_t base_addr, uint32_t len)
{
	bool ret = true;
	/* enable SPI flash access */
	if (!prepare_flash_access())
		return false;

	try {
		SPIFlash flash(this, false, _verbose);
		ret = flash.dump(_filename, base_addr, len, 256);
	} catch (std::exception &e) {
		printError(e.what());
		ret = false;
	}

	/* reload bitstream */
	return post_flash_access() && ret;
}

bool Gowin::prepare_flash_access()
{
	if (!eraseSRAM()) {
		printError("Error: fail to erase SRAM");
		return false;
	}

	if (is_gw5a) {
		if (!eraseSRAM()) {
			printError("Error: fail to erase SRAM");
			return false;
		}
		usleep(100000);
		if (!gw5a_enable_spi()) {
			printError("Error: fail to switch GW5A to SPI mode");
			return false;
		}
		usleep(100000);
	} else if (!is_gw2a) {
		if (!enableCfg()) {
			return false;
		}
		send_command(0x3D);
	}

	_jtag->setClkFreq(10000000);

	return true;
}

bool Gowin::post_flash_access()
{
	bool ret = true;

	if (is_gw5a) {
		if (!gw5a_disable_spi()) {
			printError("Error: fail to disable GW5A SPI mode");
			ret = false;
		}
	} else if (!is_gw2a) {
		if (!disableCfg()) {
			printError("Error: fail to disable configuration");
			ret = false;
		}
	}

	reset();

	return ret;
}

/*
 * Specific implementation for Arora V GW5A FPGAs
 */

/* interface mode is already configured to mimic SPI (mode 0).
 * JTAG is LSB, SPI is MSB -> all byte must be reversed.
 */
int Gowin::spi_put_gw5a(const uint8_t cmd, const uint8_t *tx, uint8_t *rx,
		uint32_t len)
{
		uint32_t kLen = len + (rx ? 1 : 0);  // cppcheck/lint happy
		uint32_t bit_len = len * 8 + (rx ? 3 : 0);  // 3bits delay when read
		uint8_t jtx[kLen], jrx[kLen];
		uint8_t _cmd = FsParser::reverseByte(cmd);  // reverse cmd.
		uint8_t curr_tdi = cmd & 0x01;

		if (tx != NULL) {  // SPI: MSB, JTAG: LSB -> reverse Bytes
			for (uint32_t i = 0; i < len; i++)
				jtx[i] = FsParser::reverseByte(tx[i]);
			curr_tdi = tx[len-1] & 0x01;
		} else {
			memset(jtx, curr_tdi, kLen);
		}

		// set TMS/CS low by moving to a state where TMS == 0,
		// first cmd bit is also sent here (ie before next TCK rise).
		_jtag->set_state(Jtag::RUN_TEST_IDLE, _cmd & 0x01);
		_cmd >>= 1;

		// complete with 7 remaining cmd bits
		if (0 != _jtag->read_write(&_cmd, NULL, 7, 0))
			return -1;

		// write/read the sequence. Force set to 0 to manage state here
		// (with jtag last bit is sent with tms rise)
		if (0 != _jtag->read_write(jtx, (rx) ? jrx : NULL, bit_len, 0))
			return -1;
		// set TMS/CS high by moving to a state where TMS == 1
		_jtag->set_state(Jtag::TEST_LOGIC_RESET, curr_tdi);
		_jtag->toggleClk(5);  // Required ?
		_jtag->flushTMS(true);
		if (rx) {  // Reconstruct read sequence and drop first 3bits.
			for (uint32_t i = 0; i < len; i++)
				rx[i] = FsParser::reverseByte((jrx[i] >> 3) |
					(((jrx[i+1]) & 0x07) << 5));
		}

		return 0;
}

int Gowin::spi_wait_gw5a(uint8_t cmd, uint8_t mask, uint8_t cond,
		uint32_t timeout, bool verbose)
{
	uint8_t tmp;
	uint32_t count = 0;

	do {
		// sent command and read flash answer
		if (0 != spi_put_gw5a(cmd, NULL, &tmp, 1)) {
			printError("Error: cant write/read status");
			return -1;
		}

		count++;
		if (count == timeout) {
			printf("timeout: %x\n", tmp);
			break;
		}
		if (verbose) {
			printf("%x %x %x %u\n", tmp, mask, cond, count);
		}
	} while ((tmp & mask) != cond);

	return (count == timeout) ? -1 : 0;
}

bool Gowin::gw5a_enable_spi()
{
	enableCfg();
	send_command(0x3F);
	disableCfg();
	if (_verbose)
		displayReadReg("toto", readStatusReg());

	/* UG704 3.4.3 'ExtFlash Programming -> Program External Flash via JTAG-SPI' */
	send_command(NOOP);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(126*8);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	send_command(0x16);
	send_command(0x00);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(625*8);
	_jtag->set_state(Jtag::TEST_LOGIC_RESET);
	/* save current read/write edge cfg before switching to SPI mode0
	 * (rising edge: read / falling edge: write)
	 */
	_prev_wr_edge = _jtag->getWriteEdge();
	_prev_rd_edge = _jtag->getReadEdge();
	_jtag->setWriteEdge(JtagInterface::FALLING_EDGE);
	_jtag->setReadEdge(JtagInterface::RISING_EDGE);

	return true;
}

bool Gowin::gw5a_disable_spi()
{
	/* reconfigure WR/RD edge and sent sequence to
	 * disable SPI mode
	 */
	_jtag->setWriteEdge(_prev_wr_edge);
	_jtag->setReadEdge(_prev_rd_edge);
	_jtag->flushTMS(true);
	_jtag->flush();
	// 1. sent 15 TMS pulse
	// TEST_LOGIC_RESET to SELECT_DR_SCAN: 01
	_jtag->set_state(Jtag::SELECT_DR_SCAN);
	// SELECT_DR_SCAN to CAPTURE_DR: 0
	_jtag->set_state(Jtag::CAPTURE_DR);
	// CAPTURE_DR to EXIT1_DR: 1
	_jtag->set_state(Jtag::EXIT1_DR);
	// EXIT1_DR to EXIT2_DR: 01
	_jtag->set_state(Jtag::EXIT2_DR);
	// Now we have 3 pulses
	for (int i = 0; i < 6; i++) {  // 2 each loop: 12 pulses + 3 before
		_jtag->set_state(Jtag::PAUSE_DR);  // 010
		_jtag->set_state(Jtag::EXIT2_DR);  // 1
	}
	_jtag->set_state(Jtag::EXIT1_DR);  // 01 : 16
	_jtag->set_state(Jtag::PAUSE_DR);  // 0

	_jtag->flushTMS(true);
	_jtag->flush();
	// 2. 8 TCK clock cycle with TMS=1
	_jtag->set_state(Jtag::TEST_LOGIC_RESET);  // 5 cycles
	_jtag->toggleClk(5);
	return true;
}
