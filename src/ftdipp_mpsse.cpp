#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>

#ifdef USE_UDEV
#include <libudev.h>
#endif
#include <libusb.h>

#include "ftdipp_mpsse.hpp"

using namespace std;

//#define DEBUG 1
#define display(...) \
	do { if (_verbose) fprintf(stdout, __VA_ARGS__);}while(0)

FTDIpp_MPSSE::FTDIpp_MPSSE(const mpsse_bit_config &cable, const string &dev,
				const std::string &serial, uint32_t clkHZ, bool verbose):
				_verbose(verbose), _vid(0),
				_pid(0), _bus(-1), _addr(-1),
				_interface(cable.interface),
				_clkHZ(clkHZ), _buffer_size(2*32768), _num(0)
{
	sprintf(_product, "");
	if (!dev.empty()) {
		if (!search_with_dev(dev)) {
			cerr << "No cable found" << endl;
			throw std::exception();
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
		throw std::exception();
	}
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
		throw std::exception();
	}

	ftdi_set_interface(_ftdi, (ftdi_interface)_interface);
	if (_bus == -1 || _addr == -1)
		ret = ftdi_usb_open_desc(_ftdi, _vid, _pid, NULL, serial.empty() ? NULL : serial.c_str());
	else
#if (OLD_FTDI_VERSION == 1)
		ret = ftdi_usb_open_desc(_ftdi, _vid, _pid, _product, NULL);
#else
		ret = ftdi_usb_open_bus_addr(_ftdi, _bus, _addr);
#endif
	if (ret < 0) {
		fprintf(stderr, "unable to open ftdi device: %d (%s)\n",
			ret, ftdi_get_error_string(_ftdi));
		ftdi_free(_ftdi);
		throw std::exception();
	}
	if (ftdi_set_baudrate(_ftdi, baudrate) < 0) {
		fprintf(stderr, "baudrate error\n");
		close_device();
		throw std::exception();
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
	int rtn;
	if (_ftdi == NULL)
		return EXIT_FAILURE;

	/* purge FTDI */
	ftdi_usb_purge_rx_buffer(_ftdi);
	ftdi_usb_purge_tx_buffer(_ftdi);

	/*
	 * repompe de la fonction et des suivantes
	 */
	 if (_ftdi->usb_dev != NULL) {
		rtn = libusb_release_interface(_ftdi->usb_dev, _ftdi->interface);
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

	ftdi_free(_ftdi);
	return EXIT_SUCCESS;
}



int FTDIpp_MPSSE::init(unsigned char latency, unsigned char bitmask_mode,
				unsigned char mode,
			   mpsse_bit_config & bit_conf)
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

	if (ftdi_usb_purge_buffers(_ftdi) != 0) {
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

		if (setClkFreq(_clkHZ, 0) < 0)
			return -1;

		buf_cmd[1] = bit_conf.bit_low_val;  // 0xe8;
		buf_cmd[2] = bit_conf.bit_low_dir;  // 0xeb;

		buf_cmd[4] = bit_conf.bit_high_val;  // 0x00;
		buf_cmd[5] = bit_conf.bit_high_dir;  // 0x60;
		mpsse_store(buf_cmd, 6);
		mpsse_write();
	}

	ftdi_read_data_set_chunksize(_ftdi, _buffer_size);
	ftdi_write_data_set_chunksize(_ftdi, _buffer_size);

	return 0;
}

int FTDIpp_MPSSE::setClkFreq(uint32_t clkHZ)
{
	return setClkFreq(clkHZ, 0);
}

int FTDIpp_MPSSE::setClkFreq(uint32_t clkHZ, char use_divide_by_5)
{
	_clkHZ = clkHZ;

	int ret;
	uint8_t buffer[4] = { TCK_DIVISOR, 0x00, 0x00};
	uint32_t base_freq;
	uint32_t real_freq = 0;
	uint16_t presc;

	/* FT2232C has no divide by 5 instruction
	 * and default freq is 12MHz
	 */
	if (_ftdi->type != TYPE_2232C) {
		base_freq = 60000000;
		if (use_divide_by_5) {
			base_freq /= 5;
			mpsse_store(EN_DIV_5);
		} else {
			mpsse_store(DIS_DIV_5);
		}
	} else {
		base_freq = 12000000;
		use_divide_by_5 = false;
	}

    if ((use_divide_by_5 && _clkHZ > 6000000) || _clkHZ > 30000000) {
        fprintf(stderr, "Error: too fast frequency\n");
        return -1;
    }

    presc = (base_freq /(_clkHZ * 2)) -1;
    real_freq = base_freq / ((1+presc)*2);
	if (real_freq > clkHZ)
		presc ++;
    real_freq = base_freq / ((1+presc)*2);
    display("presc : %d input freq : %d requested freq : %d real freq : %d\n",
			presc, base_freq, _clkHZ, real_freq);
    buffer[1] = presc & 0xff;
    buffer[2] = (presc >> 8) & 0xff;

	mpsse_store(buffer, 3);
	ret = mpsse_write();
    if (ret < 0) {
        fprintf(stderr, "Error: write for frequency return %d\n", ret);
        return -1;
    }
	ret = ftdi_read_data(_ftdi, buffer, 4);
	ftdi_usb_purge_buffers(_ftdi);

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
			mpsse_write();
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
	mpsse_write();

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
