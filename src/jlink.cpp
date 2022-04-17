// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2021 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include "jlink.hpp"

#include <libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "display.hpp"

#define VID 0x1366
#define PID 0x0105

using namespace std;

// convert 2Byte to 1 short
#define CONV_16B(_val) ((((uint16_t) _val[0]) << 0) | \
					    (((uint16_t) _val[1]) << 8))
// convert 4Byte to 1 word
#define CONV_32B(_val) ((((uint32_t) _val[0]) <<  0) | \
					    (((uint32_t) _val[1]) <<  8) | \
					    (((uint32_t) _val[2]) << 16) | \
					    (((uint32_t) _val[3]) << 24))
// a 0-byte is introduce when reading a packet with
// size multiple of 64 Byte but < 0x8000
#define HAS_0BYTE(_len) ((_len != 0) && (_len % 64 == 0) && (_len != 0x8000))

// buffer capacity
#define BUF_SIZE 2048

Jlink::Jlink(uint32_t clkHz, int8_t verbose):_base_freq(0), _min_div(0),
	jlink_write_ep(-1), jlink_read_ep(-1), jlink_interface(-1),
	_verbose(verbose > 0), _debug(verbose > 1), _quiet(verbose < 0),
	_num_bits(0), _last_tms(0), _last_tdi(0),
	_hw_type(0), _major(0), _minor(0), _revision(0)
{
	// init libusb context
	if (libusb_init(&jlink_ctx) < 0)
		throw std::runtime_error("libusb init failed\n");

	// search for all compatible devices
	if (!jlink_scan_usb())
		throw std::runtime_error("can't find compatible device");

	// get device capacity
	if (!get_caps())
		throw std::runtime_error("can't read device CAPS");

	// get hw version
	if (get_hw_version() < 0)
		throw std::runtime_error("can't read hw version");

	get_speeds();

	// configure device in JTAG mode
	set_interface(0);

	// configure JTAG TCK frequency
	setClkFreq(clkHz);

	if (!set_ks_power(true))
		throw std::runtime_error("can't set KS power");
}

Jlink::~Jlink()
{
	// flush buffers before quit
	if (_num_bits != 0)
		flush();
	// release interface
	libusb_release_interface(jlink_handle, jlink_interface);
	// close device
	libusb_close(jlink_handle);
	// context cleanup
	libusb_exit(jlink_ctx);
}

int Jlink::writeTMS(uint8_t *tms, uint32_t len, bool flush_buffer)
{
	// empty buffer
	// if asked flush
	if (len == 0)
		return ((flush_buffer) ? flush() : 0);

	for (uint32_t pos = 0; pos < len; pos++) {
		// buffer full -> write
		if (_num_bits == BUF_SIZE * 8) {
			// write
			ll_write(NULL);
			_num_bits = 0;
		}

		_last_tms = (tms[pos >> 3] & (1 << (pos & 0x07))) ? 1 : 0;

		if (_last_tms)
			_tms[(_num_bits >> 3)] |= (1 << (_num_bits & 0x07));
		else
			_tms[(_num_bits >> 3)] &= ~(1 << (_num_bits & 0x07));
		if (_last_tdi)
			_tdi[(_num_bits >> 3)] |= (1 << (_num_bits & 0x07));
		else
			_tdi[(_num_bits >> 3)] &= ~(1 << (_num_bits & 0x07));
		_num_bits++;
	}

	// flush where it's asked or if the buffer is full
	if (flush_buffer || _num_bits == BUF_SIZE * 8)
		return flush();
	return len;
}

