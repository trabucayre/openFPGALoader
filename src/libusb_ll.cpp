// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <libusb.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <list>
#include <vector>
#include <string>
#include <sstream> // For std::stringstream
#include <stdexcept>

#include "cable.hpp"
#include "display.hpp"
#include "libusb_ll.hpp"

using namespace std;

libusb_ll::libusb_ll(int vid, int pid, int8_t _verbose):
	_usb_ctx(nullptr), _verbose(_verbose >= 2)
{
	(void)vid;
	(void)pid;
	if (libusb_init(&_usb_ctx) < 0)
		throw std::runtime_error("libusb_init_failed");
	ssize_t list_size = libusb_get_device_list(_usb_ctx, &_dev_list);
	if (list_size < 0)
		throw std::runtime_error("libusb_get_device_list_failed");
	if (list_size == 0)
		printError("No USB devices found");
	if (_verbose)
		printf("found %zd\n", list_size);
}

libusb_ll::~libusb_ll()
{
	libusb_free_device_list(_dev_list, 1);
	libusb_exit(_usb_ctx);
}

int libusb_ll::get_devices_list(const cable_t *cable)
{
	int vid = 0, pid = 0;
	uint8_t bus_addr = 0, device_addr = 0;
	bool vid_pid_filter = false;  // if vid/pid only keep matching nodes
	bool bus_dev_filter = false;  // if bus/dev only keep matching nodes

	if (cable != nullptr) {
		vid = cable->vid;
		pid = cable->pid;
		bus_addr = cable->bus_addr;
		device_addr = cable->device_addr;
		vid_pid_filter = (vid != 0) && (pid != 0);
		bus_dev_filter = (bus_addr != 0) && (device_addr != 0);
	}

	int i = 0;
	libusb_device *usb_dev;

	_usb_dev_list.clear();

	while ((usb_dev = _dev_list[i++]) != nullptr) {
		if (_verbose) {
			printf("%x %x %x %x\n", bus_addr, device_addr,
					libusb_get_device_address(usb_dev),
					libusb_get_bus_number(usb_dev));
		}

		/* bus addr and device addr provided: check */
		if (bus_dev_filter && (
				bus_addr != libusb_get_device_address(usb_dev) ||
				device_addr != libusb_get_bus_number(usb_dev)))
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

		_usb_dev_list.push_back(usb_dev);
	}

	return static_cast<int>(_usb_dev_list.size());
}

struct cable_details_t {
	uint8_t bus;
	uint8_t device;
	uint16_t vid;
	uint16_t pid;
	std::string probe;
	std::string manufacturer;
	std::string serial;
	std::string product;
	cable_details_t(uint8_t& b, uint8_t& d,
		uint16_t& v, uint16_t& p,
		std::string prb, std::string m,
		std::string s, std::string prd):
		bus(b), device(d), vid(v), pid(p),
		probe(prb), manufacturer(m),
		serial(s), product(prd) {}
};
std::string formatHex(uint16_t c, int len) {
	std::stringstream ss;
	ss << "0x";
	ss << std::hex << std::setfill('0') << std::setw(len)
	   << (static_cast<unsigned int>(static_cast<unsigned short>(c)) & 0xFFFF);
	return ss.str();
}

std::string formatDec(char c, int len) {
	std::stringstream ss;
	ss << std::setfill('0') << std::setw(len) << std::to_string(c);
	return ss.str();
}

bool libusb_ll::scan()
{
	std::list<cable_details_t> list_cables;
	size_t manufacturer_len = 12;
	size_t probe_len = 10;
	size_t serial_len = 6;

	get_devices_list(nullptr);

	for (libusb_device *usb_dev : _usb_dev_list) {
		bool found = false;
		struct libusb_device_descriptor desc;
		if (libusb_get_device_descriptor(usb_dev, &desc) != 0) {
			printError("Unable to get device descriptor");
			return false;
		}

		char probe_type[256];

		/* ftdi devices */
		// FIXME: missing iProduct in cable_list
		if (desc.idVendor == 0x403) {
			switch (desc.idProduct) {
			case 0x6010:
				snprintf(probe_type, 256, "FTDI2232");
				break;
			case 0x6011:
				snprintf(probe_type, 256, "ft4232");
				break;
			case 0x6001:
				snprintf(probe_type, 256, "ft232RL");
				break;
			case 0x6014:
				snprintf(probe_type, 256, "ft232H");
				break;
			case 0x6015:
				snprintf(probe_type, 256, "ft231X");
				break;
			case 0x6043:
				snprintf(probe_type, 256, "FT4232HP");
				break;
			default:
				snprintf(probe_type, 256, "unknown FTDI");
				break;
			}
			found = true;
		} else {
			// FIXME: DFU device can't be detected here
			for (const auto& b : cable_list) {
				const cable_t *c = &b.second;
				if (c->vid == desc.idVendor && c->pid == desc.idProduct) {
					snprintf(probe_type, 256, "%s", b.first.c_str());
					found = true;
					break;
				}
			}
		}

		if (!found)
			continue;

		libusb_device_handle *handle;
		int ret = libusb_open(usb_dev, &handle);
		if (ret != 0) {
			char mess[1024];
			snprintf(mess, 1024,
				"Error: can't open device with vid:vid = 0x%04x:0x%04x. "
				"Error code %d %s",
				desc.idVendor, desc.idProduct,
				ret, libusb_strerror(static_cast<libusb_error>(ret)));
			printError(mess);
			continue;
		}

		uint8_t iproduct[200];
		uint8_t iserial[200];
		uint8_t imanufacturer[200];
		ret = libusb_get_string_descriptor_ascii(handle,
			desc.iProduct, iproduct, 200);
		if (ret < 0)
			snprintf(reinterpret_cast<char*>(iproduct), 200, "none");
		ret = libusb_get_string_descriptor_ascii(handle,
			desc.iManufacturer, imanufacturer, 200);
		if (ret < 0)
			snprintf(reinterpret_cast<char*>(imanufacturer), 200, "none");
		ret = libusb_get_string_descriptor_ascii(handle,
			desc.iSerialNumber, iserial, 200);
		if (ret < 0)
			snprintf(reinterpret_cast<char*>(iserial), 200, "none");
		uint8_t bus_addr = libusb_get_bus_number(usb_dev);
		uint8_t dev_addr = libusb_get_device_address(usb_dev);

		list_cables.emplace_back(cable_details_t(
			bus_addr, dev_addr, desc.idVendor, desc.idProduct,
			std::string(probe_type), std::string((const char *)imanufacturer),
			std::string((const char *)iserial), std::string((const char *)iproduct)));

		if (strlen((const char *)imanufacturer) > manufacturer_len)
			manufacturer_len = strlen((const char *)imanufacturer);
		if (strlen((const char *)probe_type) > probe_len)
			probe_len = strlen((const char *)probe_type);
		if (strlen((const char *)iserial) > serial_len)
			serial_len = strlen((const char *)iserial);

		libusb_close(handle);
	}

	manufacturer_len++;
	serial_len++;
	probe_len++;

	std::stringstream buffer;
	buffer << std::left
		<< std::setw(4) << "Bus"
		<< std::setw(7) << "device"
		<< std::setw(14) << "vid:pid"
		<< std::setw(probe_len) << "probe type"
		<< std::setw(manufacturer_len) << "manufacturer"
		<< std::setw(serial_len) << "serial"
		<< "product";
	printSuccess(buffer.str());

	for (const auto& cable : list_cables) {
		std::stringstream buffer;
		buffer << std::left // Left-align all fields
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
