// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2021 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <cstdint>
#include <cstring>
#ifdef ENABLE_CMSISDAP_V1
#include <hidapi.h>
#endif
#ifdef ENABLE_CMSISDAP_V2
#include <libusb.h>
#endif
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

static std::map<uint8_t, std::string> cmsisdap_info_id_str = {
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

enum cmsisdap_backend_type {
	BACKEND_NONE = -1,
	BACKEND_HID = 0,
	BACKEND_USBBULK = 1,
};

CmsisDAP::CmsisDAP(const cable_t &cable, int index, uint32_t clkHZ, int8_t verbose):_verbose(verbose>0),
		_device_idx(0),  _vid(cable.vid), _pid(cable.pid), _serial_number(L""),
#ifdef ENABLE_CMSISDAP_V1
		_hid_dev(NULL),
#endif
#ifdef ENABLE_CMSISDAP_V2
		_usb_dev(NULL), _ctx(NULL),
#endif
		_packet_size(0),
		_ep_in(0), _ep_out(0), _num_tms(0), _is_connect(false), _backend(BACKEND_NONE)
{
	_ll_buffer = (unsigned char *)malloc(sizeof(unsigned char) * 1024);
	if (!_ll_buffer)
		throw std::runtime_error("internal buffer allocation failed");
	_buffer = _ll_buffer+2;
	char t[256];
#ifdef ENABLE_CMSISDAP_V2
	try {
		_backend = BACKEND_USBBULK;
		initWithBulk(cable, verbose);
	} catch (std::runtime_error const& e) {
		snprintf(t, sizeof(t), "try USB bulk init but failed with: %s", e.what());
		printInfo(t);
		_backend = BACKEND_NONE;
	}
#endif
#ifdef ENABLE_CMSISDAP_V1
	if (_backend == BACKEND_NONE) {
		try {
			_backend = BACKEND_HID;
			initWithHID(cable, index, verbose);
		} catch (std::runtime_error const& e) {
			snprintf(t, sizeof(t), "try HID init but failed with: %s", e.what());
			printInfo(t);
			_backend = BACKEND_NONE;
		}
	}
#endif
	if (_backend == BACKEND_NONE)
		throw std::runtime_error("Error: no USB backend available");
	if (clkHZ > 0)
		setClkFreq(clkHZ);
}

CmsisDAP::~CmsisDAP()
{
	/* disconnect and close device
	 * and free context
	 */
	switch(_backend){
#ifdef ENABLE_CMSISDAP_V1
		case BACKEND_HID:
			if (_is_connect)
				dapDisconnect();
			if (_hid_dev)
				hid_close(_hid_dev);
			hid_exit();
			break;
#endif
#ifdef ENABLE_CMSISDAP_V2
		case BACKEND_USBBULK:
			libusb_close(_usb_dev);
			libusb_exit(_ctx);
			break;
#endif
		default:
			break;
	}
	if (_ll_buffer)
		free(_ll_buffer);
}

#ifdef ENABLE_CMSISDAP_V1
void CmsisDAP::initWithHID(const cable_t &cable, int index, int8_t verbose){
		std::vector<struct hid_device_info *> dev_found;

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
	_vendor = dev_found[_device_idx]->manufacturer_string;
	_product_name = dev_found[_device_idx]->product_string;
	if (dev_found[_device_idx]->serial_number != NULL)
		_serial_number = std::wstring(dev_found[_device_idx]->serial_number);
	/* open the device */
	_hid_dev = hid_open_path(dev_found[_device_idx]->path);
	if (!_hid_dev) {
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
		hid_close(_hid_dev);
		hid_exit();
		char t[256];
		snprintf(t, sizeof(t), "Error %d for command %d\n", res, INFO_ID_HWCAP);
		throw std::runtime_error(t);
	}

	if (verbose)
		printf("Hardware cap %02x %02x %02x\n", _buffer[0], _buffer[1], _buffer[2]);
	if (!(_buffer[2] & (1 << 1))) {
		hid_close(_hid_dev);
		hid_exit();
		throw std::runtime_error("JTAG is not supported by the probe");
	}

	/* send connect */
	if (dapConnect() != 1) {
		hid_close(_hid_dev);
		hid_exit();
		throw std::runtime_error("DAP connection in JTAG mode failed");
	}

	printInfo("HID init successful");
}
#endif

#ifdef ENABLE_CMSISDAP_V2
void CmsisDAP::initWithBulk(const cable_t &cable, int8_t verbose)
{
	if (libusb_init(&_ctx) != 0)
		throw std::runtime_error("libusb init failed");

	std::vector<cmsis_dap_v2_dev_t> devices = findCmsisDapDevices(cable.vid, cable.pid);

	/* Check if one and only device is found
	 * otherwise fails when no devices found or more than one present
	 */
	if (devices.empty()) {
		libusb_exit(_ctx);
		throw std::runtime_error("Error: failed to find compatible CMSIS-DAP device");
	}
	if (devices.size() > 1) {
		for (auto &d : devices)
			libusb_close(d.handle);
		libusb_exit(_ctx);
		throw std::runtime_error(
			"Error: more than one device. Please provide VID/PID");
	}

	cmsis_dap_v2_dev_t &dev = devices[0];
	_usb_dev = dev.handle;
	_vid = dev.vid;
	_pid = dev.pid;
	_serial_number = dev.serial;
	_vendor = dev.vendor;
	_product_name = dev.product;

	printInfo("Found 1 compatible device:");
	char val[256];
	snprintf(val, sizeof(val), "\t0x%04x 0x%04x", _vid, _pid);
	printInfo(val);

	int current_config;
	if (libusb_get_configuration(_usb_dev, &current_config) != 0) {
		libusb_close(_usb_dev);
		libusb_exit(_ctx);
		throw std::runtime_error("could not find current configuration");
	}

	if (dev.config_num != current_config) {
		int ret = libusb_set_configuration(_usb_dev, dev.config_num);
		if (ret != 0 && ret != LIBUSB_ERROR_NOT_SUPPORTED) {
			libusb_close(_usb_dev);
			libusb_exit(_ctx);
			throw std::runtime_error("could not set current configuration");
		}
	}

	if (libusb_claim_interface(_usb_dev, dev.interface_num) != 0) {
		libusb_close(_usb_dev);
		libusb_exit(_ctx);
		throw std::runtime_error("could not claim interface");
	}

	_packet_size = dev.packet_size;
	_ep_out = dev.ep_out;
	_ep_in = dev.ep_in;

	if (!libusb_dev_mem_alloc(_usb_dev, _packet_size)) {
		libusb_close(_usb_dev);
		libusb_exit(_ctx);
		throw std::runtime_error("failed to alloc DMA memory for device");
	}

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

	memset(_buffer, 0, 63);
	int res = read_info(INFO_ID_HWCAP, _buffer, 63);
	if (res < 0) {
		libusb_close(_usb_dev);
		libusb_exit(_ctx);
		char t[256];
		snprintf(t, sizeof(t), "Error %d for command %d\n", res, INFO_ID_HWCAP);
		throw std::runtime_error(t);
	}

	if (verbose)
		printf("Hardware cap %02x %02x %02x\n", _buffer[0], _buffer[1], _buffer[2]);

	if (!(_buffer[2] & (1 << 1))) {
		libusb_close(_usb_dev);
		libusb_exit(_ctx);
		throw std::runtime_error("JTAG is not supported by the probe");
	}

	if (dapConnect() != 1) {
		libusb_close(_usb_dev);
		libusb_exit(_ctx);
		throw std::runtime_error("DAP connection in JTAG mode failed");
	}

	printInfo("USB bulk init successful");
}

/* Enumerate all USB devices and return those that expose a CMSIS-DAP v2
 * bulk interface.
 * Enumerate may be filtered when vid and pid are non-zero.
 * Each returned entry carries an already-open libusb handle; the caller
 * is responsible for closing handles it does not use.
 */
std::vector<CmsisDAP::cmsis_dap_v2_dev_t>
CmsisDAP::findCmsisDapDevices(uint16_t vid, uint16_t pid)
{
	std::vector<cmsis_dap_v2_dev_t> result;
	struct libusb_device **devs;

	int devs_len = libusb_get_device_list(_ctx, &devs);
	if (devs_len <= 0)
		return result;

	for (int i = 0; i < devs_len; i++) {
		struct libusb_device *dev = devs[i];
		struct libusb_device_descriptor dev_desc;

		if (libusb_get_device_descriptor(dev, &dev_desc) != 0) {
			printWarn("could not get device descriptor for device " + std::to_string(i));
			continue;
		}

		/* optional VID/PID filter: if either is non-zero, both must match */
		if ((vid != 0 || pid != 0) &&
			(dev_desc.idVendor != vid || dev_desc.idProduct != pid))
			continue;

		libusb_device_handle *handle = NULL;
		if (libusb_open(dev, &handle) != 0 || !handle)
			continue;

		cmsis_dap_v2_dev_t candidate = {};
		candidate.handle = handle;
		candidate.vid = dev_desc.idVendor;
		candidate.pid = dev_desc.idProduct;

		char str[256];

		memset(str, 0, sizeof(str));
		if (libusb_get_string_descriptor_ascii(handle, dev_desc.iSerialNumber,
				(uint8_t *)str, sizeof(str)) >= 0)
			candidate.serial = std::wstring(str, str + std::strlen(str));

		memset(str, 0, sizeof(str));
		if (libusb_get_string_descriptor_ascii(handle, dev_desc.iManufacturer,
				(uint8_t *)str, sizeof(str)) >= 0)
			candidate.vendor = std::wstring(str, str + std::strlen(str));

		memset(str, 0, sizeof(str));
		if (libusb_get_string_descriptor_ascii(handle, dev_desc.iProduct,
				(uint8_t *)str, sizeof(str)) >= 0)
			candidate.product = std::wstring(str, str + std::strlen(str));

		bool intf_found = false;
		for (int cfg = 0; cfg < dev_desc.bNumConfigurations && !intf_found; cfg++) {
			struct libusb_config_descriptor *cfg_desc;
			if (libusb_get_config_descriptor(dev, cfg, &cfg_desc) != 0) {
				char t[256];
				snprintf(t, sizeof(t), "could not get configuration descriptor %d "
						 "for device 0x%04x:0x%04x", cfg, candidate.vid, candidate.pid);
				printError(t);
				continue;
			}

			for (int intf = 0; intf < cfg_desc->bNumInterfaces && !intf_found; intf++) {
				const struct libusb_interface_descriptor *intf_desc =
					&cfg_desc->interface[intf].altsetting[0];

				/* require two bulk endpoints: [0] OUT, [1] IN */
				if (intf_desc->bNumEndpoints < 2 ||
					(intf_desc->endpoint[0].bmAttributes & 3) != LIBUSB_TRANSFER_TYPE_BULK  ||
					(intf_desc->endpoint[0].bEndpointAddress & 0x80) != LIBUSB_ENDPOINT_OUT ||
					(intf_desc->endpoint[1].bmAttributes & 3) != LIBUSB_TRANSFER_TYPE_BULK  ||
					(intf_desc->endpoint[1].bEndpointAddress & 0x80) != LIBUSB_ENDPOINT_IN)
					continue;

				/* check whether the interface string contains "CMSIS-DAP" */
				bool intf_str_valid = false;
				if (intf_desc->iInterface != 0) {
					char intf_str[256] = {0};
					if (libusb_get_string_descriptor_ascii(handle, intf_desc->iInterface,
							(uint8_t *)intf_str, sizeof(intf_str)) >= 0 &&
						std::strstr(intf_str, "CMSIS-DAP"))
						intf_str_valid = true;
				}

				/* CMSIS-DAP v2 spec requires vendor-specific class/subclass/protocol.
				 * Accept deviations (e.g. KitProg3 uses class 0) only when the
				 * interface string reliably identifies the interface as CMSIS-DAP
				 * and the class is not a well-known one (CDC, MSC …). */
				if (intf_desc->bInterfaceClass != LIBUSB_CLASS_VENDOR_SPEC ||
					intf_desc->bInterfaceSubClass != 0 ||
					intf_desc->bInterfaceProtocol != 0) {
					if (intf_str_valid &&
						(intf_desc->bInterfaceClass == 0 ||
						 intf_desc->bInterfaceClass > 0x12)) {
						char t[256];
						snprintf(t, sizeof(t), "Using interface %d with wrong class %d, "
							"subclass %d or protocol %d",
							intf_desc->bInterfaceNumber,
							intf_desc->bInterfaceClass,
							intf_desc->bInterfaceSubClass,
							intf_desc->bInterfaceProtocol);
						printWarn(t);
					} else {
						continue;
					}
				}

				candidate.interface_num = intf_desc->bInterfaceNumber;
				candidate.config_num = cfg_desc->bConfigurationValue;
				candidate.ep_out = intf_desc->endpoint[0].bEndpointAddress;
				candidate.ep_in = intf_desc->endpoint[1].bEndpointAddress;
				candidate.packet_size = intf_desc->endpoint[0].wMaxPacketSize;
				intf_found = true;
			}

			libusb_free_config_descriptor(cfg_desc);
		}

		if (intf_found)
			result.push_back(candidate);
		else
			libusb_close(handle);
	}

	libusb_free_device_list(devs, true);
	return result;
}
#endif

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
int CmsisDAP::writeTMS(const uint8_t *tms, uint32_t len, bool flush_buffer,
		__attribute__((unused)) const uint8_t tdi)
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
int CmsisDAP::writeJtagSequence(uint8_t tms, const uint8_t *tx, uint8_t *rx,
		uint32_t len, bool end)
{
	int ret;
	int real_len = len - (end ? 1 : 0);  // full xfer size according to end
	uint8_t *rx_ptr = rx;
	const uint8_t *tx_ptr = tx;  // rd & wr ptr
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
int CmsisDAP::writeTDI(const uint8_t *tx, uint8_t *rx, uint32_t len, bool end)
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
	int ret = -1, bulk_len = 0;
	_ll_buffer[0] = 0;
	_ll_buffer[1] = instruction;

	switch(_backend){
#ifdef ENABLE_CMSISDAP_V1
		case BACKEND_HID:
			ret = hid_write(_hid_dev, _ll_buffer, 65);
			if (ret == -1) {
				printError("Error: HID write failed\n");
				return ret;
			}
			ret = hid_read_timeout(_hid_dev, _ll_buffer, 65, 1000);
			if (ret <= 0) {
				if (ret == 0)
					printError("Error: HID read timeout\n");
				else if (ret == -1)
					printError("Error: HID comm failed\n");
				return ret;
			}
			break;
#endif
#ifdef ENABLE_CMSISDAP_V2
		case BACKEND_USBBULK:
			memset(&_ll_buffer[1 + tx_len + 1], 0, 64 - (tx_len + 1));
			ret = libusb_bulk_transfer(_usb_dev, _ep_out, &_ll_buffer[1], 64, &bulk_len, 1000);
			if (ret != 0) {
				printError("Error: Bulk write failed\n");
				return ret;
			}
			memset(&_ll_buffer[0], 0, 1024);
			ret = libusb_bulk_transfer(_usb_dev, _ep_in, &_ll_buffer[0], _packet_size, &bulk_len, 1000);
			// sleep for 1ms to ensure that polling behavior aligns with HID backend
			usleep(1000);
			if (ret != 0 && ret != LIBUSB_ERROR_TIMEOUT) {
				printError("Error: Bulk read failed\n");
				return ret;
			}
			if(bulk_len == 0){
				printError("Error: Bulk timeout\n");
				return -1;
			}
			// if (rx_buff && bulk_len < rx_len + 2) {
                // printf("bulk short read: expected %d, got %d\n", rx_len + 2, bulk_len);
            // }
			ret = bulk_len;
			break;
#endif
		default:
			printError("Error: unknown USB backend\n");
			break;
	}

	if (_ll_buffer[0] != instruction) {
		printError("Error: command error\n");
		return -1;
	}
	if (_ll_buffer[1] != DAP_OK) {
		printError("Error: DAP status error\n");
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
	int ret = -1, bulk_len = 0;
	_ll_buffer[0] = 0;

	switch(_backend){
#ifdef ENABLE_CMSISDAP_V1
		case BACKEND_HID:
			ret = hid_write(_hid_dev, _ll_buffer, 65);
			if (ret == -1) {
				printError("Error: HID write failed\n");
				return ret;
			}
			ret = hid_read_timeout(_hid_dev, _ll_buffer, 65, 1000);
			if (ret <= 0) {
				if (ret == 0)
					printError("Error: HID read timeout\n");
				else if (ret == -1)
					printError("Error: HID comm failed\n");
				return ret;
			}
			break;
#endif
#ifdef ENABLE_CMSISDAP_V2
		case BACKEND_USBBULK:
			memset(&_ll_buffer[1 + tx_len + 1], 0, 64 - (tx_len + 1));
			ret = libusb_bulk_transfer(_usb_dev, _ep_out, &_ll_buffer[1], 64, &bulk_len, 1000);
			if (ret != 0) {
				printError("Error: Bulk write failed\n");
				return ret;
			}
			memset(&_ll_buffer[0], 0, 1024);
			ret = libusb_bulk_transfer(_usb_dev, _ep_in, &_ll_buffer[0], _packet_size, &bulk_len, 1000);
			// sleep for 1ms to ensure that polling behavior aligns with HID backend
			usleep(1000);
			if (ret != 0 && ret != LIBUSB_ERROR_TIMEOUT) {
				printError("Error: Bulk read failed\n");
				return ret;
			}
			if(bulk_len == 0){
				printError("Error: Bulk timeout\n");
				return -1;
			}
			// if (rx_buff && bulk_len < rx_len + 2) {
                // printf("bulk short read: expected %d, got %d\n", rx_len + 2, bulk_len);
            // }
			ret = bulk_len;
			break;
#endif
		default:
			printError("Error: unknown USB backend\n");
			break;
	}

	if (rx_len)
		memmove(rx_buff, _ll_buffer, rx_len);

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
		} else if (info == INFO_ID_TARGET_DEV_NAME) {
			if (!_product_name.empty()){
				snprintf(val, sizeof(val), "\t%s: %ls",
					cmsisdap_info_id_str[info].c_str(), _product_name.c_str());
			} else {
				printError("\t" + cmsisdap_info_id_str[info] + " : NA");
				return;
			}
		} else if (info == INFO_ID_TARGET_DEV_VENDOR) {
			if (!_vendor.empty()){
				snprintf(val, sizeof(val), "\t%s: %ls",
					cmsisdap_info_id_str[info].c_str(), _vendor.c_str());
			} else {
				printError("\t" + cmsisdap_info_id_str[info] + " : NA");
				return;
			}
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