int Jlink::writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end)
{
	if (len == 0)  // nothing to do
		return 0;
	if (_num_bits != 0)  // flush buffer to simplify next step
		flush();

	uint32_t xfer_len = BUF_SIZE * 8;  // default to buffer capacity
	uint8_t tms = (_last_tms) ? 0xff : 0x00;  // set tms byte
	uint8_t *tx_ptr = tx, *rx_ptr = rx;  // use pointer to simplify algo

	/* write by burst */
	for (uint32_t rest = 0; rest < len; rest += xfer_len) {
		if ((xfer_len + rest) > len)  // len < buffer size
			xfer_len = len - rest;  // reduce xfer len
		uint16_t tt = (xfer_len + 7) >> 3;  // convert to Byte
		memset(_tms, tms, tt);  // fill tms buffer
		memcpy(_tdi, tx_ptr, tt);  // fill tdi buffer
		_num_bits = xfer_len;  // set buffer size in bit
		if (end && xfer_len + rest == len) {  // last sequence: set tms 1
			_last_tms = 1;
			uint16_t idx = _num_bits - 1;
			_tms[(idx >> 3)] |= (1 << (idx & 0x07));
		}
		ll_write((rx) ? rx_ptr : NULL);  // write

		tx_ptr += tt;
		if (rx)
			rx_ptr += tt;
	}

	return len;
}

// toggle clk with constant TDI and TMS. More or less same idea as writeTDI
int Jlink::toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len)
{
	// nothing to do
	if (clk_len == 0)
		return 0;
	if (_num_bits != 0)
		flush();

	_last_tms = tms;
	_last_tdi = tdi;
	uint8_t curr_tms = (tms) ? 0xff: 0x00;
	uint8_t curr_tdi = (tdi) ? 0xff: 0x00;

	uint32_t len = clk_len;

	// flush buffer before starting
	if (_num_bits != 0)
		flush();

	memset(_tdi, curr_tdi, BUF_SIZE);
	memset(_tms, curr_tms, BUF_SIZE);
	do {
		_num_bits = BUF_SIZE * 8;
		if (len < _num_bits)
			_num_bits = len;
		len -= _num_bits;
		ll_write(NULL);
	} while (len > 0);

	return clk_len;
}

int Jlink::flush()
{
	return ll_write(NULL);
}

bool Jlink::writeTMSTDI(const uint8_t *tms, const uint8_t *tdi, uint8_t *tdo,
		uint32_t numbits)
{
	// use pointer to access all vectors
	const uint8_t *tms_ptr = tms;
	const uint8_t *tdi_ptr = tdi;
	uint8_t *tdo_ptr = tdo;

	uint32_t xfer_len = 0;

	while (numbits > 0) {
		// if bits to send are greater than internal buffer
		// limits to buffer size
		if (numbits > (BUF_SIZE * 8))
			xfer_len = BUF_SIZE * 8;
		else  // or direct xfer
			xfer_len = numbits;
		// convert size in Byte
		uint16_t numbytes = (xfer_len + 7) >> 8;

		// copy buffers to internals
		memcpy(_tms, tms_ptr, numbytes);
		memcpy(_tdi, tdi_ptr, numbytes);
		// save size to transmit
		_num_bits = xfer_len;
		// send
		if (!ll_write(tdo_ptr))
			return false;
		// decrement bits to send
		numbits -= xfer_len;
		// move pointers
		tms_ptr += numbytes;
		tdi_ptr += numbytes;
		if (tdo)
			tdo_ptr += numbytes;
	}

	return true;
}

