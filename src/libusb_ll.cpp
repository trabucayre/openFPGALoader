// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <libusb.h>

#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>  // For std::stringstream
#include <stdexcept>
#include <utility>

#include "cable.hpp"
#include "display.hpp"
#include "libusb_ll.hpp"

libusb_ll::libusb_ll(int vid, int pid, int8_t _verbose):
	_usb_ctx(nullptr), _dev_handle(nullptr), _verbose(_verbose >= 2),
	_if_num(-1), _kernel_driver_detached(false), _interface_claimed(false)
{
	(void)vid;
	(void)pid;
	if (libusb_init(&_usb_ctx) < 0)
		throw std::runtime_error("libusb_init_failed");
	ssize_t list_size = libusb_get_device_list(_usb_ctx, &_dev_list);
	if (list_size < 0) {
		libusb_exit(_usb_ctx);
		_usb_ctx = nullptr;
		throw std::runtime_error("libusb_get_device_list_failed");
	}
	if (list_size == 0)
		printError("No USB devices found");
	if (_verbose)
		printf("found %zd\n", list_size);
}

libusb_ll::~libusb_ll()
{
	disconnect();
	libusb_free_device_list(_dev_list, 1);
	libusb_exit(_usb_ctx);
}

/* simple helper to request the string for a descriptor/value.
 * return the ascii value or none if failed to retrieves
 */
static std::string get_usb_string(libusb_device_handle *handle, uint8_t index)
{
	if (index == 0)
		return "none";
	uint8_t buf[200];
	const int ret = libusb_get_string_descriptor_ascii(handle, index,
		buf, 200);
	if (ret < 0)
		return "none";

	return std::string(reinterpret_cast<const char *>(buf),
		static_cast<size_t>(ret));
}

int libusb_ll::get_devices_list(const cable_t *cable, const std::string &serial,
	bool get_infos)
{
	int vid = 0, pid = 0;
	uint8_t bus_addr = 0, device_addr = 0;

	if (cable != nullptr) {
		vid = cable->vid;
		pid = cable->pid;
		bus_addr = cable->bus_addr;
		device_addr = cable->device_addr;
	}
	return get_devices_list(vid, pid, bus_addr, device_addr, serial, get_infos);
}

int libusb_ll::get_devices_list(const int vid, const int pid,
	const uint8_t bus_addr, const uint8_t device_addr, const std::string &serial,
	bool get_infos)
{
	// if vid/pid only keep matching nodes
	const bool vid_pid_filter = vid != 0 && pid != 0;
	// if bus/dev only keep matching nodes
	const bool bus_dev_filter = bus_addr != 0 && device_addr != 0;
	// if serial only keep matching nodes
	const bool serial_filter = !serial.empty();

	int i = 0;
	libusb_device *usb_dev;

	_usb_dev_list.clear();
	_cable_list.clear();

	while ((usb_dev = _dev_list[i++]) != nullptr) {
		if (_verbose) {
			printf("%x %x %x %x\n", bus_addr, device_addr,
				libusb_get_bus_number(usb_dev),
				libusb_get_device_address(usb_dev));
		}

		/* bus addr and device addr provided: check */
		if (bus_dev_filter && (
				bus_addr != libusb_get_bus_number(usb_dev) ||
				device_addr != libusb_get_device_address(usb_dev)))
			continue;

		struct libusb_device_descriptor desc;
		if (libusb_get_device_descriptor(usb_dev, &desc) != 0) {
			printError("Unable to get device descriptor");
			continue;
		}

		if (_verbose) {
			printf("%x %x %x %x\n", vid, pid,
				desc.idVendor, desc.idProduct);
		}

		/* Linux host controller */
		if (desc.idVendor == 0x1d6b)
			continue;

		/* check for VID/PID */
		if (vid_pid_filter && (
				vid != desc.idVendor || pid != desc.idProduct))
			continue;

		/* Get the type of the probe
		 * when unknown ignores the device
		 */
		std::string probe_type = get_probe_type(desc.idVendor, desc.idProduct);
		if (probe_type.empty()) {
			/* When VID/PID or bus/dev addr are user provided
			 * the device may be not listed / customized
			 * -> uses unknown instead of ignoring them
			 */
			if (!vid_pid_filter && !bus_dev_filter)
				continue;
			probe_type = "unknown";
		}

		/* No informations requested nor serial provided
		 * => store the current device and check the next one
		 */
		if (!serial_filter && !get_infos) {
			_usb_dev_list.push_back(usb_dev);
			continue;
		}

		/* serial filtering and/or informations requires an handle */
		libusb_device_handle *handle = nullptr;
		const int ret = libusb_open(usb_dev, &handle);
		if (ret != 0) {
			char mess[1024];
			snprintf(mess, 1024,
				"Error: can't open device with vid:pid = 0x%04x:0x%04x. "
				"Error code %d %s",
				desc.idVendor, desc.idProduct,
				ret, libusb_strerror(static_cast<libusb_error>(ret)));
			printError(mess);
			handle = nullptr;
			/* unable to read the serial: impossible to check the match */
			if (serial_filter)
				continue;
		}

		/* check for serial */
		if (serial_filter) {
			/* no match: next one */
			if (serial != get_usb_string(handle, desc.iSerialNumber)) {
				libusb_close(handle);
				continue;
			}
		}

		/* store the device */
		_usb_dev_list.push_back(usb_dev);

		/* full informations requested: the full cable details must be
		 * obtained and stored
		 */
		if (get_infos) {
			struct cable_details_t cable(desc.idVendor, desc.idProduct,
				usb_dev);
			cable.probe = std::move(probe_type);
			cable.bus = libusb_get_bus_number(usb_dev);
			cable.device = libusb_get_device_address(usb_dev);
			cable.product = get_usb_string(handle, desc.iProduct);
			cable.serial = get_usb_string(handle, desc.iSerialNumber);
			cable.manufacturer = get_usb_string(handle, desc.iManufacturer);
			_cable_list.push_back(std::move(cable));
		}

		if (handle)
			libusb_close(handle);
	}

	return static_cast<int>(_usb_dev_list.size());
}

