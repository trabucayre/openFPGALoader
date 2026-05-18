// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2021-2026 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_FX2_LL_HPP_
#define SRC_FX2_LL_HPP_

#include <libusb.h>
#include <stdint.h>

#include <string>

/*!
 * \file fx2_ll
 * \class FX2_ll
 * \brief low level driver for cypress fx2 device
 * \author Gwenhael Goavec-Merou
 */
class FX2_ll {
	public:
		/*!
		 * \brief constructor
		 * \param[in] uninit_vid: vendor ID for uninitialized device
		 * \param[in] uninit_pid: product ID for uninitialized device
		 * \param[in] vid: vendor ID for initialized device
		 * \param[in] pid: product ID for initialized device
		 * \param[in] firmware_path: firmware to load
		 */
		FX2_ll(uint16_t uninit_vid, uint16_t uninit_pid,
				uint16_t vid, uint16_t pid, const std::string &firmware_path);
		~FX2_ll();

		/*!
		 * \brief libusb_set_interface_alt_setting wrapper
		 * \param[in] if_num: Interface number
		 * \param[in] alt_setting: alternate setting
		 * \return true on success
		 */
		bool set_interface_alt_setting(const int if_num, const int alt_setting);

		/*!
		 * \brief bulk write
		 * \param[in] endpoint: endpoint to use
		 * \param[in] buff: buffer to write
		 * \param[in] len: buffer length
		 * \return -1 when transfer fails, number of bytes otherwise
		 */
		int write(uint8_t endpoint, uint8_t *buff, uint16_t len);
		/*!
		 * \brief bulk read
		 * \param[in] endpoint: endpoint to use
		 * \param[in] buff: buffer to fill
		 * \param[in] len: buffer length
		 * \return -1 when transfer fails, number of bytes otherwise
		 */
		int read(uint8_t endpoint, uint8_t *buff, uint16_t len);
		/*!
		 * \brief control write
		 * \param[in] bRequest
		 * \param[in] wValue
		 * \param[in] buff: buffer to write
		 * \param[in] len: buffer length
		 * \param[in] wIndex: the index field for the setup packet
		 * \return false when transfer fails, true otherwise
		 */
		int write_ctrl(uint8_t bRequest, uint16_t wValue,
				uint8_t *buff, uint16_t len, uint16_t wIndex=0x0000);
		/*!
		 * \brief control write
		 * \param[in] bRequest
		 * \param[in] wValue
		 * \param[in] buff: buffer to write
		 * \param[in] len: buffer length
		 * \param[in] wIndex: the index field for the setup packet
		 * \return false when transfer fails, true otherwise
		 */
		int read_ctrl(uint8_t bRequest, uint16_t wValue,
				uint8_t *buff, uint16_t len, uint16_t wIndex=0x0000);

	private:
		/*!
		 * \brief load firmware into device
		 * \param[in] firmware_path: firmware to load
		 * \return false if reset or firmware load fail
		 */
		bool load_firmware(std::string firmware_path);
		/*!
		 * \brief set/unset reset bit in CPUCS register
		 * \param[in] res8051: set or reset
		 * \return false if something fails
		 */
		bool reset(uint8_t res8051);
		bool close();

		libusb_device_handle *dev_handle;
		libusb_context *usb_ctx;
};
#endif  // SRC_FX2_LL_HPP_

