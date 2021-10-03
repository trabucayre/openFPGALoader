// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2021 Uwe Bonnes <bon@elektron.ikp.physik.tu-darmstadt.de>
 */

#include "bmp.hpp"
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
# include<stdarg.h>
#include <fcntl.h>
#include <unistd.h>

#include "display.hpp"

#include "bmp_remote.h"

#define BMP_IDSTRING "usb-Black_Sphere_Technologies_Black_Magic_Probe"

#define REMOTE_MAX_MSG_SIZE (1024)

#define FREQ_FIXED -1

void Bmp::DEBUG_WIRE(const char *format, ...)
{
	if (!_verbose)
		return;
	va_list ap;
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}

static const char hexdigits[] = "0123456789abcdef";
static void open_bmp(std::string dev, const std::string &serial);

Bmp::Bmp(std::string dev,
		 const std::string &serial, uint32_t clkHZ, bool verbose)
{
	_verbose = verbose;
	open_bmp(dev, serial);
	char construct[REMOTE_MAX_MSG_SIZE];
	int c = snprintf(construct, REMOTE_MAX_MSG_SIZE, "%s", REMOTE_START_STR);
	platform_buffer_write(construct, c);
	c = platform_buffer_read(construct, REMOTE_MAX_MSG_SIZE);
	if ((c < 1) || (construct[0] == REMOTE_RESP_ERR)) {
		printWarn("Remote Start failed, error " +
				   c ? (char *)&(construct[1]) : "unknown");
		throw std::runtime_error("remote_init failed");
	}
	printInfo("Remote is " + std::string(&construct[1]));

	/* Init JTAG */
	c = snprintf((char *)construct, REMOTE_MAX_MSG_SIZE, "%s",
				 REMOTE_JTAG_INIT_STR);
	platform_buffer_write(construct, c);
	c = platform_buffer_read(construct, REMOTE_MAX_MSG_SIZE);
	if ((c < 0) || (construct[0] == REMOTE_RESP_ERR)) {
		printWarn("jtagtap_init failed, error " +
				   c ? (char *)&(construct[1]) : "unknown");
		throw std::runtime_error("jtag_init failed");
	}
	/* Get Version*/
	int s = snprintf((char *)construct, REMOTE_MAX_MSG_SIZE, "%s",
					 REMOTE_HL_CHECK_STR);
	platform_buffer_write(construct, s);
	s = platform_buffer_read(construct, REMOTE_MAX_MSG_SIZE);
#define NEEDED_BMP_VERSION 2
	if ((s < 0) || (construct[0] == REMOTE_RESP_ERR) ||
		((construct[1] - '0') <	 NEEDED_BMP_VERSION)) {
		printWarn("Please update BMP, expected version " +
				  std::to_string(NEEDED_BMP_VERSION) + ", got " +
				  construct[1]);
		return;
	}

	setClkFreq(clkHZ);
};

int Bmp::setClkFreq(uint32_t clkHZ)
{
	char construct[REMOTE_MAX_MSG_SIZE];
	int s;
	s = snprintf(construct, REMOTE_MAX_MSG_SIZE, REMOTE_FREQ_SET_STR,
				 clkHZ);
	platform_buffer_write(construct, s);

	s = platform_buffer_read(construct, REMOTE_MAX_MSG_SIZE);

	if ((s < 1) || (construct[0] == REMOTE_RESP_ERR)) {
		printWarn("Update Firmware to allow to set max SWJ frequency\n");
	}
	s = snprintf((char *)construct, REMOTE_MAX_MSG_SIZE,"%s",
				 REMOTE_FREQ_GET_STR);
	platform_buffer_write(construct, s);

	s = platform_buffer_read(construct, REMOTE_MAX_MSG_SIZE);

	if ((s < 1) || (construct[0] == REMOTE_RESP_ERR))
		return FREQ_FIXED;

	uint32_t freq[1];
	unhexify(freq, &construct[1], 4);
	printInfo("Running at " + std::to_string(freq[0]) + " Hz");
	_clkHZ = freq[0];
	return freq[0];
}

