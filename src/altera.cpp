// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include "altera.hpp"

#include <string.h>

#include <map>
#include <string>

#include "common.hpp"
#include "device.hpp"
#include "epcq.hpp"
#include "jtag.hpp"
#include "progressBar.hpp"
#include "rawParser.hpp"
#if defined (_WIN64) || defined (_WIN32)
#include "pathHelper.hpp"
#endif
#include "pofParser.hpp"

#define IDCODE 6
#define USER0  0x0C
#define USER1  0x0E
#define BYPASS 0x3FF
#define IRLENGTH 10

Altera::Altera(Jtag *jtag, const std::string &filename,
	const std::string &file_type, Device::prog_type_t prg_type,
	const std::string &device_package,
	const std::string &spiOverJtagPath, bool verify, int8_t verbose,
	const std::string &flash_sectors,
	bool skip_load_bridge, bool skip_reset):
	Device(jtag, filename, file_type, verify, verbose),
	SPIInterface(filename, verbose, 256, verify, skip_load_bridge,
				 skip_reset),
	_device_package(device_package), _spiOverJtagPath(spiOverJtagPath),
	_vir_addr(0x1000), _vir_length(14), _clk_period(1),
	_flash_sectors(flash_sectors)
{
	/* check device family */
	_idcode = _jtag->get_target_device_id();
	string family = fpga_list[_idcode].family;
	if (family == "MAX 10") {
		_fpga_family = MAX10_FAMILY;
	} else {
		_fpga_family = CYCLONE_MISC;  // FIXME
	}

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
			} else if (_file_extension == "pof") {  // MAX10
				_mode = Device::FLASH_MODE;
			} else if (_fpga_family == MAX10_FAMILY) {
				_mode = Device::FLASH_MODE;
			} else {  // unknown type -> sanity check
				if (prg_type == Device::WR_SRAM) {
					printError("file has an unknown type:");
					printError("\tplease use rbf or svf file");
					printError("\tor use --write-flash with: ", false);
					printError("-b board_name or --fpga_part xxxx");
					throw std::runtime_error("Error: wrong file");
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
	const uint8_t *data = _bit.getData();

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
	_jtag->toggleClk(1000000/_clk_period);
	/* write */
	ProgressBar progress("Load SRAM", byte_length, 50, _quiet);

	int xfer_len = 512;
	int tx_len;
	Jtag::tapState_t tx_end;

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
	_jtag->toggleClk(5000/_clk_period);
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
	_jtag->toggleClk(4099645/_clk_period);
	/* RUNTEST 512 TCK; */
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(512);
	 /* SIR 10 TDI (3FF); */
	*reinterpret_cast<uint16_t *>(cmd) = BYPASS;
	_jtag->shiftIR(cmd, NULL, IRLENGTH, Jtag::PAUSE_IR);

	/* RUNTEST 12000 TCK; */
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000000/_clk_period);
	/* -> idle */
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
}

bool Altera::post_flash_access()
{
	if (_skip_reset)
		printInfo("Skip resetting device");
	else
		reset();
	return true;
}

bool Altera::prepare_flash_access()
{
	if (_skip_load_bridge) {
		printInfo("Skip loading bridge for spiOverjtag");
		return true;
	}
	return load_bridge();
}

