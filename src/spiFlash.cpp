// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <cmath>
#include <map>
#include <iostream>

#include "progressBar.hpp"
#include "display.hpp"
#include "spiFlash.hpp"
#include "spiFlashdb.hpp"
#include "spiInterface.hpp"

/* read/write status register : 0B addr + 0 dummy */
#define FLASH_WRSR     0x01
#define FLASH_RDSR     0x05
#	define FLASH_RDSR_WIP	(0x01)
#	define FLASH_RDSR_WEL	(0x02)
/* flash program */
#define FLASH_PP       0x02
/* write [en|dis]able : 0B addr + 0 dummy */
#define FLASH_WRDIS    0x04
#define FLASH_WREN     0x06
/* sector (4Kb) erase */
#define FLASH_SE       0x20
/* read configuration register */
#define FLASH_RDCR     0x35
/* write function register (at least ISSI) */
#define FLASH_WRFR     0x42
/* read function register (at least ISSI) */
#define FLASH_RDFR     0x48
/* Read OTP : 3 B addr + 8 clk cycle*/
#define FLASH_ROTP     0x4B
/* block (32Kb) erase */
#define FLASH_BE32     0x52
#define FLASH_POWER_UP 0xAB
#define FLASH_POWER_DOWN 0xB9
/* read/write non volatile register: 0B addr + 0 dummy */
#define FLASH_RDNVCR   0xB5
#define FLASH_WRNVCR   0xB1
/* read/write volatile register */
#define FLASH_RDVCR    0x85
#define FLASH_WRVCR    0x81
/* bulk erase */
#define FLASH_CE       0xC7
/* block (64kb) erase */
#define FLASH_BE64     0xD8
/* read/write lock register : 3B addr + 0 dummy */
#define FLASH_WRLR     0xE5
#define FLASH_RDLR     0xE8
/* read/clear flag status register : 0B addr + 0 dummy */
#define FLASH_CLFSR    0x50
#define FLASH_RFSR     0x70
/* */
#define FLASH_WRVECR   0x61
#define FLASH_RDVECR   0x65

/* microchip SST26VF032B / SST26VF032BA */
/* Read Block Protection Register */
#define FLASH_RBPR 0x72
/* Global Block Protection unlock */
#define FLASH_ULBPR 0x98

SPIFlash::SPIFlash(SPIInterface *spi, bool unprotect, int8_t verbose):
	_spi(spi), _verbose(verbose), _jedec_id(0),
	_flash_model(NULL), _unprotect(unprotect)
{
	reset();
	power_up();
	read_id();
}

int SPIFlash::bulk_erase()
{
	if (write_enable() == -1)
		return -1;
	_spi->spi_put(FLASH_CE, NULL, NULL, 0);
	return _spi->spi_wait(FLASH_RDSR, FLASH_RDSR_WIP, 0x00, 100000, true);
}

/* sector -> subsector for micron */
int SPIFlash::sector_erase(int addr)
{
	uint8_t tx[4];
	tx[0] = (uint8_t)(FLASH_SE           );
	tx[1] = (uint8_t)(0xff & (addr >> 16));
	tx[2] = (uint8_t)(0xff & (addr >>  8));
	tx[3] = (uint8_t)(0xff & (addr      ));
	_spi->spi_put(tx, NULL, 4);
	return 0;
}

int SPIFlash::block32_erase(int addr)
{
	uint8_t tx[4] = {
		static_cast<uint8_t>(FLASH_BE32         ),
		static_cast<uint8_t>(0xff & (addr >> 16)),
		static_cast<uint8_t>(0xff & (addr >>  8)),
		static_cast<uint8_t>(0xff & (addr      ))};
	_spi->spi_put(tx, NULL, 4);
	return 0;
}

/* block64 -> sector for micron */
int SPIFlash::block64_erase(int addr)
{
	uint8_t tx[4] = {
		static_cast<uint8_t>(FLASH_BE64         ),
		static_cast<uint8_t>(0xff & (addr >> 16)),
		static_cast<uint8_t>(0xff & (addr >>  8)),
		static_cast<uint8_t>(0xff & (addr      ))};
	_spi->spi_put(tx, NULL, 4);
	return 0;
}