bool Jlink::ll_write(uint8_t *tdo)
{
	if (_num_bits == 0)
		return true;
	uint32_t numbytes = (_num_bits + 7) >> 3;
	uint8_t rx_buf[numbytes+2];
	uint8_t status;
	// 1. cmd + dummy + numbits + tms + tdi
	_xfer_buf[0] = EMU_CMD_HW_JTAG3;
	_xfer_buf[1] = 0;  // dummy
	_xfer_buf[2] = static_cast<uint8_t>((_num_bits >> 0) & 0xff);
	_xfer_buf[3] = static_cast<uint8_t>((_num_bits >> 8) & 0xff);
	memcpy(_xfer_buf + 4, _tms, numbytes);
	memcpy(_xfer_buf + 4 + numbytes, _tdi, numbytes);

	if (_debug) {
		printf("Out       : %u\n", numbytes);
		printf("cmd       : %02x\n", _xfer_buf[0]);
		printf("dummy     : %02x\n", _xfer_buf[1]);
		printf("bitlength : %02x %02x (%u)\n", _xfer_buf[2], _xfer_buf[3], _num_bits);
		printf("tms       : ");
		if (numbytes > 16) {
			printf("snip");
		} else {
			for (uint32_t i = 0; i < numbytes; i++)
				printf("%02x ", _xfer_buf[i+4]);
		}
		printf("\n");
		printf("tdi       : ");
		if (numbytes > 16) {
			printf("snip");
		} else {
			for (uint32_t i = 0; i < numbytes; i++)
				printf("%02x ", _xfer_buf[i+4+numbytes]);
		}
		printf("\n");
		printf("buffer    : ");
		for (uint32_t i = 0; i < 4 + (2 * numbytes); i++)
			printf("%02x ", _xfer_buf[i]);
		printf("\n");
	}

	if (!write_device(_xfer_buf, 4 + (2 * numbytes))) {
		printError("fails to send buffer");
		throw std::runtime_error("fails to send buffer");
	}

	// 2. read tdo + status
	int ret = read_device(rx_buf, numbytes+1);
	if (ret < 0) {
		printError("fails to read tdo");
		return false;
	}

	// 3. read status
	if ((uint32_t)ret == numbytes) {
		printError("read status");
		if (!read_device(&status, 1)) {
			printError("fails to read status\n");
			return false;
		}
	} else {
		status = rx_buf[numbytes];
	}

	if (tdo) {
		memcpy(tdo, rx_buf, numbytes);

		if (_debug) {
			printf("tdo       : ");
			for (uint32_t i = 0; i < numbytes; i+=16) {
				for (int ii = 0; ii < 16 && ((ii + i) < numbytes); ii++)
					printf("%02x ", tdo[i+ii]);
				printf("\n");
			}
		}
	}
	if (_debug)
		printf("\n");

	_num_bits = 0;  // clear counter

	return status == 0;
}

bool Jlink::cmd_read(uint8_t cmd, uint8_t *val, int size)
{
	int actual_length;
	int ret = libusb_bulk_transfer(jlink_handle, jlink_write_ep,
				&cmd, 1, &actual_length, 5000);
	if (ret < 0) {
		printf("Error write cmd_read %d %s %s\n", ret,
				libusb_error_name(ret),
				libusb_strerror(static_cast<libusb_error>(ret)));
		return false;
	}

	return (size == read_device(val, size));
}

bool Jlink::cmd_read(uint8_t cmd, uint16_t *val)
{
	if (!cmd_read(cmd, _xfer_buf, 2))
		return false;

	*val = CONV_16B(_xfer_buf);

	return true;
}

bool Jlink::cmd_read(uint8_t cmd, uint32_t *val)
{
	if (!cmd_read(cmd, _xfer_buf, 4))
		return false;

	*val = CONV_32B(_xfer_buf);

	return true;
}

bool Jlink::cmd_write(uint8_t cmd, uint16_t param)
{
	uint8_t tx_buf[3] = {cmd,
						static_cast<uint8_t>((param >> 0) & 0xff),
						static_cast<uint8_t>((param >> 8) & 0xff)};

	int actual_length;
	int ret = libusb_bulk_transfer(jlink_handle, jlink_write_ep,
				tx_buf, 3, &actual_length, 5000);
	if (ret < 0) {
		printf("Error write cmd_write %d\n", ret);
		printf("%s %s\n", libusb_error_name(ret),
				libusb_strerror(static_cast<libusb_error>(ret)));
		return ret;
	}

	return true;
}

bool Jlink::cmd_write(uint8_t cmd, uint8_t param)
{
	uint8_t tx_buf[2] = {cmd, param};

	int actual_length;
	int ret = libusb_bulk_transfer(jlink_handle, jlink_write_ep,
				tx_buf, 2, &actual_length, 5000);
	if (ret < 0) {
		printf("Error write cmd_write %d\n", ret);
		printf("%s %s\n", libusb_error_name(ret),
				libusb_strerror(static_cast<libusb_error>(ret)));
		return false;
	}

	return true;
}

