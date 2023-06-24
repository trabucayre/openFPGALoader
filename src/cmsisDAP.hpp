// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2021 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_CMSISDAP_HPP_
#define SRC_CMSISDAP_HPP_

#include <hidapi.h>
#include <libusb.h>

#include <string>
#include <vector>

#include "cable.hpp"
#include "jtagInterface.hpp"

class CmsisDAP: public JtagInterface {
	public:
		/*!
		 * \brief constructor: open device with vid/pid if != 0
		 *                    else search for a compatible device
		 * \param[in] vid: vendor id
		 * \param[in] pid: product id
		 * \param[in] index: interface number
		 * \param[in] verbose: verbose level 0 normal, 1 verbose
		 */
		CmsisDAP(const cable_t &cable, int index, uint8_t verbose);

		~CmsisDAP();

		/*!
		 * \brief configure probe clk frequency
		 * \param[in] clkHZ: frequency in Hertz
		 * \return <= 0 if something wrong, clkHZ otherwise
		 */
		int setClkFreq(uint32_t clkHZ) override;

		/*!
		 * \brief store a len tms bits in a buffer. send is only done if
		 *   flush_buffer
		 * \param[in] tms: serie of tms state
		 * \param[in] len: number of tms bits
		 * \param[in] flush_buffer: force buffer to be send or not
		 * \return <= 0 if something wrong, len otherwise
		 */
		int writeTMS(uint8_t *tms, uint32_t len, bool flush_buffer) override;

		/*!
		 * \brief write and read len bits with optional tms set to 1 if end
		 * \param[in] tx: serie of tdi state to send 
		 * \param[out] rx: buffer to store tdo bits from device
		 * \param[in] len: number of bit to read/write
		 * \param[in] end: if true tms is set to one with the last tdi bit
		 * \return <= 0 if something wrong, len otherwise
		 */
		int writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end) override;

		/*!
		 * \brief send a serie of clock cycle with constant TMS and TDI
		 * \param[in] tms: tms state
		 * \param[in] tdi: tdi state
		 * \param[in] clk_len: number of clock cycle
		 * \return <= 0 if something wrong, clk_len otherwise
		 */
		int toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len) override;

		/*!
		 * \brief flush TMS buffer
		 * \return <=0 if something fail, > 0 otherwise
		 */
		int flush() override;

		/* not used */
		int get_buffer_size() override { return 0;}
		/* not used */
		bool isFull() override {return false;}

	private:
		/*!
		 * \brief connect device in JTAG mode
		 * \return 1 if success <= 0 otherwise
		 */
		int dapConnect();

		/*!
		 * \brief disconnect device
		 * \return 1 if success <= 0 otherwise
		 */
		int dapDisconnect();
		int dapResetTarget();
		int read_info(uint8_t info, uint8_t *rd_info, int max_len);
		int xfer(int tx_len, uint8_t *rx_buff, int rx_len);
		int xfer(uint8_t instruction, int tx_len,
				uint8_t *rx_buff, int rx_len);

		void display_info(uint8_t info, uint8_t type);
		int writeJtagSequence(uint8_t tms, uint8_t *tx, uint8_t *rx,
				uint32_t len, bool end);

		uint8_t _verbose;                /**< display more message */
		int16_t _device_idx;          /**< device index */
		uint16_t _vid;                /**< device Vendor ID */
		uint16_t _pid;                /**< device Product ID */
		std::wstring _serial_number;  /**< device serial number */

		hid_device *_dev;          /**< hid device used to communicate */

		unsigned char *_ll_buffer; /**< message buffer */
		unsigned char *_buffer;    /**< subset of _ll_buffer */
		int _num_tms;              /**< current tms length */
		int _is_connect;           /**< device status ((dis)connected) */
};

#endif  // SRC_CMSISDAP_HPP_
