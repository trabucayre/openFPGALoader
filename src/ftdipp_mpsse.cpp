// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>
#include <stdexcept>

#ifdef USE_UDEV
#include <libudev.h>
#endif
#include <libusb.h>

#include "display.hpp"
#include "ftdipp_mpsse.hpp"

using namespace std;

//#define DEBUG 1
#define display(...) \
	do { if (_verbose) fprintf(stdout, __VA_ARGS__);}while(0)

FTDIpp_MPSSE::FTDIpp_MPSSE(const mpsse_bit_config &cable, const string &dev,
				const std::string &serial, uint32_t clkHZ, uint8_t verbose):
				_verbose(verbose > 1), _cable(cable), _vid(0),
				_pid(0), _bus(-1), _addr(-1),
				_interface(cable.interface),
				_clkHZ(clkHZ), _buffer_size(2*32768), _num(0)
{
	strcpy(_product, "");
	if (!dev.empty()) {
		if (!search_with_dev(dev)) {
			cerr << "No cable found" << endl;
			throw std::runtime_error("No cable found");
		}
	} else {
		_vid = cable.vid;
		_pid = cable.pid;
	}

	open_device(serial, 115200);
	_buffer_size = _ftdi->max_packet_size;

	_buffer = (unsigned char *)malloc(sizeof(unsigned char) * _buffer_size);
	if (!_buffer) {
		cout << "_buffer malloc failed" << endl;
		throw std::runtime_error("_buffer malloc failed");
	}

	/* search for iProduct -> need to have
	 * ftdi->usb_dev (libusb_device_handler) -> libusb_device ->
	 * libusb_device_descriptor
	 */
	struct libusb_device * usb_dev = libusb_get_device(_ftdi->usb_dev);
	struct libusb_device_descriptor usb_desc;
	libusb_get_device_descriptor(usb_dev, &usb_desc);
	libusb_get_string_descriptor_ascii(_ftdi->usb_dev, usb_desc.iProduct,
		_iproduct, 200);
}

FTDIpp_MPSSE::~FTDIpp_MPSSE()
{
	ftdi_set_bitmode(_ftdi, 0, BITMODE_RESET);

	ftdi_usb_reset(_ftdi);
	close_device();
	free(_buffer);
}

void FTDIpp_MPSSE::open_device(const std::string &serial, unsigned int baudrate)
{
	int ret;

	display("try to open %x %x %d %d\n", _vid, _pid, _bus, _addr);

	_ftdi = ftdi_new();
	if (_ftdi == NULL) {
		cout << "open_device: failed to initialize ftdi" << endl;
		throw std::runtime_error("open_device: failed to initialize ftdi");
	}
#if (ATTACH_KERNEL && (FTDI_VERSION >= 105))
	_ftdi->module_detach_mode = AUTO_DETACH_REATACH_SIO_MODULE;
#endif

	ftdi_set_interface(_ftdi, (ftdi_interface)_interface);
	if (_bus == -1 || _addr == -1)
		ret = ftdi_usb_open_desc(_ftdi, _vid, _pid, NULL, serial.empty() ? NULL : serial.c_str());
	else
#if (FTDI_VERSION < 104)
		ret = ftdi_usb_open_desc(_ftdi, _vid, _pid, _product, NULL);
#else
		ret = ftdi_usb_open_bus_addr(_ftdi, _bus, _addr);
#endif
	if (ret < 0) {
		fprintf(stderr, "unable to open ftdi device: %d (%s)\n",
			ret, ftdi_get_error_string(_ftdi));
		ftdi_free(_ftdi);
		throw std::runtime_error("unable to open ftdi device");
	}
	if (ftdi_set_baudrate(_ftdi, baudrate) < 0) {
		fprintf(stderr, "baudrate error\n");
		close_device();
		throw std::runtime_error("baudrate error");
	}
}

/* cf. ftdi.c same function */
void FTDIpp_MPSSE::ftdi_usb_close_internal()
{
	libusb_close(_ftdi->usb_dev);
	_ftdi->usb_dev = NULL;
}

