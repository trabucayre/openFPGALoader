/*
 * Copyright (C) 2020 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

//#include <libusb.h>
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <cstdio>

#include "display.hpp"
#include "fpgalink.hpp"
#include "makestuff/libfpgalink.h"



using namespace std;

#undef CHECK_STATUS

#define CHECK_STATUS(fStatus)\
    if (fStatus) \
	{ \
		printError("Error: ", false);\
		printError(error);\
		flFreeError(error);\
		fStatus = progClose(handle, &error);\
		if (error)\
			flFreeError(error);\
		flClose(handle);\
		handle = NULL;\
		throw std::exception();\
	}

FpgaLink::FpgaLink(bool verbose):
			_verbose(verbose)
{

}


int FpgaLink::writeTMS(uint8_t *tms, int len, bool flush_buffer)
{
	(void)flush_buffer;
	FLStatus fStatus;
	const char *error = NULL;
	uint32_t bitPattern = 0;
	int ulen = len;
	int i = 0; 
	while (ulen > 0)
	{
		{bitPattern = tms[i++];} 
		ulen-=8;
		if (ulen > 0) {bitPattern |= tms[i++] << 8;}
		ulen-=8;
		if (ulen > 0) {bitPattern |= tms[i++] << 16;}
		ulen-=8;
		if (ulen > 0) {bitPattern |= tms[i++] << 24;}
		ulen-=8;
		fStatus = jtagClockFSM(handle, bitPattern, (ulen > 0) ? 32 : 32 + ulen, &error);
		CHECK_STATUS(fStatus);
	}

	return len;
}

int FpgaLink::toggleClk(uint8_t tms, uint8_t tdi, uint32_t clk_len)
{
	(void)tms;
	(void)tdi;
	FLStatus fStatus;
	const char *error = NULL;
	fStatus = jtagClocks(handle, clk_len, &error);
	CHECK_STATUS(fStatus);
	return EXIT_SUCCESS;
}

int FpgaLink::flush()
{
	return 0;
}

int FpgaLink::writeTDI(uint8_t *tx, uint8_t *rx, uint32_t len, bool end)
{
	FLStatus fStatus;
	const char *error = NULL;
	if (rx)
		fStatus = jtagShiftInOut(handle, len, tx, rx, end, &error);
	else
	{
		fStatus = jtagShiftInOnly(handle, len, tx, end, &error);
	}
		
	CHECK_STATUS(fStatus);

	return EXIT_SUCCESS;
}


FX2Cable::FX2Cable(bool verbose, const jtag_pins_conf_t *pin_conf, const char *ivp, const char *vp):
			FpgaLink::FpgaLink(verbose)
{
	FLStatus fStatus;
	const char *error = NULL;
	fStatus = flInitialise(0, &error);
	CHECK_STATUS(fStatus);
	char portConfig[10];

	vp = "1D50:602B";
	std::string svp(vp); 

	printInfo("Attempting to open connection to FPGALink device " + svp +"...", false);
	fStatus = flOpen(vp, &handle, NULL);
	if ( fStatus ) {
		{
			int count = 60;
			uint8 flag;
			ivp = "04b4:8613";
			std::string sivp(ivp);
			printInfo("Loading firmware into " + sivp, true);
			fStatus = flLoadStandardFirmware(ivp, vp, &error);
			CHECK_STATUS(fStatus);
			
			printInfo("Awaiting renumeration");
			flSleep(1000);
			do {
				printInfo(".", false);
				fflush(stdout);
				fStatus = flIsDeviceAvailable(vp, &flag, &error);
				CHECK_STATUS(fStatus);
				flSleep(250);
				count--;
			} while ( !flag && count );
			printf("\n");
			if ( !flag ) {
				printError("Error: FPGALink device did not renumerate properly as " + svp, true);
				flClose(handle);
				handle = NULL;
				throw std::exception();
			}

			printInfo("Attempting to open connection to FPGLink device " + svp + " again...", false);
			fStatus = flOpen(vp, &handle, &error);
			CHECK_STATUS(fStatus);
			printInfo("success", true);
		}
	} else {
		printInfo("success", true);
	} 

	transcode_pin_config(pin_conf, portConfig);
	fStatus = progOpen(handle, portConfig, &error);
	CHECK_STATUS(fStatus);

}

void FX2Cable::transcode_pin_config(const jtag_pins_conf_t *pin_conf, char* buffer)
{
	sprintf(buffer, "%X%X%X%X",pin_conf->tdo_pin, pin_conf->tdi_pin, pin_conf->tms_pin, pin_conf->tck_pin);
}

FX2Cable::~FX2Cable()
{
	const char *error = NULL;
	FLStatus  fStatus;
	(void)fStatus;
	if (handle)
	{
		fStatus = progClose(handle, &error);
		flClose(handle);
		handle = NULL;
	}
}