int Jlink::read_device(uint8_t *buf, uint32_t size)
{
	int actual_length, tries = 3;
	uint32_t recv = 0, rest = size;
	uint8_t *rx_ptr = buf;

	do {
		int ret = libusb_bulk_transfer(jlink_handle, jlink_read_ep,
				rx_ptr, rest, &actual_length, 1000);
		if (ret == 0) {
			rx_ptr += actual_length;
			recv += actual_length;
			rest -= actual_length;
		} else if (ret == LIBUSB_ERROR_TIMEOUT) {
			tries--;
		} else {
			char toto[256];
			snprintf(toto, sizeof(toto), "Error read length %d %d %u %s %s\n",
					ret, actual_length, size, libusb_error_name(ret),
					libusb_strerror(static_cast<libusb_error>(ret)));
			return ret;
		}
	} while (recv < size && tries != 0);

	if (tries == 0)
		printError("fail");

	return recv;
}

bool Jlink::write_device(const uint8_t *buf, uint32_t size)
{
	int actual_length, tries = 4;
	int rest_size = size, recv = 0;
	uint8_t *buf_ptr = (uint8_t*)buf;

	do {
		int ret = libusb_bulk_transfer(jlink_handle, jlink_write_ep,
					(uint8_t *)buf_ptr, rest_size, &actual_length,
					1000);
		if (ret == 0) {
			rest_size -= actual_length;
			buf_ptr += actual_length;
			recv += actual_length;
		} else if (ret == LIBUSB_ERROR_TIMEOUT) {
			tries--;
		} else {
			printf("Error write %d\n", ret);
			printf("%s %s\n", libusb_error_name(ret),
					libusb_strerror(static_cast<libusb_error>(ret)));
			return false;
		}
	} while (tries > 0 && rest_size > 0);

	if (tries == 0 && rest_size != 0) {
		printf("error\n");
		return false;
	}

	return ((uint32_t)recv == size);
}

string Jlink::get_version()
{
	uint16_t length;
	cmd_read(EMU_CMD_VERSION, &length);
	uint8_t version[length];
	read_device(version, length);
	return string(reinterpret_cast<char*>(version));
}

int Jlink::get_hw_version()
{
	if (!(_caps & EMU_CAP_GET_HW_VERSION)) {
		printf("get hw version is not supported\n");
		printf("%u\n", _caps & EMU_CAP_GET_HW_VERSION);
		return 0;
	}
	uint32_t version;
	if (!cmd_read(EMU_CMD_GET_HW_VERSION, &version))
		return -1;

	_hw_type = (version / 1000000) % 100;
    _major = (version / 10000) % 100;
    _minor = (version / 100) % 100;
    _revision = version % 100;

	if (_debug)
		printf("%08x ", version);
	if (!_quiet) {
		printInfo("device type: " + jlink_hw_type[_hw_type] + 
				  " major: " + std::to_string(_major) +
				  " minor: " + std::to_string(_minor) +
				  " revision: " + std::to_string(_revision));
	}

	return version;
}

void Jlink::get_speeds()
{
	cmd_read(EMU_CMD_GET_SPEEDS, _xfer_buf, 6);
	_base_freq = CONV_32B(_xfer_buf);
	_min_div = CONV_16B((&_xfer_buf[4]));

	if (_debug) {
		for (int i = 0; i < 6; i++)
			printf("%02x ", _xfer_buf[i]);
		printf("\n");

		printf("%02x %04x\n", _base_freq, _min_div);
		printf("%u %u\n", _base_freq, _min_div);
	}
}

int Jlink::setClkFreq(uint32_t clkHz)
{
	uint32_t max_freq = _base_freq / _min_div;

	if (clkHz > max_freq) {
		printWarn("Jlink probe limited to " +
				std::to_string(max_freq/1000) + "kHz");
		clkHz = max_freq;
	}

	if (!cmd_write(EMU_CMD_SET_SPEED, static_cast<uint16_t>(clkHz / 1000))) {
		printError("setClkFreq: fail to configure frequency");
		return -EXIT_FAILURE;
	}

	_clkHZ = clkHz;
	return _clkHZ;
}

