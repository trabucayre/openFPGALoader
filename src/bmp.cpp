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
#include <sys/select.h>
# include<stdarg.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include "bmp_remote.h"

#define BMP_IDSTRING "usb-Black_Sphere_Technologies_Black_Magic_Probe"
#define DEVICE_BY_ID "/dev/serial/by-id/"

#define REMOTE_MAX_MSG_SIZE (1024)

#define FREQ_FIXED -1
void Bmp::DEBUG_WARN(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}

void Bmp::DEBUG_WIRE(const char *format, ...)
{
    if (!_verbose)
	return;
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}

void Bmp::DEBUG_PROBE(const char *format, ...)
{
    if (!_verbose)
	return;
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}

static const char hexdigits[] = "0123456789abcdef";

Bmp::Bmp(std::string dev,
	 const std::string &serial, uint32_t clkHZ, bool verbose)
{
    char name[4096];
    _verbose = verbose;
    if (dev.empty()) {
	DEBUG_PROBE("No device given\n");
	/* Try to find some BMP if0*/
	struct dirent *dp;
	DIR *dir = opendir(DEVICE_BY_ID);
	if (!dir) {
	    fprintf(stderr,"No serial device found\n");
	    return;
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
	    fprintf(stderr,"No BMP probe found\n");
	    return;
	} else if (num_devices != 1) {
	    fprintf(stderr,"Available Probes:\n");
	    dir = opendir(DEVICE_BY_ID);
	    if (dir) {
		while ((dp = readdir(dir)) != NULL) {
		    if ((strstr(dp->d_name, BMP_IDSTRING)) &&
			(strstr(dp->d_name, "-if00")))
			fprintf(stderr, "%s\n", dp->d_name);
		}
		closedir(dir);
		if (serial.empty())
		    fprintf(stderr, "Select Probe with -s <(Partial) Serial "
			       "Number\n");
		else
		    fprintf(stderr, "Do no match given serial \"%s\"\n", serial.c_str());
	    } else {
		fprintf(stderr, "Could not opendir %s: %s\n", name, strerror(errno));
	    }
	    return;
	}
    } else {
	strncpy(name, dev.c_str(), sizeof(name) - 1);
    }
    fd = open(name, O_RDWR | O_SYNC | O_NOCTTY);
    if (fd < 0) {
	fprintf(stderr, "Couldn't open serial port %s\n", name);
	return;
    }
    fprintf(stderr, "Found %s\n", name);
    if (set_interface_attribs()) {
	fprintf(stderr, "Can not set line\n");
	throw std::runtime_error("_buffer malloc failed");
    }
    char construct[REMOTE_MAX_MSG_SIZE];
    int c = snprintf(construct, REMOTE_MAX_MSG_SIZE, "%s", REMOTE_START_STR);
    platform_buffer_write(construct, c);
    c = platform_buffer_read(construct, REMOTE_MAX_MSG_SIZE);
    if ((!c) || (construct[0] == REMOTE_RESP_ERR)) {
	DEBUG_WARN("Remote Start failed, error %s\n",
		   c ? (char *)&(construct[1]) : "unknown");
	 throw std::runtime_error("remote_init failed");
    }
    DEBUG_PROBE("Remote is %s\n", &construct[1]);

    /* Init JTAG */
    c = snprintf((char *)construct, REMOTE_MAX_MSG_SIZE, "%s",
		 REMOTE_JTAG_INIT_STR);
    platform_buffer_write(construct, c);
    c = platform_buffer_read(construct, REMOTE_MAX_MSG_SIZE);
    if ((!c) || (construct[0] == REMOTE_RESP_ERR)) {
	DEBUG_WARN("jtagtap_init failed, error %s\n",
		   c ? (char *)&(construct[1]) : "unknown");
	 throw std::runtime_error("jtag_init failed");
    }
    /* Get Version*/
    int s = snprintf((char *)construct, REMOTE_MAX_MSG_SIZE, "%s",
		     REMOTE_HL_CHECK_STR);
    platform_buffer_write(construct, s);
    s = platform_buffer_read(construct, REMOTE_MAX_MSG_SIZE);
    if ((!s) || (construct[0] == REMOTE_RESP_ERR) ||
	((construct[1] - '0') <  2)) {
	fprintf(stderr,
	    "Please update BMP!\n");
	return;
    }

    setClkFreq(clkHZ);
};

Bmp::~Bmp()
{
    close(fd);
    fprintf(stderr, "Close\n");
}

