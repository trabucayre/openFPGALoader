// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2021 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "display.hpp"
#include "progressBar.hpp"

#include "dfu.hpp"

using namespace std;

/* USB request write */
static const uint8_t DFU_REQUEST_OUT = LIBUSB_ENDPOINT_OUT |
	LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
/* USB request read */
static const uint8_t DFU_REQUEST_IN = LIBUSB_ENDPOINT_IN |
	LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;

/* DFU command */
enum dfu_cmd {
	DFU_DETACH    = 0,
	DFU_DNLOAD    = 1,
	DFU_UPLOAD    = 2,
	DFU_GETSTATUS = 3,
	DFU_CLRSTATUS = 4,
	DFU_GETSTATE  = 5,
	DFU_ABORT     = 6
};

/* Question: -
 * - add vid/pid and override DFU file ?
 * - index as jtag chain (fix issue when more than one device connected)
 */

DFU::DFU(const string &filename, uint16_t vid, uint16_t pid,
		int16_t altsetting,
		int verbose_lvl):_verbose(verbose_lvl > 0),
		_quiet(verbose_lvl < 0), dev_idx(0), _vid(0), _pid(0),
		_altsetting(altsetting),
		usb_ctx(NULL), dev_handle(NULL), curr_intf(0), transaction(0),
		_bit(NULL)
{
	struct dfu_status status;
	int dfu_vid = 0, dfu_pid = 0;

	printInfo("Open file ", false);

	try {
		_bit = new DFUFileParser(filename, _verbose > 0);
		printSuccess("DONE");
	} catch (std::exception &e) {
		printError("FAIL");
		throw runtime_error("Error: Fail to open file");
	}

	printInfo("Parse file ", false);
	try {
		_bit->parse();
		printSuccess("DONE");
	} catch (std::exception &e) {
		printError("FAIL");
		delete _bit;
		throw runtime_error("Error: Fail to parse file");
	}

	if (_verbose > 0)
		_bit->displayHeader();

	/* get VID and PID from dfu file */
	try {
		dfu_vid = std::stoi(_bit->getHeaderVal("idVendor"), 0, 16);
		dfu_pid = std::stoi(_bit->getHeaderVal("idProduct"), 0, 16);
	} catch (std::exception &e) {
		if (_verbose)
			printWarn(e.what());
	}

	if (libusb_init(&usb_ctx) < 0) {
		delete _bit;
		throw std::runtime_error("libusb init failed");
	}

	/* no vid or pid provided by DFU file or by params */
	if ((dfu_vid == 0 || dfu_pid == 0) && (vid == 0 || pid == 0)) {
		// search all DFU compatible devices
		if (searchDFUDevices() != EXIT_SUCCESS) {
			delete _bit;
			throw std::runtime_error("Devices enumeration failed");
		}
	} else {
		bool found = false;
		if (dfu_vid != 0 && dfu_pid != 0)
			found = searchWithVIDPID(dfu_vid, dfu_pid);
		if (vid != 0 && pid != 0 && !found)
			found = searchWithVIDPID(vid, pid);
	}

	/* check if DFU compatible devices are present */
	if (dfu_dev.size() != 0) {
		/* more than one: only possible if file is not DFU */
		if (dfu_dev.size() > 1 && !filename.empty())
			throw std::runtime_error("Only one device supported");
	} else {
		throw std::runtime_error("No DFU compatible device found");
	}

	if (_verbose)
		displayDFU();

	/* don't try device without vid/pid */
	if (_vid == 0 || _pid == 0) {
		throw std::runtime_error("Can't open device vid/pid == 0");
	}

	/* open the first */
	if (open_DFU(0) == EXIT_FAILURE) {
		delete _bit;
		throw std::runtime_error("Fail to claim device");
	}

	printf("%02x %02x\n", _vid, _pid);

	char state = get_state();
	if (_verbose > 0) {
		printInfo("Default DFU status " + dfu_dev_state_val[state]);
		get_status(&status);
	}
}

DFU::~DFU()
{
	close_DFU();
	libusb_exit(usb_ctx);
	delete _bit;
}

