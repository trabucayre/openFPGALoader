// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2021 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <hidapi.h>
#include <libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "display.hpp"

#include "cmsisDAP.hpp"

using namespace std;

#define DAP_JTAG_SEQ_TDO_CAPTURE  (1 << 7)
#define DAP_JTAG_SEQ_TMS_SHIFT(x) ((x & 0x01) << 6)
#define DAP_JTAG_SEQ_NB_TCK(x)    (x & 0x3f)

enum datalink_cmd {
	DAP_INFO          = 0x00,
	DAP_HOSTSTATUS    = 0x01,
	DAP_CONNECT       = 0x02,  // Connect to device and select mode
	DAP_DISCONNECT    = 0x03,  // Disconnect to device
	DAP_RESETTARGET   = 0x0A,  // reset the target
	DAP_SWJ_CLK       = 0x11,  // Select maximum frequency
	DAP_SWJ_SEQUENCE  = 0x12,  // Generate TMS sequence
	DAP_JTAG_SEQUENCE = 0x14   // Generate TMS, TDI and capture TDO Sequence
};

enum cmsisdap_connect_mode {
	DAP_CONNECT_DFLT = 0x00,  // Default mode: no configuration
	DAP_CONNECT_SWD  = 0x01,  // Serial Wire Debug mode
	DAP_CONNECT_JTAG = 0x02   // 4/5 pins JTAG mode
};

enum cmsisdap_info_id {
	INFO_ID_VID                = 0x01,  // Get the Vendor ID (string).
	INFO_ID_PID                = 0x02,  // Get the Product ID (string).
	INFO_ID_SERNUM             = 0x03,  // Get the Serial Number (string).
	INFO_ID_FWVERS             = 0x04,  // Get the CMSIS-DAP Firmware
							     	    // Version (string).
	INFO_ID_TARGET_DEV_VENDOR  = 0x05,  // Get the Target Device Vendor (string).
	INFO_ID_TARGET_DEV_NAME    = 0x06,  // Get the Target Device Name (string).
	INFO_ID_HWCAP              = 0xF0,  // Get information about the
							     	    // Capabilities (BYTE) of the Debug Unit
	INFO_ID_SWO_TEST_TIM_PARAM = 0xF1,  // Get the Test Domain Timer parameter
	INFO_ID_SWO_TRACE_BUF_SIZE = 0xFD,  // Get the SWO Trace Buffer Size (WORD).
	INFO_ID_MAX_PKT_CNT        = 0xFE,  // Get the maximum Packet Count (BYTE).
	INFO_ID_MAX_PKT_SZ         = 0xFF   // Get the maximum Packet Size (SHORT).
};

static map<uint8_t, string> cmsisdap_info_id_str = {
	{INFO_ID_VID,                "VID"},
	{INFO_ID_PID,                "PID"},
	{INFO_ID_SERNUM,             "serial number"},
	{INFO_ID_FWVERS,             "firmware version"},

	{INFO_ID_TARGET_DEV_VENDOR,  "target device vendor"},
	{INFO_ID_TARGET_DEV_NAME,    "target device name"},
	{INFO_ID_HWCAP,              "hardware capabilities"},

	{INFO_ID_SWO_TEST_TIM_PARAM, "test domain timer parameter"},
	{INFO_ID_SWO_TRACE_BUF_SIZE, "SWO trace buffer size"},
	{INFO_ID_MAX_PKT_CNT,        "max packet cnt"},
	{INFO_ID_MAX_PKT_SZ,         "max packet size"}
};

enum cmsisdap_info_type {
	DAPLINK_INFO_STRING = 0x00,
	DAPLINK_INFO_BYTE,
	DAPLINK_INFO_SHORT,
	DAPLINK_INFO_WORD
};

enum cmsisdap_status {
	DAP_OK    = 0x00,
	DAP_ERROR = 0xff
};