int Bmp::setClkFreq(uint32_t clkHZ)
{
    char construct[REMOTE_MAX_MSG_SIZE];
    int s;
    s = snprintf(construct, REMOTE_MAX_MSG_SIZE, REMOTE_FREQ_SET_STR,
		 clkHZ);
    platform_buffer_write(construct, s);

    s = platform_buffer_read(construct, REMOTE_MAX_MSG_SIZE);

    if ((!s) || (construct[0] == REMOTE_RESP_ERR)) {
	fprintf(stderr,"Update Firmware to allow to set max SWJ frequency\n");
    }
    s = snprintf((char *)construct, REMOTE_MAX_MSG_SIZE,"%s",
		 REMOTE_FREQ_GET_STR);
    platform_buffer_write(construct, s);

    s = platform_buffer_read(construct, REMOTE_MAX_MSG_SIZE);

    if ((!s) || (construct[0] == REMOTE_RESP_ERR))
	return FREQ_FIXED;

    uint32_t freq[1];
    unhexify(freq, &construct[1], 4);
    fprintf(stderr, "%d Hz\n", freq[0]);
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
	if ((!s) || (construct[0] == REMOTE_RESP_ERR)) {
	    DEBUG_WARN("jtagtap_tms_seq failed, error %s\n",
		       s ? (char *)&(construct[1]) : "unknown");
	    throw std::runtime_error("writeTMS failed");
	}
    }
    (void)flush_buffer;
    return ret;
}

int Bmp::writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end)
{
    int ret = len;

    if(!len || (!tx && !rx))
	return len;
    while (len) {
	int chunk = (len > 2000) ? 2000 : len;
	len -= chunk;
	int byte_count = (chunk + 7) >> 3;
	char construct[REMOTE_MAX_MSG_SIZE];
	int s = snprintf(
	    construct, REMOTE_MAX_MSG_SIZE,
	    REMOTE_JTAG_IOSEQ_STR,
	    (!len && end) ? REMOTE_IOSEQ_TMS : REMOTE_IOSEQ_NOTMS,
	    (((tx) ? REMOTE_IOSEQ_FLAG_IN  : REMOTE_IOSEQ_FLAG_NONE) |
	     ((rx) ? REMOTE_IOSEQ_FLAG_OUT : REMOTE_IOSEQ_FLAG_NONE)),
	    chunk);
	char *p = construct + s;
	if (tx) {
	    hexify(p, tx, byte_count);
	    p += 2 * byte_count;
	}
	*p++ = REMOTE_EOM;
	*p   = 0;
	platform_buffer_write(construct, p - construct);
	s = platform_buffer_read(construct, REMOTE_MAX_MSG_SIZE);
	if ((s > 0) && (construct[0] == REMOTE_RESP_OK)) {
	    if (rx) {
		unhexify(rx, (const char*)&construct[1], byte_count);
		rx += byte_count;
	    }
	    continue;
	}
	DEBUG_WARN("%s error %d\n",
		   __func__, s);
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
    if ((!s) || (construct[0] == REMOTE_RESP_ERR)) {
	DEBUG_WARN("toggleClk, error %s\n",
		   s ? (char *)&(construct[1]) : "unknown");
	throw std::runtime_error("toggleClk");
    }
    return clk_len;
}

int Bmp::set_interface_attribs(void)
{
    struct termios tty;
    memset (&tty, 0, sizeof tty);
    if (tcgetattr (fd, &tty) != 0) {
	fprintf(stderr, "error %d from tcgetattr", errno);
	return -1;
    }
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    // disable IGNBRK for mismatched speed tests; otherwise receive break
    // as \000 chars
    tty.c_iflag &= ~IGNBRK;         // disable break processing
    tty.c_lflag = 0;                // no signaling chars, no echo,
    // no canonical processing
    tty.c_oflag = 0;                // no remapping, no delays
    tty.c_cc[VMIN]  = 0;            // read doesn't block
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

    tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
    // enable reading
    tty.c_cflag &= ~CSTOPB;
#if defined(CRTSCTS)
    tty.c_cflag &= ~CRTSCTS;
#endif
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
	fprintf(stderr, "error %d from tcsetattr", errno);
	return -1;
    }
    return 0;
}

int Bmp::platform_buffer_write(const char *data, int size)
{
    int s;

    DEBUG_WIRE("%s\n", data);
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
    fd_set  rset;
    struct timeval tv;

    c = data;
    tv.tv_sec = 0;

    tv.tv_sec = 500 * 1000 ;
    tv.tv_usec = 0;

    /* Look for start of response */
    do {
	FD_ZERO(&rset);
	FD_SET(fd, &rset);

	ret = select(fd + 1, &rset, NULL, NULL, &tv);
	if (ret < 0) {
	    DEBUG_WARN("Failed on select\n");
	    return(-3);
	}
	if(ret == 0) {
	    DEBUG_WARN("Timeout on read RESP\n");
	    return(-4);
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
	    DEBUG_WARN("Failed on select\n");
	    exit(-4);
	}
	if(ret == 0) {
	    DEBUG_WARN("Timeout on read\n");
	    return(-5);
	}
	s = read(fd, c, 1);
	if (*c==REMOTE_EOM) {
	    *c = 0;
	    DEBUG_WIRE("       %s\n",data);
	    return (c - data);
	} else {
	    c++;
	}
    }while ((s >= 0) && ((c - data) < maxsize));
    DEBUG_WARN("Failed to read\n");
    return(-6);
    return 0;
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