int SPIFlash::sectors_erase(int base_addr, int size)
{
	int ret = 0;
	int start_addr = base_addr;
	/* compute end_addr to be multiple of 4Kb */
	int end_addr = (base_addr + size + 0xfff) & ~0xfff;
	ProgressBar progress("Erasing", end_addr, 50, _verbose < 0);
	/* start with block size (64Kb) */
	int step = 0x10000;

	for (int addr = start_addr; addr < end_addr; addr += step) {
		if (write_enable() == -1) {
			ret = -1;
			break;
		}

		/* if block erase + addr end out of end_addr -> use sector_erase (4Kb) */
		if (addr + 0x10000 > end_addr && _flash_model && _flash_model->subsector_erase) {
			step = 0x1000;
			ret = sector_erase(addr);
		} else {
			ret = block64_erase(addr);
		}

		if (ret == -1) {
			break;
		}
		if (_spi->spi_wait(FLASH_RDSR, FLASH_RDSR_WIP, 0x00, 100000, false) == -1) {
			ret = -1;
			break;
		}
		progress.display(addr);
	}
	if (ret == 0)
		progress.done();
	else
		progress.fail();

	return ret;
}

int SPIFlash::write_page(int addr, uint8_t *data, int len)
{
	uint8_t tx[len+3];
	tx[0] = (uint8_t)(0xff & (addr >> 16));
	tx[1] = (uint8_t)(0xff & (addr >>  8));
	tx[2] = (uint8_t)(0xff & (addr      ));

	memcpy(tx+3, data, len);

	if (write_enable() == -1)
		return -1;

	_spi->spi_put(FLASH_PP, tx, NULL, len+3);
	return _spi->spi_wait(FLASH_RDSR, FLASH_RDSR_WIP, 0x00, 1000);
}

int SPIFlash::read(int base_addr, uint8_t *data, int len)
{
	uint8_t tx[len+3];
	uint8_t rx[len+3];
	tx[0] = (uint8_t)(0xff & (base_addr >> 16));
	tx[1] = (uint8_t)(0xff & (base_addr >>  8));
	tx[2] = (uint8_t)(0xff & (base_addr      ));

	int ret = _spi->spi_put(0x03, tx, rx, len+3);
	if (ret == 0)
		memcpy(data, rx+3, len);
	else
		printf("error\n");
	return ret;
}

bool SPIFlash::dump(const std::string &filename, const int &base_addr,
		const int &len, int rd_burst)
{
	if (rd_burst == 0)
		rd_burst = len;

	/* segfault with buffer > 1M */
	if (rd_burst > 0x100000)
		rd_burst = 0x100000;

	std::string data;
	data.resize(rd_burst);

	printInfo("dump flash (May take time)");

	printInfo("Open dump file ", false);
	FILE *fd = fopen(filename.c_str(), "wb");
	if (!fd) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}

	ProgressBar progress("Read flash ", len, 50, false);
	for (int i = 0; i < len; i += rd_burst) {
		if (rd_burst + i > len)
			rd_burst = len - i;
		if (0 != read(base_addr + i, (uint8_t*)&data[0], rd_burst)) {
			progress.fail();
			printError("Failed to read flash");
			fclose(fd);
			return false;
		}
		fwrite(data.c_str(), sizeof(uint8_t), rd_burst, fd);
		progress.display(i);
	}

	progress.done();

	fclose(fd);

	return true;
}