CmsisDAP::CmsisDAP(const cable_t &cable, int index, uint8_t verbose):_verbose(verbose),
		_device_idx(0),  _vid(cable.vid), _pid(cable.pid),
		_serial_number(L""), _dev(NULL), _num_tms(0), _is_connect(false)
{
	std::vector<struct hid_device_info *> dev_found;
	_ll_buffer = (unsigned char *)malloc(sizeof(unsigned char) * 65);
	if (!_ll_buffer)
		std::runtime_error("internal buffer allocation failed");
	_buffer = _ll_buffer+2;

	/* only hid support */
	struct hid_device_info *devs, *cur_dev;

	if (hid_init() != 0) {
		throw std::runtime_error("hidapi init failed");
	}

	/* search for HID compatible devices
	 * if vid/pid are 0 this function return all;
	 * if vid/pid are >0 only one (or 0) device returned
	 */
	devs = hid_enumerate(cable.vid, cable.pid);

	for (cur_dev = devs; NULL != cur_dev; cur_dev = cur_dev->next) {
		dev_found.push_back(cur_dev);
	}

	/* no devices: stop */
	if (dev_found.empty()) {
		hid_exit();
		throw std::runtime_error("No device found");
	}
	/* more than one device: can't continue without more information */
	if (dev_found.size() > 1 && index == -1) {
		hid_exit();
		throw std::runtime_error(
				"Error: more than one device. Please provides VID/PID or cable-index");
	}

	/* if index check for if interface exist */
	if (index != -1) {
		bool found = false;
		for (size_t i = 0; i < dev_found.size(); i++) {
			if (dev_found[i]->interface_number == index) {
				found = true;
				_device_idx = i;
				break;
			}
		}
		if (!found) {
			hid_exit();
			throw std::runtime_error(
				"Error: no compatible interface with index " + std::to_string(_device_idx));
		}
	}

	printInfo("Found " + std::to_string(dev_found.size()) + " compatible device:");
	for (size_t i = 0; i < dev_found.size(); i++) {
		char val[256];
		snprintf(val, sizeof(val), "\t0x%04x 0x%04x 0x%d %ls",
				dev_found[i]->vendor_id,
				dev_found[i]->product_id,
				dev_found[i]->interface_number,
				dev_found[i]->product_string);
		printInfo(val);
	}

	/* store params about device to use */
	_vid = dev_found[_device_idx]->vendor_id;
	_pid = dev_found[_device_idx]->product_id;
	if (dev_found[_device_idx]->serial_number != NULL)
		_serial_number = wstring(dev_found[_device_idx]->serial_number);
	/* open the device */
	_dev = hid_open_path(dev_found[_device_idx]->path);
	if (!_dev) {
		throw std::runtime_error(
				std::string("Couldn't open device. Check permissions for ") + dev_found[_device_idx]->path);
	}
	/* cleanup enumeration */
	hid_free_enumeration(devs);

	if (verbose) {
		display_info(INFO_ID_VID               , DAPLINK_INFO_STRING);
		display_info(INFO_ID_PID               , DAPLINK_INFO_STRING);
		display_info(INFO_ID_SERNUM            , DAPLINK_INFO_STRING);
		display_info(INFO_ID_FWVERS            , DAPLINK_INFO_STRING);
		display_info(INFO_ID_TARGET_DEV_VENDOR , DAPLINK_INFO_STRING);
		display_info(INFO_ID_TARGET_DEV_NAME   , DAPLINK_INFO_STRING);
		display_info(INFO_ID_HWCAP             , DAPLINK_INFO_BYTE);
		display_info(INFO_ID_SWO_TRACE_BUF_SIZE, DAPLINK_INFO_WORD);
		display_info(INFO_ID_MAX_PKT_CNT       , DAPLINK_INFO_BYTE);
		display_info(INFO_ID_MAX_PKT_SZ        , DAPLINK_INFO_SHORT);
	}

	/* read device capabilities -> check if it's JTAG compatible
	 * 0 -> info
	 * 1 -> len (1: info0, 2: info0, info1)
	 * Available transfer protocols to target:
    	Info0 - Bit 0: 1 = SWD Serial Wire Debug communication is implemented
                       0 = SWD Commands not implemented
        Info0 - Bit 1: 1 = JTAG communication is implemented
                       0 = JTAG Commands not implemented
       Serial Wire Trace (SWO) support:
        Info0 - Bit 2: 1 = SWO UART - UART Serial Wire Output is implemented
                       0 = not implemented
        Info0 - Bit 3: 1 = SWO Manchester - Manchester Serial Wire Output is implemented
                       0 = not implemented
       Command extensions for transfer protocol:
        Info0 - Bit 4: 1 = Atomic Commands - Atomic Commands support is implemented
                       0 = Atomic Commands not implemented
       Time synchronisation via Test Domain Timer:
        Info0 - Bit 5: 1 = Test Domain Timer - debug unit support for Test Domain Timer is implemented
                       0 = not implemented
       SWO Streaming Trace support:
        Info0 - Bit 6: 1 = SWO Streaming Trace is implemented (0 = not implemented).
	*/
	memset(_buffer, 0, 63);
	int res = read_info(INFO_ID_HWCAP, _buffer, 63);
	if (res < 0) {
		hid_close(_dev);
		hid_exit();
		char t[256];
		snprintf(t, sizeof(t), "Error %d for command %d\n", res, INFO_ID_HWCAP);
		throw std::runtime_error(t);
	}

	if (verbose)
		printf("Hardware cap %02x %02x %02x\n", _buffer[0], _buffer[1], _buffer[2]);
	if (!(_buffer[2] & (1 << 1))) {
		hid_close(_dev);
		hid_exit();
		throw std::runtime_error("JTAG is not supported by the probe");
	}

	/* send connect */
	if (dapConnect() != 1) {
		hid_close(_dev);
		hid_exit();
		throw std::runtime_error("DAP connection in JTAG mode failed");
	}
}

