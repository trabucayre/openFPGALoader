#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <iostream>
#include <string>

#include "jtag.hpp"

/* GGM: TODO: program must have an optional
 * offset
 * and question: bitstream to load bitstream in SPI mode must
 * be hardcoded or provided by user?
 */
class Device {
	public:
		enum prog_mode {
			NONE_MODE = 0,
			SPI_MODE = 1,
			FLASH_MODE = 1,
			MEM_MODE = 2
		};

		typedef enum {
			WR_SRAM = 0,
			WR_FLASH = 1
		} prog_type_t;

		Device(Jtag *jtag, std::string filename, int8_t verbose = false);
		virtual ~Device();
		virtual void program(unsigned int offset = 0) = 0;
		virtual int  idCode() = 0;
		virtual void reset();

	protected:
		Jtag *_jtag;
		std::string _filename;
		std::string _file_extension;
		enum prog_mode _mode;
		bool _verbose;
		bool _quiet;
};

#endif
