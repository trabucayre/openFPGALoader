#ifndef BOARD_HPP
#define BOARD_HPP

#include <map>

#include "cable.hpp"

/* AN_232R-01_Bit_Bang_Mode_Available_For_FT232R_and_Ft245R */
enum {
	FT232RL_TXD = 0,
	FT232RL_RXD = 1,
	FT232RL_RTS = 2,
	FT232RL_CTS = 3,
	FT232RL_DTR = 4,
	FT232RL_DSR = 5,
	FT232RL_DCD = 6,
	FT232RL_RI  = 7
};

/*!
 * \brief for bitbang mode this structure provide offset for each JTAG signals
 */
typedef struct {
	uint8_t tms_pin; /*! TMS pin offset */
	uint8_t tck_pin; /*! TCK pin offset */
	uint8_t tdi_pin; /*! TDI pin offset */
	uint8_t tdo_pin; /*! TDO pin offset */
} jtag_pins_conf_t;

typedef struct {
	uint8_t cs_pin;    /*! CS pin offset */
	uint8_t sck_pin;   /*! SCK pin offset */
	uint8_t miso_pin;  /*! MISO pin offset */
	uint8_t mosi_pin;  /*! MOSI pin offset */
	uint8_t holdn_pin; /*! HOLDN pin offset */
	uint8_t wpn_pin;   /*! WPN pin offset */
} spi_pins_conf_t;

/*!
 * \brief a board has a target cable and optionnally a pin configuration
 * (bitbang mode)
 */
typedef struct {
	std::string cable_name; /*! provide name of one entry in cable_list */
	jtag_pins_conf_t pins_config; /*! for bitbang, provide struct with pins offset */
} target_cable_t;

#define JTAG_BOARD(_name, _cable) \
	{_name, {_cable, {}}}
#define JTAG_BITBANG_BOARD(_name, _cable, _tms, _tck, _tdi, _tdo) \
	{_name, {_cable, { _tms, _tck, _tdi, _tdo }}}

static std::map <std::string, target_cable_t> board_list = {
	JTAG_BOARD("arty",       "digilent"  ),
	JTAG_BOARD("nexysVideo", "digilent_b"),
	JTAG_BOARD("colorlight", ""          ),
	JTAG_BOARD("crosslinknx_evn", "ft2232"),
	JTAG_BOARD("cyc1000",    "ft2232"     ),
	JTAG_BOARD("de0nano",    "usb-blaster"),
	JTAG_BOARD("ecp5_evn",   "ft2232"     ),
	JTAG_BOARD("machXO2EVN", "ft2232"     ),
	JTAG_BOARD("machXO3SK",  "ft2232"     ),
	JTAG_BOARD("machXO3EVN", "ft2232"     ),
	JTAG_BOARD("licheeTang", "anlogicCable"),
	JTAG_BOARD("littleBee",  "ft2232"      ),
	JTAG_BOARD("spartanEdgeAccelBoard", "" ),
	JTAG_BOARD("pipistrello", "ft2232"     ),
	JTAG_BOARD("qmtechCycloneV", ""        ),
	JTAG_BOARD("tangnano",   "ft2232"      ),
	JTAG_BITBANG_BOARD("ulx2s", "ft232RL", FT232RL_RI, FT232RL_DSR, FT232RL_CTS, FT232RL_DCD),
	JTAG_BITBANG_BOARD("ulx3s", "ft231X",  FT232RL_DCD, FT232RL_DSR, FT232RL_RI, FT232RL_CTS),
	JTAG_BOARD("ecpix5",     "ecpix5-debug"),
};

#endif