int Bmp::writeTMS(uint8_t *tms, int len, bool flush_buffer)
{
	char construct[REMOTE_MAX_MSG_SIZE];
	int s;
	uint32_t tms_word;
	int ret = len;
	while (len) {
		int chunk = len;
		if (chunk > 32)
			chunk = 32;
		tms_word = *tms++;
		if (chunk > 8)
			tms_word |= *tms++ << 8;
		if (chunk > 16)
			tms_word |= *tms++ << 16;
		if (chunk > 24)
			tms_word |= *tms++ << 24;
		len -= chunk;
		s = snprintf((char *)construct, REMOTE_MAX_MSG_SIZE,
					 REMOTE_JTAG_TMS_STR, chunk, tms_word);
		platform_buffer_write(construct, s);

		s = platform_buffer_read(construct, REMOTE_MAX_MSG_SIZE);
		if ((s < 1) || (construct[0] == REMOTE_RESP_ERR)) {
			printWarn("jtagtap_tms_seq failed, error \n" +
					   s ? (char *)&(construct[1]) : "unknown");
			throw std::runtime_error("writeTMS failed");
		}
	}
	(void)flush_buffer;
	return ret;
}

int Bmp::writeTDI(uint8_t *tdi, uint8_t *tdo, uint32_t len, bool end)
{
	int ret = len;

	if(!len || (!tdi && !tdo))
		return len;
	while (len) {
		int chunk = (len > 4000) ? 4000 : len;
		len -= chunk;
		int byte_count = (chunk + 7) >> 3;
		char construct[REMOTE_MAX_MSG_SIZE];
		int s = snprintf(
			construct, REMOTE_MAX_MSG_SIZE,
			REMOTE_JTAG_IOSEQ_STR,
			(!len && end) ? REMOTE_IOSEQ_TMS : REMOTE_IOSEQ_NOTMS,
			(((tdi) ? REMOTE_IOSEQ_FLAG_IN	: REMOTE_IOSEQ_FLAG_NONE) |
			 ((tdo) ? REMOTE_IOSEQ_FLAG_OUT : REMOTE_IOSEQ_FLAG_NONE)),
			chunk);
		char *p = construct + s;
		if (tdi) {
			hexify(p, tdi, byte_count);
			p += 2 * byte_count;
			tdi += byte_count;
		}
		*p++ = REMOTE_EOM;
		*p	 = 0;
		platform_buffer_write(construct, p - construct);
		s = platform_buffer_read(construct, REMOTE_MAX_MSG_SIZE);
		if ((s > 0) && (construct[0] == REMOTE_RESP_OK)) {
			if (tdo) {
				unhexify(tdo, (const char*)&construct[1], byte_count);
				tdo += byte_count;
			}
			continue;
		}
		printWarn(std::string(__func__) + " error: " + std::to_string(s));
		break;
	}
	return ret;
}

int Bmp::toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len)
{
	char construct[REMOTE_MAX_MSG_SIZE];
	int s = snprintf(
		construct, REMOTE_MAX_MSG_SIZE, REMOTE_JTAG_JTCK_STR,
		(tms) ? 1 : 0, (tdi) ? 1 : 0, clk_len);
	platform_buffer_write(construct, s);

	s = platform_buffer_read(construct, REMOTE_MAX_MSG_SIZE);
	if ((s < 1) || (construct[0] == REMOTE_RESP_ERR)) {
		printWarn("toggleClk, error" +
				  s ? (char *)&(construct[1]) : "unknown");
		throw std::runtime_error("toggleClk");
	}
	return clk_len;
}

char *Bmp::hexify(char *hex, const void *buf, size_t size)
{
	char *tmp = hex;
	const uint8_t *b = (const uint8_t *)buf;

	while (size--) {
		*tmp++ = hexdigits[*b >> 4];
		*tmp++ = hexdigits[*b++ & 0xF];
	}
	*tmp++ = 0;

	return hex;
}

uint8_t unhex_digit(char hex)
{
	uint8_t tmp = hex - '0';
	if(tmp > 9)
		tmp -= 'A' - '0' - 10;
	if(tmp > 16)
		tmp -= 'a' - 'A';
	return tmp;
}

char *Bmp::unhexify(void *buf, const char *hex, size_t size)
{
	uint8_t *b = (uint8_t *)buf;
	while (size--) {
		*b = unhex_digit(*hex++) << 4;
		*b++ |= unhex_digit(*hex++);
	}
	return (char *)buf;
}
#ifdef __APPLE__
static void open_bmp(std::string dev, const std::string &serial)
{
	throw std::runtime_error("lease implement find_debuggers for MACOS!\n");
}