bool libusb_ll::connect(const int vid, const int pid, const int if_num)
{
	int ret;
	if (_dev_handle) {
		printError("libusb_ll:connect: USB device already open");
		return false;
	}

	_dev_handle = libusb_open_device_with_vid_pid(_usb_ctx, vid, pid);
	if (!_dev_handle) {
		printError("fails to open device");
		return false;
	}
	_if_num = if_num;

	/* Linux: if a kernel driver is attached, detach it before claiming. */
	ret = libusb_kernel_driver_active(_dev_handle, if_num);
	if (ret == 1) {
		ret = libusb_detach_kernel_driver(_dev_handle, if_num);
		if (ret != 0) {
			printError("libusb error while detaching kernel driver " +
				std::to_string(ret));
			disconnect();
			return false;
		}
		_kernel_driver_detached = true;
	} else if (ret < 0 && ret != LIBUSB_ERROR_NOT_SUPPORTED) {
		printError("libusb error while checking kernel driver state " +
			std::to_string(ret));
		disconnect();
		return false;
	}

	ret = libusb_claim_interface(_dev_handle, if_num);
	if (ret) {
		printError("libusb error while claiming device interface " +
			std::to_string(ret));
		disconnect();
		return false;
	}
	_interface_claimed = true;

	return true;
}

bool libusb_ll::disconnect()
{
	int ret;
	if (_dev_handle == nullptr)
		return true;

	if (_interface_claimed && _if_num >= 0) {
		ret = libusb_release_interface(_dev_handle, _if_num);
		if (ret != 0)
			printError("libusb error while releasing interface " +
				std::to_string(_if_num) + ": " + std::to_string(ret));
	}

	if (_kernel_driver_detached && _if_num >= 0) {
		ret = libusb_attach_kernel_driver(_dev_handle, _if_num);
		if (ret != 0 && ret != LIBUSB_ERROR_NOT_SUPPORTED)
			printError("libusb error while reattaching kernel driver on if " +
				std::to_string(_if_num) + ": " + std::to_string(ret));
	}

	libusb_close(_dev_handle);
	_dev_handle = nullptr;
	_if_num = -1;
	_kernel_driver_detached = false;
	_interface_claimed = false;
	return true;
}

bool libusb_ll::control_xfer(const uint8_t bmRequestType,
		const uint8_t bRequest, const uint16_t wValue,
		const uint16_t wIndex, const uint8_t *data, const uint32_t len,
		unsigned int timeout)
{
	int ret;
	if (!_dev_handle) {
		printError("libusb_ll:control_xfer: device not connected");
		return false;
	}

	ret = libusb_control_transfer(_dev_handle, bmRequestType, bRequest,
		wValue, wIndex, (unsigned char *)data, len, timeout);
	if (ret < 0) {
		printError("libusb_ll:control_xfer: failed " + std::to_string(ret) +
			" (" + libusb_strerror(static_cast<libusb_error>(ret)) + ")");
		return false;
	}
	return true;
}