CmsisDAP::~CmsisDAP()
{
	/* disconnect and close device
	 * and free context
	 */
	if (_is_connect)
		dapDisconnect();
	if (_dev)
		hid_close(_dev);
	hid_exit();

	if (_ll_buffer)
		free(_ll_buffer);
}

/* send connect instruction (0x02) to switch
 * in JTAG mode (0x02)
 */
int CmsisDAP::dapConnect()
{
	if (_is_connect)
		return 1;
	_ll_buffer[1] = DAP_CONNECT;
	_ll_buffer[2] = DAP_CONNECT_JTAG;
	uint8_t response[2];
	int ret = xfer(2, response, 2);
	if (ret <= 0)
		return ret;
	if (response[0] != DAP_CONNECT || response[1] != DAP_CONNECT_JTAG)
		return 0;
	_is_connect = true;
	return 1;
}

/* send disconnect instruction (0x03)
 */
int CmsisDAP::dapDisconnect()
{
	if (!_is_connect)
		return 1;
	_ll_buffer[1] = DAP_DISCONNECT;
	int ret = xfer(1, NULL, 0);
	if (ret <= 0)
		return ret;
	_is_connect = false;
	return 1;
}

/* send resetTarget instruction (0x0A)
 */
int CmsisDAP::dapResetTarget()
{
	_ll_buffer[1] = DAP_RESETTARGET;
	int ret = xfer(1, NULL, 0);
	if (ret <= 0)
		return ret;
	return 1;
}

/* configure clk using instruction 0x11 followed
 * by 32bits (LSB first) frequency in Hz
 */
int CmsisDAP::setClkFreq(uint32_t clkHZ)
{
	_clkHZ = clkHZ;
	_buffer[3] = (uint8_t)(_clkHZ >> 24);
	_buffer[2] = (uint8_t)(_clkHZ >> 16);
	_buffer[1] = (uint8_t)(_clkHZ >>  8);
	_buffer[0] = (uint8_t)(_clkHZ >>  0);
	if (xfer(DAP_SWJ_CLK, 4, NULL, 0) <= 0) {
		printError("Failed to configure clk frequency");
		return -1;
	} else if (_verbose) {
		printSuccess("clk frequency conf done");
	}
	return 0;
}

