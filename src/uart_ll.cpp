// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include "uart_ll.hpp"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include <stdexcept>

#include "display.hpp"

Uart_ll::Uart_ll(const std::string &filename, uint32_t clkHz,
		uint8_t byteSize, bool twoStopBits): _clkHz(clkHz),
		_byteSize(CS8), _stopBits(twoStopBits)
{
	switch (byteSize) {
		case 5:
			_byteSize = CS5;
			break;
		case 6:
			_byteSize = CS6;
			break;
		case 7:
			_byteSize = CS7;
			break;
		case 8:
			_byteSize = CS8;
			break;
		default:
			std::runtime_error("Error: byteSize must be between 5 and 8");
	}

	/* open device */
	_serial = open(filename.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (_serial == -1)
		throw std::runtime_error("Error: failed to open " + filename +
				" (" + strerror(errno) + ")");

	/* configure port */
	if (port_configure() < 0)
		throw std::runtime_error("Error: port configuration failed");

	// set baudrate
	if (setClkFreq(_clkHz) < -1)
		throw std::runtime_error("Error: baudrate configuration failed");
}

Uart_ll::~Uart_ll() {
	int err;
	/* reapply original configuration */
	if ((err = tcsetattr(_serial, TCSANOW, &_prev_termios)) != 0) {
		printError("error %d from tcsetattr: " + std::to_string(err));
	}

	/* close device */
	close(_serial);
}

int Uart_ll::port_configure(void)
{
	int err;

	/* store current configuration */
	if ((err = tcgetattr(_serial, &_prev_termios)) != 0) {
		printError("error to retrieve current configuration: " +
				std::to_string(err) + " " + strerror(errno));
		return -1;
	}

	/* copy termios structure configuration */
	memcpy(&_curr_termios, &_prev_termios, sizeof(_prev_termios));

	/* controls flags */
	/* clear default configuration */
	_curr_termios.c_cflag &= ~(CSIZE | CSTOPB | CRTSCTS);
	/* set byte size (5,6,7,8)
	 * enable receiver
	 * disable modem control lines
	 */
	_curr_termios.c_cflag |= (_byteSize | CREAD | CLOCAL);
	/* 2 stop bits ? */
	if (_stopBits)
		_curr_termios.c_cflag |= CSTOPB;

	/* input mode */
	_curr_termios.c_iflag &= ~IGNBRK;  // disable break processing
	_curr_termios.c_iflag &= ~(IXON | IXOFF | IXANY);  // shut off xon/xoff ctrl

	/* output mode */
	_curr_termios.c_oflag = 0;  // no remapping, no delays

	// no canonical processing
	_curr_termios.c_lflag = 0;  // no signaling chars, no echo,

	_curr_termios.c_cc[VMIN] = 0;  // read doesn't block
	_curr_termios.c_cc[VTIME] = 5;  // 0.5 seconds read timeout

	if ((err = tcsetattr(_serial, TCSANOW, &_curr_termios)) != 0) {
		printError("error %d from tcsetattr: " + std::to_string(err) +
			" " + strerror(errno));
		return -1;
	}

	return 0;
}

int Uart_ll::setClkFreq(uint32_t clkHz)
{
	int err;
	speed_t baud = freq_to_baud(clkHz);

	/* set input baudrate */
	cfsetispeed(&_curr_termios, baud);
	/* set output baudrate */
	cfsetospeed(&_curr_termios, baud);
	/* apply modifications */
	if ((err = tcsetattr(_serial, TCSANOW, &_curr_termios)) != 0) {
		printError("Error during baudrate configuration: tcsetattr == "
			+ std::to_string(err));
		return -1;
	}
	_clkHz = clkHz;
	_baudrate = baud;
	return clkHz;
}

uint32_t Uart_ll::getClkFreq()
{
	return baud_to_freq(cfgetispeed(&_curr_termios));
}

int Uart_ll::write(const unsigned char *data, int size)
{
	const char *ptr = reinterpret_cast<const char *>(data);
	ssize_t len = size;
	/* set a select to block for serial data or timeout */
	fd_set fd_write;
	timespec timeout_ts;
	timeout_ts.tv_sec = 5;
	timeout_ts.tv_nsec = 0;

	do {
		ssize_t s = ::write(_serial, ptr, len);
		if (s < 0) {
			if (errno != 11) {
				printError("Error: Failed to write: " + std::to_string(errno) +
					" " + strerror(errno));
				return s;
			}
		} else if (s == len) {
			break;
		}

		ptr += s;
		len -= s;

		FD_ZERO(&fd_write);
		FD_SET(_serial, &fd_write);
		int r = pselect(_serial + 1, NULL, &fd_write, NULL, &timeout_ts, NULL);

		if (r < 0) {
			int t = errno;
			printf("uart_ll errror: write failed with error %d (%d: %s)\n", r,
					errno, strerror(errno));
			// interrupt ?
			return (t == EINTR) ? -1 : -2;
		}

		/* timeout */
		if (r == 0)
			return -3;

		if (!FD_ISSET(_serial, &fd_write)) {
			printError(
				"uart_ll error: something to write but from file descriptor\n");
			return -4;
		}
	} while (len > 0);

	return size;
}

int Uart_ll::read(std::string *buf, int maxsize)
{
	int size = maxsize;
	int read_size = 0;
	char c;

	do {
		ssize_t ret = ::read(_serial, &c, 1);
		if (ret < 0) {
			char mess[256];
			snprintf(mess, 256, "Error: Failed to read: %d %s",
					errno, strerror(errno));
			printError(mess);
			return ret;
		}
		buf->append(1, c);
		read_size++;
		size--;
	} while (size > 0);

	return read_size;
}

bool Uart_ll::flush(int maxsize)
{
	int size = maxsize;
	int read_size = 0;
	char c;
	int timeout = 1000;
	std::string buf;

	do {
		ssize_t ret = ::read(_serial, &c, 1);
		if (ret < 0) {
			if (errno == 11)
				return true;
			char mess[256];
			snprintf(mess, 256, "Error: Failed to read: %d %s",
					errno, strerror(errno));
			printError(mess);
			return ret;
		} else if (ret == 0) {
			timeout--;
		} else {
			buf.append(1, c);
			read_size++;
			size--;
		}
	} while (size > 0 && timeout > 0);

	return true;
}
int Uart_ll::read_until(std::string *buf, uint8_t end)
{
	int read_size = 0;
	char c[256];
	bool is_end = false;
	/* set a select to block for serial data or timeout */
	fd_set fd_read;
	timespec timeout_ts;
	timeout_ts.tv_sec = 5;
	timeout_ts.tv_nsec = 0;
	do {
		FD_ZERO(&fd_read);
		FD_SET(_serial, &fd_read);
		int r = pselect(_serial + 1, &fd_read, NULL, NULL, &timeout_ts, NULL);

		if (r < 0) {
			int t = errno;
			printf("uart_ll errror: read failed with error %d (%d: %s)\n", r,
					errno, strerror(errno));
			if (t == EINTR)  // interrupt
				return -1;
			return -2;
		}

		/* timeout */
		if (r == 0)
			return -3;

		if (!FD_ISSET(_serial, &fd_read)) {
			printError(
				"uart_ll error: something to read but from file descriptor\n");
			return -4;
		}

		int ret = ::read(_serial, c, 256);
		if (ret < 0) {
			if (errno == 11) {  // temporarily unavailable
				printError("uart_ll error: device unavailable\n");
				return -5;
			}
		}

		for (int i = 0; i < ret; i++) {
			if (c[i] == '\r' && end == '\n')
				continue;
			if (c[i] == end) {
				is_end = true;
				break;
			}
			buf->append(1, c[i]);
			read_size++;
		}
	} while (!is_end);

	return read_size;
}

speed_t Uart_ll::freq_to_baud(uint32_t clkHz)
{
	if (clkHz > 115200)
		return B230400;
	else if (clkHz > 57600)
		return B115200;
	else if (clkHz > 38400)
		return B57600;
	else if (clkHz > 19200)
		return B38400;
	else if (clkHz > 9600)
		return B19200;
	else if (clkHz > 4800)
		return B9600;
	else if (clkHz > 2400)
		return B4800;
	else  // no exhaustive list
		return B2400;
}

uint32_t Uart_ll::baud_to_freq(speed_t baud)
{
	switch(baud) {
	case B230400:
		return 230400;
	case B115200:
		return 115200;
	case B57600:
		return 57600;
	case B38400:
		return 38400;
	case B19200:
		return 19200;
	case B9600:
		return 9600;
	case B4800:
		return 4800;
	default:  // no exhaustive list
		return 2400;
	}
}