bool Jlink::set_speed(uint16_t freqHz)
{
	uint16_t freqKHz = freqHz / 1000;
	uint16_t max_speed = _base_freq / _min_div;

	if (freqKHz > max_speed) {
		printf("max freq limited to %d\n", max_speed * 1000);
		freqKHz = max_speed;
	}

	return cmd_write(EMU_CMD_SET_SPEED, freqKHz);
}

bool Jlink::get_caps()
{
	if (!cmd_read(EMU_CMD_GET_CAPS, &_caps))
		return false;

	if (_verbose) {
		printf("%04x\n", _caps);
		for (int i = 0; i < 32; i++) {
			if ((_caps >> i) & 0x01)
				printf("%2d %s\n", i, jlink_caps_flags[i].c_str());
		}
	}

	return true;
}

bool Jlink::get_result()
{
	uint8_t error_bit;
	if (cmd_read(EMU_CMD_HW_JTAG_GET_RESULT, &error_bit, 1) != 1) {
		printError("get result failed");
		return false;
	}
	printInfo("get_result " + std::to_string(error_bit));
	if (error_bit != 0)
		printError("pas bon");
	return error_bit == 0;
}

bool Jlink::set_ks_power(bool val)
{
	if (!cmd_write(EMU_CMD_SET_KS_POWER, static_cast<uint8_t>((val) ? 1 : 0)))
		return false;
	return true;
}

bool Jlink::max_mem_block(uint32_t *max_mem)
{
	if (!cmd_read(EMU_CMD_GET_MAX_MEM_BLOCK, max_mem))
		return false;
	return true;
}

// There is a typo in RM08001:
//   select interface is 0 for JTAG and 1 for SWD
//   so SubCmd must be 0..31 instead of 1..31
bool Jlink::set_interface(uint8_t interface)
{
	uint8_t buf[2] = {EMU_CMD_SELECT_IF, interface};
	uint8_t res[4];
	write_device(buf, 2);
	read_device(res, 4);
	if (_debug) {
		printf("set interface: ");
		for (int i = 0; i < 4; i++)
			printf("%02x ", res[i]);
		printf("\n");
	}
	return true;
}

void Jlink::read_config()
{
	jlink_cfg_t cfg;
	cmd_read(EMU_CMD_READ_CONFIG, reinterpret_cast<uint8_t*>(&cfg), 256);

	if (_verbose) {
		printf("usb_adr   : %02x\n", cfg.usb_adr);
		printf("kickstart : %08x\n", cfg.kickstart);
		printf("ip_address: %08x\n", cfg.ip_address);
		printf("subnetmask: %08x\n", cfg.subnetmask);
		printf("mac addr  : ");
		for (int i = 0; i < 6; i++) {
			printf("%02x", (uint8_t)cfg.mackaddr[i]);
			if (i < 5)
				printf(":");
		}
		printf("\n");
	}
}

bool Jlink::jlink_search_interface(libusb_device *dev,
		libusb_device_descriptor *desc, int *interface_idx,
		int *config_idx, int *alt_idx)
{
	bool found = false;
	*interface_idx = -1;
	*config_idx = -1;
	/* 1. iterate on all interface */
	for (int cfg_idx = 0; cfg_idx < desc->bNumConfigurations; cfg_idx++) {
		struct libusb_config_descriptor *cfg;
		int ret = libusb_get_config_descriptor(dev, cfg_idx, &cfg);
		if (ret != 0) {
			printf("Fail to retrieve config_descriptor \n");
			return false;
		}

		for (int if_idx=0; if_idx < cfg->bNumInterfaces; if_idx++) {
			const struct libusb_interface *uif = &cfg->interface[if_idx];
			for (int intf_idx = 0; intf_idx < uif->num_altsetting; intf_idx++) {
				const struct libusb_interface_descriptor *intf = &uif->altsetting[intf_idx];
				uint8_t intfClass = intf->bInterfaceClass;
				uint8_t intfSubClass = intf->bInterfaceSubClass;
				if (_debug)
					printf("intfClass: %x intfSubClass: %x\n", intfClass, intfSubClass);
				if (intfClass == 0xff && intfSubClass == 0xff) {
					if (found) {
						printError("too many compatible interface");
						return false;
					}
					found = true;
					*interface_idx = if_idx;
					*config_idx = cfg_idx;
					*alt_idx = intf_idx;
				}
			}
			if (_debug)
				printf("%d\n", if_idx);
		}
		libusb_free_config_descriptor(cfg);
	}
	return true;
}