/* fill buffer with one or more tms state
 * if tms count == 256 (max allowed by CMSIS-DAP)
 * flush the buffer
 * tms states are written only if max or if flush_buffer set
 */
int CmsisDAP::writeTMS(uint8_t *tms, uint32_t len, bool flush_buffer)
{
	/* nothing to send
	 * check if the buffer must be flushed
	 */
	if (len == 0) {
		if (flush_buffer)
			return flush();
		return 0;
	}

	/* fill buffer with tms states */
	for (uint32_t pos = 0; pos < len; pos++) {
		/* max tms states allowed by CMSIS-DAP -> flush */
		if (_num_tms == 256) {
			if (flush() < 0) {
				printError("Flush error");
				return -1;
			}
		}

		if (tms[pos >> 3] & (1 << (pos & 0x07)))
			_buffer[(_num_tms >> 3)+1] |= (1 << (_num_tms & 0x07));
		else
			_buffer[(_num_tms >> 3)+1] &= ~(1 << (_num_tms & 0x07));
		_num_tms++;
	}

	/* flush is it's asked or if the buffer is full */
	if (flush_buffer || _num_tms == 256)
		return flush();
	return len;
}

/* 0x14 + number of sequence + seq1 details + tdi + seq2 details + tdi + ...
 */
int CmsisDAP::writeJtagSequence(uint8_t tms, uint8_t *tx, uint8_t *rx,
		uint32_t len, bool end)
{
	int ret;
	int real_len = len - (end ? 1 : 0);  // full xfer size according to end
	uint8_t *rx_ptr = rx, *tx_ptr = tx;  // rd & wr ptr
	int xfer_byte_len, xfer_bit_len;  // size of one sequence
                                              // in byte and bit
	int byte_to_read = 0;  // for rd operation number of read in one xfer
	/* constant part of all sequences info byte */
	uint8_t seq_info_base = ((rx) ? DAP_JTAG_SEQ_TDO_CAPTURE : 0) |
								  DAP_JTAG_SEQ_TMS_SHIFT(tms);
	int seq_num = 0;  // count number of sequence in buffer
	int pos = 1;      // 0: num of sequence, 1: seq1 detail
	int xfer_rest = real_len;  // main loop

	flush();  // force TMS flush to free _buffer

	while (xfer_rest > 0) {
		if (xfer_rest >= 64) {  // fully fill one sequence
			xfer_byte_len = 8;
			xfer_bit_len = 64;
		} else {  // fill one sequence with rest
			xfer_byte_len = (xfer_rest + 7) / 8;
			xfer_bit_len = xfer_rest;
		}

		/* buffer is 65bits with
		 * [0]   : hid
		 * [1]   : cmsisdap operation
		 * [2]   : number of sequence
		 * [64:3]: sequence with
		 *    [n]        : sequence infos
		 *    [n+m+1:n+1]: data
		 * So only 62 bits are available to send sequences
		 * and 64bits (full sequence) mean 8bits
		 * => one sequence == 9Bytes and 9*7 == 63
		 * then we have 6 * 8 fully filled sequence + one up to 56bits
		 */
		if (xfer_byte_len + 1 + pos > 63) {
			xfer_byte_len = 63 - pos - 1;  // number of free bytes
			xfer_bit_len = xfer_byte_len * 8;
		}

		/* update sequence info with number of bit */
		_buffer[pos++] = seq_info_base |
			DAP_JTAG_SEQ_NB_TCK((xfer_bit_len == 64?0:xfer_bit_len));
		if (tx) {  // use tx only if not NULL
			memcpy(&_buffer[pos], (unsigned char *)tx_ptr, xfer_byte_len);
			tx_ptr += xfer_byte_len;
		}
		xfer_rest -= xfer_bit_len;  // update remaining number of bit
		seq_num++;  // update sequence counter
		pos += xfer_byte_len;  // update buffer position
		byte_to_read += xfer_byte_len;  // update read lenght

		/* when it's the last sequence or
		 * buffer is fully filled
		 * => flush
		 * if it's the last sequence and end is true, don't do anything
		 * here -> see bellow
		 */
		if ((!end && xfer_rest == 0) || seq_num == 7) {
			_buffer[0] = seq_num;  // set number of sequences
			ret = xfer(DAP_JTAG_SEQUENCE, pos,
					(rx) ? rx_ptr: NULL, byte_to_read);
			if (ret <= 0) {
				printError("writeTDI: failed to send sequence");
				return ret;
			}
			if (rx)  // if read: move pointer to the next position
				rx_ptr += byte_to_read;

			/* reset all variables */
			pos = 1;
			seq_num = 0;  // no sequence
			byte_to_read = 0;
		}
	}

	/* add a dedicated sequence to the last bit
	 * with !tms in info
	 * used with writeTDI to change TMS state at the same time
	 * as last bit to send
	 */
	if (end) {
		byte_to_read++;   // residual (or 0) from previous iter + 1 Byte
		uint8_t val[byte_to_read];
		_buffer[0] = seq_num + 1;
		_buffer[pos++] = ((rx) ? DAP_JTAG_SEQ_TDO_CAPTURE : 0) |
								  DAP_JTAG_SEQ_TMS_SHIFT(0x01&(!tms)) |
								  DAP_JTAG_SEQ_NB_TCK(1);
		_buffer[pos++] = (tx[(real_len) >> 3] & (1 << (real_len & 0x07))) ? 1 : 0;
		ret = xfer(DAP_JTAG_SEQUENCE, pos, (rx) ? val: NULL, byte_to_read);
		if (ret <= 0) {
			printError("writeTDI: failed to send last sequence");
			return ret;
		}
		if (rx) {
			memcpy(rx_ptr, val, byte_to_read-1);
			if (val[byte_to_read-1] & 0x01)
				rx[real_len >> 3] |= 1 << ((real_len) & 0x07);
			else
				rx[real_len >> 3] &= ~(1 << ((real_len) & 0x07));
		}
	}

	return len;
}

