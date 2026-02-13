// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2024 openFPGALoader contributors
 * BPI (Parallel NOR) Flash support via JTAG bridge
 */

#include "bpiFlash.hpp"

#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "display.hpp"
#include "progressBar.hpp"

/* Bit-reverse a byte (MSB <-> LSB).
 * Required for BPI x16: the FPGA's D00 pin is the MSBit of each config byte
 * (AR#7112), but flash DQ[0] is the LSBit. write_cfgmem applies this
 * transformation; we must do the same.
 */
static inline uint8_t reverseByte(uint8_t b)
{
	b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4);
	b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2);
	b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1);
	return b;
}

BPIFlash::BPIFlash(Jtag *jtag, int8_t verbose)
	: _jtag(jtag), _verbose(verbose), _irlen(6),
	  _capacity(0), _block_size(256 * 1024),
	  _manufacturer_id(0), _device_id(0),
	  _has_burst(false)
{
}

BPIFlash::~BPIFlash()
{
}

/*
 * Protocol: Single JTAG DR shift containing:
 *   TX: [start=1][cmd:4][addr:25][wr_data:16] = 46 bits
 *   RX: Data returned after execution delay
 *
 * Commands:
 *   0x1 = Write word
 *   0x2 = Read word
 *   0x3 = NOP
 *   0x4 = Burst write (addr + count + N×data words)
 */

uint16_t BPIFlash::bpi_read(uint32_t word_addr)
{
	/* Build packet: start(1) + cmd(4) + addr(25) + padding for read response */
	/* Extra bit needed due to pipeline delay in Verilog (data at offset 51, not 50) */
	const int total_bits = 1 + 4 + 25 + 20 + 16 + 1;  /* 67 bits total */
	const int total_bytes = (total_bits + 7) / 8;

	uint8_t tx[total_bytes];
	uint8_t rx[total_bytes];
	memset(tx, 0, total_bytes);
	memset(rx, 0, total_bytes);

	/* Pack: start=1, cmd=2 (read), addr (LSB first) */
	uint64_t packet = 1;                           /* start bit */
	packet |= ((uint64_t)CMD_READ) << 1;           /* cmd at bits [4:1] */
	packet |= ((uint64_t)(word_addr & 0x1FFFFFF)) << 5;  /* addr at bits [29:5] */

	/* Convert to bytes (LSB first for JTAG) */
	for (int i = 0; i < 5; i++) {
		tx[i] = (packet >> (i * 8)) & 0xFF;
	}

	/* Select USER1 instruction */
	uint8_t user1[] = {0x02};
	_jtag->shiftIR(user1, NULL, _irlen);

	/* Shift data and get response */
	_jtag->shiftDR(tx, rx, total_bits);
	_jtag->flush();

	/* Extract read data from response - it appears after the command execution */
	/* Data starts at bit 51 (after start+cmd+addr+exec_delay+1 pipeline delay) */
	int data_offset = 51;
	uint16_t data = 0;
	for (int i = 0; i < 16; i++) {
		int bit_pos = data_offset + i;
		int byte_idx = bit_pos / 8;
		int bit_idx = bit_pos % 8;
		if (rx[byte_idx] & (1 << bit_idx))
			data |= (1 << i);
	}

	return data;
}

void BPIFlash::bpi_write(uint32_t word_addr, uint16_t data)
{
	/* Build packet: start(1) + cmd(4) + addr(25) + data(16) + exec delay */
	const int total_bits = 1 + 4 + 25 + 16 + 20;
	const int total_bytes = (total_bits + 7) / 8;

	uint8_t tx[total_bytes];
	memset(tx, 0, total_bytes);

	/* Pack: start=1, cmd=1 (write), addr, data (all LSB first) */
	uint64_t packet = 1;                           /* start bit */
	packet |= ((uint64_t)CMD_WRITE) << 1;          /* cmd at bits [4:1] */
	packet |= ((uint64_t)(word_addr & 0x1FFFFFF)) << 5;  /* addr at bits [29:5] */
	packet |= ((uint64_t)data) << 30;              /* data at bits [45:30] */

	/* Convert to bytes (LSB first for JTAG) */
	for (int i = 0; i < 8; i++) {
		tx[i] = (packet >> (i * 8)) & 0xFF;
	}

	if (_verbose > 1) {
		char buf[256];
		snprintf(buf, sizeof(buf), "bpi_write(0x%06x, 0x%04x) TX:", word_addr, data);
		std::string msg = buf;
		for (int i = 0; i < total_bytes; i++) {
			snprintf(buf, sizeof(buf), " %02x", tx[i]);
			msg += buf;
		}
		printInfo(msg);
	}

	/* Select USER1 instruction */
	uint8_t user1[] = {0x02};
	_jtag->shiftIR(user1, NULL, _irlen);

	/* Shift data */
	_jtag->shiftDR(tx, NULL, total_bits);
	_jtag->flush();
}

