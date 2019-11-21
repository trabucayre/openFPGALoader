#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include <iostream>

#include <libudev.h>
#include <libusb.h>

#include "ftdipp_mpsse.hpp"

using namespace std;

//#define DEBUG 1
#define display(...) \
	do { if (_verbose) fprintf(stdout, __VA_ARGS__);}while(0)

FTDIpp_MPSSE::FTDIpp_MPSSE(const string &dev, unsigned char interface,
			   uint32_t clkHZ, bool verbose):_verbose(verbose), _vid(0),
				_pid(0), _bus(-1), _addr(-1), _product(""), _interface(interface),
				_clkHZ(clkHZ), _buffer_size(2*32768), _num(0)
{
	if (!search_with_dev(dev)) {
		cerr << "No cable found" << endl;
		throw std::exception();
	}

	open_device(115200);

	_buffer = (unsigned char *)malloc(sizeof(unsigned char) * _buffer_size);
	if (!_buffer) {
		cout << "_buffer malloc failed" << endl;
		throw std::exception();
	}
}

FTDIpp_MPSSE::FTDIpp_MPSSE(int vid, int pid, unsigned char interface,
			   uint32_t clkHZ, bool verbose):_verbose(verbose), _vid(vid),
			   _pid(pid), _bus(-1),
			   _addr(-1), _product(""), _interface(interface),
			   _clkHZ(clkHZ), _buffer_size(2*32768), _num(0)
{
	open_device(115200);

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

void FTDIpp_MPSSE::open_device(unsigned int baudrate)
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
		ret = ftdi_usb_open_desc(_ftdi, _vid, _pid, NULL, NULL);
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
		fprintf(stderr, "erreur baudrate\n");
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
		if (_ftdi->module_detach_mode == AUTO_DETACH_SIO_MODULE) {
			rtn = libusb_attach_kernel_driver(_ftdi->usb_dev, _ftdi->interface);
			if( rtn != 0)
				fprintf(stderr, "detach error %d\n", rtn);
		}
	}
	ftdi_usb_close_internal();

	ftdi_free(_ftdi);
	return EXIT_SUCCESS;
}



int FTDIpp_MPSSE::init(unsigned char latency, unsigned char bitmask_mode,
			   mpsse_bit_config & bit_conf)
{
	unsigned char buf_cmd[6] = { SET_BITS_LOW, 0, 0,
		SET_BITS_HIGH, 0, 0
	};

	if (ftdi_usb_reset(_ftdi) != 0) {
		cout << "erreur de reset" << endl;
		return -1;
	}

	if (ftdi_set_bitmode(_ftdi, 0x00, BITMODE_RESET) < 0) {
		cout << "erreur de bitmode_reset" << endl;
		return -1;
	}
	if (ftdi_usb_purge_buffers(_ftdi) != 0) {
		cout << "erreur de reset" << endl;
		return -1;
	}
	if (ftdi_set_latency_timer(_ftdi, latency) != 0) {
		cout << "erreur de reset" << endl;
		return -1;
	}
	/* enable MPSSE mode */
	if (ftdi_set_bitmode(_ftdi, bitmask_mode, BITMODE_MPSSE) < 0) {
		cout << "erreur de bitmode_mpsse" << endl;
		return -1;
	}

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

	return 0;
}

int FTDIpp_MPSSE::setClkFreq(uint32_t clkHZ)
{
	return setClkFreq(clkHZ, 0);
}

int FTDIpp_MPSSE::setClkFreq(uint32_t clkHZ, char use_divide_by_5)
{
	_clkHZ = clkHZ;

    uint8_t buffer[4] = { 0x08A, 0x86, 0x00, 0x00};
    uint32_t base_freq = 60000000;
    uint32_t real_freq = 0;
    uint16_t presc;

    if (use_divide_by_5) {
        base_freq /= 5;
        buffer[0] = 0x8B;
    }

    if ((use_divide_by_5 && _clkHZ > 6000000) || _clkHZ > 30000000) {
        fprintf(stderr, "Error: too fast frequency\n");
        return -1;
    }

    presc = (base_freq /(_clkHZ * 2)) -1;
    real_freq = base_freq / ((1+presc)*2);
    display("presc : %d input freq : %d requested freq : %d real freq : %d\n",
			presc, base_freq, _clkHZ, real_freq);
    buffer[2] = presc & 0xff;
    buffer[3] = (presc >> 8) & 0xff;

    if (ftdi_write_data(_ftdi, buffer, 4) != 4) {
        fprintf(stderr, "Error: write for frequency\n");
        return -1;
    }

    return real_freq;
}

int FTDIpp_MPSSE::mpsse_store(unsigned char c)
{
	return mpsse_store(&c, 1);
}

int FTDIpp_MPSSE::mpsse_store(unsigned char *buff, int len)
{
	unsigned char *ptr = buff;
	/* this case theorically never happen */
	if (len > _buffer_size) {
		mpsse_write();
		for (; len > _buffer_size; len -= _buffer_size) {
			memcpy(_buffer, ptr, _buffer_size);
			mpsse_write();
			ptr += _buffer_size;
		}
	}

	if (_num + len + 1 >= _buffer_size) {
		if (mpsse_write() < 0) {
			cout << "erreur de write_data dans " << __func__ << endl;
			return -1;
		}
	}
#ifdef DEBUG
	display("%s %d %d\n", __func__, _num, len);
#endif
	memcpy(_buffer + _num, ptr, len);
	_num += len;
	return 0;
}

int FTDIpp_MPSSE::mpsse_write()
{
	if (_num == 0)
		return 0;

#ifdef DEBUG
	display("%s %d\n", __func__, _num);
#endif

	if (ftdi_write_data(_ftdi, _buffer, _num) != _num) {
		cout << "erreur de write" << endl;
		return -1;
	}

	_num = 0;
	return 0;
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