/* send TDI by filling jtag sequence
 * tx buffer is considered to be correctly aligned (LSB first)
 */
int CmsisDAP::writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end)
{
	return writeJtagSequence(0, tx, rx, len, end);
}

/* unlike TMS the is no dedicated instruction to toggle clk
 * so fill a buffer with tdi state and call same method as writeTDI
 */
int CmsisDAP::toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len)
{
	const int byte_len = (clk_len + 7) / 8;
	uint8_t tx[byte_len];
	memset(tx, (tdi) ? 0xff : 0x00, byte_len);
	/* use false as last param to maintain tms in the current state */
	return writeJtagSequence(tms, tx, NULL, clk_len, false);
}

/* flush buffer filled with TMS states
 */
int CmsisDAP::flush()
{
	int ret;
	if (_num_tms == 0)
		return 0;
	_buffer[0] = (uint8_t)(_num_tms & 0xff);
	//                                                         +1 (buff size)
	ret = xfer(DAP_SWJ_SEQUENCE, ((_num_tms + 7) / 8) + 1, NULL, 0);
	_num_tms = 0;
	return ret;
}

/* fill low level buffer with
 * 0: 0] -> hid
 * 1: instruction
 * 2->n: message
 * check if response contains instructions + status (with status == 0x00
 * if read copy 2-n
 */
int CmsisDAP::xfer(uint8_t instruction, int tx_len,
		uint8_t *rx_buff, int rx_len)
{
	(void)tx_len;

	_ll_buffer[0] = 0;
	_ll_buffer[1] = instruction;

	int ret = hid_write(_dev, _ll_buffer, 65);
	if (ret == -1) {
		printf("Error\n");
		return ret;
	}

	ret = hid_read_timeout(_dev, _ll_buffer, 65, 1000);
	if (ret <= 0) {
		if (ret == 0)
			printError("Error timeout\n");
		else if (ret == -1)
			printError("Error comm\n");
		return ret;
	}
	if (_ll_buffer[0] != instruction && _ll_buffer[1] != DAP_OK) {
		printf("Error: command error");
		return -1;
	}

	if (rx_buff) {
		memcpy(rx_buff, _buffer, rx_len);
	}

	return ret;
}

