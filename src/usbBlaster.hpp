// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef USBBLASTER_H
#define USBBLASTER_H
#include <ftdi.h>
#include <iostream>
#include <string>
#include <vector>

#include "cable.hpp"
#include "jtagInterface.hpp"
#include "ftdipp_mpsse.hpp"
#include "fx2_ll.hpp"

/*!
 * \file UsbBlaster.hpp
 * \class UsbBlaster_ll
 * \brief altera/intel usb blaster abstract class for blasterI and blasterII
 * \author Gwenhael Goavec-Merou
 */

class UsbBlaster_ll {
	public:
		virtual ~UsbBlaster_ll() {}
		virtual int setClkFreq(uint32_t clkHZ) = 0;
		virtual uint32_t getClkFreq() = 0;
		virtual int write(uint8_t *wr_buf, int wr_len,
			uint8_t *rd_buf, int rd_len) = 0;
};

/*!
 * \file UsbBlaster.hpp
 * \class UsbBlaster
 * \brief altera/intel usb blaster implementation
 * \author Gwenhael Goavec-Merou
 */

class UsbBlaster : public JtagInterface {
 public:
	UsbBlaster(const cable_t &cable, const std::string &firmware_path,
			uint8_t verbose = 0);
	virtual ~UsbBlaster();

	int setClkFreq(uint32_t clkHZ) override;
	uint32_t getClkFreq() override;

	/*!
	 * \brief drive TMS to move in JTAG state machine
	 * \param tms serie of TMS state
	 * \param len number of TMS state
	 * \param flush_buffer force flushing the buffer
	 * \return number of state written
	 * */
	int writeTMS(uint8_t *tms, uint32_t len, bool flush_buffer) override;

	/* TDI */
	int writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;
	/*!
	 * \brief toggle clock with static tms and tdi
	 * \param tms TMS state
	 * \param tdi TDI state
	 * \param clk_len number of clock cycle
	 * \return number of clock cycle
	 * */
	int toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len) override;

	/*!
	 * \brief return internal buffer size (in byte).
	 * \return _buffer_size divided by 2 (two byte for clk) and divided by 8 (one
	 * state == one byte)
	 */
	int get_buffer_size() override { return 63/8/2; }

	bool isFull() override { return _nb_bit == 8*get_buffer_size();}

	int flush() override;

 private:
	UsbBlaster_ll *ll_driver;
	int writeByte(uint8_t *tdo, int nb_byte);
	int writeBit(uint8_t *tdo, int nb_bit);
	int write(bool read, int rd_len);
	int setBitmode(uint8_t mode);
	uint8_t *_in_buf;

	uint8_t _verbose;
	uint8_t _tck_pin; /*!< tck pin: 1 << pin id */
	uint8_t _tms_pin; /*!< tms pin: 1 << pin id */
	uint8_t _tdi_pin; /*!< tdi pin: 1 << pin id */
	int _nb_bit;
	uint8_t _curr_tms;
	uint16_t _buffer_size;
};

/*!
 * \file UsbBlaster.hpp
 * \class UsbBlasterI
 * \brief altera/intel low level usb blaster class for blasterI
 * \author Gwenhael Goavec-Merou
 */
class UsbBlasterI: public UsbBlaster_ll {
	public:
		UsbBlasterI();
		virtual ~UsbBlasterI();

		int setClkFreq(uint32_t clkHZ) override;
		uint32_t getClkFreq() override;
		int write(uint8_t *wr_buf, int wr_len,
				uint8_t *rd_buf, int rd_len) override;
	private:
		/*
		 * \brief init and configure FT245
		 */
		void init_internal();
		struct ftdi_context *_ftdi; /*!< ftid_context */
};

/*!
 * \file UsbBlaster.hpp
 * \class UsbBlasterII
 * \brief altera/intel low level usb blaster class for blasterII
 * \author Gwenhael Goavec-Merou
 */
class UsbBlasterII: public UsbBlaster_ll {
	public:
		UsbBlasterII(const std::string &firmware_path);
		virtual ~UsbBlasterII();

		int setClkFreq(uint32_t clkHZ) override;
		uint32_t getClkFreq() override;
		int write(uint8_t *wr_buf, int wr_len,
				uint8_t *rd_buf, int rd_len) override;
	private:
		FX2_ll *fx2;
};
#endif
