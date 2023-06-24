// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2021 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_DFU_HPP_
#define SRC_DFU_HPP_

#include <libusb.h>

#include <map>
#include <string>
#include <vector>

#include "dfuFileParser.hpp"

class DFU {
	public:
		/*!
		 * \brief contructor
		 * \param[in] filename: bitstream
		 * \param[in] vid: vendor id
		 * \param[in] pid: product id
		 * \param[in] altsetting: device altsetting to use
		 * \param[in] verbose_lvl: verbose level 0 normal, -1 quiet, 1 verbose
		 */
		DFU(const std::string &filename, bool bypass_bitstream,
				uint16_t vid, uint16_t pid,
				int16_t altsetting, int verbose_lvl);

		~DFU();

		/*!
		 * \brief enumerate all USB peripherals configuration to detect DFU devices
		 * \return EXIT_FAILURE when something is wrong, EXIT_SUCCESS otherwise
		 */

		int searchDFUDevices();
		/*!
		 * \brief display details about all DFU compatible devices found
		 */
		void displayDFU();

		/*!
		 * \brief download file content
		 * \param[in] filename: filename to download
		 */
		int download();

	private:
		/**
		 * \brief dfu descriptor structure (not provided by libusb
		 */
		struct dfu_desc {
		    uint8_t  bLength;
		    uint8_t  bDescriptorType;
		    uint8_t  bmAttributes;
		    uint16_t wDetachTimeOut;
		    uint16_t wTransferSize;
		    uint16_t bcdDFUVersion;
		} __attribute__((__packed__));

		struct dfu_dev {
			uint16_t vid;
			uint16_t pid;
			uint8_t  bus;
			uint8_t  interface;
			uint16_t altsettings;
			uint8_t  device;
			uint8_t  path[8];
			uint8_t  iProduct[128];
			uint8_t  iInterface[128];
			uint32_t bMaxPacketSize0;
			struct   dfu_desc dfu_desc;
		};

		/* USB Device firmware Upgrade Specification, Revision 1.1 6.1.2 */
		/* p.20 */
		struct dfu_status {
			uint8_t bStatus;
			uint32_t bwPollTimeout;
			uint8_t bState;
			uint8_t iString;
		};

		/* USB Device firmware Upgrade Specification, Revision 1.1 6.1.2 */
		/* p.21 */
		enum dfu_dev_status {
			STATUS_OK              = 0x00,
			STATUS_errTARGET       = 0x01,
			STATUS_errFILE         = 0x02,
			STATUS_errWRITE        = 0x03,
			STATUS_errERASE        = 0x04,
			STATUS_errCHECK_ERASED = 0x05,
			STATUS_errPROG         = 0x06,
			STATUS_errVERIFY       = 0x07,
			STATUS_errADDRESS      = 0x08,
			STATUS_errNOTDONE      = 0x09,
			STATUS_errFIRMWARE     = 0x0A,
			STATUS_errVENDOR       = 0x0B,
			STATUS_errUSBR         = 0x0C,
			STATUS_errPOR          = 0x0D,
			STATUS_errUNKNOWN      = 0x0E,
			STATUS_errSTALLEDPKT   = 0x0F
		};

		std::map<uint8_t, std::string> dfu_dev_status_val = {
			{STATUS_OK,              "STATUS_OK"},
			{STATUS_errTARGET,       "STATUS_errTARGET"},
			{STATUS_errFILE,         "STATUS_errFILE"},
			{STATUS_errWRITE,        "STATUS_errWRITE"},
			{STATUS_errERASE,        "STATUS_errERASE"},
			{STATUS_errCHECK_ERASED, "STATUS_errCHECK_ERASED"},
			{STATUS_errPROG,         "STATUS_errPROG"},
			{STATUS_errVERIFY,       "STATUS_errVERIFY"},
			{STATUS_errADDRESS,      "STATUS_errADDRESS"},
			{STATUS_errNOTDONE,      "STATUS_errNOTDONE"},
			{STATUS_errFIRMWARE,     "STATUS_errFIRMWARE"},
			{STATUS_errVENDOR,       "STATUS_errVENDOR"},
			{STATUS_errUSBR,         "STATUS_errUSBR"},
			{STATUS_errPOR,          "STATUS_errPOR"},
			{STATUS_errUNKNOWN,      "STATUS_errUNKNOWN"},
			{STATUS_errSTALLEDPKT, 	 "STATUS_errSTALLEDPKT"}
		};

		/* p.22 */
		enum dfu_dev_state {
			STATE_appIDLE                = 0,
			STATE_appDETACH              = 1,
			STATE_dfuIDLE                = 2,
			STATE_dfuDNLOAD_SYNC         = 3,
			STATE_dfuDNBUSY              = 4,
			STATE_dfuDNLOAD_IDLE         = 5,
			STATE_dfuMANIFEST_SYNC       = 6,
			STATE_dfuMANIFEST            = 7,
			STATE_dfuMANIFEST_WAIT_RESET = 8,
			STATE_dfuUPLOAD_IDLE         = 9,
			STATE_dfuERROR               = 10
		};

