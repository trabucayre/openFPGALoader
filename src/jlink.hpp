// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2021 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_JLINK_HPP_
#define SRC_JLINK_HPP_

#include <libusb.h>

#include <string>
#include <vector>

#include "jtagInterface.hpp"

/*!
 * \brief Segger JLink probe driver
 */
class Jlink: public JtagInterface {
	public:
		/*!
		 * \brief constructor: open device 
		 * \param[in] clkHz: output clock frequency
		 * \param[in] verbose: verbose level -1 quiet, 0 normal,
		 * 								1 verbose, 2 debug
		 */
		Jlink(uint32_t clkHz, int8_t verbose);

		~Jlink();

		// jtag Interface requirement
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
		 * \brief access ll_write outer this class / directly receives
		 *        fully filled tms, tdi buffers, and optionally tdo
		 * \param[in] tms: tms buffer
		 * \param[in] tdi: tdi buffer
		 * \param[out] tdo: tdo buffer
		 * \param[in] numbits: tms/tdi/tdo buffer size (in bit)
		 */
		virtual bool writeTMSTDI(const uint8_t *tms, const uint8_t *tdi,
				uint8_t *tdo, uint32_t numbits) override;

		/*!
		 * \brief send a serie of clock cycle with constant TMS and TDI
		 * \param[in] tms: tms state
		 * \param[in] tdi: tdi state
		 * \param[in] clk_len: number of clock cycle
		 * \return <= 0 if something wrong, clk_len otherwise
		 */
		int toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len) override;

		/*!
		 * \brief flush internal buffer
		 * \return <=0 if something fail, > 0 otherwise
		 */
		int flush() override;

		/*
		 * unused
		 */
		int get_buffer_size() override { return 2048;}
		bool isFull() override { return false;}

		// JLINK specifics methods
		std::string get_version();
		void get_speeds();
		bool set_speed(uint16_t freqHz);
		bool get_caps();
		bool set_ks_power(bool val);
		void read_config();
		int get_hw_version();
		bool get_result();
		bool max_mem_block(uint32_t *max_mem);

	private:
		// Jlink EMU_CMD code
		enum {
			EMU_CMD_VERSION            = 0x01,
			EMU_CMD_SET_SPEED          = 0x05,
			EMU_CMD_SET_KS_POWER       = 0x08,
			EMU_CMD_GET_SPEEDS         = 0xC0,
			EMU_CMD_GET_MAX_MEM_BLOCK  = 0xD4,
			EMU_CMD_HW_JTAG_GET_RESULT = 0xD6,
			EMU_CMD_SELECT_IF          = 0xC7,
			EMU_CMD_HW_JTAG2           = 0xCE,
			EMU_CMD_HW_JTAG3           = 0xCF,
			EMU_CMD_GET_CAPS           = 0xE8,
			EMU_CMD_GET_HW_VERSION     = 0xF0,
			EMU_CMD_READ_CONFIG        = 0xF2,
			EMU_CMD_WRITE_CONFIG       = 0xF3
		};
		
		// JLink hardware type
		const std::string jlink_hw_type[4] = {
			"J-Link",
			"J-Trace",
			"Flasher",
			"J-Link Pro"
		};
		
		// Jlink configuration structure
		struct jlink_cfg_t {
			uint8_t usb_adr;
			uint8_t reserved1[3];   // 0x01 - 0x03: 0xff
			uint32_t kickstart;     // Kickstart power on JTAG-pin 19
			uint8_t reserved2[24];  // 0x08 - 0x1F: 0xff
			uint32_t ip_address;    // IP-Address (Only for J-Link Pro)
			uint32_t subnetmask;    // subnetmask (Only for J-Link Pro)
			uint8_t reserved3[8];   // 0x08 - 0x1F: 0xff
			uint8_t mackaddr[6];    // MAC-Address (Only for J-Link Pro)
			uint8_t reserved[202];  // MAC-Address (Only for J-Link Pro)
		} __attribute__((__packed__));
		typedef jlink_cfg_t jlink_cfg;

		// JLink caps code
		typedef enum {
			EMU_CAP_GET_HW_VERSION = (1 <<  1),
			EMU_CAP_READ_CONFIG    = (1 <<  4),
			EMU_CAP_WRITE_CONFIG   = (1 <<  5),
			EMU_CAP_SPEED_INFO     = (1 <<  9),
			EMU_CAP_GET_HW_INFO    = (1 << 12),
			EMU_CAP_SELECT_IF      = (1 << 17),
			EMU_CAP_GET_CPU_CAPS   = (1 << 21)
		} emu_caps_t;

		// JLink caps code -> string
		const std::string jlink_caps_flags[32] {
			"EMU_CAP_RESERVED",
			"EMU_CAP_GET_HW_VERSION",
			"EMU_CAP_WRITE_DCC",
			"EMU_CAP_ADAPTIVE_CLOCKING",
			"EMU_CAP_READ_CONFIG",
			"EMU_CAP_WRITE_CONFIG",
			"EMU_CAP_TRACE",
			"EMU_CAP_WRITE_MEM",
			"EMU_CAP_READ_MEM",
			"EMU_CAP_SPEED_INFO",
			"EMU_CAP_EXEC_CODE",
			"EMU_CAP_GET_MAX_BLOCK_SIZE",
			"EMU_CAP_GET_HW_INFO",
			"EMU_CAP_SET_KS_POWER",
			"EMU_CAP_RESET_STOP_TIMED",
			"Reserved",
			"EMU_CAP_MEASURE_RTCK_REACT",
			"EMU_CAP_SELECT_IF",  // 17
			"EMU_CAP_RW_MEM_ARM79",
			"EMU_CAP_GET_COUNTERS",
			"EMU_CAP_READ_DCC",  // 20
			"EMU_CAP_GET_CPU_CAPS",
			"EMU_CAP_EXEC_CPU_CMD",
			"EMU_CAP_SWO",
			"EMU_CAP_WRITE_DCC_EX",
			"EMU_CAP_UPDATE_FIRMWARE_EX",  // 25
			"EMU_CAP_FILE_IO",
			"EMU_CAP_REGISTER",
			"EMU_CAP_INDICATORS",
			"EMU_CAP_TEST_NET_SPEED",
			"EMU_CAP_RAWTRACE",
			"Reserved"
		};

		/*!
		 * \brief lowlevel write: EMU_CMD_HW_JTAGx implementation
		 * \param[out]: tdo: TDO read buffer (may be null)
		 * \return false when failure
		 */
		bool ll_write(uint8_t *tdo);

		/*!
		 * \brief read size Bytes using read endpoint
		 * \param[in] cmd: Jlink cmd
		 * \param[out] val: received Bytes
		 * \param[in] size: number of Bytes to read
		 * \return false when transaction failure, true otherwise
		 */
		bool cmd_read(uint8_t cmd, uint8_t *val, int size);
		/*!
		 * \brief read one short using read endpoint
		 * \param[in] cmd: Jlink cmd
		 * \param[out] val: received short
		 * \return false when transaction failure, true otherwise
		 */
		bool cmd_read(uint8_t cmd, uint16_t *val);
		/*!
		 * \brief read one word using read endpoint
		 * \param[in] cmd: Jlink cmd
		 * \param[out] val: received word
		 * \return false when transaction failure, true otherwise
		 */
		bool cmd_read(uint8_t cmd, uint32_t *val);
		/*!
		 * \brief write one short using write endpoint
		 * \param[in] cmd: Jlink cmd
		 * \param[in] val: value to send
		 * \return false when transaction failure, true otherwise
		 */
		bool cmd_write(uint8_t cmd, uint16_t param);
		/*!
		 * \brief write one Byte using write endpoint into EMU_CMD_X register
		 * \param[in] cmd: Jlink cmd
		 * \param[in] val: value to send
		 * \return false when transaction failure, true otherwise
		 */
		bool cmd_write(uint8_t cmd, uint8_t param);

		/*!
		 * \brief lowlevel method to read using libusb_bulk_transfer
		 *             with read endpoint. If required do OByte read
		 * \param[out] buf: buffer used to store read Bytes
		 * \param[in] size: buffer size
		 * \return number of Byte reads or libusb error code
		 */
		int read_device(uint8_t *buf, uint32_t size);
		/*!
		 * \brief lowlevel method to write using libusb_bulk_transfer
		 *             with write endpoint.
		 * \param[in] buf: Bytes to send
		 * \param[in] size: buffer size
		 * \return false when failure, true otherwise
		 */
		bool write_device(const uint8_t *buf, uint32_t size);

		/*!
		 * \brief configure interface (JTAG/SWD)
		 * \param[in] interface: 0 -> JTAG, 1 -> SWD
		 */
		bool set_interface(uint8_t interface);

		/*!
		 * \brief analyze one USB peripheral to search if compatible and
		 *        and for interface/config/alt IDs
		 * \param[in] dev: USB device
		 * \param[in] desc: libusb_device_descriptor
		 * \param[out] interface_idx: interface ID
		 * \param[out] config_idx: configuration descriptor ID
		 * \return false if failure or no interface found
		 */
		bool jlink_search_interface(libusb_device *dev,
				libusb_device_descriptor *desc,
				int *interface_idx, int *config_idx, int *alt_idx);

		/*!
		 * \brief iterate on all USB peripheral to find one JLink
		 * \return false when failure, unable to open or no device found
		 */
		bool jlink_scan_usb();

		typedef struct {
			libusb_device *usb_dev;
			int if_idx;
			int cfg_idx;
			int alt_idx;
		} jlink_devices_t;

		uint32_t _base_freq; /*!< JLink interface frequency */
		uint16_t _min_div;   /*!> dividor applied to base freq */

		int jlink_write_ep;  /*!< jlink write endpoint */
		int jlink_read_ep;   /*!< jlink read endpoint */
		int jlink_interface; /*!< jlink usb interface */
		libusb_device_handle *jlink_handle;
		libusb_context *jlink_ctx;
		std::vector<jlink_devices_t> device_available; /*!< list of compatible devices */
		bool _verbose; /*!< display informations */
		bool _debug;   /*!< display debug messages */
		bool _quiet;   /*!< no messages */

		// buffers for xfer, tdi and tdo
		// each jlink's buffer have 2K Byte
		// enough to send full jtag write
		// buffers must be independent
		uint8_t _xfer_buf[(2048*2) + 4]; /*!> internal buffer */
		uint8_t _tms[2048]; /*!< TMS buffer */
		uint8_t _tdi[2048]; /*!< TDI buffer */
		uint32_t _num_bits; /*!< number of bits stored */
		uint32_t _last_tms; /*!< last known TMS state */
		uint32_t _last_tdi; /*!< last known TDI state */

		uint32_t _caps; /*!< current probe capacity */
		uint8_t _hw_type;
		uint8_t _major; /*!< major Jlink probe release number */
		uint8_t _minor; /*!< minor Jlink probe release number */
		uint8_t _revision; /*!< revision number */
};
#endif  // SRC_JLINK_HPP_
