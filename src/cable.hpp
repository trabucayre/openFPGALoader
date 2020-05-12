#ifndef CABLE_HPP
#define CABLE_HPP

#include <map>
#include <string>

#include "ftdipp_mpsse.hpp"

/*!
 * \brief define type of communication
 */
enum {
	MODE_FTDI_BITBANG = 0, /*! used with ft232RL/ft231x */
	MODE_FTDI_SERIAL  = 1  /*! ft2232, ft232H */
} communication_type_t;

typedef struct {
	int type;
	FTDIpp_MPSSE::mpsse_bit_config config;
} cable_t;

static std::map <std::string, cable_t> cable_list = {
	// last 4 bytes are ADBUS7-0 value, ADBUS7-0 direction, ACBUS7-0 value, ACBUS7-0 direction
	// some cables requires explicit values on some of the I/Os
	{"bus_blaster",  {MODE_FTDI_SERIAL,  {0x0403, 0x6010, INTERFACE_A, 0x08, 0x1B, 0x08, 0x0B}}},
	{"bus_blaster_b",{MODE_FTDI_SERIAL,  {0x0403, 0x6010, INTERFACE_B, 0x08, 0x0B, 0x08, 0x0B}}},
	{"digilent",     {MODE_FTDI_SERIAL,  {0x0403, 0x6010, INTERFACE_A, 0xe8, 0xeb, 0x00, 0x60}}},
	{"digilent_hs2", {MODE_FTDI_SERIAL,  {0x0403, 0x6014, INTERFACE_A, 0xe8, 0xeb, 0x00, 0x60}}},
	{"digilent_hs3", {MODE_FTDI_SERIAL,  {0x0403, 0x6014, INTERFACE_A, 0x88, 0x8B, 0x20, 0x30}}},
	{"ft2232",       {MODE_FTDI_SERIAL,  {0x0403, 0x6010, INTERFACE_A, 0x08, 0x0B, 0x08, 0x0B}}},
	{"ft231X",       {MODE_FTDI_BITBANG, {0x0403, 0x6015, INTERFACE_A, 0x00, 0x00, 0x00, 0x00}}},
	{"ft232",        {MODE_FTDI_SERIAL,  {0x0403, 0x6014, INTERFACE_A, 0x08, 0x0B, 0x08, 0x0B}}},
	{"ft232RL",      {MODE_FTDI_BITBANG, {0x0403, 0x6001, INTERFACE_A, 0x08, 0x0B, 0x08, 0x0B}}},
	{"ft4232",       {MODE_FTDI_SERIAL,  {0x0403, 0x6011, INTERFACE_A, 0x08, 0x0B, 0x08, 0x0B}}},
	{"ecpix5-debug", {MODE_FTDI_SERIAL,  {0x0403, 0x6010, INTERFACE_A, 0xF8, 0xFB, 0xFF, 0xFF}}},
};

#endif
