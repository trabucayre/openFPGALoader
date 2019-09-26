#ifndef ALTERA_HPP
#define ALTERA_HPP

#include "bitparser.hpp"
#include "device.hpp"
#include "ftdijtag.hpp"

class Altera: public Device {
	public:
		Altera(FtdiJtag *jtag, enum prog_mode mode, std::string filename);
		~Altera();

		void program();
		int idCode();
	private:
		BitParser _bitfile;
};

#endif
