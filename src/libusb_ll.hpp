// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_LIBUSB_LL_HPP_
#define SRC_LIBUSB_LL_HPP_

#include <libusb.h>

#include <vector>

#include "cable.hpp"

class libusb_ll {
	public:
		explicit libusb_ll(int vid, int pid, int8_t verbose);
		~libusb_ll();

		bool scan();
		const std::vector<struct libusb_device *>usb_dev_list() { return _usb_dev_list; }
		int get_devices_list(const cable_t *cable);

	protected:
		struct libusb_context *_usb_ctx;
		bool _verbose;
	private:
		libusb_device **_dev_list;
		std::vector<struct libusb_device *> _usb_dev_list;
};

#endif  // SRC_LIBUSB_LL_HPP_
