#ifndef XILINX_HPP
#define XILINX_HPP

#include "bitparser.hpp"
#include "device.hpp"
#include "ftdijtag.hpp"

class Xilinx: public Device {
	public:
		Xilinx(FtdiJtag *jtag, std::string filename);
		~Xilinx();

		void program(unsigned int offset = 0) override;
		int idCode();
		void reset();
	private:
		BitParser _bitfile;
};

#endif