bool Jlink::jlink_scan_usb()
{
	libusb_device **dev_list;
	libusb_device *usb_dev;
	libusb_device_handle *handle;
	ssize_t list_size = libusb_get_device_list(jlink_ctx, &dev_list);
	int i = 0;

	if (list_size == 0)
		return false;

	while ((usb_dev = dev_list[i++]) != NULL) {
		struct libusb_device_descriptor desc;
		if (libusb_get_device_descriptor(usb_dev, &desc) != 0) {
			printError("Unable to get device descriptor");
			return EXIT_FAILURE;
		}

		if (desc.idProduct != PID || desc.idVendor != VID)
			continue;

		if (_verbose)
			printf("%04x:%04x (bus %d, device %2d)\n",
				desc.idVendor, desc.idProduct,
				libusb_get_bus_number(usb_dev),
				libusb_get_device_address(usb_dev));
		/* try to open device to search for interface */
		int ret = libusb_open(usb_dev, &handle);
		if (ret != 0)
			return false;
		int if_idx, cfg_idx, alt_idx;
		if (jlink_search_interface(usb_dev, &desc, &if_idx,
					&cfg_idx, &alt_idx)) {
			jlink_devices_t dev;
			dev.usb_dev = usb_dev;
			dev.alt_idx = alt_idx;
			dev.if_idx = if_idx;
			dev.cfg_idx = cfg_idx;
			device_available.push_back(dev);
		}
		libusb_close(handle);
	}
	libusb_free_device_list(dev_list, 1);

	// no JLINK probes found
	if (device_available.size() == 0) {
		printError("Error: no device found");
		return false;
	}

	if (_debug) {
		for (size_t d = 0; d < device_available.size(); d++)
			printf("%x %x\n", device_available[d].if_idx,
					device_available[d].cfg_idx);
	}

	// more than one device plugged: TODO how to deal with that ?
	if (device_available.size() > 1) {
		printError("Error: to many devices");
		return false;
	}

	// try to open JLINK device
	int ret = libusb_open(device_available[0].usb_dev, &jlink_handle);
	if (ret != 0)
		return false;

	// request interface
	jlink_interface = device_available[0].if_idx;
	int cfg_idx = device_available[0].cfg_idx;
	ret = libusb_claim_interface(jlink_handle, jlink_interface);
	if (ret != 0) {
		printError("Fail to claim interface");
		return false;
	}

	// search for IN and OUT endpoint
	struct libusb_config_descriptor *cfg;
	ret = libusb_get_config_descriptor(device_available[0].usb_dev, cfg_idx, &cfg);
	if (ret != 0) {
		printError("Can't get config descriptor");
		return false;
	}
	const struct libusb_interface *uif = &cfg->interface[jlink_interface];
	const struct libusb_interface_descriptor *intf = &uif->altsetting[cfg_idx];
	for (int i = 0; i < intf->bNumEndpoints; i++) {
		struct libusb_endpoint_descriptor endpoint = intf->endpoint[i];
		if ((endpoint.bEndpointAddress & 0x80)) {
			jlink_read_ep = endpoint.bEndpointAddress;
		} else {
			jlink_write_ep = endpoint.bEndpointAddress;
		}
	}

	libusb_free_config_descriptor(cfg);

	if (jlink_write_ep == -1 || jlink_read_ep == -1 || jlink_interface == -1) {
		printError("error");
		return false;
	}

	return true;
}