bool Altera::load_bridge()
{
	std::string bitname;
	if (!_spiOverJtagPath.empty()) {
		bitname = _spiOverJtagPath;
	} else {
		if (_device_package.empty()) {
			printError("Can't program SPI flash: missing device-package information");
			return false;
		}

		bitname = get_shell_env_var("OPENFPGALOADER_SOJ_DIR", DATA_DIR "/openFPGALoader");
#ifdef HAS_ZLIB
		bitname += "/spiOverJtag_" + _device_package + ".rbf.gz";
#else
		bitname += "/spiOverJtag_" + _device_package + ".rbf";
#endif
	}

#if defined (_WIN64) || defined (_WIN32)
	/* Convert relative path embedded at compile time to an absolute path */
	bitname = PathHelper::absolutePath(bitname);
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

	/* Access clk frequency to store clk_period required
	 * for all operations. Can't be done at CTOR because
	 * frequency be changed between these 2 methods.
	 *
	 */
	_clk_period = 1e9/static_cast<float>(_jtag->getClkFreq());

	/* Specific case for MAX10 */
	if (_fpga_family == MAX10_FAMILY) {
		max10_program(offset);
		return;
	}

	/* in all case we consider svf is mandatory
	 * MEM_MODE : svf file provided for constructor
	 *            is the bitstream to use
	 * SPI_MODE : svf file provided is bridge to have
	 *            access to the SPI flash
	 */
	/* mem mode -> svf */
	if (_mode == Device::MEM_MODE) {
		RawParser _bit(_filename, false);
		_bit.parse();
		programMem(_bit);
	} else if (_mode == Device::SPI_MODE) {
		// reverse only bitstream raw binaries data no
		bool reverseOrder = false;
		if (_file_extension == "rbf" || _file_extension == "rpd")
			reverseOrder = true;

		/* prepare data to write */
		const uint8_t *data = NULL;
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

uint32_t Altera::idCode()
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

/* MAX 10 specifics methods */
/* ------------------------ */
#define MAX10_ISC_ADDRESS_SHIFT {0x03, 0x02}
#define MAX10_ISC_READ          {0x05, 0x02}
#define MAX10_ISC_ENABLE        {0xcc, 0x02}
#define MAX10_ISC_DISABLE       {0x01, 0x02}
#define MAX10_ISC_ADDRESS_SHIFT {0x03, 0x02}
#define MAX10_ISC_ERASE         {0xf2, 0x02}
#define MAX10_ISC_PROGRAM       {0xf4, 0x02}
#define MAX10_DSM_ICB_PROGRAM   {0xF4, 0x03}
#define MAX10_DSM_VERIFY        {0x07, 0x03}
#define MAX10_DSM_CLEAR         {0xf2, 0x03}
#define MAX10_BYPASS            {0xFF, 0x03}

struct Altera::max10_mem_t {
	uint32_t check_addr0;  // something to check before sequence
	uint32_t dsm_addr;
	uint32_t dsm_len;  // 32bits
	uint32_t ufm_addr;  // UFM1 addr
	uint32_t ufm_len[2]; // 32bits
	uint32_t cfm_addr;  // CFM2 addr
	uint32_t cfm_len[3]; // 32bits
	uint32_t sectors_erase_addr[5]; // UFM1, UFM0, CFM2, CFM1, CFM0
	uint32_t done_bit_addr;
	uint32_t pgm_success_addr;
};

const std::map<uint32_t, Altera::max10_mem_t> Altera::max10_memory_map = {
	{0x031820dd, { // 10M08SAU
		.check_addr0 = 0x80005,  // check_addr0
		.dsm_addr = 0x0000, 512,  // DSM
		.ufm_addr = 0x0200, .ufm_len = {4096, 4096},  // UFM
		.cfm_addr = 0x2200, .cfm_len = {35840, 14848, 20992},  // CFM
		.sectors_erase_addr = {0x17ffff, 0x27ffff, 0x37ffff, 0x47ffff, 0x57ffff}, // sectors erase address
		.done_bit_addr = 0x0009,  // done bit
		.pgm_success_addr = 0x000b}  // program success addr
	},
	{0x031830dd, { // 10M16SA
		.check_addr0 = 0x80009,  // check_addr0
		.dsm_addr = 0x0000, 1024,  // DSM
		.ufm_addr = 0x0400, .ufm_len = {4096, 4096},  // UFM
		.cfm_addr = 0x2400, .cfm_len = {67584, 28672, 38912},  // CFM
		.sectors_erase_addr = {0x17ffff, 0x27ffff, 0x37ffff, 0x47ffff, 0x57ffff}, // sectors erase address
		.done_bit_addr = 0x0011,  // done bit
		.pgm_success_addr = 0x0015}  // program success addr
	},
};

/* Write an arbitrary file in UFM1, UFM0 by default and also CFM2 and CFM1 if
 * requested.
 */
bool Altera::max10_program_ufm(const Altera::max10_mem_t *mem, uint32_t offset,
		uint8_t update_sectors)
{
	uint32_t start_addr = 0; // 32bit align
	uint32_t end_addr = 0; // 32bit align
	uint8_t erase_sectors_mask;

	/* check CFM0 is not mentionned */
	if (update_sectors & (1 << 4))
		std::runtime_error("Error: CFM0 cant't be used to store User Binary");

	/* First task: search for the first and the last sector to use */
	sectors_mask_start_end_addr(mem, update_sectors,
		&start_addr, &end_addr, &erase_sectors_mask);

	RawParser _bit(_filename, true);
	_bit.parse();
	if (_verbose)
		_bit.displayHeader();

	const uint8_t *data = _bit.getData();
	const uint32_t length = _bit.getLength() / 8;
	const uint32_t base_addr = start_addr + offset / 4; // 32bit align
	const uint32_t flash_len = (end_addr - base_addr) * 4; // Byte align

	/* check */
	if (base_addr > end_addr) { // wrong offset
		printError("Error: start offset is out of xFM region");
		return false;
	}
	if (flash_len < length) { // too big file
		printError("Error: no enough space to write\n");
		return false;
	}
	if (base_addr + (length / 4) > end_addr) {
		printError("Error: end address is out of xFM region");
		return false;
	}

	uint8_t *buff = (uint8_t *)malloc(length);
	if (!buff) {
		printError("max10_program_ufm: Failed to allocate buffer");
		return false;
	}

	/* data needs to be re-ordered */
	for (uint32_t i = 0; i < length; i+=4) {
		for (int b = 0; b < 4; b++) {
			buff[i + b] = data[i + (3-b)];
		}
	}

	printf("%x %x %x %x\n", update_sectors, erase_sectors_mask,
		base_addr, end_addr);

	// Start!
	max10_flow_enable();

	/* Erase xFM sectors */
	printInfo("Erase xFM ", false);
	max10_flow_erase(mem, erase_sectors_mask);
	printInfo("Done");

	/* Program xFM */
	// Simplify code:
	// UFM0 follows UFM1,
	// CFM2 follow UFM0, etc...
	// so we don't need to iterate
	printInfo("Write xFM");
	writeXFM(buff, base_addr, 0, length / 4);

	/* Verify */
	if (_verify)
		verifyxFM(buff, base_addr, 0, length / 4);

	max10_flow_disable();

	free(buff);

	return true;
}

void Altera::max10_program(unsigned int offset)
{
	uint32_t base_addr;
	uint8_t update_sectors;

	/* Needs to have some specifics informations about internal flash size/organisation
	 * and some magics.
	 */
	auto mem_map = max10_memory_map.find(_idcode);
	if (mem_map == max10_memory_map.end()) {
		printError("Model not supported. Please update max10_memory_map.");
		throw std::runtime_error("Model not supported. Please update max10_memory_map.");
	}
	const Altera::max10_mem_t mem = mem_map->second;

	/* Check for a full update or only for a subset */
	update_sectors = max10_flash_sectors_to_mask(_flash_sectors);

	if (_file_extension != "pof") {
		max10_program_ufm(&mem, offset,
			(_flash_sectors.size() == 0) ? 0 : update_sectors);
		return;
	}

	POFParser _bit(_filename, _verbose);
	_bit.parse();
	_bit.displayHeader();

	/*
	 * MAX10 memory map differs according to internal configuration mode
	 * - 1   dual       compressed image:               CFM0 is used for img0, CFM1 + CFM2 for img1
	 * - 2   single   uncompressed image:               CFM0 + CFM1 are used, CFM2 used to additional UFM
	 * - 3/4 single (un)compressed image with mem init: CFM0 + CFM1 + CFM2
	 * - 5   single     compressed image:               CFM0 is used, CFM1&CFM2 used to additional UFM
	 */
	/* For Mode (POF content):
	 * 1  :  UFM: UFM1+UFM0 (in this order, this POF section size == memory section size),
	 *      CFM1: CFM2+CFM1 (in this order, this section == CFM2+CFM1 size),
	 *      CFM0: CFM0 (this section size == CFM0 size)
	 *
	 * 2  :  UFM: UFM1+UFM0+CFM2 (in this order, this section size == full UFM section size + CFM2 size)
	 *      CFM0: CFM1+CFM0 (in this order, this section size == CFM1+CFM0)
	 *
	 * 3/4:  UFM: UFM1+UFM0 (in this order, this section size == full UFM section size)
	 *      CFM0: CFM2+CFM1+CFM0 (in this order, this section size == full CFM section size)
	 *
	 * 5  :  UFM: UFM1+UFM0+CFM2+CFM1 (in this order, this section size == full UFM section size + CFM2 size + CFM1 size)
	 *      CFM0: CFM0 (this section size == CFM0)
	 */
	/* OPTIONS:
	 * ON_CHIP_BITSTREAM_DECOMPRESSION ON/OFF
	 * Dual Compressed Images (256Kbits UFM):
	 *     set_global_assignment -name INTERNAL_FLASH_UPDATE_MODE "DUAL IMAGES"
	 * Single Compressed Image (1376Kbits UFM):
	 *     set_global_assignment -name INTERNAL_FLASH_UPDATE_MODE "SINGLE COMP IMAGE"
	 * Single Compressed Image with Memory Initialization (256Kbits UFM):
	 *     set_global_assignment -name INTERNAL_FLASH_UPDATE_MODE "SINGLE COMP IMAGE WITH ERAM"
	 * Single Uncompressed Image (912Kbits UFM):
	 *     set_global_assignment -name INTERNAL_FLASH_UPDATE_MODE "SINGLE IMAGE"
	 * Single Uncompressed Image with Memory Initialization (256Kbits UFM):
	 *     set_global_assignment -name INTERNAL_FLASH_UPDATE_MODE "SINGLE IMAGE WITH ERAM"
	 */

	/*
	 * Memory organisation based on internal flash configuration mode is great but in fact
	 * POF configuration data match MAX10 memory organisation:
	 * its more easy to start with POF's CFM section and uses pointer based on prev ptr and section size
	 */

	uint8_t *ufm_data[2], *cfm_data[3];  // memory pointers (2 for UFM, 3 for CFM)

	// UFM Mapping
	ufm_data[1] = _bit.getData("UFM");
	ufm_data[0] = &ufm_data[1][mem.ufm_len[1] * 4];  // Just after UFM1 (but size may differs

	// CFM Mapping
	cfm_data[2] = &ufm_data[0][mem.ufm_len[0] * 4];  // First CFM section in FPGA internal flash
	cfm_data[1] = &cfm_data[2][mem.cfm_len[2] * 4];  // Second CFM section but just after CFM2
	cfm_data[0] = &cfm_data[1][mem.cfm_len[1] * 4];  // last CFM section but just after CFM1

	// DSM Mapping
	const uint8_t *dsm_data = _bit.getData("ICB");
	const int dsm_len = _bit.getLength("ICB") / 32;  // getLength (bits) dsm_len in 32bits word

	// Start!
	max10_flow_enable();

	max10_flow_erase(&mem, update_sectors);
	if (update_sectors == 0x1f)
		max10_dsm_verify();

	/* Write */

	// UFM 1 -> 0
	base_addr = mem.ufm_addr;
	for (int i = 1; i >= 0; i--) {
		if (update_sectors & (1 << i)) {
			printInfo("Write UFM" + std::to_string(i));
			writeXFM(ufm_data[i], base_addr, 0, mem.ufm_len[i]);
		}
		base_addr += mem.ufm_len[i];
	}

	// CFM2 -> 0
	base_addr = mem.cfm_addr;
	for (int i = 2; i >= 0; i--) {
		if (update_sectors & (1 << ((2 - i) + 2))) {
			printInfo("Write CFM" + std::to_string(i));
			writeXFM(cfm_data[i], base_addr, 0, mem.cfm_len[i]);
		}
		base_addr += mem.cfm_len[i];
	}

	/* Verify */
	if (_verify) {
		// UFM 1 -> 0
		base_addr = mem.ufm_addr;
		for (int i = 1; i >= 0; i--) {
			if (update_sectors & (1 << i)) {
				printInfo("Verify UFM" + std::to_string(i));
				verifyxFM(ufm_data[i], base_addr, 0, mem.ufm_len[i]);
			}
			base_addr += mem.ufm_len[i];
		}

		// CFM2->0
		base_addr = mem.cfm_addr;
		for (int i = 2; i >= 0; i--) {
			if (update_sectors & (1 << ((2 - i) + 2))) {
				printInfo("Verify CFM" + std::to_string(i));
				verifyxFM(cfm_data[i], base_addr, 0, mem.cfm_len[i]);
			}
			base_addr += mem.cfm_len[i];
		}
	}

	// DSM

	if (update_sectors == 0x1F) {
		max10_dsm_program(dsm_data, dsm_len);
		max10_dsm_verify();

		max10_flow_program_donebit(mem.done_bit_addr);
		max10_dsm_verify();
		max10_dsm_program_success(mem.pgm_success_addr);
		max10_dsm_verify();
	}

	/* disable ISC flow */
	max10_flow_disable();
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
}

static void word_to_array(uint32_t in, uint8_t *out) {
	out[0] = (in >>  0) & 0xff;
	out[1] = (in >>  8) & 0xff;
	out[2] = (in >> 16) & 0xff;
	out[3] = (in >> 24) & 0xff;
}

/* Two possibilities:
 * - full internal flash must be erased (to write a POF file): DSM_CLEAR must be used to
 *   perform a full erase
 * - only a sub-set of sectors must be erased (to update a CPU firmware): only specified
 *   sectors have to be erased using MAX10_ISC_ERASE instruction after moving to a specific
 *   erase address
 */
void Altera::max10_flow_erase(const Altera::max10_mem_t *mem, const uint8_t erase_sectors)
{
	const uint32_t dsm_clear_delay = 350000120 / _clk_period;
	/* All sectors must be erased: DSM_CLEAR is better/faster */
	if (erase_sectors == 0x1f) {
		const uint8_t dsm_clear[2] = MAX10_DSM_CLEAR;

		max10_addr_shift(0x000000);

		_jtag->shiftIR((unsigned char *)dsm_clear, NULL, IRLENGTH);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(dsm_clear_delay);
	} else {
		//const uint32_t isc_clear_delay = 5120 / _clk_period;
		const uint8_t isc_erase[2] = MAX10_ISC_ERASE;

		/* each bit is a sector to erase */
		for (int sect = 0; sect < 5; sect++) {
			if ((erase_sectors >> sect) & 0x01) {
				max10_addr_shift(mem->sectors_erase_addr[sect]);
				_jtag->shiftIR((unsigned char *)isc_erase, NULL, IRLENGTH);
				_jtag->set_state(Jtag::RUN_TEST_IDLE);
				_jtag->toggleClk(dsm_clear_delay);
			}
		}
	}
}

void Altera::writeXFM(const uint8_t *cfg_data, uint32_t base_addr, uint32_t offset, uint32_t len)
{
	uint8_t *ptr = (uint8_t *)cfg_data + offset;  // FIXME: maybe adding offset here ?
	const uint8_t isc_program[2] = MAX10_ISC_PROGRAM;

	/* precompute some delays required during loop */
	const uint32_t isc_program2_delay = 320000 / _clk_period;  // ns must be 350us

	ProgressBar progress("Write Flash", len, 50, _quiet);
	for (uint32_t i = 0; i < len; i+=512) {
		bool must_send_sir = true;
		uint32_t max = (i + 512 <= len)? 512 : len - i;
		for (uint32_t ii = 0; ii < max; ii++) {
			const uint32_t data = ARRAY2INT32(ptr);
			progress.display(i);

			/* flash was erased before: to save time skip write when cfg_data
			 * contains only bit set to high
			 */
			if (data == 0xffffffff) {
				must_send_sir = true;
				ptr += 4;
				continue;
			}

			/* TODO: match more or less svf but not bsdl */
			if (must_send_sir) {
				/* Set base addr */
				max10_addr_shift(base_addr + i + ii);

				/* set ISC_PROGRAM/DSM_PROGRAM */
				_jtag->shiftIR((unsigned char *)isc_program, NULL, IRLENGTH, Jtag::PAUSE_IR);
				must_send_sir = false;
			}

			_jtag->shiftDR(ptr, NULL, 32, Jtag::RUN_TEST_IDLE);
			_jtag->toggleClk(isc_program2_delay);
			ptr += 4;
		}
	}
	progress.done();
}

uint32_t Altera::verifyxFM(const uint8_t *cfg_data, uint32_t base_addr, uint32_t offset,
	uint32_t len)
{
	uint8_t *ptr = (uint8_t *)cfg_data + offset;  // avoid passing offset ?

	const uint8_t read_cmd[2] = MAX10_ISC_READ;
	uint32_t errors = 0;

	ProgressBar progress("Verify", len, 50, _quiet);
	for (uint32_t i = 0; i < len; i+=512) {
		const uint32_t max = (i + 512 <= len)? 512 : len - i;
		progress.display(i);

		/* send address */
		max10_addr_shift(base_addr + i);

		/* send read command */
		_jtag->shiftIR((unsigned char *)read_cmd, NULL, IRLENGTH, Jtag::PAUSE_IR);

		for (uint32_t ii = 0; ii < max; ii++) {
			uint8_t data[4];

			_jtag->shiftDR(NULL, data, 32, Jtag::RUN_TEST_IDLE);
			/* TODO: compare */
			for (uint8_t pos = 0; pos < 4; pos++) {
				if (ptr[pos] != data[pos]) {
					printf("Error@%d: %02x %02x %02x %02x ", pos, data[0], data[1], data[2], data[3]);
					printf("%02x %02x %02x %02x\n", ptr[0], ptr[1], ptr[2], ptr[3]);
					errors++;
					break;
				}
			}
			ptr += 4;
		}
	}
	if (errors == 0)
		progress.done();
	else
		progress.fail();

	return errors;
}

void Altera::max10_flow_enable()
{
	const int enable_delay  = 350000120 / _clk_period;  // must be 1 tck
	const uint8_t cmd[2] = MAX10_ISC_ENABLE;

	_jtag->shiftIR((unsigned char *)cmd, NULL, IRLENGTH);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(enable_delay);
}

void Altera::max10_flow_disable()
{
	// ISC_DISABLE  WAIT  100.0e-3)
	// BYPASS	   WAIT  305.0e-6
	const int disable_len = (1e9 * 350e-3) / _clk_period;
	const int bypass_len = (3 + (1e9 * 1e-3) / _clk_period);
	const uint8_t cmd0[2] = MAX10_ISC_DISABLE;
	const uint8_t cmd1[2] = MAX10_BYPASS;

	_jtag->shiftIR((unsigned char *)cmd0, NULL, IRLENGTH);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(disable_len);

	_jtag->shiftIR((unsigned char *)cmd1, NULL, IRLENGTH);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(bypass_len);
}

void Altera::max10_dsm_program(const uint8_t *dsm_data, const uint32_t dsm_len)
{
	const int program_del = 5120 / _clk_period;
	const int write_del = 320000 / _clk_period;

	uint32_t *icb_dat = (uint32_t *)dsm_data;
	const int len = dsm_len / 32;
	const uint8_t cmd[2] = MAX10_DSM_ICB_PROGRAM;
	uint8_t dat[4];
	/* Instead of writing the full section
	 * only write word with a value != 0xffffffff
	 */
	for (int i = 0; i < len; i++) {
		if (icb_dat[i] != 0xffffffff) {
			/* send addr */
			max10_addr_shift(i);
			word_to_array(icb_dat[i], dat);

			_jtag->shiftIR((unsigned char *)cmd, NULL, IRLENGTH);
			_jtag->set_state(Jtag::RUN_TEST_IDLE);
			_jtag->toggleClk(program_del);
			_jtag->shiftDR(dat, NULL, 32, Jtag::RUN_TEST_IDLE);
			_jtag->toggleClk(write_del);  // 305.0e-6
		}
	}
}

bool Altera::max10_dsm_verify()
{
	const uint32_t dsm_delay = 5120 / _clk_period;
	const uint8_t cmd[2] = MAX10_DSM_VERIFY;

	const uint8_t tx = 0x00;  // 1 in bsdl, 0 in svf
	uint8_t rx = 0;

	_jtag->shiftIR((unsigned char *)cmd, NULL, IRLENGTH);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(dsm_delay);
	_jtag->shiftDR(&tx, &rx, 1, Jtag::RUN_TEST_IDLE);

	printf("BSM: %02x\n", rx);
	if ((rx & 0x01) == 0x01) {
		printInfo("DSM Verify: OK");
		return true;
	}
	printError("DSM Verify: KO");
	return false;
}

void Altera::max10_addr_shift(uint32_t addr)
{
	const uint8_t cmd[2] = MAX10_ISC_ADDRESS_SHIFT;
	const uint32_t base_addr = POFParser::reverse_32(addr) >> 9;

	uint8_t addr_arr[4];
	word_to_array(base_addr, addr_arr);

	/* FIXME/TODO:
	 * 1. in bsdl file no delay between IR and DR
	 *	but 1TCK after DR
	 * 2. PAUSE IR is a state where loop is possible -> required to move
	 *	to RUN_TEST_IDLE ?
	 */
	_jtag->shiftIR((unsigned char *)cmd, NULL, IRLENGTH, Jtag::PAUSE_IR);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(5120 / _clk_period);  // fine delay ?
	_jtag->shiftDR(addr_arr, NULL, 23, Jtag::RUN_TEST_IDLE);
}

void Altera::max10_dsm_program_success(const uint32_t pgm_success_addr)
{
	const uint32_t prog_len = 5120 / _clk_period;  // ??
	const uint32_t prog2_len = 320000 / _clk_period;  // ??

	const uint8_t cmd[2] = MAX10_DSM_ICB_PROGRAM;

	uint8_t magic[4];
	word_to_array(0x6C48A50F, magic);  // FIXME: uses define instead

	max10_addr_shift(pgm_success_addr);

	/* Send 'Magic' code */
	_jtag->shiftIR((unsigned char *)cmd, NULL, IRLENGTH, Jtag::PAUSE_IR);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(prog_len);  // fine delay ?
	_jtag->shiftDR(magic, NULL, 32, Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(prog2_len);  // must wait 305.0e-6
}

void Altera::max10_flow_program_donebit(const uint32_t done_bit_addr)
{
	const uint32_t addr_shift_delay = 5120 / _clk_period;  // ??
	const uint32_t icb_program_delay = 320000 / _clk_period;  // ??

	uint8_t cmd[2] = MAX10_DSM_ICB_PROGRAM;

	uint8_t magic[4];
	word_to_array(0x6C48A50F, magic);  // FIXME: uses define instead

	/* Send target address */
	max10_addr_shift(done_bit_addr);

	/* Send 'Magic' code */
	_jtag->shiftIR(cmd, NULL, IRLENGTH, Jtag::PAUSE_IR);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(addr_shift_delay);  // fine delay ?
	_jtag->shiftDR(magic, NULL, 32, Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(icb_program_delay);  // must wait 305.0e-6
}

bool Altera::max10_read_section(FILE *fd, const uint32_t base_addr, const uint32_t len)
{
	const uint8_t read_cmd[2] = MAX10_ISC_READ;

	ProgressBar progress("Dump", len, 50, _quiet);
	for (uint32_t i = 0; i < len; i += 512) {
		const uint32_t max = (i + 512 <= len)? 512 : len - i;
		progress.display(i);

		/* send address */
		max10_addr_shift(base_addr + i);

		/* send read command */
		_jtag->shiftIR((unsigned char *)read_cmd, NULL, IRLENGTH, Jtag::PAUSE_IR);

		for (uint32_t ii = 0; ii < max; ii++) {
			uint8_t data[4];

			_jtag->shiftDR(NULL, data, 32, Jtag::RUN_TEST_IDLE);
			fwrite(data, sizeof(uint8_t), 4, fd);
		}
	}
	progress.done();

	return true;
}

bool Altera::max10_dump()
{
	uint32_t base_addr;

	auto mem_map = max10_memory_map.find(_idcode);
	if (mem_map == max10_memory_map.end()) {
		printError("Model not supported. Please update max10_memory_map.");
		return false;
	}
	const max10_mem_t mem = mem_map->second;

	/* Access clk frequency to store clk_period required
	 * for all operations. Can't be done at CTOR because
	 * frequency be changed between these 2 methods.
	 */
	_clk_period = 1e9/static_cast<float>(_jtag->getClkFreq());

	FILE *fd = fopen(_filename.c_str(), "w");
	if (!fd) {
		printError("Failed to open dump file.");
		return false;
	}

	max10_flow_enable();

	/* UFM 1 -> 0 */
	base_addr = mem.ufm_addr;
	for (int i = 1; i >= 0; i--) {
		printInfo("Dump UFM" + std::to_string(i));
		max10_read_section(fd, base_addr, mem.ufm_len[i]);
		base_addr += mem.ufm_len[i];
	}

	/* CFM 2 -> 0 */
	base_addr = mem.cfm_addr;
	for (int i = 2; i >= 0; i--) {
		printInfo("Dump CFM" + std::to_string(i));
		max10_read_section(fd, base_addr, mem.cfm_len[i]);
		base_addr += mem.cfm_len[i];
	}

	max10_flow_disable();

	fclose(fd);
	return true;
}

uint8_t Altera::max10_flash_sectors_to_mask(std::string flash_sectors)
{
	uint8_t mask = 0;

	if (flash_sectors.size() > 0) {
		const std::vector<std::string> sectors = splitString(flash_sectors, ',');
		for (const auto &sector: sectors) {
			if (sector == "UFM1")
				mask |= (1 << 0);
			else if (sector == "UFM0")
				mask |= (1 << 1);
			else if (sector == "CFM2")
				mask |= (1 << 2);
			else if (sector == "CFM1")
				mask |= (1 << 3);
			else if (sector == "CFM0")
				mask |= (1 << 4);
			else
				throw std::runtime_error("Unknown sector " + sector);
		}
	} else { // full update
		mask = 0x1F;
	}

	return mask;
}

bool Altera::sectors_mask_start_end_addr(const Altera::max10_mem_t *mem,
	const uint8_t update_sectors, uint32_t *start, uint32_t *end,
	uint8_t *sectors_mask)
{
	uint32_t saddr = mem->ufm_addr;
	uint32_t eaddr = saddr;
	uint8_t start_bit = 0, end_bit = 0;
	/* For sake of simplicity: create an array with all length aligned
	 * as it in MAX10 devices
	 */
	const uint32_t mem_map_length[] = {
		mem->ufm_len[1], mem->ufm_len[0],
		mem->cfm_len[2], mem->cfm_len[1]
	};

	if (update_sectors == 0) {
		eaddr = mem->ufm_addr + (mem->ufm_len[0] + mem->ufm_len[1]);
		*sectors_mask = 0x3;
	} else {
		/* eaddr start with full memory size */
		for (uint8_t i = 0; i < 4; i++)
			eaddr += mem_map_length[i];
		/* search first bit == 1 and increment start address */
		for (uint8_t i = 0; i < 4; i++) {
			if (update_sectors & (1 << i)) {
				start_bit = i;
				break;
			}
			saddr += mem_map_length[i];
		}

		/* decrement eaddr until last bit == 1 found */
		for (int8_t i = 3; i >= 0; i--) {
			if (update_sectors & (1 << i)) {
				end_bit = i + 1;
				break;
			}
			eaddr -= mem_map_length[i];
		}
		*sectors_mask = ((1 << end_bit) - 1) - ((1 << start_bit) - 1);
	}
	*start = saddr;
	*end = eaddr;
	return true;
}

/* SPI interface */

int Altera::spi_put(uint8_t cmd, const uint8_t *tx, uint8_t *rx, uint32_t len)
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

int Altera::spi_put(const uint8_t *tx, uint8_t *rx, uint32_t len)
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
		Jtag::tapState_t end_state, bool debug)
{
	(void) debug;
	uint8_t tx_ir[2] = {USER0, 0};
	_jtag->shiftIR(tx_ir, NULL, IRLENGTH, Jtag::UPDATE_IR);
	_jtag->shiftDR(tx, rx, len, end_state);
}