#elif defined(__WIN32__) || defined(__CYGWIN__)
#include <windows.h>
static HANDLE hComm;
static void open_bmp(std::string dev, const std::string &serial)
{
	if (dev.empty()) {
		/* FIXME: Implement searching for BMPs!*/
			throw std::runtime_error("Please specify device\n");
	}
	char device[256];
	if (strstr(dev.c_str(), "\\\\.\\")) {
		strncpy(device, dev.c_str(), sizeof(device) - 1);
	} else {
		strcpy(device,  "\\\\.\\");
		strncat(device, dev.c_str(), sizeof(device) - strlen(device) - 1);
	}
	hComm = CreateFile(device,                //port name
					  GENERIC_READ | GENERIC_WRITE, //Read/Write
					  0,							// No Sharing
					  NULL,							// No Security
					  OPEN_EXISTING,// Open existing port only
					  0,			// Non Overlapped I/O
					  NULL);		// Null for Comm Devices}
	if (hComm == INVALID_HANDLE_VALUE) {
		throw std::runtime_error("Could not open " + std::string(device) +
								 std::to_string(GetLastError()));
	}
	DCB dcbSerialParams;
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
	if (!GetCommState(hComm, &dcbSerialParams)) {
		throw std::runtime_error("GetCommState failed: " +
								 std::to_string(GetLastError()));
	}
	dcbSerialParams.ByteSize = 8;
	dcbSerialParams.fDtrControl = DTR_CONTROL_ENABLE;
	if (!SetCommState(hComm, &dcbSerialParams)) {
		throw std::runtime_error("SetCommState failed: " +
								 std::to_string(GetLastError()));
	}
	COMMTIMEOUTS timeouts = {0};
	timeouts.ReadIntervalTimeout         = 10;
	timeouts.ReadTotalTimeoutConstant    = 10;
	timeouts.ReadTotalTimeoutMultiplier  = 10;
	timeouts.WriteTotalTimeoutConstant   = 10;
	timeouts.WriteTotalTimeoutMultiplier = 10;
	if (!SetCommTimeouts(hComm, &timeouts)) {
			throw std::runtime_error("SetCommTimeouts failed: " +
									 std::to_string(GetLastError()));
	}
}

int Bmp::platform_buffer_write(const char *data, int size)
{
	DEBUG_WIRE("%s\n",data);
	int s = 0;

	do {
		DWORD written;
		if (!WriteFile(hComm, data + s, size - s, &written, NULL)) {
			printWarn("Serial write failed with error " +
					  std::to_string(GetLastError())	+
					  ": written " + std::to_string(s));
			return -1;
		}
		s += written;
	} while (s < size);
	return 0;
}

int Bmp::platform_buffer_read(char *data, int maxsize)
{
	DWORD s;
	uint8_t response = 0;
	DWORD startTime = GetTickCount();
	uint32_t endTime = startTime + 500;
	do {
		if (!ReadFile(hComm, &response, 1, &s, NULL)) {
			printWarn("ERROR on read RESP");
			return -1;
		}
		if (GetTickCount() > endTime) {
			printWarn("Timeout on read RESP");
			return -4;
		}
	} while (response != REMOTE_RESP);
	char *c = data;
	do {
		if (!ReadFile(hComm, c, 1, &s, NULL)) {
			printWarn("Error on read");
			return -3;
		}
		if (s > 0 ) {
			Bmp::DEBUG_WIRE("%c", *c);
			if (*c == REMOTE_EOM) {
				*c = 0;
				Bmp::DEBUG_WIRE("\n");
				return (c - data);
			} else {
				c++;
			}
		}
	} while (((c - data) < maxsize) && (GetTickCount() < endTime));
	printWarn("Failed to read EOM after " +
			  std::to_string(GetTickCount() - startTime));
	return 0;
}

Bmp::~Bmp()
{
	CloseHandle(hComm);
	printInfo("Close");
}

#else
#include <sys/select.h>
#include <termios.h>
#define DEVICE_BY_ID "/dev/serial/by-id/"
static int fd;
static int set_interface_attribs(void)
{
	struct termios tty;
	memset (&tty, 0, sizeof tty);
	if (tcgetattr (fd, &tty) != 0) {
		printWarn( "error " + std::to_string(errno) + " from tcgetattr");
		return -1;
	}
	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;	// 8-bit chars
	// disable IGNBRK for mismatched speed tests; otherwise receive break
	// as \000 chars
	tty.c_iflag &= ~IGNBRK;		// disable break processing
	tty.c_lflag = 0;				// no signaling chars, no echo,
	// no canonical processing
	tty.c_oflag = 0;				// no remapping, no delays
	tty.c_cc[VMIN]	= 0;			// read doesn't block
	tty.c_cc[VTIME] = 5;			// 0.5 seconds read timeout

	tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

	tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
	// enable reading
	tty.c_cflag &= ~CSTOPB;
#if defined(CRTSCTS)
	tty.c_cflag &= ~CRTSCTS;
#endif
	if (tcsetattr(fd, TCSANOW, &tty) != 0) {
		printWarn( "tcsetattr error :" + std::string(strerror(errno)));
		return -1;
	}
	return 0;
}

