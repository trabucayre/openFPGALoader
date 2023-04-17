// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2023 Alexey Starikovskiy <aystarik@gmail.com>
 */

#define _DEFAULT_SOURCE

#include <unistd.h>
#include <libusb.h>
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <map>
#include <unistd.h>
#include <vector>
#include <string>
#include <cassert>

#include "ch347jtag.hpp"
//#include "display.hpp"

using namespace std;

#define CH347JTAG_VID 0x1a86
#define CH347JTAG_PID 0x55dd

#define CH347JTAG_INTF        2
#define CH347JTAG_WRITE_EP    0x06
#define CH347JTAG_READ_EP     0x86

#define CH347JTAG_TIMEOUT     100

enum CH347JtagCmd {
    CMD_BYTES_WO = 0xd3,
    CMD_BYTES_WR = 0xd4,
    CMD_BITS_WO  = 0xd1,
    CMD_BITS_WR  = 0xd2,
    CMD_CLK      = 0xd0,
};

enum CH347JtagSig {
    SIG_TCK =       0b1,
    SIG_TMS =      0b10,
    SIG_TDI =   0b10000,
};

int CH347Jtag::setClk(uint clk) {
    uint8_t *ptr = obuf;
    *ptr++ = CMD_CLK;
    *ptr++ = 6;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = clk;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;
    *ptr++ = 0;
    int actual = 0;
    if (_verbose == 2) {
        fprintf(stderr, "obuf[%ld] = {", ptr - obuf);
        for (int i = 0; i < ptr - obuf; ++i) {
            fprintf(stderr, "%02x, ", obuf[i]);
        }
        fprintf(stderr, "}\n\n");
    }
    int rv = libusb_bulk_transfer(dev_handle, CH347JTAG_WRITE_EP, obuf, ptr - obuf, &actual, 300);
    if (rv || actual != ptr - obuf)
        return -1;
    rv = libusb_bulk_transfer(dev_handle, CH347JTAG_READ_EP, ibuf, 4, &actual, 300);
    if (_verbose == 2) {
        fprintf(stderr, "ibuf[%d] = {", 4);
        for (int i = 0; i < 4; ++i) {
            fprintf(stderr, "%02x ", ibuf[i]);
        }
        fprintf(stderr, "}\n\n");
    }
    if (rv || actual != 4)
        return -1;
    if (ibuf[0] != 0xd0 || ibuf[3] != 0) {
        return -1;
    }
    return 0;
}

CH347Jtag::CH347Jtag(uint32_t clkHZ, uint8_t verbose):
            _verbose(verbose), dev_handle(NULL), usb_ctx(NULL)
{
    if (libusb_init(&usb_ctx) < 0) {
        cerr << "libusb init failed" << endl;
        goto err_exit;
    }
    dev_handle = libusb_open_device_with_vid_pid(usb_ctx, CH347JTAG_VID, CH347JTAG_PID);
    if (!dev_handle) {
        cerr << "fails to open device" << endl;
        goto usb_exit;
    }
    if (libusb_claim_interface(dev_handle, CH347JTAG_INTF)) {
        cerr << "libusb error while claiming CH347JTAG interface" << endl;
        goto usb_close;
    }
    {
        unsigned i = 0, sl = 2000;
        for (; i < 6; ++i, sl *= 2) {
            if (clkHZ < sl) break;
        }
        if (setClk(i)) {
            cerr << "failed to set clock rate" << endl;
            goto usb_release;
        }
        fprintf(stderr, "JTAG TCK frequency set to %d kHz", sl);
    }
    return;
usb_release:
    libusb_release_interface(dev_handle, CH347JTAG_INTF);
usb_close:
    libusb_close(dev_handle);
usb_exit:
    libusb_exit(usb_ctx);
err_exit:
    throw std::exception();
}

CH347Jtag::~CH347Jtag()
{
    if (dev_handle) {
        libusb_release_interface(dev_handle, CH347JTAG_INTF);
        libusb_close(dev_handle);
        dev_handle = 0;
    }
    if (usb_ctx) {
        libusb_exit(usb_ctx);
        usb_ctx = 0;
    }
}

int CH347Jtag::setClkFreq(uint32_t clkHZ)
{
    return clkHZ;
}

