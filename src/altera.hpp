#ifndef ALTERA_HPP
#define ALTERA_HPP

#include "bitparser.hpp"
#include "device.hpp"
#include "jtag.hpp"
#include "svf_jtag.hpp"

class Altera: public Device {
	public:
		Altera(Jtag *jtag, std::string filename, bool verbose);
		~Altera();

		void program(unsigned int offset = 0);
		int idCode();
		void reset() override;
	private:
		SVF_jtag _svf;
};

#endif