int SPIFlash::erase_and_prog(int base_addr, uint8_t *data, int len)
{
	if (_jedec_id == 0)
		read_id();
	bool must_relock = false;  // used to relock after write;

	/* microchip SST26VF032B have global lock set
	 * at powerup. global unlock must be send inconditionally
	 * with or without block protection
	 */
	if (_jedec_id == 0xbf2642bf) {  // microchip SST26VF032B
		if (!global_unlock())
			return -1;
	}
	/* check Block Protect Bits (hide WIP/WEN bits) */
	uint8_t status = read_status_reg() & ~0x03;
	if (_verbose > 0)
		display_status_reg(status);
	/* if known chip */
	if (_flash_model) {
		/* check if offset + len fit in flash */
		if ((unsigned int)(base_addr + len) > (_flash_model->nr_sector * 0x10000) - 1) {
			printError("flash overflow");
			return -1;
		}
		/* compute protected area */
		int8_t tb = get_tb();
		if (tb == -1)
			return -1;
		std::map<std::string, uint32_t> lock_len = bp_to_len(status, tb);
		printf("%08x %08x %08x %02x\n", base_addr,
				lock_len["start"], lock_len["end"], status);

		/* if some blocks are locked */
		if (lock_len["start"] != 0 || lock_len["end"] != 0) {
			/* if overlap */
			if (tb == 1) {  // bottom blocks are protected
							// check if start is in protected blocks
				if ((uint32_t)base_addr <= lock_len["end"])
					must_relock = true;
			} else {  // top blocks
				if ((uint32_t)(base_addr + len) >= lock_len["start"])
					must_relock = true;
			}
		}
		/* ISSI IS25LP032 seems have a bug:
		 * block protection is always in top mode regardless of
		 * the TB bit: if write is not at offset 0 -> force unlock
		 */
		if ((_jedec_id >> 8) == 0x9d6016 && tb == 1 && base_addr != 0) {
			_unprotect = true;
			must_relock = true;
		}
	} else {  // unknown chip: basic test
		printWarn("flash chip unknown: use basic protection detection");
		if ((status & 0x1c) != 0)
			must_relock = true;
	}

	/* if it's needs to unlock */
	if (must_relock) {
		printf("unlock blocks\n");
		if (!_unprotect) {
			printError("Error: block protection is set");
			printError("       can't unlock without --unprotect-flash");
			return -1;
		} else  {
			if (disable_protection() != 0)
				return -1;
		}
	}

	/* Now we can erase sector and write new data */
	ProgressBar progress("Writing", len, 50, _verbose < 0);
	if (sectors_erase(base_addr, len) == -1)
		return -1;

	uint8_t *ptr = data;
	int size = 0;
	for (int addr = 0; addr < len; addr += size, ptr+=size) {
		size = (addr + 256 > len)?(len-addr) : 256;
		if (write_page(base_addr + addr, ptr, size) == -1)
			return -1;
		progress.display(addr);
	}
	progress.done();

	/* and if required: relock blocks */
	if (must_relock) {
		enable_protection(status);
		if (_verbose > 0)
			display_status_reg(read_status_reg());
	}
	return 0;
}

bool SPIFlash::verify(const int &base_addr, const uint8_t *data,
		const int &len, int rd_burst)
{
	if (rd_burst == 0)
		rd_burst = len;

	printInfo("Verifying write (May take time)");

	std::string verify_data;
	verify_data.resize(rd_burst);

	ProgressBar progress("Read flash ", len, 50, false);
	for (int i = 0; i < len; i += rd_burst) {
		if (rd_burst + i > len)
			rd_burst = len - i;
		if (0 != read(base_addr + i, (uint8_t*)&verify_data[0], rd_burst)) {
			progress.fail();
			printError("Failed to read flash");
			return false;
		}

		for (int ii = 0; ii < rd_burst; ii++) {
			if ((uint8_t)verify_data[ii] != data[i+ii]) {
				progress.fail();
				printError("Verification failed at " +
						std::to_string(base_addr + i + ii));
				return false;
			}
		}
		progress.display(i);
	}

	progress.done();

	return true;
}

void SPIFlash::reset()
{
	uint8_t data[8];
	memset(data, 0xff, 8);
	_spi->spi_put(0xff, data, NULL, 8);
}