int CH347Jtag::writeTMS(uint8_t *tms, uint32_t len, bool flush_buffer)
{
    (void) flush_buffer;

    if (len == 0)
        return 0;

    uint8_t *ptr = obuf;
    for (uint32_t i = 0; i < len; ++i) {
        if (ptr == obuf) {
            *ptr++ = CMD_BITS_WO;
            ptr += 2; // leave place for length;
        }
        uint8_t x = SIG_TDI | ((tms[i >> 3] & (1 << (i & 7))) ? SIG_TMS : 0);
        *ptr++ = x;
        *ptr++ = x | SIG_TCK;
        unsigned wlen = ptr - obuf;
        if (wlen > sizeof(obuf) - 3 || i == len - 1) {
            *ptr++ = x; // clear TCK
            ++wlen;
            obuf[1] = wlen - 3;
            obuf[2] = (wlen - 3) >> 8;
            int actual_length;
            if (_verbose == 2) {
                fprintf(stderr, "obuf[%d] = {", wlen);
                for (unsigned i = 0; i < wlen; ++i) {
                    fprintf(stderr, "%02x ", obuf[i]);
                }
                fprintf(stderr, "}\n\n");
            }
            int ret = libusb_bulk_transfer(dev_handle, CH347JTAG_WRITE_EP, obuf, wlen, &actual_length, CH347JTAG_TIMEOUT);
            if (ret < 0) {
                cerr << "writeTMS: usb bulk write failed: " << libusb_strerror(ret) << endl;
                return -EXIT_FAILURE;
            }
            ptr = obuf;
        }
    }
    return len;
}

int CH347Jtag::toggleClk(uint8_t tms, uint8_t tdi, uint32_t len)
{
    uint8_t bits = SIG_TDI;
    if (tms) bits |= SIG_TMS;
    if (tdi) bits |= SIG_TDI;

    uint8_t *ptr = obuf;
    for (uint32_t i = 0; i < len; ++i) {
        if (ptr == obuf) {
            *ptr++ = CMD_BITS_WO;
            ptr += 2; // leave place for length;
        }
        *ptr++ = bits;
        *ptr++ = bits | SIG_TCK;
        unsigned wlen = ptr - obuf;
        if (wlen > sizeof(obuf) - 3 || i == len - 1) {
            *ptr++ = bits; // clear TCK
            ++wlen;
            obuf[1] = wlen - 3;
            obuf[2] = (wlen - 3) >> 8;
            int actual_length;
            if (_verbose == 2) {
                fprintf(stderr, "obuf[%d] = {", wlen);
                for (unsigned i = 0; i < wlen; ++i) {
                    fprintf(stderr, "%02x, ", obuf[i]);
                }
                fprintf(stderr, "}\n\n");
            }
            int ret = libusb_bulk_transfer(dev_handle, CH347JTAG_WRITE_EP, obuf, wlen, &actual_length, CH347JTAG_TIMEOUT);
            if (ret < 0) {
                cerr << "writeCLK: usb bulk write failed: " << libusb_strerror(ret) << endl;
                return -EXIT_FAILURE;
            }
            ptr = obuf;
        }
    }
    return EXIT_SUCCESS;
}

