// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2021 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <libusb.h>
#include <stdint.h>
#include <unistd.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "display.hpp"
#include "fx2_ll.hpp"
#include "ihexParser.hpp"

using namespace std;

#define FX2_FIRM_LOAD          0xA0
#define FX2_GCR_CPUCS          0xe600
#define FX2_GCR_CPUCS_8051_RES (1 << 0)

FX2_ll::FX2_ll(uint16_t uninit_vid, uint16_t uninit_pid,
		uint16_t vid, uint16_t pid, const string &firmware_path)
{
	int ret;
	bool reenum = false;

	if (libusb_init(&usb_ctx) < 0) {
		throw std::runtime_error("libusb init failed");
	}

	/* try to open uninitialized device */
	if (uninit_vid != 0 && uninit_pid != 0) {
		dev_handle = libusb_open_device_with_vid_pid(usb_ctx,
				uninit_vid, uninit_pid);
		if (dev_handle) {
			ret = libusb_claim_interface(dev_handle, 0);
			if (ret) {
				libusb_close(dev_handle);
				libusb_exit(usb_ctx);
				throw std::runtime_error("claim interface failed");
			}
			/* load firmware */
			load_firmware(firmware_path);
			close();
			reenum = true;
		}
	}

	/* try to open an already init device
	 * since fx2 may be not immediately ready
	 * retry with a delay
	 */
	int timeout = 100;
	do {
		dev_handle = libusb_open_device_with_vid_pid(usb_ctx,
				vid, pid);
		timeout--;
		if (!dev_handle)
			sleep(1);
	} while (!dev_handle && timeout > 0 && reenum);

	if (!dev_handle)
		throw std::runtime_error("FX2: fail to open device");

	ret = libusb_claim_interface(dev_handle, 0);
	if (ret) {
		libusb_close(dev_handle);
		libusb_exit(usb_ctx);
		throw std::runtime_error("claim interface failed");
	}
}

/* destructor: close current device and
 * destroy context
 */
FX2_ll::~FX2_ll()
{
	close();
	libusb_exit(usb_ctx);
}

/* close device after releasing interface
 */
bool FX2_ll::close()
{
	if (dev_handle) {
        int ret;
        ret = libusb_release_interface(dev_handle, 0);
        if (ret != 0) {
            /* device is already disconnected ... */
            if (ret == LIBUSB_ERROR_NO_DEVICE) {
                return true;
            } else {
                printError("Error: Fail to release interface");
                return false;
            }
        }
        libusb_close(dev_handle);
        dev_handle = NULL;
    }
	return true;
}

/* write len byte in bulk using endpoint
 */
int FX2_ll::write(uint8_t endpoint, uint8_t *buff, uint16_t len)
{
	int ret, actual_length;
	ret = libusb_bulk_transfer(dev_handle, LIBUSB_ENDPOINT_OUT | endpoint,
			buff, len, &actual_length, 1000);
	if (ret != LIBUSB_SUCCESS) {
		printError("FX2 write error: " + std::string(libusb_error_name(ret)));
		return -1;
	}
	return actual_length;
}

/* read len byte in bulk using endpoint
 */
int FX2_ll::read(uint8_t endpoint, uint8_t *buff, uint16_t len)
{
	int ret, actual_length;
	ret = libusb_bulk_transfer(dev_handle, LIBUSB_ENDPOINT_IN | endpoint,
			buff, len, &actual_length, 1000);
	if (ret != LIBUSB_SUCCESS) {
		printError("FX2 read error: " + std::string(libusb_error_name(ret)));
		return -1;
	}
	return actual_length;
}

/* write len data using control
 */
int FX2_ll::write_ctrl(uint8_t bRequest, uint16_t wValue,
		uint8_t *buff, uint16_t len)
{
	int ret = libusb_control_transfer(dev_handle,
			LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT,
			bRequest, wValue, 0x0000, buff, len, 100);
	if (ret < 0) {
		printError("Unable to send control request: "
				+ std::string(libusb_error_name(ret)));
		return false;
	}
	return true;
}

/* read len data using control
 */
int FX2_ll::read_ctrl(uint8_t bRequest, uint16_t wValue,
		uint8_t *buff, uint16_t len)
{
	int ret = libusb_control_transfer(dev_handle,
			LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN,
			bRequest, wValue, 0x0000, buff, len, 100);
	if (ret < 0) {
		printError("Unable to read control request: "
				+ std::string(libusb_error_name(ret)));
		return false;
	}
	return true;
}

/* load firmware section by section
 * and 64B by 64B
 * set CPU in reset state before and restart after
 */
bool FX2_ll::load_firmware(string firmware_path)
{
	IhexParser ihex(firmware_path, false, true);
	ihex.parse();

	/* reset */
	if (!reset(1))
		return false;
	/* load */
	vector<IhexParser::data_line_t> data = ihex.getDataArray();
	for (size_t i = 0; i < data.size(); i++) {
		IhexParser::data_line_t data_line = data[i];

		uint16_t toSend = data_line.length;
		uint8_t *tx_buff = data_line.line_data.data();
		uint16_t addr = data_line.addr;
		while (toSend > 0) {
			uint16_t xfer_len = (toSend > 64) ? 64: toSend;
			if (!write_ctrl(FX2_FIRM_LOAD, addr, tx_buff, xfer_len)) {
				printError("load firmware failed\n");
				return false;
			}
			toSend -= xfer_len;
			tx_buff += xfer_len;
			addr += xfer_len;
		}
	}

	/* unset reset */
	if (!reset(0))
		return false;
	return true;
}

/* set or unset 8051RES in CPUCS register
 */
bool FX2_ll::reset(uint8_t res8051)
{
	unsigned char buf[1];
	int ret;

	buf[0] = res8051;
	if (!(ret = write_ctrl(FX2_FIRM_LOAD, FX2_GCR_CPUCS, buf, 1))) {
		printError("Unable to send control request: "
				+ std::string(libusb_error_name(ret)));
		return false;
	}
	return true;
}
