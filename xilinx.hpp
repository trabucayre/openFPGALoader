#ifndef XILINX_HPP
#define XILINX_HPP

#include "bitparser.hpp"
#include "device.hpp"
#include "ftdijtag.hpp"

class Xilinx: public Device {
	public:
		Xilinx(FtdiJtag *jtag, enum prog_mode mode, std::string filename);
		~Xilinx();

		void program();
		int idCode();
		void reset();
	private:
		void flow_enable();
		void flow_disable();
		//FtdiJtag *_jtag;
		//std::string _filename;
		BitParser _bitfile;
};

#endif