void SPIFlash::read_id()
{
	int len = 4;
	uint8_t rx[512];
	bool has_edid = false;

	_spi->spi_put(0x9F, NULL, rx, 4);
	_jedec_id = 0;
	for (int i=0; i < 4; i++) {
		_jedec_id = _jedec_id << 8;
		_jedec_id |= (0x00ff & (int)rx[i]);
		if (_verbose > 0)
			printf("%x ", rx[i]);
	}

	if (_verbose > 0)
		printf("read %x\n", _jedec_id);
	auto t = flash_list.find(_jedec_id >> 8);
	if (t != flash_list.end()) {
		_flash_model = &(*t).second;
		char content[256];
		snprintf(content, 256, "Detected: %s %s %u sectors size: %uMb",
				_flash_model->manufacturer.c_str(), _flash_model->model.c_str(),
				_flash_model->nr_sector, _flash_model->nr_sector * 0x80000 / 1048576);
		printInfo(content);
	} else {
		/* read extented */
		if ((_jedec_id & 0xff) != 0) {
			has_edid = true;
			len += (_jedec_id & 0x0ff);
			_spi->spi_put(0x9F, NULL, rx, len);
		}

		/* must be 0x20BA1810 ... */

		printf("Detail: \n");
		printf("Jedec ID          : %02x\n", rx[0]);
		printf("memory type       : %02x\n", rx[1]);
		printf("memory capacity   : %02x\n", rx[2]);
		if (has_edid) {
			printf("EDID + CFD length : %02x\n", rx[3]);
			printf("EDID              : %02x%02x\n", rx[5], rx[4]);
			printf("CFD               : ");
			if (_verbose > 0) {
				for (int i = 6; i < len; i++)
					printf("%02x ", rx[i]);
				printf("\n");
			} else {
				printf("\n");
			}
		}
	}
}

void SPIFlash::display_status_reg(uint8_t reg)
{
	uint8_t tb, bp;
	if (!_flash_model) {
		tb = (reg >> 5) & 0x01;
		bp = (((reg >> 6) & 0x01) << 3) | ((reg >> 2) & 0x07);
	} else {
		tb = (reg & _flash_model->tb_offset) ? 1 : 0;
		bp = 0;
		for (int i = 0; i < _flash_model->bp_len; i++)
			if (reg & _flash_model->bp_offset[i])
				bp |= 1 << i;
	}

	printf("RDSR : %02x\n", reg);
	printf("WIP  : %d\n", reg&0x01);
	printf("WEL  : %d\n", (reg>>1)&0x01);
	printf("BP   : %x\n", bp);
	if ((_jedec_id >> 8) != 0x9d60) {
		printf("TB   : %d\n", tb);
	} else {  // ISSI IS25LP
		printf("QE   : %d\n", ((reg >> 6) & 0x01));
	}
	printf("SRWD : %d\n", ((reg >> 7) & 0x01));

	/* function register */
	switch (_jedec_id >> 8) {
		case 0x9d60:
			_spi->spi_put(FLASH_RDFR, NULL, &reg, 1);
			printf("\nFunction Register\n");
			printf("RDFR : %02x\n", reg);
			printf("RES  : %d\n", ((reg >> 0) & 0x01));
			printf("TBS  : %d\n", ((reg >> 1) & 0x01));
			printf("PSUS : %d\n", ((reg >> 2) & 0x01));
			printf("ESUS : %d\n", ((reg >> 3) & 0x01));
			printf("IRL  : %x\n", ((reg >> 4) & 0x0f));
			break;
		case 0x010216:
			_spi->spi_put(FLASH_RDCR, NULL, &reg, 1);
			printf("\nConfiguration Register\n");
			printf("RDCR   : %02x\n", reg);
			printf("FREEZE : %d\n", ((reg >> 0) & 0x01));
			printf("QUAD   : %d\n", ((reg >> 1) & 0x01));
			printf("TBPARM : %d\n", ((reg >> 2) & 0x01));
			printf("BPNV   : %d\n", ((reg >> 3) & 0x01));
			printf("TBPROT : %d\n", ((reg >> 5) & 0x01));
			break;
	}
}

uint8_t SPIFlash::read_status_reg()
{
	uint8_t rx;
	_spi->spi_put(FLASH_RDSR, NULL, &rx, 1);
	return rx;
}

uint16_t SPIFlash::readNonVolatileCfgReg()
{
	uint8_t rx[2];
	_spi->spi_put(FLASH_RDNVCR, NULL, rx, 2);
	if (_verbose > 0)
		printf("Non Volatile %x %x\n", rx[0], rx[1]);
	return (rx[1] << 8) | rx[0];
}

