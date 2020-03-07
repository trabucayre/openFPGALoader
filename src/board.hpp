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

/*!
 * \brief a board has a target cable and optionnally a pin configuration
 * (bitbang mode)
 */
typedef struct {
	std::string cable_name; /*! provide name of one entry in cable_list */
	jtag_pins_conf_t pins_config; /*! for bitbang, provide struct with pins offset */
} target_cable_t;

static std::map <std::string, target_cable_t> board_list = {
	{"arty",       {"digilent",   {}}},
	{"colorlight", {"",           {}}},
	{"cyc1000",    {"ft2232",     {}}},
	{"de0nano",    {"usbblaster", {}}},
	{"ecp5_evn",   {"ft2232",     {}}},
	{"machXO3SK",  {"ft2232",     {}}},
	{"littleBee",  {"ft2232",     {}}},
	{"spartanEdgeAccelBoard", {"",{}}},
	{"tangnano",   {"ft2232",     {}}},
	{"ulx3s",      {"ft231X",   {FT232RL_DCD, FT232RL_DSR, FT232RL_RI, FT232RL_CTS}}}
};

#endif
