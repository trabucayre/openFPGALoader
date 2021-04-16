#ifndef BOARD_HPP
#define BOARD_HPP

#include <map>

#include "cable.hpp"

/* AN_232R-01_Bit_Bang_Mode_Available_For_FT232R_and_Ft245R */
enum {
	FT232RL_TXD = (1 << 0),
	FT232RL_RXD = (1 << 1),
	FT232RL_RTS = (1 << 2),
	FT232RL_CTS = (1 << 3),
	FT232RL_DTR = (1 << 4),
	FT232RL_DSR = (1 << 5),
	FT232RL_DCD = (1 << 6),
	FT232RL_RI  = (1 << 7)
};

/* AN_108_Command_Processor_for_MPSSE_and_MCU_Host_Bus_Emulation_Modes */
enum {
	DBUS0 = (1 <<  0),
	DBUS1 = (1 <<  1),
	DBUS2 = (1 <<  2),
	DBUS3 = (1 <<  3),
	DBUS4 = (1 <<  4),
	DBUS5 = (1 <<  5),
	DBUS6 = (1 <<  6),
	DBUS7 = (1 <<  7),
	CBUS0 = (1 <<  8),
	CBUS1 = (1 <<  9),
	CBUS2 = (1 << 10),
	CBUS3 = (1 << 11),
	CBUS4 = (1 << 12),
	CBUS5 = (1 << 13),
	CBUS6 = (1 << 14),
	CBUS7 = (1 << 15)
};

/*!
 * \brief for bitbang mode this structure provide value for each JTAG signals
 */
typedef struct {
	uint8_t tms_pin; /*! TMS pin value */
	uint8_t tck_pin; /*! TCK pin value */
	uint8_t tdi_pin; /*! TDI pin value */
	uint8_t tdo_pin; /*! TDO pin value */
} jtag_pins_conf_t;

typedef struct {
	uint16_t cs_pin;    /*! CS pin value */
	uint16_t sck_pin;   /*! SCK pin value */
	uint16_t miso_pin;  /*! MISO pin value */
	uint16_t mosi_pin;  /*! MOSI pin value */
	uint16_t holdn_pin; /*! HOLDN pin value */
	uint16_t wpn_pin;   /*! WPN pin value */
} spi_pins_conf_t;

enum {
	COMM_JTAG = (1 << 0),
	COMM_SPI  = (1 << 1)
};

/*!
 * \brief a board has a target cable and optionnally a pin configuration
 * (bitbang mode)
 */
typedef struct {
	std::string manufacturer;
	std::string cable_name; /*! provide name of one entry in cable_list */
	uint16_t reset_pin;      /*! reset pin value */
	uint16_t done_pin;       /*! done pin value */
	uint16_t mode;           /*! communication type (JTAG or SPI) */
	jtag_pins_conf_t jtag_pins_config; /*! for bitbang, provide struct with pins value */
	spi_pins_conf_t spi_pins_config; /*! for SPI, provide struct with pins value */
} target_cable_t;

#define JTAG_BOARD(_name, _cable, _rst, _done) \
	{_name, {"", _cable, _rst, _done, COMM_JTAG, {}, {}}}
#define JTAG_BITBANG_BOARD(_name, _cable, _rst, _done, _tms, _tck, _tdi, _tdo) \
	{_name, {"", _cable, _rst, _done, COMM_JTAG, { _tms, _tck, _tdi, _tdo }, {}}}
#define SPI_BOARD(_name, _manufacturer, _cable, _rst, _done, _cs, _sck, _si, _so, _holdn, _wpn) \
	{_name, {_manufacturer, _cable, _rst, _done, COMM_SPI, {}, \
		{_cs, _sck, _so, _si, _holdn, _wpn}}}

static std::map <std::string, target_cable_t> board_list = {
	JTAG_BOARD("acornCle215", "",          0, 0),
	JTAG_BOARD("alchitry_au", "ft2232",    0, 0),
	JTAG_BOARD("arty",       "digilent",   0, 0),
	JTAG_BOARD("nexysVideo", "digilent_b", 0, 0),
	JTAG_BOARD("kc705", "digilent", 0, 0),
	JTAG_BOARD("colorlight", "",           0, 0),
	JTAG_BOARD("crosslinknx_evn", "ft2232", 0, 0),
	JTAG_BOARD("cyc1000",    "ft2232",     0, 0),
	JTAG_BOARD("de0",        "usb-blaster",0, 0),
	JTAG_BOARD("de0nano",    "usb-blaster",0, 0),
	JTAG_BOARD("ecp5_evn",   "ft2232",     0, 0),
	SPI_BOARD("fireant", "efinix", "ft232",
			DBUS4, DBUS5, DBUS3, DBUS0, DBUS1, DBUS2, DBUS6, 0),
	/* most ice40 boards uses the same pinout */
	SPI_BOARD("ice40_generic", "lattice", "ft2232",
			DBUS7, DBUS6,
			DBUS4, DBUS0, DBUS1, DBUS2,
			0, 0),
	JTAG_BOARD("machXO2EVN", "ft2232",     0, 0),
	JTAG_BOARD("machXO3SK",  "ft2232",     0, 0),
	JTAG_BOARD("machXO3EVN", "ft2232",     0, 0),
	JTAG_BOARD("licheeTang", "anlogicCable", 0, 0),
	/* left for backward compatibility, use tec0117 instead */
	JTAG_BOARD("littleBee",  "ft2232",     0, 0),
	JTAG_BOARD("spartanEdgeAccelBoard", "",0, 0),
	JTAG_BOARD("pipistrello", "ft2232",    0, 0),
	JTAG_BOARD("qmtechCycloneV", "",       0, 0),
	JTAG_BOARD("runber",     "ft232",      0, 0),
	JTAG_BOARD("tangnano",   "ft2232",     0, 0),
	JTAG_BOARD("tec0117",    "ft2232",     0, 0),
	JTAG_BITBANG_BOARD("ulx2s", "ft232RL", 0, 0, FT232RL_RI, FT232RL_DSR, FT232RL_CTS, FT232RL_DCD),
	JTAG_BITBANG_BOARD("ulx3s", "ft231X",  0, 0, FT232RL_DCD, FT232RL_DSR, FT232RL_RI, FT232RL_CTS),
	JTAG_BOARD("ecpix5",     "ecpix5-debug", 0, 0),
	JTAG_BOARD("xtrx",       ""            , 0, 0),
	SPI_BOARD("xyloni_spi", "efinix", "efinix_spi",
			DBUS4 | DBUS7, DBUS5, DBUS3, DBUS0, DBUS1, DBUS2, DBUS6, 0),
};

#endif