void BPIFlash::bpi_write_no_flush(uint32_t word_addr, uint16_t data)
{
	/* Same packet as bpi_write() but no shiftIR or flush —
	 * caller sets IR once before the loop and flushes once after.
	 */
	const int total_bits = 1 + 4 + 25 + 16 + 20;
	const int total_bytes = (total_bits + 7) / 8;

	uint8_t tx[total_bytes];
	memset(tx, 0, total_bytes);

	uint64_t packet = 1;                           /* start bit */
	packet |= ((uint64_t)CMD_WRITE) << 1;          /* cmd at bits [4:1] */
	packet |= ((uint64_t)(word_addr & 0x1FFFFFF)) << 5;  /* addr at bits [29:5] */
	packet |= ((uint64_t)data) << 30;              /* data at bits [45:30] */

	for (int i = 0; i < 8; i++) {
		tx[i] = (packet >> (i * 8)) & 0xFF;
	}

	_jtag->shiftDR(tx, NULL, total_bits);
}

void BPIFlash::bpi_burst_write(uint32_t word_addr, const uint16_t *data,
				uint32_t count)
{
	if (count == 0)
		return;

	/* Burst packet: start(1) + cmd(4) + addr(25) + count(16) + N×(data(16) + pad(21))
	 * Header: 46 bits.  Per word: 37 bits.
	 */
	const uint32_t header_bits = 1 + 4 + 25 + 16;  /* 46 */
	const uint32_t per_word_bits = 16 + 21;          /* 37: 20 exec cycles + 1 transition */
	const uint32_t total_bits = header_bits + count * per_word_bits;
	const uint32_t total_bytes = (total_bits + 7) / 8;

	std::vector<uint8_t> tx(total_bytes, 0);

	/* Helper to set a single bit in the tx buffer */
	auto set_bit = [&](uint32_t bit_pos) {
		tx[bit_pos / 8] |= (1 << (bit_pos % 8));
	};

	/* Pack header LSB-first */
	uint32_t pos = 0;

	/* start bit = 1 */
	set_bit(pos);
	pos++;

	/* cmd = CMD_BURST_WRITE (4 bits) */
	for (int i = 0; i < 4; i++) {
		if (CMD_BURST_WRITE & (1 << i))
			set_bit(pos);
		pos++;
	}

	/* addr (25 bits) */
	for (int i = 0; i < 25; i++) {
		if (word_addr & (1u << i))
			set_bit(pos);
		pos++;
	}

	/* count (16 bits) */
	for (int i = 0; i < 16; i++) {
		if (count & (1u << i))
			set_bit(pos);
		pos++;
	}

	/* Pack each data word: 16 data bits + 21 padding bits */
	for (uint32_t w = 0; w < count; w++) {
		for (int i = 0; i < 16; i++) {
			if (data[w] & (1 << i))
				set_bit(pos);
			pos++;
		}
		pos += 21;  /* 20 exec cycles + 1 transition cycle */
	}

	uint8_t user1[] = {0x02};
	_jtag->shiftIR(user1, NULL, _irlen);
	_jtag->shiftDR(tx.data(), NULL, total_bits);
	_jtag->flush();
}

