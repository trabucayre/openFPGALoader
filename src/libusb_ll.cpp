// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <libusb.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
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

bool libusb_ll::scan()
{
	char *mess = reinterpret_cast<char *>(malloc(1024));
	if (!mess) {
		printError("Error: failed to allocate buffer");
		return false;
	}

	get_devices_list(nullptr);

	snprintf(mess, 1024, "%3s %3s %-13s %-15s %-12s %-20s %s",
			"Bus", "device", "vid:pid", "probe type", "manufacturer",
			"serial", "product");
	printSuccess(mess);

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

		snprintf(mess, 1024, "%03d %03d    0x%04x:0x%04x %-15s %-12s %-20s %s",
				bus_addr, dev_addr,
				desc.idVendor, desc.idProduct,
				probe_type, imanufacturer, iserial, iproduct);

		printInfo(mess);

		libusb_close(handle);
	}

	free(mess);

	return true;
}
