// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_LIBUSB_LL_HPP_
#define SRC_LIBUSB_LL_HPP_

#include <libusb.h>

class libusb_ll {
	public:
		explicit libusb_ll(int vid = -1, int pid = -1);
		~libusb_ll();

		bool scan();

	private:
		struct libusb_context *_usb_ctx;
		bool _verbose;
};

#endif  // SRC_LIBUSB_LL_HPP_