		std::map <uint8_t, std::string> dfu_dev_state_val = {
			{STATE_appIDLE,                "STATE_appIDLE"},
			{STATE_appDETACH,              "STATE_appDETACH"},
			{STATE_dfuIDLE,                "STATE_dfuIDLE"},
			{STATE_dfuDNLOAD_SYNC,         "STATE_dfuDNLOAD-SYNC"},
			{STATE_dfuDNBUSY,              "STATE_dfuDNBUSY"},
			{STATE_dfuDNLOAD_IDLE,         "STATE_dfuDNLOAD-IDLE"},
			{STATE_dfuMANIFEST_SYNC,       "STATE_dfuMANIFEST-SYNC"},
			{STATE_dfuMANIFEST,            "STATE_dfuMANIFEST"},
			{STATE_dfuMANIFEST_WAIT_RESET, "STATE_dfuMANIFEST-WAIT-RESET"},
			{STATE_dfuUPLOAD_IDLE,         "STATE_dfuUPLOAD-IDLE"},
			{STATE_dfuERROR,               "STATE_dfuERROR"}
		};

		/*!
		 * \brief take control to the DFU device
		 * \param[in] index: device index in dfu_dev list
		 * \return EXIT_FAILURE when open device fails, EXIT_SUCCESS otherwise
		 */
		int open_DFU(int index);
		/*!
		 * \brief release control to the DFU device
		 * \return EXIT_FAILURE when close fails, EXIT_SUCCESS otherwise
		 */
		int close_DFU();
		/*!
		 * \brief send detach command
		 * return EXIT_FAILURE when transaction fails, EXIT_SUCCESS otherwise
		 */
		int dfu_detach();
		/*!
		 * \brief read device status
		 * \param[out] status: struct dfu_status device payload
		 * return error/return code from libusb or number of bytes read/write
		 */
		int get_status(struct dfu_status *status);
		/*!
		 * \brief move through DFU state from current state to newState
		 * \param[in] newState: targeted state
		 * \return -1 when something fail, 0 otherwise
		 */
		int set_state(char newState);
		/*!
		 * \brief get current state without changing DFU state (6.1.5)
		 * \return -1 when USB transaction fail, state otherwise
		 */
		char get_state();
		/*!
		 * \brief poll status until device is in state mode
		 * \param[in] targeted state
		 * \return value < 0 when transaction fails, new state otherwise
		 */
		int poll_state(uint8_t state);
		/*!
		 * \brief verify if DFU device always exist
		 * \return false if lost, true if present
		 */
		bool checkDevicePresent();
		/*!
		 * \brief send an IN/OUT request
		 */
		int send(bool out, uint8_t brequest, uint16_t wvalue,
				unsigned char *data, uint16_t length);
		/*!
		 * \brief fill specific DFU structure with extra descriptor
		 * \param[in] intf: interface descriptor with extra area
		 * \param[out] dfu_desc: DFU descriptor
		 * \param[in] dfu_desc_size: DFU descriptor structure size
		 * \return -1 if extra len is too small, 0 otherwise
		 * */
		int parseDFUDescriptor(const struct libusb_interface_descriptor *intf,
				uint8_t *dfu_desc, int dfu_desc_size);
		/*!
		 * \brief try to open device specified by vid and pid. If found
		 *        search for compatible interface
		 * \param[in] vid: USB VID
		 * \param[in] pid: USB PID
		 * \return false when no device, unable to connect, or no DFU interface.
		 */
		bool searchWithVIDPID(uint16_t vid, uint16_t pid);
		/*!
		 * \brief search, for the specified device, if it has a DFU interface
		 * \param[in] dev: USB device
		 * \param[in] desc: USB device descriptor
		 * \return 1 when can't read config, 0 otherwise
		 */
		int searchIfDFU(struct libusb_device_handle *handle,
				struct libusb_device *dev,
				struct libusb_device_descriptor *desc);

		bool _verbose;                       /**< display more message */
		bool _debug;                         /**< display debug message */
		bool _quiet;						 /**< don't use progressBar */
		std::vector<struct dfu_dev> dfu_dev; /**< available dfu devices */
		int dev_idx;                         /**< device index in dfu_dev */
		uint16_t _vid;                       /**< device Vendor ID */
		uint16_t _pid;                       /**< device Product ID */
		int16_t _altsetting;                 /**< device altsetting */
		struct libusb_context *usb_ctx;      /**< usb context */
		libusb_device_handle * dev_handle;   /**< current device handle */
		int curr_intf;                       /**< device interface to use */
		int transaction;					 /**< download transaction ID */

		DFUFileParser *_bit;                 /**< dfu file to load */
};

#endif  // SRC_DFU_HPP_
