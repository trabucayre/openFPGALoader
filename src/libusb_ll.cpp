// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <string.h>

#include "cable.hpp"
#include "display.hpp"
#include "libusb_ll.hpp"

using namespace std;

libusb_ll::libusb_ll(int vid, int pid):_verbose(true)
{
	(void)vid;
	(void)pid;
	if (libusb_init(&_usb_ctx) < 0)
		throw std::runtime_error("libusb_init_failed");
}

libusb_ll::~libusb_ll()
{
	libusb_exit(_usb_ctx);
}

bool libusb_ll::scan()
{
	int i = 0;
	libusb_device **dev_list;
	libusb_device *usb_dev;
	libusb_device_handle *handle;

	/* iteration */
	ssize_t list_size = libusb_get_device_list(_usb_ctx, &dev_list);
	if (_verbose)
		printInfo("found " + std::to_string(list_size) + " USB device");

	char *mess = (char *)malloc(1024);
	snprintf(mess, 1024, "%3s %3s %-13s %-15s %-12s %-20s %s",
			"Bus", "device", "vid:pid", "probe type", "manufacturer",
			"serial", "product");
	printSuccess(mess);

	while ((usb_dev = dev_list[i++]) != NULL) {
		bool found = false;
		struct libusb_device_descriptor desc;
		if (libusb_get_device_descriptor(usb_dev, &desc) != 0) {
			printError("Unable to get device descriptor");
			return false;
		}

		char probe_type[256];

		/* Linux host controller */
		if (desc.idVendor == 0x1d6b)
			continue;

		/* ftdi devices */
		// FIXME: missing iProduct in cable_list
		if (desc.idVendor == 0x403) {
			if (desc.idProduct == 0x6010)
				snprintf(probe_type, 256, "FTDI2232");
			else if (desc.idProduct == 0x6011)
				snprintf(probe_type, 256, "ft4232");
			else if (desc.idProduct == 0x6001)
				snprintf(probe_type, 256, "ft232RL");
			else if (desc.idProduct == 0x6014)
				snprintf(probe_type, 256, "ft232H");
			else if (desc.idProduct == 0x6015)
				snprintf(probe_type, 256, "ft231X");
			else
				snprintf(probe_type, 256, "unknown FTDI");
			found = true;
		} else {
			// FIXME: DFU device can't be detected here
			for (auto b = cable_list.begin(); b != cable_list.end(); b++) {
				cable_t *c = &(*b).second;
				if (c->vid == desc.idVendor && c->vid == desc.idProduct) {
					snprintf(probe_type, 256, "%s", (*b).first.c_str());
					found = true;
				}
			}
		}

		if (!found)
			continue;

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
			snprintf((char*)iproduct, 200, "none");
		ret = libusb_get_string_descriptor_ascii(handle,
			desc.iManufacturer, imanufacturer, 200);
		if (ret < 0)
			snprintf((char*)imanufacturer, 200, "none");
		ret = libusb_get_string_descriptor_ascii(handle,
			desc.iSerialNumber, iserial, 200);
		if (ret < 0)
			snprintf((char*)iserial, 200, "none");
		uint8_t bus_addr = libusb_get_bus_number(usb_dev);
		uint8_t dev_addr = libusb_get_device_address(usb_dev);

		snprintf(mess, 1024, "%03d %03d    0x%04x:0x%04x %-15s %-12s %-20s %s",
				bus_addr, dev_addr,
				desc.idVendor, desc.idProduct,
				probe_type, imanufacturer, iserial, iproduct);

		printInfo(mess);

		libusb_close(handle);
	}

	libusb_free_device_list(dev_list, 1);
	free(mess);

	return true;
}