/* same as previous method but
 * 1/ instruction is already in tx_buff
 * 2/ no check is done to device answer
 */
int CmsisDAP::xfer(int tx_len, uint8_t *rx_buff, int rx_len)
{
	(void)tx_len;

	_ll_buffer[0] = 0;

	int ret = hid_write(_dev, _ll_buffer, 65);
	if (ret == -1) {
		printf("Error\n");
		return ret;
	}

	ret = hid_read_timeout(_dev, _ll_buffer, 65, 1000);
	if (ret <= 0) {
		if (ret == 0)
			printf("Error timeout\n");
		else if (ret == -1)
			printf("Error comm\n");
		return ret;
	}
	if (rx_len)
		memcpy(rx_buff, _ll_buffer, rx_len);


	return ret;
}

int CmsisDAP::read_info(uint8_t info, uint8_t *rd_info, int max_len)
{
	_ll_buffer[1] = DAP_INFO;
	_ll_buffer[2] = info;
	int ret = xfer(2, rd_info, max_len);
	if (ret <= 0)
		return ret;
	else
		return static_cast<int>(rd_info[1]);
}

void CmsisDAP::display_info(uint8_t info, uint8_t type)
{
	uint8_t buffer[65];
	memset(buffer, 0, 65);
	int ret = read_info(info, buffer, 64);
	if (ret < 0) {
		printf("received error %d for command %d\n", ret, info);
		return;
	}

	if (ret == 0) {
		char val[256];
		if (info == INFO_ID_VID) {
			snprintf(val, sizeof(val), "\t%s: %04x",
					cmsisdap_info_id_str[info].c_str(), _vid);
		} else if (info == INFO_ID_PID) {
			snprintf(val, sizeof(val), "\t%s: %04x",
					cmsisdap_info_id_str[info].c_str(), _pid);
		} else if (info == INFO_ID_SERNUM) {
			if (!_serial_number.empty()) {
				snprintf(val, sizeof(val), "\t%s: %ls",
						cmsisdap_info_id_str[info].c_str(),
						_serial_number.c_str());
			} else {
				printError("\t" + cmsisdap_info_id_str[info] + " : NA");
				return;
			}
		} else if (info == INFO_ID_TARGET_DEV_NAME || \
				info == INFO_ID_TARGET_DEV_VENDOR) {
			/* nothing */
			return;
		} else {
			printError("\t" + cmsisdap_info_id_str[info] + " : NA");
			return;
		}
		printInfo(val);

		return;
	}

	bool fail = true;

	if (type == DAPLINK_INFO_BYTE && ret != 1) {
		printf("Error: Waiting for 1Byte received %d\n", ret);
	} else if (type == DAPLINK_INFO_SHORT && ret != 2) {
		printf("Error: Waiting for 2Byte received %d\n", ret);
	} else if (type == DAPLINK_INFO_WORD && ret != 4) {
		printf("Error: Waiting for 2Byte received %d\n", ret);
	} else {
		fail = false;
	}

	if (fail == true) {
		for (int i = 0; i < 64; i++) {
			printf("%02x ", buffer[i]);
		}
		printf("\n");
		return;
	}

	printInfo("\t" + cmsisdap_info_id_str[info] + " : ", false);

	if (type == DAPLINK_INFO_BYTE) {
		printf("%02x\n", buffer[2]);
	} else if (type == DAPLINK_INFO_SHORT) {
		uint16_t val = (buffer[3] << 8) | buffer[2];
		printf("%d\n", val);
	} else if (type == DAPLINK_INFO_WORD) {
		uint32_t val = (buffer[5] << 24) | (buffer[4] << 16) |
						(buffer[3] << 8) | buffer[2];
		printf("%u\n", val);
	} else {
		char val[ret];
		memcpy(val, &buffer[2], ret);
		printf("%s\n", val);
	}
}
