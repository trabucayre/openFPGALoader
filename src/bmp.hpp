// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2021 Uwe Bonnes bon@elektron.ikp.physik.tu-darmstadt.de
 */

#ifndef SRC_BMP_HPP_
#define SRC_BMP_HPP_

#include <libusb.h>

#include "jtagInterface.hpp"

/*!
 * \file Bmp.hpp
 * \class Bmp
 * \brief concrete class between jtag implementation and blackmagic debug probe remote protocoll
 * \author Uwe Bonnes
 */

class Bmp : public JtagInterface {
 public:
    Bmp(std::string dev,
	const std::string &serial, uint32_t clkHZ, bool verbose);
    ~Bmp(void);
    int setClkFreq(uint32_t clkHZ) override;
    uint32_t getClkFreq() { return _clkHZ;}
    /* TMS */
    int writeTMS(uint8_t *tms, int len, bool flush_buffer) override;
    /* TDI */
    int writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;
    /* clk */
    int toggleClk(uint8_t tms, uint8_t tdo, uint32_t clk_len) override;
    int get_buffer_size() override { return 0;}
    bool isFull() override {return false;}
    int flush() override { return 0;};
private:
    bool _verbose;                /**< display more message */
    int fd;
    int set_interface_attribs(void);
    int platform_buffer_write(const char *data, int size);
    int platform_buffer_read(char *data, int maxsize);
    char *unhexify(void *buf, const char *hex, size_t size);
    char *hexify(char *hex, const void *buf, size_t size);
    void DEBUG_WARN(const char *format, ...);
    void DEBUG_WIRE(const char *format, ...);
    void DEBUG_PROBE(const char *format, ...);
protected:
    uint32_t _clkHZ;
};
#endif // SRC_BMP_HPP_