bool BPIFlash::detect()
{
	printInfo("Detecting BPI flash...");

	/* Issue Read ID command to flash: write 0x0090 to any address */
	bpi_write(0, FLASH_CMD_READ_ID);

	/* Small delay for command to take effect */
	usleep(1000);

	/* Read manufacturer ID at offset 0x00 */
	_manufacturer_id = bpi_read(0x00);

	/* Read device ID at offset 0x01 */
	_device_id = bpi_read(0x01);

	/* Return to read array mode */
	bpi_write(0, FLASH_CMD_READ_ARRAY);
	usleep(1000);

	if (_verbose) {
		char buf[64];
		snprintf(buf, sizeof(buf), "Raw Manufacturer ID: 0x%04x", _manufacturer_id);
		printInfo(buf);
		snprintf(buf, sizeof(buf), "Raw Device ID: 0x%04x", _device_id);
		printInfo(buf);
	}

	if (_manufacturer_id == 0x0089 || _manufacturer_id == 0x8900) {
		printInfo("Intel/Micron flash detected");
	} else if (_manufacturer_id == 0x0020 || _manufacturer_id == 0x2000) {
		printInfo("Micron flash detected");
	} else if (_manufacturer_id == 0xFFFF || _manufacturer_id == 0x0000) {
		char buf[64];
		snprintf(buf, sizeof(buf), "No BPI flash detected (ID: 0x%04x)", _manufacturer_id);
		printError(buf);
		return false;
	} else {
		char buf[64];
		snprintf(buf, sizeof(buf), "Unknown manufacturer: 0x%04x", _manufacturer_id);
		printWarn(buf);
	}

	{
		char buf[64];
		snprintf(buf, sizeof(buf), "Manufacturer ID: 0x%04x", _manufacturer_id);
		printInfo(buf);
		snprintf(buf, sizeof(buf), "Device ID: 0x%04x", _device_id);
		printInfo(buf);
	}

	/* MT28GU512AAA = 512Mbit = 64MB */
	_capacity = 64 * 1024 * 1024;
	_block_size = 256 * 1024;
	printInfo("Flash capacity: 64 MB (512 Mbit)");

	/* Enable burst write — assumes v02.00+ JTAG bitstream is loaded.
	 * Future: could auto-detect via USER4 version readback.
	 */
	_has_burst = true;

	return true;
}

bool BPIFlash::wait_ready(uint32_t timeout_ms)
{
	uint32_t elapsed = 0;
	const uint32_t poll_interval = 10;

	/* Issue read status command */
	bpi_write(0, FLASH_CMD_READ_STATUS);
	usleep(100);

	while (elapsed < timeout_ms) {
		uint16_t status = bpi_read(0);

		/* Intel CFI status register is 8 bits, upper byte undefined */
		uint8_t sr = status & 0xFF;

		if (sr & SR_READY) {
			if (sr & (SR_ERASE_ERR | SR_PROG_ERR | SR_VPP_ERR)) {
				char buf[64];
				snprintf(buf, sizeof(buf), "BPI Flash error: status = 0x%02x", sr);
				printError(buf);
				bpi_write(0, FLASH_CMD_CLEAR_STATUS);
				return false;
			}
			/* Return to read array mode */
			bpi_write(0, FLASH_CMD_READ_ARRAY);
			return true;
		}

		usleep(poll_interval * 1000);
		elapsed += poll_interval;
	}

	printError("BPI Flash timeout");
	return false;
}

bool BPIFlash::unlock_block(uint32_t word_addr)
{
	bpi_write(word_addr, FLASH_CMD_UNLOCK_BLOCK);
	usleep(100);
	bpi_write(word_addr, FLASH_CMD_UNLOCK_CONF);
	usleep(100);
	return true;
}