int FTDIpp_MPSSE::close_device()
{
	if (_ftdi == NULL)
		return EXIT_FAILURE;

	/* purge FTDI */
#if (FTDI_VERSION < 105)
	ftdi_usb_purge_rx_buffer(_ftdi);
	ftdi_usb_purge_tx_buffer(_ftdi);

	/*
	 * repompe de la fonction et des suivantes
	 */
	 if (_ftdi->usb_dev != NULL) {
		int rtn = libusb_release_interface(_ftdi->usb_dev, _ftdi->interface);
		if (rtn < 0) {
			fprintf(stderr, "release interface failed %d\n", rtn);
			return EXIT_FAILURE;
		}
#ifdef ATTACH_KERNEL
		/* libusb_attach_kernel_driver is only available on Linux. */
		if (_ftdi->module_detach_mode == AUTO_DETACH_SIO_MODULE) {
			rtn = libusb_attach_kernel_driver(_ftdi->usb_dev, _ftdi->interface);
			if( rtn != 0)
				fprintf(stderr, "detach error %d\n", rtn);
		}
#endif
	}
	ftdi_usb_close_internal();
#else
	ftdi_tciflush(_ftdi);
	ftdi_tcoflush(_ftdi);
	ftdi_usb_close(_ftdi);
#endif

	ftdi_free(_ftdi);
	return EXIT_SUCCESS;
}



