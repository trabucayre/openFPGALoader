// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2021 Uwe Bonnes <bon@elektron.ikp.physik.tu-darmstadt.de>
 */

#include <stdio.h>
#include "bmd.hpp"
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
# include<stdarg.h>
#include <fcntl.h>
#include <unistd.h>

#include "display.hpp"

#include "bmd_remote.h"

#define BMD_IDSTRING "usb-Black_Magic_Debug_Black_Magic_Probe"

#define REMOTE_MAX_MSG_SIZE (1024)

#define FREQ_FIXED -1

void Bmd::DEBUG_WIRE(const void *const data, const size_t length)
{
	if (!_verbose)
		return;
	uint8_t *c = (uint8_t *) data;
	for (unsigned int i = 0; i < length; i++) {
		fprintf(stderr, "%c", c[i]);
	}
	fprintf(stderr, " -> ");
}

static const char hexdigits[] = "0123456789abcdef";
static void open_bmd(std::string dev, const std::string &serial);
#undef REMOTE_START_STR
static const char REMOTE_START_STR[] = {																		\
	'+', REMOTE_EOM, REMOTE_SOM, REMOTE_GEN_PACKET, REMOTE_START, REMOTE_EOM, 0};
#undef REMOTE_JTAG_INIT_STR
static const char REMOTE_JTAG_INIT_STR[] = {							\
	'+', REMOTE_EOM, REMOTE_SOM, REMOTE_JTAG_PACKET, REMOTE_INIT, REMOTE_EOM, 0};
#undef REMOTE_FREQ_SET_STR
static const char REMOTE_FREQ_SET_STR[] = {								\
	REMOTE_SOM, REMOTE_GEN_PACKET, REMOTE_FREQ_SET, '%', '0', '8', 'x', REMOTE_EOM, 0};
#undef REMOTE_FREQ_GET_STR
static const char REMOTE_FREQ_GET_STR[] = {
	REMOTE_SOM, REMOTE_GEN_PACKET, REMOTE_FREQ_GET, REMOTE_EOM, 0};
#undef REMOTE_HL_CHECK_STR
static const char REMOTE_HL_CHECK_STR[] = {
	REMOTE_SOM, REMOTE_HL_PACKET, REMOTE_HL_CHECK, REMOTE_EOM, 0};
#undef REMOTE_JTAG_TMS_STR
static const char REMOTE_JTAG_TMS_STR[] = {                                                                                           \
	REMOTE_SOM, REMOTE_JTAG_PACKET, REMOTE_TMS, '%', '0', '2', 'x', '%', 'x', REMOTE_EOM, 0};
#undef REMOTE_JTAG_TDIDO_STR
	static const char REMOTE_JTAG_TDIDO_STR[] = {
	REMOTE_SOM, REMOTE_JTAG_PACKET, '%', 'c', '%', '0', '2', 'x', '%', 'l', 'x', REMOTE_EOM, 0};


Bmd::Bmd(std::string dev,
		 const std::string &serial, uint32_t clkHZ, bool verbose)
{
	_verbose = verbose;
	open_bmd(dev, serial);
	platform_buffer_write(REMOTE_START_STR, sizeof(REMOTE_START_STR));
	char buffer[REMOTE_MAX_MSG_SIZE];
	int c = platform_buffer_read(buffer, REMOTE_MAX_MSG_SIZE);
	if ((c < 1) || (buffer[0] == REMOTE_RESP_ERR)) {
		printWarn("Remote Start failed, error " +
				   c ? (char *)&(buffer[1]) : "unknown");
		throw std::runtime_error("remote_init failed");
	}
	printInfo("Remote is " + std::string(&buffer[1]));

	/* Init JTAG */
	platform_buffer_write(REMOTE_JTAG_INIT_STR, sizeof(REMOTE_JTAG_INIT_STR));
	c = platform_buffer_read(buffer, REMOTE_MAX_MSG_SIZE);
	if ((c < 0) || (buffer[0] == REMOTE_RESP_ERR)) {
		printWarn("jtagtap_init failed, error " +
				   c ? (char *)&(buffer[1]) : "unknown");
		throw std::runtime_error("jtag_init failed");
	}
	/* Get Version*/
	platform_buffer_write(REMOTE_HL_CHECK_STR, sizeof(REMOTE_HL_CHECK_STR));
	int s = platform_buffer_read(buffer, REMOTE_MAX_MSG_SIZE);
#define NEEDED_BMD_VERSION 3
	if (s < 0) {
		printWarn("Communication error");
		return;
	}
	if (buffer[0] == REMOTE_RESP_ERR) {
		printWarn("Update BMD firmware!");
		return;
	}
	if ((buffer[1] - '0') <	 NEEDED_BMD_VERSION) {
		printWarn("Please update BMD, expected version " +
				  std::to_string(NEEDED_BMD_VERSION) + ", got " +
				  buffer[1]);
		return;
	}
	setClkFreq(clkHZ);
};