bool BPIFlash::erase_block(uint32_t addr)
{
	uint32_t word_addr = addr >> 1;

	if (_verbose) {
		char buf[64];
		snprintf(buf, sizeof(buf), "Erasing block at 0x%06x", addr);
		printInfo(buf);
	}

	unlock_block(word_addr);

	/* Block erase command sequence */
	bpi_write(word_addr, FLASH_CMD_BLOCK_ERASE);
	usleep(100);
	bpi_write(word_addr, FLASH_CMD_CONFIRM);

	if (!wait_ready(30000)) {
		printError("Block erase failed");
		return false;
	}

	/* Verify erase by reading first few words */
	if (_verbose) {
		/* Send READ_ARRAY to the erased block's address (not addr 0),
		 * because multi-bank flash requires per-bank mode commands */
		bpi_write(word_addr, FLASH_CMD_READ_ARRAY);
		usleep(100);
		char buf[128];
		snprintf(buf, sizeof(buf), "Verify erase at 0x%06x:", addr);
		printInfo(buf);
		for (int i = 0; i < 4; i++) {
			uint16_t val = bpi_read(word_addr + i);
			snprintf(buf, sizeof(buf), "  [0x%06x] = 0x%04x %s",
				addr + i*2, val, (val == 0xFFFF) ? "(OK)" : "(NOT ERASED!)");
			printInfo(buf);
		}
	}

	return true;
}

bool BPIFlash::bulk_erase()
{
	printInfo("Bulk erasing BPI flash...");

	uint32_t num_blocks = _capacity / _block_size;
	ProgressBar progress("Erasing", num_blocks, 50, _verbose > 0);

	for (uint32_t i = 0; i < num_blocks; i++) {
		uint32_t block_addr = i * _block_size;
		if (!erase_block(block_addr)) {
			progress.fail();
			return false;
		}
		progress.display(i + 1);
	}

	progress.done();
	return true;
}

bool BPIFlash::read(uint8_t *data, uint32_t addr, uint32_t len)
{
	if (_verbose)
		printInfo("Reading " + std::to_string(len) + " bytes from 0x" +
			std::to_string(addr));

	/* Ensure read array mode */
	bpi_write(0, FLASH_CMD_READ_ARRAY);
	usleep(100);

	ProgressBar progress("Reading", len, 50, _verbose > 0);

	for (uint32_t i = 0; i < len; i += 2) {
		uint32_t word_addr = (addr + i) >> 1;
		uint16_t word = bpi_read(word_addr);

		data[i] = reverseByte((word >> 8) & 0xFF);
		if (i + 1 < len)
			data[i + 1] = reverseByte(word & 0xFF);

		if ((i & 0xFFF) == 0)
			progress.display(i);
	}

	progress.done();
	return true;
}

