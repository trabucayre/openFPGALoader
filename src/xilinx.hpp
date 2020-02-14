#ifndef XILINX_HPP
#define XILINX_HPP

#include "bitparser.hpp"
#include "device.hpp"
#include "ftdijtag.hpp"

class Xilinx: public Device {
	public:
		Xilinx(FtdiJtag *jtag, std::string filename, bool verbose);
		~Xilinx();

		void program(unsigned int offset = 0) override;
		void program_spi(unsigned int offset = 0);
		void program_mem(BitParser &bitfile);
		int idCode();
		void reset();
};

#endif