static void open_bmp(std::string dev, const std::string &serial)
{
	char name[4096];
	if (dev.empty()) {
		/* Try to find some BMP if0*/
		struct dirent *dp;
		DIR *dir = opendir(DEVICE_BY_ID);
		if (!dir) {
			 throw std::runtime_error("No serial device found");
		}
		int num_devices = 0;
		int num_total = 0;
		while ((dp = readdir(dir)) != NULL) {
			if ((strstr(dp->d_name, BMP_IDSTRING)) &&
				(strstr(dp->d_name, "-if00"))) {
				num_total++;
				if ((!serial.empty()) && (!strstr(dp->d_name, serial.c_str())))
					continue;
				num_devices++;
				strcpy(name, DEVICE_BY_ID);
				strncat(name, dp->d_name, sizeof(name) - strlen(name) - 1);
			}
		}
		closedir(dir);
		if ((num_devices == 0) && (num_total == 0)){
			throw std::runtime_error("No serial device found");
		} else if (num_devices != 1) {
			printWarn("Available Probes:");
			dir = opendir(DEVICE_BY_ID);
			if (dir) {
				while ((dp = readdir(dir)) != NULL) {
					if ((strstr(dp->d_name, BMP_IDSTRING)) &&
						(strstr(dp->d_name, "-if00")))
						printWarn(std::string(dp->d_name));
				}
				closedir(dir);
				if (serial.empty())
					throw std::runtime_error(
						"Select Probe with -s <(Partial) Serial "
						"Number");
				else
					throw std::runtime_error(
						"Do no match given serial " +
						std::string(serial.c_str()));
			} else {
				throw std::runtime_error(
					"Could not opendir " + std::string(name) +
					"error: " + std::string(strerror(errno)));
			}
		}
	} else {
		strncpy(name, dev.c_str(), sizeof(name) - 1);
	}
	fd = open(name, O_RDWR | O_SYNC | O_NOCTTY);
	if (fd < 0) {
		throw std::runtime_error(
			"Couldn't open serial port " + std::string(name) + ": " +
			std::string(strerror(errno)));
	}
	printInfo("Found " + std::string(name));
	if (set_interface_attribs()) {
		throw std::runtime_error("Can not set line, error " +
								 std::string(strerror(errno)));
	}
}

int Bmp::platform_buffer_write(const char *data, int size)
{
	int s;

	Bmp::DEBUG_WIRE("%s\n", data);
	s = write(fd, data, size);
	if (s < 0) {
		fprintf(stdout, "Failed to write\n");
		throw std::runtime_error("bmp write failed");
	}

	return size;
}

int Bmp::platform_buffer_read(char *data, int maxsize)
{
	char *c;
	int s;
	int ret;
	fd_set	rset;
	struct timeval tv;

	c = data;
	tv.tv_sec = 0;
	tv.tv_usec = 500 * 1000 ;

	/* Look for start of response */
	do {
		FD_ZERO(&rset);
		FD_SET(fd, &rset);

		ret = select(fd + 1, &rset, NULL, NULL, &tv);
		if (ret < 0) {
			printWarn("Failed on select");
			return -3;
		}
		if(ret == 0) {
			printWarn("Timeout on read RESP");
			return -4;
		}

		s = read(fd, c, 1);
	}
	while ((s > 0) && (*c != REMOTE_RESP));
	/* Now collect the response */
	do {
		FD_ZERO(&rset);
		FD_SET(fd, &rset);
		ret = select(fd + 1, &rset, NULL, NULL, &tv);
		if (ret < 0) {
			printWarn("Failed on select");
			return -4;
		}
		if(ret == 0) {
			printWarn("Timeout on read");
			return -5 ;
		}
		s = read(fd, c, 1);
		if (*c==REMOTE_EOM) {
			*c = 0;
			Bmp::DEBUG_WIRE("	   %s\n",data);
			return (c - data);
		} else {
			c++;
		}
	} while ((s >= 0) && ((c - data) < maxsize));
	printWarn("Failed to read");
	return(-6);
}

Bmp::~Bmp()
{
	close(fd);
	printInfo("Close");
}
#endif