/* open the device using VID and PID
 */
int DFU::open_DFU(int index)
{
	struct dfu_dev curr_dfu;

	if (_vid == 0 || _pid == 0) {
		printError("Error: Can't open device without VID/PID");
		return EXIT_FAILURE;
	}

	dev_idx = index;
	curr_dfu = dfu_dev[dev_idx];
	curr_intf = curr_dfu.interface;

	dev_handle = libusb_open_device_with_vid_pid(usb_ctx,
		curr_dfu.vid, curr_dfu.pid);
	if (!dev_handle) {
		printError("Error: fail to open device");
		return EXIT_FAILURE;
	}
	if (libusb_claim_interface(dev_handle, curr_intf) != 0) {
		libusb_close(dev_handle);
		printError("Error: fail to claim interface");
		return EXIT_FAILURE;
	}
	if (libusb_set_interface_alt_setting(dev_handle, curr_intf, 0) != 0) {
		libusb_release_interface(dev_handle, curr_intf);
		libusb_close(dev_handle);
		printError("Error: fail to set interface");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

/* if the is claimed close it */
int DFU::close_DFU() {
	if (dev_handle) {
		int ret;
		ret = libusb_release_interface(dev_handle, dfu_dev[dev_idx].interface);
		if (ret != 0) {
			/* device is already disconnected ... */
			if (ret == LIBUSB_ERROR_NO_DEVICE) {
				return EXIT_SUCCESS;
			} else {
				printError("Error: Fail to release interface");
				return EXIT_FAILURE;
			}
		}
		libusb_close(dev_handle);
		dev_handle = NULL;
	}
	return EXIT_SUCCESS;
}

/* search one device using VID/PID */
bool DFU::searchWithVIDPID(uint16_t vid, uint16_t pid)
{
	char mess[40];
	/* search device using vid/pid */
	libusb_device_handle *handle = NULL;

	/* try first by using VID:PID from DFU file */
	snprintf(mess, 40, "Open device %04x:%04x ",
			vid, pid);

	printInfo(mess, false);
	handle = libusb_open_device_with_vid_pid(usb_ctx, vid, pid);
	/* No device found */
	if (!handle) {
		printWarn("Not found");
		if (_verbose)
			printError("Error: unable to connect to device");
		return false;
	} else {
		printSuccess("DONE");
	}

	/* retrieve usb device structure */
	libusb_device *dev = libusb_get_device(handle);
	if (!dev) {
		libusb_close(handle);
		if (_verbose)
			printError("Error: unable to retrieve usb device");
		return false;
	}

	/* and device descriptor */
	struct libusb_device_descriptor desc;
	int r = libusb_get_device_descriptor(dev, &desc);
	if (r != 0) {
		libusb_close(handle);
		printError("Error: fail to retrieve usb descriptor");
		return false;
	}

	/* search if one descriptor is DFU compatible */
	int ret = searchIfDFU(handle, dev, &desc);
	if (ret == 1) {
		if (_verbose)
			printError("Error: No DFU interface");
	}
	_vid = vid;
	_pid = pid;

	libusb_close(handle);  // no more needed -> reopen after

	return (ret == 1) ? false : true;
}

/* Tree steps are required to discover all
 * DFU capable devices
 * 1. loop over devices
 */
int DFU::searchDFUDevices()
{
	int i = 0;
	libusb_device **dev_list;
	libusb_device *usb_dev;
	libusb_device_handle *handle;

	/* clear dfu list */
	dfu_dev.clear();

	/* iteration */
	ssize_t list_size = libusb_get_device_list(usb_ctx, &dev_list);
	if (_verbose)
		printInfo("found " + to_string(list_size) + " USB device");

	while ((usb_dev = dev_list[i++]) != NULL) {
		struct libusb_device_descriptor desc;
		if (libusb_get_device_descriptor(usb_dev, &desc) != 0) {
			printError("Unable to get device descriptor");
			return EXIT_FAILURE;
		}

		if (_verbose > 0) {
			printf("%04x:%04x (bus %d, device %2d)\n",
            	desc.idVendor, desc.idProduct,
            	libusb_get_bus_number(usb_dev),
				libusb_get_device_address(usb_dev));
		}

		libusb_open(usb_dev, &handle);

		if (searchIfDFU(handle, usb_dev, &desc) != 0) {
			return EXIT_FAILURE;
		}
		libusb_close(handle);
	}

	libusb_free_device_list(dev_list, 1);

	return EXIT_SUCCESS;
}

/* 2. loop over configuration
 */
int DFU::searchIfDFU(struct libusb_device_handle *handle,
		struct libusb_device *dev,
		struct libusb_device_descriptor *desc)
{
	/* configuration descriptor iteration */
	for (int i = 0; i < desc->bNumConfigurations; i++) {
		struct libusb_config_descriptor *cfg;
		int ret = libusb_get_config_descriptor(dev, i, &cfg);
		if (ret != 0) {
			printError("Fail to retrieve config_descriptor " + to_string(i));
			return 1;
		}
		/* configuration interface iteration */
		for (int if_idx=0; if_idx < cfg->bNumInterfaces; if_idx++) {
			const struct libusb_interface *uif = &cfg->interface[if_idx];

			/* altsettings interation */
			for (int intf_idx = 0; intf_idx < uif->num_altsetting; intf_idx++) {
				const struct libusb_interface_descriptor *intf = &uif->altsetting[intf_idx];
				uint8_t intfClass = intf->bInterfaceClass;
				uint8_t intfSubClass = intf->bInterfaceSubClass;
				if (_altsetting != -1 && _altsetting != intf_idx)
					continue;
				if (intfClass == 0xfe && intfSubClass == 0x01) {
					struct dfu_dev my_dev;
					if (_verbose)
						printInfo("DFU found");

					/* dfu functional descriptor */
					if (parseDFUDescriptor(intf, reinterpret_cast<uint8_t *>(&my_dev.dfu_desc),
								sizeof(my_dev.dfu_desc)) != 0)
						continue;  // not compatible
					my_dev.vid = desc->idVendor;
					my_dev.pid = desc->idProduct;
					my_dev.altsettings = intf_idx;
					my_dev.interface = if_idx;
					my_dev.bus = libusb_get_bus_number(dev);
					my_dev.device = libusb_get_device_address(dev);
					my_dev.bMaxPacketSize0 = desc->bMaxPacketSize0;

					libusb_get_string_descriptor_ascii(handle, desc->iProduct,
						my_dev.iProduct, 128);
					libusb_get_string_descriptor_ascii(handle, intf->iInterface,
						my_dev.iInterface, 128);

					int r = libusb_get_port_numbers(dev, my_dev.path, sizeof(my_dev.path));
					my_dev.path[r] = '\0';
					dfu_dev.push_back(my_dev);
				}
			}
		}

		libusb_free_config_descriptor(cfg);
	}

	return 0;
}

/* 3. check if altsettings contains a valid 
 * dfu descriptor.
 * Since libusb has no support for those structure
 * fill a custom structure
 */
int DFU::parseDFUDescriptor(const struct libusb_interface_descriptor *intf,
		uint8_t *dfu_desc, int dfu_desc_size)
{
	const uint8_t *extra = intf->extra;
	int extra_len = intf->extra_length;

	if (extra_len < 9)
		return -1;

	/* map memory to structure */
	for (int j = 0; j + 1  < extra_len; j++) {
		if (extra[j+1] == 0x21) {
			memcpy(dfu_desc, (const void *)&extra[j], dfu_desc_size);
			break;
		}
	}

	return 0;
}

/* abstraction to send/receive to control */
int DFU::send(bool out, uint8_t brequest, uint16_t wvalue,
		unsigned char *data, uint16_t length)
{
	uint8_t type = out ? DFU_REQUEST_OUT : DFU_REQUEST_IN;
	int ret = libusb_control_transfer(dev_handle, type,
				brequest, wvalue, curr_intf,
				data, length, 5000);
	if (ret < 0) {
		if (checkDevicePresent()) {
			/* close device ? */
			return 0;
		}
	}
	return ret;
}

/* subset of state transitions
 */

int DFU::set_state(char newState)
{
	int ret = 0;
	struct dfu_status status;
	char curr_state = get_state();
	while (curr_state != newState) {
		switch (curr_state) {
		case STATE_appIDLE:
			if (dfu_detach() == EXIT_FAILURE)
				return -1;
			if (get_status(&status) <= 0)
				return -1;
			if (status.bState != STATE_appDETACH ||
					status.bStatus != STATUS_OK) {
				cerr << dfu_dev_status_val[status.bStatus] << endl;
				return -1;
			}
			curr_state = status.bState;
			break;
		case STATE_appDETACH:
			if (newState == STATE_appIDLE) {
				// ? reset + timeout ?
			} else {
				// reset
				// reenum
			}
			break;
		case STATE_dfuIDLE:
			/* better to test upload/download
			 * an handle others states */
			if (newState == STATE_appIDLE) {
				// reset
				// reenum
			} else {  // download or upload
					  // are handled by download() and upload()
				return -2;
			}
			break;
		case STATE_dfuDNLOAD_IDLE:
			if (newState == STATE_dfuMANIFEST_SYNC) {
				/* send the zero sized download request
				 * dfuDNLOAD_IDLE -> dfuMANITEST-SYNC */
				if (_verbose)
					printInfo("send zero sized download request");
				ret = send(true, DFU_DNLOAD, transaction, NULL, 0);
			} else {  // dfuIDLE
				ret = send(true, DFU_ABORT, 0, NULL, 0);
			}

			if (ret < 0) {
				printError("Fails to send packet\n");
				return ret;
			}
			if ((ret = get_state()) < 0)
				return ret;
			/* not always true:
			 * the newState may be the next one or another */
			if (ret != newState) {
				printError(dfu_dev_state_val[ret]);
				return -1;
			}
			curr_state = ret;
			break;
		case STATE_dfuERROR:
			if (newState == STATE_appIDLE) {
				ret = libusb_reset_device(dev_handle);
			} else {  // dfuIDLE
				ret = send(true, DFU_CLRSTATUS, 0, NULL, 0);
			}
			/* if command fails
			 * try to determine if device is lost
			 * or if it's another issue */
			if (ret < 0) {
				if (checkDevicePresent() == 1) {
					printError("Fails to send packet\n");
				} else {
					printInfo("device disconnected\n");
				}
				return ret;
			}
			/* now state must be appIDLE or dfuIDLE
			 * use GETSTATUS to check */
			if ((ret = get_status(&status)) <= 0)
				return ret;
			if (status.bState != newState ||
					status.bStatus != STATUS_OK) {
				cerr << dfu_dev_status_val[status.bStatus] << endl;
				return -1;
			}
			curr_state = status.bState;
			break;
		}
	}

	return ret;
}

/* detach device -> move from appIDLE to dfuIDLE
 */
int DFU::dfu_detach()
{
	int res = send(true, DFU_DETACH, 0, NULL, 0);
	return (res < 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}

int DFU::get_status(struct dfu_status *status)
{
	uint8_t buffer[6];
	int res;

	res = send(false, DFU_GETSTATUS, 0, buffer, 6);

	if (res == 6) {
		status->bStatus = buffer[0];
		status->bwPollTimeout = (((uint32_t)buffer[3] & 0xff) << 16) ||
								(((uint32_t)buffer[2] & 0xff) <<  8) ||
								(((uint32_t)buffer[1] & 0xff) <<  0);
		status->bState = buffer[4];
		status->iString = buffer[5];
	}
	return res;
}

/* read current device state -> unlike status
 * this request didn't change state
 */
char DFU::get_state()
{
	char c;

	int res = send(false, DFU_GETSTATE, 0, (unsigned char *)&c, 1);

	if (res != 1)
		return res;
	return c;
}

/* read status until device state match
 * wait for bwPollTimeout ms between each requests
 */
int DFU::poll_state(uint8_t state) {
	int ret = 0;
	struct dfu_status status;

	do {
		ret = get_status(&status);
		if (ret <= 0) {
			printError("Error: poll state " + string(libusb_error_name(ret)));
			break;
		}
		/* millisecond */
		usleep(1000 * status.bwPollTimeout);
	} while (status.bState != state &&
				status.bState != STATE_dfuERROR);

	return (ret > 0) ? status.bState : ret;
}

/* display details about device informations and capabilities
 */
void DFU::displayDFU()
{
	/* display dfu device */
	printf("Found DFU:\n");
	for (unsigned int i = 0; i < dfu_dev.size(); i++) {
		printf("%04x:%04x (bus %d, device %2d),",
            dfu_dev[i].vid, dfu_dev[i].pid,
            dfu_dev[i].bus, dfu_dev[i].device);
		printf(" path: %d",dfu_dev[i].path[0]);
		for (size_t j = 1; j < strlen(((const char *)dfu_dev[i].path)); j++)
			printf(".%d", dfu_dev[i].path[j]);
		printf(", alt: %d, iProduct \"%s\", iInterface \"%s\"",
				dfu_dev[i].altsettings,
				dfu_dev[i].iProduct, dfu_dev[i].iInterface);
		printf("\n");
		printf("\tDFU details\n");
		printf("\t\tbLength         %02x\n", dfu_dev[i].dfu_desc.bLength);
		printf("\t\tbDescriptorType %02x\n",
				dfu_dev[i].dfu_desc.bDescriptorType);
		printf("\t\tbmAttributes    %02x\n", dfu_dev[i].dfu_desc.bmAttributes);
		printf("\t\twDetachTimeOut  %04x\n",
				dfu_dev[i].dfu_desc.wDetachTimeOut);
		printf("\t\twTransferSize   %04d\n",
				libusb_le16_to_cpu(dfu_dev[i].dfu_desc.wTransferSize));
		printf("\t\tbcdDFUVersion   %04x\n",
				libusb_le16_to_cpu(dfu_dev[i].dfu_desc.bcdDFUVersion));
		uint8_t bmAttributes = dfu_dev[i].dfu_desc.bmAttributes;
		printf("\tDFU attributes %02x\n", bmAttributes);
		printf("\t\tDFU_DETACH            : ");
		if (bmAttributes & (1 << 3))
			printf("ON\n");
		else
			printf("OFF\n");
		printf("\t\tBitManifestionTolerant: ");
		if (bmAttributes & (1 << 2))
			printf("ON\n");
		else
			printf("OFF\n");
		printf("\t\tUPLOAD                : ");
		if (bmAttributes & (1 << 1))
			printf("ON\n");
		else
			printf("OFF\n");
		printf("\t\tDOWNLOAD              : ");
		if (bmAttributes & (1 << 0))
			printf("ON\n");
		else
			printf("OFF\n");
	}
}

/*!
 * \brief download filename content
 * \param[in] filename: name of the file to download
 * \return -1 when issue with file
 *         -2 when file parse fail
 */
int DFU::download()
{
	/* a device must be open. Can't try
	 * to download image on an enumerate device
	 */
	if (!dev_handle) {
		printError("Error: No device. Can't download file");
		return -1;
	}

	int ret, ret_val = EXIT_SUCCESS;
	uint8_t *buffer, *ptr;
	int size, xfer_len;
	int bMaxPacketSize0 = dfu_dev[dev_idx].bMaxPacketSize0;

	struct dfu_status status;
	struct dfu_dev curr_dev = dfu_dev[dev_idx];

	/* check if device allows download */
	if (!(curr_dev.dfu_desc.bmAttributes & (1 << 0))) {
		printError("Error: download is not supported by the device\n");
		return -1;
	}

	/* download must start in dfu IDLE state */
	if (get_state() != STATE_dfuIDLE)
		set_state(STATE_dfuIDLE);

	xfer_len = libusb_le16_to_cpu(curr_dev.dfu_desc.wTransferSize);
	if (xfer_len < bMaxPacketSize0)
		xfer_len = bMaxPacketSize0;

	size = _bit->getLength() / 8;
	buffer = _bit->getData();

	if (size == 0) {
		printError("Error: empty configuration file\n");
		return -3;
	}

	ptr = buffer;

	int max_iter = size / xfer_len;
	if (max_iter * xfer_len != size)
		max_iter++;

	ProgressBar progress("Loading", max_iter, 50, _quiet);

	/* send data configuration by up to xfer_len bytes */
	for (transaction = 0; transaction < max_iter; transaction++, ptr+=xfer_len) {
		int residual = size - (xfer_len * transaction);
		if (residual < xfer_len)
			xfer_len = residual;

		ret = send(true, DFU_DNLOAD, transaction,
				(xfer_len) ? ptr : NULL, xfer_len);
		if (ret != xfer_len) {  // can't be wrong here
			printf("Fails to send packet %s\n", libusb_error_name(ret));
			ret_val = -4;
			break;
		}

		/* wait until dfu state is again STATE_dfuDNLOAD_IDLE
		 * using status pollTimeout value
		 */
		ret_val = poll_state(STATE_dfuDNLOAD_IDLE);

		if (ret_val != STATE_dfuDNLOAD_IDLE) {
			printf("download: failed %d %d\n", ret_val, STATE_dfuDNLOAD_IDLE);
			break;
		}
		progress.display(transaction);
	}

	if (ret_val != STATE_dfuDNLOAD_IDLE) {
		progress.fail();
		ret_val = -5;
		return ret_val;
	}

	progress.done();

	/* send the zero sized download request
	 * dfuDNLOAD_IDLE -> dfuMANITEST-SYNC
	 */
	ret = set_state(STATE_dfuMANIFEST_SYNC);
	if (ret < 0) {
		printError("Error: fail to change state " + to_string(ret));
		return -6;
	}

	/* Now FSM must be in dfuMANITEST-SYNC */
	bool must_continue = true;
	do {
		ret = get_status(&status);
		if (ret != 6) {
			/* we consider device disconnected
			 * is ret == 0
			 */
			if (ret < 0 && ret != LIBUSB_ERROR_NO_DEVICE) {
				printf("Error: fail to get status %d\n", ret);
				printf("%s\n", libusb_error_name(ret));
				ret_val = ret;
			}
			break;
		}
		if (_verbose) {
			printInfo("status " + dfu_dev_state_val[status.bState] +
					" " + dfu_dev_status_val[status.bStatus]);
		}
		usleep(1000 * status.bwPollTimeout);

		switch (status.bState) {
			case STATE_dfuMANIFEST_SYNC:
			case STATE_dfuMANIFEST:
				break;
			/* need send reset */
			case STATE_dfuMANIFEST_WAIT_RESET:
				ret = libusb_reset_device(dev_handle);
				if (ret < 0) {
					/* dfu device may be disconnected */
					if (ret != LIBUSB_ERROR_NOT_FOUND) {
						printf("%s\n", libusb_error_name(ret));
						printf("ret %d\n", ret);
						ret_val = -7;
					}
				}
				must_continue = false;
				break;
			case STATE_dfuERROR:
				printError("Error: dfuERROR state\n");
				/* before quit try to cleanup the state */
				set_state(STATE_dfuIDLE);
				ret_val = -7;
				must_continue = false;
				break;
			case STATE_dfuIDLE:
			case STATE_appIDLE:
				must_continue = false;
				break;
		}
	} while (must_continue);

	return ret_val;
}

/* After download and manifest
 * device may reset itself with
 * a lost of connection
 * return 1 if the device is present
 *        0 if the device is lost
 */
bool DFU::checkDevicePresent()
{
	struct dfu_dev curr_dfu = dfu_dev[dev_idx];

	libusb_device_handle *handle;
	handle = libusb_open_device_with_vid_pid(usb_ctx,
			curr_dfu.vid, curr_dfu.pid);
	if (_verbose) {
		printInfo("device present ", false);
		if (handle)
			printInfo("True");
		else
			printInfo("False");
	}

	return (handle != NULL);
}
