// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2023 Alexey Starikovskiy <aystarik@gmail.com>
 */
#pragma once

#include <libusb.h>

#include "jtagInterface.hpp"

/*!
 * \file CH347Jtag.hpp
 * \class CH347Jtag
 * \brief concrete class between jtag implementation and FTDI capable bitbang mode
 * \author Gwenhael Goavec-Merou
 */

class CH347Jtag : public JtagInterface {
 public:
    CH347Jtag(uint32_t clkHZ, uint8_t verbose);
    virtual ~CH347Jtag();

    int setClkFreq(uint32_t clkHZ) override;

    /* TMS */
    int writeTMS(uint8_t *tms, uint32_t len, bool flush_buffer) override;
    /* TDI */
    int writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;
    /* clk */
    int toggleClk(uint8_t tms, uint8_t tdo, uint32_t clk_len) override;

    /*!
     * \brief return internal buffer size (in byte).
     * \return _buffer_size divided by 2 (two byte for clk) and divided by 8 (one
     * state == one byte)
     */
    int get_buffer_size() override { return 0;}

    bool isFull() override { return false;}

    int flush() override {return 0;}

 private:
    uint8_t _verbose;

    int setClk(uint32_t clk);

    libusb_device_handle *dev_handle;
    libusb_context *usb_ctx;

    uint8_t ibuf[512];
    uint8_t obuf[512];

    //uint8_t _tdi;
    //uint8_t _tms;
    //uint8_t _version;
};