int Bmd::setClkFreq(uint32_t clkHZ)
{
	char buffer[REMOTE_MAX_MSG_SIZE];
	int s;
	s = snprintf(buffer, REMOTE_MAX_MSG_SIZE, REMOTE_FREQ_SET_STR,
				 clkHZ);
	platform_buffer_write(buffer, s);

	s = platform_buffer_read(buffer, REMOTE_MAX_MSG_SIZE);

	if ((s < 1) || (buffer[0] == REMOTE_RESP_ERR)) {
		printWarn("Update Firmware to allow to set max SWJ frequency\n");
	}
	s = snprintf((char *)buffer, REMOTE_MAX_MSG_SIZE,"%s",
				 REMOTE_FREQ_GET_STR);
	platform_buffer_write(buffer, s);

	s = platform_buffer_read(buffer, REMOTE_MAX_MSG_SIZE);

	if ((s < 1) || (buffer[0] == REMOTE_RESP_ERR))
		return FREQ_FIXED;

	uint32_t freq[1];
	unhexify(freq, &buffer[1], 4);
	printInfo("Running at " + std::to_string(freq[0]/(1.0 *1000 *1000)) + " MHz");
	_clkHZ = freq[0];
	return freq[0];
}

int Bmd::writeTMS(uint8_t *tms, uint32_t len, bool flush_buffer)
{
	char buffer[REMOTE_MAX_MSG_SIZE];
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
		s = snprintf((char *)buffer, REMOTE_MAX_MSG_SIZE,
					 REMOTE_JTAG_TMS_STR, chunk, tms_word);
		platform_buffer_write(buffer, s);

		s = platform_buffer_read(buffer, REMOTE_MAX_MSG_SIZE);
		if ((s < 1) || (buffer[0] == REMOTE_RESP_ERR)) {
			printWarn("jtagtap_tms_seq failed, error \n" +
					   s ? (char *)&(buffer[1]) : "unknown");
			throw std::runtime_error("writeTMS failed");
		}
	}
	(void)flush_buffer;
	return ret;
}

#define HTON(x)    (((x) <= '9') ? (x) - '0' : ((TOUPPER(x)) - 'A' + 10))
#define TOUPPER(x) ((((x) >= 'a') && ((x) <= 'z')) ? ((x) - ('a' - 'A')) : (x))
#define ISHEX(x)   ((((x) >= '0') && ((x) <= '9')) || (((x) >= 'A') && ((x) <= 'F')) || (((x) >= 'a') && ((x) <= 'f')))

uint64_t Bmd::remote_hex_string_to_num(const uint32_t max, const char *const str)
{
	uint64_t ret = 0;
	for (size_t i = 0; i < max; ++i) {
		const char value = str[i];
		if (!ISHEX(value))
			return ret;
		ret = (ret << 4U) | HTON(value);
	}
	return ret;
}

void Bmd::remote_v0_jtag_tdi_tdo_seq(uint8_t *data_out, bool final_tms, const uint8_t *data_in, size_t clock_cycles)
{
	/* NB: Until firmware version v1.7.1-233, the remote can only handle 32 clock cycles at a time */
	if (!clock_cycles || (!data_in && !data_out))
		return;

	char buffer[REMOTE_MAX_MSG_SIZE];
	size_t offset = 0;
	/* Loop through the data to send/receive and handle it in chunks of up to 32 bits */
	for (size_t cycle = 0; cycle < clock_cycles; cycle += 32U) {
		/* Calculate how many bits need to be in this chunk, capped at 32 */
		size_t chunk_length = clock_cycles - cycle;
		if (chunk_length > 32U)
			chunk_length = 32U;
		/* If the result would complete the transaction, check if TMS needs to be high at the end */
		const char packet_type =
			cycle + chunk_length == clock_cycles && final_tms ? REMOTE_TDITDO_TMS : REMOTE_TDITDO_NOTMS;

		/* Build a representation of the data to send safely */
		uint32_t data = 0U;
		const size_t bytes = (chunk_length + 7U) >> 3U;
		if (data_in) {
			for (size_t idx = 0; idx < bytes; ++idx)
				data |= data_in[offset + idx] << (idx * 8U);
		}
		/*
		 * Build the remote protocol message to send, and send it.
		 * This uses its own copy of the REMOTE_JTAG_TDIDO_STR to correct for how
		 * formatting a uint32_t is platform-specific.
		 */
		int length = snprintf(
			buffer, REMOTE_MAX_MSG_SIZE, "!J%c%02zx%" PRIx32 "%c", packet_type, chunk_length, data, REMOTE_EOM);
		platform_buffer_write(buffer, length);

		/* Receive the response and check if it's an error response */
		length = platform_buffer_read(buffer, REMOTE_MAX_MSG_SIZE);
		if (!length || buffer[0] == REMOTE_RESP_ERR) {
			printWarn("remote_jtag_tdi_tdo_seq failed, error %s\n", length ? buffer + 1 : "unknown");
			exit(-1);
		}
		if (data_out) {
			const uint64_t data = remote_hex_string_to_num(-1, buffer + 1);
			for (size_t idx = 0; idx < bytes; ++idx)
				data_out[offset + idx] = (uint8_t)(data >> (idx * 8U));
		}
		offset += bytes;
	}
}