bool libusb_ll::bulk_write(uint8_t write_ep, const uint8_t *tx_buf,
	uint32_t len, int *real_xfer_len, int timeout)
{
	int ret;
	if (!_dev_handle) {
		printError("libusb_ll:bulk_write: device not connected");
		*real_xfer_len = -1;
		return false;
	}

	ret = libusb_bulk_transfer(_dev_handle, write_ep,
		(unsigned char *)tx_buf, len, real_xfer_len, timeout);
	if (ret < 0) {
		printError("libusb_ll:bulk_write: write failed " + std::to_string(ret));
		*real_xfer_len = ret;
		return false;
	}
	if (_verbose)
		printf("write actual_length %d ret %d\n", *real_xfer_len, ret);
	if (*real_xfer_len != static_cast<int>(len)) {
		printError("libusb_ll:bulk_write: short write " +
			std::to_string(*real_xfer_len) +
			" / " + std::to_string(len));
		return false;
	}
	return true;
}

bool libusb_ll::bulk_read(uint8_t read_ep, uint8_t *rx_buf,
	uint32_t len, int *real_xfer_len, int timeout)
{
	int ret;
	if (!_dev_handle) {
		printError("libusb_ll:bulk_read: device not connected");
		*real_xfer_len = -1;
		return false;
	}
	ret = libusb_bulk_transfer(_dev_handle, read_ep,
		rx_buf, len, real_xfer_len, timeout);
	if (ret < 0) {
		printError("libusb_ll: bulk_read: read failed " + std::to_string(ret));
		*real_xfer_len = ret;
		return false;
	}
	if (*real_xfer_len == 0) {
		printError("libusb_ll: bulk_read: received 0 bytes");
		return false;
	}
	if (_verbose) {
		printf("actual_length %d max: %u ret %d\n", *real_xfer_len, len, ret);
		for (int i = 0; i < *real_xfer_len; i++)
			printf("%d %02x\n", i, rx_buf[i]);
	}
	return true;
}

std::string formatHex(uint16_t c, int len) {
	std::stringstream ss;
	ss << "0x";
	ss << std::hex << std::setfill('0') << std::setw(len)
	   << (static_cast<unsigned int>(static_cast<uint16_t>(c)) & 0xFFFF);
	return ss.str();
}

std::string formatDec(uint8_t c, int len) {
	std::stringstream ss;
	ss << std::setfill('0') << std::setw(len) <<
		static_cast<unsigned int>(c);
	return ss.str();
}

std::string libusb_ll::get_probe_type(const uint16_t vid, const uint16_t pid)
{
	/* ftdi devices */
	// FIXME: missing iProduct in cable_list
	if (vid == 0x403) {
		switch (pid) {
		case 0x6010:
			return "FTDI2232";
		case 0x6011:
			return "ft4232";
		case 0x6001:
			return "ft232RL";
		case 0x6014:
			return "ft232H";
		case 0x6015:
			return "ft231X";
		case 0x6043:
			return "FT4232HP";
		default:
			return "unknown FTDI";
		}
	}
	// FIXME: DFU device can't be detected here
	for (const auto& b : cable_list) {
		const cable_t &c = b.second;
		if (c.vid == vid && c.pid == pid)
			return b.first;
	}

	return {};
}

bool libusb_ll::scan()
{
	size_t manufacturer_len = 12;
	size_t probe_len = 10;
	size_t serial_len = 6;

	/* get devices list with all associated informations */
	get_devices_list(nullptr, "", true);

	for (const cable_details_t &cable : _cable_list) {
		const uint32_t ml = static_cast<uint32_t>(cable.manufacturer.size());
		const uint32_t pl = static_cast<uint32_t>(cable.probe.size());
		const uint32_t sl = static_cast<uint32_t>(cable.serial.size());

		if (ml > manufacturer_len)
			manufacturer_len = ml;
		if (pl > probe_len)
			probe_len = pl;
		if (sl > serial_len)
			serial_len = sl;
	}

	manufacturer_len++;
	serial_len++;
	probe_len++;

	std::stringstream buffer;
	buffer << std::left
		<< std::setw(4) << "Bus"
		<< std::setw(7) << "device"
		<< std::setw(14) << "vid:pid"
		<< std::setw(probe_len) << "probe_type"
		<< std::setw(manufacturer_len) << "manufacturer"
		<< std::setw(serial_len) << "serial"
		<< "product";
	printSuccess(buffer.str());

	for (const auto& cable : _cable_list) {
		std::stringstream buffer;
		buffer << std::left  // Left-align all fields
			<< std::setw(4) << formatDec(cable.bus, 3)
			<< std::setw(7) << formatDec(cable.device, 3)
			<< std::setw(14)
			<< (formatHex(cable.vid, 4) + ":" + formatHex(cable.pid, 4))
			<< std::setw(probe_len) << cable.probe
			<< std::setw(manufacturer_len) << cable.manufacturer
			<< std::setw(serial_len) << cable.serial
			<< cable.product;

		printInfo(buffer.str());
	}

	return true;
}