uint16_t SPIFlash::readVolatileCfgReg()
{
	uint8_t rx[2];
	_spi->spi_put(FLASH_RDVCR, NULL, rx, 2);
	if (_verbose > 0)
		printf("Volatile %x %x\n", rx[0], rx[1]);
	return (rx[1] << 8) | rx[0];
}

void SPIFlash::power_up()
{
	_spi->spi_put(FLASH_POWER_UP, NULL, NULL, 0);
}

void SPIFlash::power_down()
{
	_spi->spi_put(FLASH_POWER_DOWN, NULL, NULL, 0);
}

int SPIFlash::write_enable()
{
	_spi->spi_put(FLASH_WREN, NULL, NULL, 0);
	/* wait WEL */
	if (_spi->spi_wait(FLASH_RDSR, FLASH_RDSR_WEL, FLASH_RDSR_WEL, 1000)) {
		printf("write en: Error\n");
		return -1;
	}

	return 0;
}

int SPIFlash::write_disable()
{
	_spi->spi_put(FLASH_WRDIS, NULL, NULL, 0);
	/* wait ! WEL */
	int ret = _spi->spi_wait(FLASH_RDSR, FLASH_RDSR_WEL, 0x00, 1000);
	if (ret == -1)
		printf("write disable: Error\n");
	else if (_verbose > 0)
		printf("write disable: Success\n");
	return ret;
}

int SPIFlash::disable_protection()
{
	uint8_t data = 0x00;
	if (write_enable() == -1)
		return -1;
	_spi->spi_put(FLASH_WRSR, &data, NULL, 1);
	if (_spi->spi_wait(FLASH_RDSR, 0xff, 0, 1000) < 0)
		return -1;

	/* read status */
	if (read_status_reg() != 0) {
		std::cout << "disable protection failed" << std::endl;
		return -1;
	}

	return 0;
}

/* write protect code to status register
 * no check for TB
 */
int SPIFlash::enable_protection(uint8_t protect_code)
{
	/* enable write (required to access WRSR) */
	if (write_enable() == -1) {
		printError("Error: can't enable write");
		return -1;
	}

	/* write status register and wait until Flash idle */
	_spi->spi_put(FLASH_WRSR, &protect_code, NULL, 1);
	if (_spi->spi_wait(FLASH_RDSR, 0xff, protect_code, 1000) < 0) {
		printError("Error: enable protection failed\n");
		return -1;
	}

	/* check status register update */
	if (read_status_reg() != protect_code) {
		printError("disable protection failed");
		return -1;
	}
	if (_verbose > 0)
		display_status_reg(read_status_reg());

	return 0;
}

int SPIFlash::enable_protection(uint32_t length)
{
	/* flash device is not listed: can't know BPx position, nor
	 * TB offset, nor TB non-volatile vs OTP */
	if (!_flash_model) {
		printError("unknown spi flash model: can't lock sectors");
		return -1;
	}

	/* convert number of sectors to bp[3:0] mask */
	uint8_t bp = len_to_bp(length);

	/* TB bit is OTP: this modification can't be revert!
	 * check if tb is already set and if not warn
	 * current (temporary) policy: do nothing
	 */
	if (_flash_model->tb_otp) {
		uint8_t tb = get_tb();
		/* check if TB is set */
		if (tb == 0) {
			printError("TOP/BOTTOM bit is OTP: can't write this bit");
			return -1;
		}
	}

	/* spansion devices have only one instruction to write
	 * both status register and configuration register
	 * we have to write 2 bytes:
	 * 0: status register
	 * 1: configuration register
	 */
	if ((_jedec_id >> 8) == 0x010216) {
		int ret = 0;
		uint8_t status;
		_spi->spi_put(FLASH_RDCR, NULL, &status, 1);
		uint8_t cfg[2] = {bp, status};
		cfg[1] |= _flash_model->tb_offset;
		_spi->spi_put(FLASH_WRSR, cfg, NULL, 2);
		if (_spi->spi_wait(FLASH_RDSR, 0x03, 0, 1000) < 0) {
			printError("Error: enable protection failed\n");
			return -1;
		}

		return ret;
	}

	/* if TB is located in status register -> set to 1 */
	if (_flash_model->tb_register == STATR)
		bp |= _flash_model->tb_offset;

	/* update status register */
	int ret = enable_protection(bp);

	/* tb is in different register */
	if (_flash_model->tb_register != STATR) {
		if (ret == -1)  // check if enable_protection has failed
			return ret;
		/* update register */
		uint8_t reg_wr, reg_rd, val;
		if (_flash_model->tb_register == FUNCR) {
			val = _flash_model->tb_offset;
			reg_wr = FLASH_WRFR;
			reg_rd = FLASH_RDFR;
		} else {
			printError("Unknown TOP/BOTTOM register");
			return -1;
		}

		/* write status register and wait until Flash idle */
		_spi->spi_put(reg_wr, &val, NULL, 1);
		if (_spi->spi_wait(FLASH_RDSR, 0x03, 0, 1000) < 0) {
			printError("Error: enable protection failed\n");
			return -1;
		}
		uint8_t rd_val;
		_spi->spi_put(reg_rd, NULL, &rd_val, 1);
		if (rd_val != val) {
			printError("failed to update TB bit");
			return -1;
		}
	}

	return ret;
}