/* \param[in] tdi: serie of tdi state to send
 * \param[out] tdo: buffer to store tdo bits from device
 * \param[in] len: number of bit to read/write
 * \param[in] end: if true tms is set to one with the last tdi bit
 * \return <= 0 if something wrong, len otherwise
 */
int Bmd::writeTDI(uint8_t *data_in, uint8_t *data_out, uint32_t clock_cycles, bool final_tms)
{
	remote_v0_jtag_tdi_tdo_seq(data_out, final_tms, data_in, clock_cycles);
	return 0;
}

/*!
 * \brief send a serie of clock cycle with constant TMS and TDI
 * \param[in] tms: tms state
 * \param[in] tdi: tdi state
 * \param[in] clk_len: number of clock cycle
 * \return <= 0 if something wrong, clk_len otherwise
 */
int Bmd::toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len)
{
	/* Set TDI as requested */
	remote_v0_jtag_tdi_tdo_seq(NULL, (clk_len > 1) ? false: (tms), &tdi, 1);
	clk_len--;
	if (clk_len > 1)
		remote_v0_jtag_tdi_tdo_seq(NULL, (tms), NULL, clk_len - 1);
	return clk_len;
}

char *Bmd::hexify(char *hex, const void *buf, size_t size)
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

char *Bmd::unhexify(void *buf, const char *hex, size_t size)
{
	uint8_t *b = (uint8_t *)buf;
	while (size--) {
		*b = unhex_digit(*hex++) << 4;
		*b++ |= unhex_digit(*hex++);
	}
	return (char *)buf;
}
#ifdef __APPLE__
static void open_bmd(std::string dev, const std::string &serial)
{
	throw std::runtime_error("lease implement find_debuggers for MACOS!\n");
}

#elif defined(__WIN32__) || defined(__CYGWIN__)
#include <windows.h>
static HANDLE hComm;
static void open_bmd(std::string dev, const std::string &serial)
{
	if (dev.empty()) {
		/* FIXME: Implement searching for BMDs!*/
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

int Bmd::platform_buffer_write(const void *const data, const size_t length)
{
	DEBUG_WIRE(data, length);
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

int Bmd::platform_buffer_read(void *data, size_t size)
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
			Bmd::DEBUG_WIRE(&c, 1);
			if (*c == REMOTE_EOM) {
				*c = 0;
				Bmd::DEBUG_WIRE("\n", 1);
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

Bmd::~Bmd()
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

static void open_bmd(std::string dev, const std::string &serial)
{
	char name[4096];
	if (dev.empty()) {
		/* Try to find some BMD if0*/
		struct dirent *dp;
		DIR *dir = opendir(DEVICE_BY_ID);
		if (!dir) {
			 throw std::runtime_error("No serial device found");
		}
		int num_devices = 0;
		int num_total = 0;
		while ((dp = readdir(dir)) != NULL) {
			if ((strstr(dp->d_name, BMD_IDSTRING)) &&
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
					if ((strstr(dp->d_name, BMD_IDSTRING)) &&
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

bool Bmd::platform_buffer_write(const void *const data, const size_t length)
{
	Bmd::DEBUG_WIRE(data, length);
	const ssize_t written = write(fd, data, length);
	if (written < 0) {
		fprintf(stdout, "Failed to write\n");
		throw std::runtime_error("bmd write failed");
	}

	return (size_t)written == length;
}

int Bmd::platform_buffer_read(void *data, const size_t length)
{
	uint8_t *c = (uint8_t *)data, *anchor = c;
	int s;
	int ret;
	fd_set	rset;
	struct timeval tv;

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
		fprintf(stderr, "%c", c[0]);
		if (*c==REMOTE_EOM) {
			*c = 0;
			fprintf(stderr, "\n");
			return (c - anchor);
		} else {
			c++;
		}
	} while ((s >= 0) && ((c < anchor + length)));
	printWarn("Failed to read");
	return(-6);
}

Bmd::~Bmd()
{
	close(fd);
	printInfo("Close");
}
#endif
