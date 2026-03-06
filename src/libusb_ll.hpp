// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_LIBUSB_LL_HPP_
#define SRC_LIBUSB_LL_HPP_

#include <libusb.h>

#include <cstdint>
#include <string>
#include <vector>

#include "cable.hpp"

/*!
 * \brief USB devices informations as reported by SetupAPI
 */
struct cable_details_t {
	uint8_t bus;
	uint8_t device;
	uint16_t vid;
	uint16_t pid;
	std::string probe;
	std::string manufacturer;
	std::string serial;
	std::string product;
	std::string path;
	libusb_device *usb_dev;
	cable_details_t():usb_dev(nullptr) {}
	cable_details_t(uint16_t v, uint16_t p,
	libusb_device *ubd):vid(v), pid(p), usb_dev(ubd) {}
};

class libusb_ll {
	public:
		libusb_ll(const libusb_ll &) = delete;
		libusb_ll &operator=(const libusb_ll &) = delete;
		explicit libusb_ll(int vid, int pid, int8_t verbose);
		~libusb_ll();

		bool scan();
		const std::vector<struct libusb_device *> &usb_dev_list() {
			return _usb_dev_list;
		}
		int get_devices_list(const cable_t *cable,
			const std::string &serial = "", bool get_infos = true);
		int get_devices_list(const int vid, const int pid,
			const uint8_t bus_addr, const uint8_t device_addr,
			const std::string &serial = "", bool get_infos = true);

		bool connect(const int vid, const int pid, const int if_num);
		bool disconnect();
		bool control_xfer(const uint8_t bmRequestType, const uint8_t bRequest,
			const uint16_t wValue, const uint16_t wIndex,
			const uint8_t *data, const uint32_t len, unsigned int timeout);
		bool bulk_write(uint8_t write_ep, const uint8_t *tx_buf, uint32_t len,
			int *real_xfer_len, int timeout);
		bool bulk_read(uint8_t read_ep, uint8_t *rx_buf, uint32_t len,
			int *real_xfer_len, int timeout);

		const std::vector<struct cable_details_t> &get_cable_list() {
			return _cable_list;
		}

	protected:
		struct libusb_context *_usb_ctx;
		libusb_device_handle *_dev_handle;
		bool _verbose;

	private:
		std::string get_probe_type(const uint16_t vid, const uint16_t pid);
		int _if_num;
		bool _kernel_driver_detached;
		bool _interface_claimed;
		libusb_device **_dev_list;
		std::vector<struct libusb_device *> _usb_dev_list;
		std::vector<struct cable_details_t> _cable_list;
};

#endif  // SRC_LIBUSB_LL_HPP_