/* retrieve TB (Top/Bottom) bit from register */
int8_t SPIFlash::get_tb()
{
	uint8_t status;
	/* read TB: not always in status register */
	switch (_flash_model->tb_register) {
	case STATR:  // status register
		status = read_status_reg();
		break;
	case FUNCR:  // function register
		_spi->spi_put(FLASH_RDFR, NULL, &status, 1);
		break;
	case CONFR:  // function register
		_spi->spi_put(FLASH_RDCR, NULL, &status, 1);
		break;
	default:  // unknown
		printError("Unknown Top/Bottom register");
		return -1;
	}

	return (status & _flash_model->tb_offset) ? 1 : 0;
}

/* convert bp area (status register) to len in byte */
std::map<std::string, uint32_t> SPIFlash::bp_to_len(uint8_t bp, uint8_t tb)
{
	std::map<std::string, uint32_t> protect_area;
	protect_area["start"] = 0;
	protect_area["end"] = 0;
	/* 0 -> no block protected */
	if (bp == 0)
		return protect_area;

	/* reconstruct code based on each BPx bit */
	uint8_t tmp = 0;
	for (int i = 0; i < 4; i++)
		if ((bp & _flash_model->bp_offset[i]))
			tmp |= (1 << i);
	/* bp code is 2^(bp-1) blocks */
	uint16_t nr_sectors = (1 << (tmp-1));
	printf("nr_sectors : %d\n", nr_sectors);
	uint32_t len = nr_sectors * 0x10000;
	if (tb == 1) {
		protect_area["start"] = 0;
		protect_area["end"] = len - 1;
	} else {
		protect_area["end"] = (_flash_model->nr_sector * 0x10000) - 1;
		protect_area["start"] = (protect_area["end"] + 1) - len;
	}

	return protect_area;
}

/* convert len (in byte) to bp (block protection) */
uint8_t SPIFlash::len_to_bp(uint32_t len)
{
	/* 0 -> no block to protect */
	if (len == 0)
		return 0;

	/* round and divide by sector size */
	len = ((len + 0xffff) & ~0xffff) / 0x10000;

	/* convert size to basic BP code */
	uint8_t bp = 1 + static_cast<int>(ceil(log2(len)));
	/* reconstruct code based on each BPx bit */
	uint8_t tmp = 0;
	for (int i = 0; i < 4; i++)
		if (bp & (1 << i))
			tmp |= _flash_model->bp_offset[i];

	return tmp;
}

/* microchip SST26VF032B has a dedicated register
 * to read sectors (un)lock status and another one to unlock
 * sectors
 */

bool SPIFlash::global_unlock()
{
	if (write_enable() != 0)
		return false;
	_spi->spi_put(FLASH_ULBPR, NULL, NULL, 0);

	if (_spi->spi_wait(FLASH_RDSR, 0xff, 0, 1000) < 0)
		return false;

	/* check if all sectors are unlocked */
	uint8_t rx2[10];
	_spi->spi_put(FLASH_RBPR, NULL, rx2, 10);
	printf("Non Volatile\n");
	for (int i = 0; i < 10; i++) {
		if (rx2[i] != 0)
			return false;
	}
	return true;
}