int CH347Jtag::writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end)
{
    if (!tx || !len)
        return 0;
    unsigned no_tms_len = (end) ? len - 1 : len;
    unsigned bytes = no_tms_len / 8;
    uint8_t *ptr = obuf;
    uint8_t *rptr = rx;
    uint8_t cmd = (rx) ? CMD_BYTES_WR : CMD_BYTES_WO;
    for (unsigned i = 0; i < bytes; ++i) {
        if (ptr == obuf) {
            *ptr++ = cmd;
            ptr += 2; // leave place for length;
        }
        *ptr++ = tx[i];
        unsigned wlen = ptr - obuf;
        if (wlen >= sizeof(obuf) || i == bytes - 1) {
            obuf[1] = wlen - 3;
            obuf[2] = (wlen - 3) >> 8;
            int actual_length;
            if (_verbose == 2) {
                fprintf(stderr, "obuf[%d] = {", wlen);
                for (unsigned i = 0; i < wlen; ++i) {
                    fprintf(stderr, "%02x ", obuf[i]);
                }
                fprintf(stderr, "}\n\n");
            }
            // there seems to be some kind of race inside the chip, if we submit messages too fast it chokes up.
            if (rx)
                usleep(20000);
            int ret = libusb_bulk_transfer(dev_handle, CH347JTAG_WRITE_EP, obuf, wlen, &actual_length, CH347JTAG_TIMEOUT);
            if (ret < 0) {
                cerr << "writeTDI: usb bulk write failed: " << libusb_strerror(ret) << endl;
                return -EXIT_FAILURE;
            }
            ptr = obuf;
            if (!rx)
                continue;
            ret = libusb_bulk_transfer(dev_handle, CH347JTAG_READ_EP, ibuf, 512, &actual_length, CH347JTAG_TIMEOUT);
            if (ret < 0) {
                cerr << "writeTDI: usb bulk read failed: " << libusb_strerror(ret) << endl;
                return -EXIT_FAILURE;
            }
            if (_verbose == 2) {
                fprintf(stderr, "ibuf[%d] = {", wlen);
                for (int i = 0; i < actual_length; ++i) {
                    fprintf(stderr, "%02x ", ibuf[i]);
                }
                fprintf(stderr, "}\n\n");
            }
            int size = ibuf[1] + ibuf[2] * 0x100;
            if (ibuf[0] != CMD_BYTES_WR || actual_length - 3 != size) {
                cerr << "writeTDI: invalid read data: " << ret << endl;
                return -EXIT_FAILURE;
            }
            memcpy(rptr, &ibuf[3], size);
            rptr += size;
        }
    }
    unsigned bits = len - bytes * 8;
    if (bits == 0)
        return EXIT_SUCCESS;
    cmd = (rx) ? CMD_BITS_WR : CMD_BITS_WO;
    ptr = obuf;
    uint8_t x = 0;
    for (unsigned i = 0; i < bits; ++i) {
        if (ptr == obuf) {
            *ptr++ = cmd;
            ptr += 2; // leave place for length;
        }
        uint8_t txb = tx[bytes + (i >> 3)];
        uint8_t _tdi = (txb & (1 << (i & 7))) ? SIG_TDI : 0;
        *ptr++ = _tdi;
        x = _tdi | SIG_TCK;
        if (end && i == bits - 1) {
            x |= SIG_TMS;
        }
        *ptr++ = x;
    }
    unsigned wlen = ptr - obuf;
    obuf[1] = wlen - 3;
    obuf[2] = (wlen - 3) >> 8;
    int actual_length;
    if (_verbose == 2) {
        fprintf(stderr, "obuf[%d] = {", wlen);
        for (unsigned i = 0; i < wlen; ++i) {
            fprintf(stderr, "%02x ", obuf[i]);
        }
        fprintf(stderr, "}\n\n");
    }
    // there seems to be some kind of race inside the chip, if we submit messages too fast it chokes up.
    if (rx)
        usleep(20000);
    int ret = libusb_bulk_transfer(dev_handle, CH347JTAG_WRITE_EP, obuf, wlen, &actual_length, CH347JTAG_TIMEOUT);
    if (ret < 0) {
        cerr << "writeTDI: usb bulk write failed: " << libusb_strerror(ret) << endl;
        return -EXIT_FAILURE;
    }
    if (!rx)
        return EXIT_SUCCESS;
    ret = libusb_bulk_transfer(dev_handle, CH347JTAG_READ_EP, ibuf, 512, &actual_length, CH347JTAG_TIMEOUT);
    if (_verbose == 2) {
        fprintf(stderr, "ibuf[%d] = {", wlen);
        for (int i = 0; i < actual_length; ++i) {
            fprintf(stderr, "%02x ", ibuf[i]);
        }
        fprintf(stderr, "}\n\n");
    }
    if (ret < 0) {
        cerr << "writeTDI: usb bulk read failed: " << libusb_strerror(ret) << endl;
        return -EXIT_FAILURE;
    }
    int size = ibuf[1] + ibuf[2] * 0x100;
    if (ibuf[0] != CMD_BITS_WR || actual_length - 3 != size) {
        cerr << "writeTDI: invalid read data: " << endl;
        return -EXIT_FAILURE;
    }

    for (int i = 0; i < size / 16; ++i) {
        uint8_t b = 0;
        uint8_t *xb = &ibuf[3 + i * 16 + 1];
        for (unsigned j = 0; j < 8; ++j)
            b |= (xb[j * 2] & 1) << j;
        *rptr++ = b;
    }

    return EXIT_SUCCESS;
}