int FTDIpp_MPSSE::init(unsigned char latency, unsigned char bitmask_mode,
				unsigned char mode)
{
	unsigned char buf_cmd[6] = { SET_BITS_LOW, 0, 0,
		SET_BITS_HIGH, 0, 0
	};

	if (ftdi_usb_reset(_ftdi) != 0) {
		cout << "reset error" << endl;
		return -1;
	}

	if (ftdi_set_bitmode(_ftdi, 0x00, BITMODE_RESET) < 0) {
		cout << "bitmode_reset error" << endl;
		return -1;
	}

#if (FTDI_VERSION < 105)
	if (ftdi_usb_purge_buffers(_ftdi) != 0) {
#else
	if (ftdi_tcioflush(_ftdi) != 0) {
#endif
		cout << "reset error" << endl;
		return -1;
	}
	if (ftdi_set_latency_timer(_ftdi, latency) != 0) {
		cout << "reset error" << endl;
		return -1;
	}
	/* enable mode */
	if (ftdi_set_bitmode(_ftdi, bitmask_mode, mode) < 0) {
		cout << "bitmode_mpsse error" << endl;
		return -1;
	}
	if (mode == BITMODE_MPSSE) {

		unsigned char buf1[5];
		ftdi_read_data(_ftdi, buf1, 5);

		if (setClkFreq(_clkHZ) < 0)
			return -1;

		int to_wr = 3;

		buf_cmd[1] = _cable.bit_low_val;  // 0xe8;
		buf_cmd[2] = _cable.bit_low_dir;  // 0xeb;

		if (_ftdi->type != TYPE_4232H) {
			buf_cmd[4] = _cable.bit_high_val;  // 0x00;
			buf_cmd[5] = _cable.bit_high_dir;  // 0x60;
			to_wr = 6;
		}
		mpsse_store(buf_cmd, to_wr);
		mpsse_write();
	}

	ftdi_read_data_set_chunksize(_ftdi, _buffer_size);
	ftdi_write_data_set_chunksize(_ftdi, _buffer_size);

	return 0;
}

int FTDIpp_MPSSE::setClkFreq(uint32_t clkHZ)
{
	int ret;
	bool use_divide_by_5;
	uint8_t buffer[4] = { TCK_DIVISOR, 0x00, 0x00};
	uint32_t base_freq;
	float real_freq = 0;
	uint16_t presc;

	_clkHZ = clkHZ;

	/* FT2232C has no divide by 5 instruction
	 * and default freq is 12MHz
	 */
	if (_ftdi->type != TYPE_2232C) {
		base_freq = 60000000;
		/* use full speed only when freq > 6MHz
		 * => more freq resolution using
		 * 2^16 to describe 0 -> 6MHz
		 */
		if (clkHZ > 6000000) {
			use_divide_by_5 = false;
			mpsse_store(DIS_DIV_5);
		} else {
			use_divide_by_5 = true;
			base_freq /= 5;
			mpsse_store(EN_DIV_5);
		}
	} else {
		base_freq = 12000000;
		use_divide_by_5 = false;
	}

	if (use_divide_by_5) {
		if (_clkHZ > 6000000) {
			printWarn("Jtag probe limited to 6MHz");
			_clkHZ = 6000000;
		}
	} else {
		if (_clkHZ > 30000000) {
			printWarn("Jtag probe limited to 30MHz");
			_clkHZ = 30000000;
		}
	}

	presc = ((base_freq /_clkHZ) -1) / 2;
	real_freq = base_freq / ((1+presc)*2);
	if (real_freq > _clkHZ)
		presc ++;
	real_freq = base_freq / ((1+presc)*2);

	/* just to have a better display */
	string clkHZ_str(10, ' ');
	string real_freq_str(10, ' ');
	if (clkHZ >= 1e6)
		snprintf(&clkHZ_str[0], 9, "%2.2fMHz", clkHZ / 1e6);
	else if (clkHZ >= 1e3)
		snprintf(&clkHZ_str[0], 10, "%3.2fKHz", clkHZ / 1e3);
	else
		snprintf(&clkHZ_str[0], 10, "%3d.00Hz", clkHZ);
	if (real_freq >= 1e6)
		snprintf(&real_freq_str[0], 9, "%2.2fMHz", real_freq / 1e6);
	else if (real_freq >= 1e3)
		snprintf(&real_freq_str[0], 10, "%3.2fKHz", real_freq / 1e3);
	else
		snprintf(&real_freq_str[0], 10, "%3.2fHz", real_freq);


	printInfo("Jtag frequency : requested " + clkHZ_str +
			" -> real " + real_freq_str);
	display("presc : %d input freq : %d requested freq : %d real freq : %f\n",
			presc, base_freq, _clkHZ, real_freq);

	/*printf("base freq %d div by 5 %c presc %d\n", base_freq, (use_divide_by_5)?'1':'0',
			presc); */


	buffer[1] = presc & 0xff;
	buffer[2] = (presc >> 8) & 0xff;

	mpsse_store(buffer, 3);
	ret = mpsse_write();
	if (ret < 0) {
		fprintf(stderr, "Error: write for frequency return %d\n", ret);
		return -1;
	}
	ret = ftdi_read_data(_ftdi, buffer, 4);
#if (FTDI_VERSION < 105)
	ftdi_usb_purge_buffers(_ftdi);
#else
	ftdi_tcioflush(_ftdi);
#endif

	_clkHZ = real_freq;

	return real_freq;
}

int FTDIpp_MPSSE::mpsse_store(unsigned char c)
{
	return mpsse_store(&c, 1);
}

int FTDIpp_MPSSE::mpsse_store(unsigned char *buff, int len)
{
	unsigned char *ptr = buff;
	int store_size;
	/* check if _buffer as space to store all */
	if (_num + len > _buffer_size) {
		/* flush buffer if already full */
		if (_num == _buffer_size)
			if (mpsse_write() == -1)
				printError("mpsse_store: Fails to first flush");
		/* loop until loop < _buffer_size */
		while (_num + len > _buffer_size) {
			/* we now have len enough to fill
			 * buffer -> just complete buffer
			 */
			store_size = _buffer_size - _num;
			memcpy(_buffer + _num, ptr, store_size);
			_num += store_size;
			if (mpsse_write() < 0) {
				cout << "write_data error in " << __func__ << endl;
				return -1;
			}
			ptr += store_size;
			len -= store_size;
		}

	}
#ifdef DEBUG
	display("%s %d %d\n", __func__, _num, len);
#endif
	if (len > 0) {
		memcpy(_buffer + _num, ptr, len);
		_num += len;
	}
	return 0;
}

int FTDIpp_MPSSE::mpsse_write()
{
	int ret;
	if (_num == 0)
		return 0;

#ifdef DEBUG
	display("%s %d\n", __func__, _num);
#endif

	if ((ret = ftdi_write_data(_ftdi, _buffer, _num)) != _num) {
		cout << "write error: " << ret << " instead of " << _num << endl;
		return ret;
	}

	_num = 0;
	return ret;
}

int FTDIpp_MPSSE::mpsse_read(unsigned char *rx_buff, int len)
{
	int n;
	int num_read = 0;
	unsigned char *p = rx_buff;

	/* force buffer transmission before read */
	mpsse_store(SEND_IMMEDIATE);
	if (mpsse_write() == -1)
		printError("mpsse_read: fails to write");

	do {
		n = ftdi_read_data(_ftdi, p, len);
		if (n < 0) {
			fprintf(stderr, "Error: ftdi_read_data in %s", __func__);
			return -1;
		}
#ifdef DEBUG
		if (_verbose) {
			display("%s %d\n", __func__, n);
			for (int i = 0; i < n; i++)
				display("\t%s %x\n", __func__, p[i]);
		}
#endif

		len -= n;
		p += n;
		num_read += n;
	} while (len > 0);
	return num_read;
}

/**
 * Read GPIO (xCBUSy + xDBUSy) bank
 * @return pins state
 */
uint16_t FTDIpp_MPSSE::gpio_get()
{
	uint8_t tx[2] = {GET_BITS_LOW, GET_BITS_HIGH};
	uint8_t rx[2];
	mpsse_store(tx, 2);
	mpsse_read(rx, 2);

	return (rx[1] << 8) | rx[0];
}

/**
 * Read low (xCBUSy) or high (xDBUSy) pins.
 * @param[in] low_pins: if true read low, read high otherwise
 * @return pins state
 */
uint8_t FTDIpp_MPSSE::gpio_get(bool low_pins)
{
	uint8_t rx;

	/* select between high and low pins */
	mpsse_store((low_pins) ? GET_BITS_LOW : GET_BITS_HIGH);
	mpsse_read(&rx, 1);
	return rx;
}

/**
 * Set one or more pins of the full bank (CBUS + DBUS).
 * @param[in] pins bitmask
 * @return false when error, true otherwise
 */
bool FTDIpp_MPSSE::gpio_set(uint16_t gpios)
{
	if (gpios & 0x00ff) {
		_cable.bit_low_val |= (0xff & gpios);
		__gpio_write(true);
	}
	if (gpios & 0xff00) {
		_cable.bit_high_val |= (0xff & (gpios >> 8));
		__gpio_write(false);
	}
	return (mpsse_write() >= 0);
}

/**
 * Set one or more pins of the given half bank (CBUS or BDUS).
 * @param[in] pin bitmask
 * @param[in] low/high half bank
 * @return false when error, true otherwise
 */
bool FTDIpp_MPSSE::gpio_set(uint8_t gpios, bool low_pins)
{
	if (low_pins)
		_cable.bit_low_val |= gpios;
	else
		_cable.bit_high_val |= gpios;
	__gpio_write(low_pins);
	return (mpsse_write() >= 0);
}

/**
 * Clear one or more pins of the full bank (CBUS + DBUS).
 * @param[in] pins bitmask
 * @return false when error, true otherwise
 */
bool FTDIpp_MPSSE::gpio_clear(uint16_t gpios)
{
	if (gpios & 0x00ff) {
		_cable.bit_low_val &= ~(0xff & gpios);
		__gpio_write(true);
	}
	if (gpios & 0xff00) {
		_cable.bit_high_val &= ~(0xff & (gpios >> 8));
		__gpio_write(false);
	}
	return (mpsse_write() >= 0);
}

/**
 * Clear one or more pins of the given half bank (CBUS or BDUS).
 * @param[in] pin bitmask
 * @param[in] low/high half bank
 * @return false when error, true otherwise
 */
bool FTDIpp_MPSSE::gpio_clear(uint8_t gpios, bool low_pins)
{
	if (low_pins)
		_cable.bit_low_val &= ~(gpios);
	else
		_cable.bit_high_val &= ~(gpios);

	__gpio_write(low_pins);
	return (mpsse_write() >= 0);
}

/**
 * Full bank write
 * @param[in] GPIOs bitmask
 * @return false when error, true otherwise
 */
bool FTDIpp_MPSSE::gpio_write(uint16_t gpio)
{
	_cable.bit_low_val = (0xff & gpio);
	_cable.bit_high_val = (0xff & (gpio >> 8));
	__gpio_write(true);
	__gpio_write(false);
	return (mpsse_write() >= 0);
}

/**
 * Half bank write
 * @param[in] gpio: pins values
 * @param[in] low_pins: high/low half bank
 * @return false when error, true otherwise
 */
bool FTDIpp_MPSSE::gpio_write(uint8_t gpio, bool low_pins)
{
	if (low_pins)
		_cable.bit_low_val = gpio;
	else
		_cable.bit_high_val = gpio;

	__gpio_write(low_pins);
	return (mpsse_write() >= 0);
}

/**
 * update half bank pins direction (no write is done at this time)
 * @param[in] dir: pins direction (1 out, 0 in) for low or high bank
 * @param[in] low_pins: high/low half bank
 */
void FTDIpp_MPSSE::gpio_set_dir(uint8_t dir, bool low_pins)
{
	if (low_pins)
		_cable.bit_low_dir = dir;
	else
		_cable.bit_high_dir = dir;
}

/**
 * update full bank pins direction (no write is done at this time)
 * @param[in] dir: pins direction (1 out, 0 in) for low or high bank
 */
void FTDIpp_MPSSE::gpio_set_dir(uint16_t dir)
{
	_cable.bit_low_dir = dir & 0xff;
	_cable.bit_high_dir = (dir >> 8) & 0xff;
}

/**
 * configure low or high pins as input
 * @param[in] gpio: pins bitmask
 * @param[int] low_pins: select between CBUS or DBUS
 */
void FTDIpp_MPSSE::gpio_set_input(uint8_t gpio, bool low_pins)
{
	if (low_pins)
		_cable.bit_low_dir &= ~gpio;
	else
		_cable.bit_high_dir &= ~gpio;
}

/**
 * configure pins as input
 * @param[in] gpio: pins bitmask
 */
void FTDIpp_MPSSE::gpio_set_input(uint16_t gpio)
{
	if (gpio & 0x00ff)
		_cable.bit_low_dir &= ~(gpio & 0x00ff);
	if (gpio & 0xff00)
		_cable.bit_high_dir &= ~((gpio >> 8) & 0x00ff);
}

/**
 * configure low or high pins as output
 * @param[in] gpio: pins bitmask
 * @param[int] low_pins: select between CBUS or DBUS
 */
void FTDIpp_MPSSE::gpio_set_output(uint8_t gpio, bool low_pins)
{
	if (low_pins)
		_cable.bit_low_dir |= gpio;
	else
		_cable.bit_high_dir |= gpio;
}

/**
 * configure pins as output
 * @param[in] gpio: pins bitmask
 */
void FTDIpp_MPSSE::gpio_set_output(uint16_t gpio)
{
	if (gpio & 0x00ff)
		_cable.bit_low_dir |= (gpio & 0x00ff);
	if (gpio & 0xff00)
		_cable.bit_high_dir |= ((gpio >> 8) & 0x00ff);
}

/**
 * private method to write ftdi half bank GPIOs (pins state are in _cable)
 * @param[in] low or high half bank
 */
bool FTDIpp_MPSSE::__gpio_write(bool low_pins)
{
	uint8_t tx[3];
	tx[0] = ((low_pins) ? SET_BITS_LOW       : SET_BITS_HIGH);
	tx[1] = ((low_pins) ? _cable.bit_low_val : _cable.bit_high_val);
	tx[2] = ((low_pins) ? _cable.bit_low_dir : _cable.bit_high_dir);
	return (mpsse_store(tx, 3) >= 0);
}

#ifdef USE_UDEV
unsigned int FTDIpp_MPSSE::udevstufftoint(const char *udevstring, int base)
{
	char *endp;
	int ret;
	errno = 0;

	if (udevstring == NULL)
		return (-1);

	ret = (unsigned int)strtol(udevstring, &endp, base);
	if (errno) {
		fprintf(stderr,
			"udevstufftoint: Unable to parse number Error : %s (%d)\n",
			strerror(errno), errno);
		return (-2);
	}
	if (endp == optarg) {
		fprintf(stderr, "udevstufftoint: No digits were found\n");
		return (-3);
	}
	return (ret);
}

bool FTDIpp_MPSSE::search_with_dev(const string &device)
{
	struct udev *udev;
	struct udev_device *dev, *usbdeviceparent;
	char devtype;

	struct stat statinfo;
	if (stat(device.c_str(), &statinfo) < 0) {
		printf("unable to stat file\n");
		return false;
	}

	/* get device type */
	switch (statinfo.st_mode & S_IFMT) {
	case S_IFBLK:
		devtype = 'b';
		break;
	case S_IFCHR:
		devtype = 'c';
		break;
	default:
		printf("not char or block device\n");
		return false;
	}

	/* Create the udev object */
	udev = udev_new();
	if (!udev) {
		printf("Can't create udev\n");
		return false;
	}

	dev = udev_device_new_from_devnum(udev, devtype, statinfo.st_rdev);

	if (dev == NULL) {
		printf("no dev\n");
		udev_device_unref(dev);
		udev_unref(udev);
		return false;
	}

	/* Get closest usb device parent (we need VIP/PID)  */
	usbdeviceparent =
		udev_device_get_parent_with_subsystem_devtype(dev, "usb",
							  "usb_device");
	if (!usbdeviceparent) {
		printf
			("Unable to find parent usb device! Is this actually an USB device ?\n");
		udev_device_unref(dev);
		udev_unref(udev);
		return false;
	}

	_bus = udevstufftoint(udev_device_get_sysattr_value(
				usbdeviceparent, "busnum"), 10);
	_addr = udevstufftoint(udev_device_get_sysattr_value(
				usbdeviceparent, "devnum"), 10);
	sprintf(_product, "%s", udev_device_get_sysattr_value(usbdeviceparent, "product"));
	_vid = udevstufftoint(
		udev_device_get_sysattr_value(usbdeviceparent, "idVendor"), 16);
	_pid = udevstufftoint(udev_device_get_sysattr_value(
		usbdeviceparent, "idProduct"), 16);

	display("vid %x pid %x bus %d addr %d product name : %s\n", _vid, _pid, _bus, _addr, _product);

	return true;
}
#else
unsigned int FTDIpp_MPSSE::udevstufftoint(const char *udevstring, int base)
{
	(void)udevstring;
	(void)base;
	return 0;
}
bool FTDIpp_MPSSE::search_with_dev(const string &device)
{
	(void)device;
	return false;
}
#endif