bool BPIFlash::write(uint32_t addr, const uint8_t *data, uint32_t len)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "Writing %u bytes to BPI flash at 0x%06x", len, addr);
	printInfo(buf);

	/* Calculate blocks to erase */
	uint32_t start_block = addr / _block_size;
	uint32_t end_block = (addr + len - 1) / _block_size;

	/* Erase required blocks */
	printInfo("Erasing " + std::to_string(end_block - start_block + 1) +
		" blocks...");
	for (uint32_t block = start_block; block <= end_block; block++) {
		if (!erase_block(block * _block_size)) {
			return false;
		}
	}

	/* Program data using buffered programming (0x00E9)
	 * Sequence: Setup(0xE9) -> WordCount(N-1) -> N data words -> Confirm(0xD0)
	 */
	printInfo("Programming (buffered mode)...");
	ProgressBar progress("Writing", len, 50, _verbose > 0);

	/* MT28GU512AAA has 1KB programming regions - must program entire region at once
	 * to avoid object mode issues. Buffer size = 512 words = 1KB = one programming region.
	 */
	const uint32_t BUFFER_WORDS = 512;  /* 512 words = 1KB = one programming region */
	const uint32_t BUFFER_BYTES = BUFFER_WORDS * 2;

	uint32_t last_block = 0xFFFFFFFF;  /* Track which block is unlocked */

	uint32_t offset = 0;
	while (offset < len) {
		uint32_t byte_addr = addr + offset;
		uint32_t word_addr = byte_addr >> 1;

		/* Calculate block address (word address of block start) */
		uint32_t block_word_addr = (byte_addr / _block_size) * (_block_size >> 1);

		/* Unlock only when entering a new block */
		uint32_t current_block = byte_addr / _block_size;
		if (current_block != last_block) {
			unlock_block(block_word_addr);
			last_block = current_block;
		}

		/* Calculate how many words to write in this buffer */
		uint32_t remaining_bytes = len - offset;
		uint32_t chunk_bytes = (remaining_bytes > BUFFER_BYTES) ? BUFFER_BYTES : remaining_bytes;
		uint32_t chunk_words = (chunk_bytes + 1) / 2;

		/* Don't cross block boundaries */
		uint32_t bytes_to_block_end = _block_size - (byte_addr % _block_size);
		if (chunk_bytes > bytes_to_block_end)
			chunk_bytes = bytes_to_block_end;
		chunk_words = (chunk_bytes + 1) / 2;

		if (_verbose > 1) {
			char buf[128];
			snprintf(buf, sizeof(buf), "Buffered write: addr=0x%06x, words=%u, block=0x%06x",
				byte_addr, chunk_words, block_word_addr << 1);
			printInfo(buf);
		}

		/* Write data words for BPI x16 boot.
		 * Two transformations (same as Vivado write_cfgmem -interface BPIx16):
		 *  1. Bit reversal within each byte: FPGA D00=MSBit, flash DQ[0]=LSBit
		 *  2. Byte swap: first bitstream byte → upper flash byte D[15:8]
		 */
		std::vector<uint16_t> word_buf(chunk_words);
		for (uint32_t w = 0; w < chunk_words; w++) {
			uint32_t data_offset = offset + w * 2;
			uint8_t b0 = data[data_offset];
			uint8_t b1 = 0xFF;  /* pad with 0xFF if odd length */
			if (data_offset + 1 < len)
				b1 = data[data_offset + 1];
			word_buf[w] = (reverseByte(b0) << 8) | reverseByte(b1);
		}

		/* Buffered Program Setup - sent to block/colony base address */
		bpi_write(0, FLASH_CMD_CLEAR_STATUS);
		bpi_write(block_word_addr, FLASH_CMD_BUFFERED_PRG);
		bpi_write(block_word_addr, chunk_words - 1);

		if (_has_burst) {
			bpi_burst_write(word_addr, word_buf.data(), chunk_words);
		} else {
			/* Software-only fallback: one IR, no per-word flush */
			uint8_t user1[] = {0x02};
			_jtag->shiftIR(user1, NULL, _irlen);
			for (uint32_t w = 0; w < chunk_words; w++) {
				bpi_write_no_flush(word_addr + w, word_buf[w]);
			}
			_jtag->flush();
		}

		/* Confirm - sent to block address */
		bpi_write(block_word_addr, FLASH_CMD_CONFIRM);

		/* Wait for program to complete */
		if (!wait_ready(5000)) {
			char buf[64];
			snprintf(buf, sizeof(buf), "Buffered program failed at address 0x%06x", byte_addr);
			printError(buf);
			progress.fail();
			return false;
		}

		offset += chunk_words * 2;

		if ((offset & 0xFFF) == 0 || offset >= len)
			progress.display(offset);
	}

	/* Return to read array mode */
	bpi_write(0, FLASH_CMD_READ_ARRAY);

	progress.done();

	/* Verify first 32 words */
	printInfo("Verifying first 32 words...");
	usleep(1000);
	bpi_write(0, FLASH_CMD_READ_ARRAY);
	usleep(100);

	bool verify_ok = true;
	for (uint32_t i = 0; i < 64 && i < len; i += 2) {
		uint8_t b0 = data[i];
		uint8_t b1 = 0xFF;
		if (i + 1 < len)
			b1 = data[i + 1];
		uint16_t expected = (reverseByte(b0) << 8) | reverseByte(b1);

		uint16_t actual = bpi_read(i >> 1);
		if (actual != expected) {
			char buf[128];
			snprintf(buf, sizeof(buf), "Verify FAIL at 0x%04x: expected 0x%04x, got 0x%04x",
				i, expected, actual);
			printError(buf);
			verify_ok = false;
		} else if (_verbose) {
			char buf[128];
			snprintf(buf, sizeof(buf), "Verify OK at 0x%04x: 0x%04x", i, actual);
			printInfo(buf);
		}
	}

	if (verify_ok) {
		printInfo("Verification passed for first 32 words");
	} else {
		printError("Verification FAILED!");
	}

	printInfo("BPI flash programming complete");
	return true;
}
