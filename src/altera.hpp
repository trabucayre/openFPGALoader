#ifndef ALTERA_HPP
#define ALTERA_HPP

#include <string>

#include "device.hpp"
#include "jtag.hpp"
#include "svf_jtag.hpp"

class Altera: public Device {
	public:
		Altera(Jtag *jtag, const std::string &filename,
				const std::string &file_type, int8_t verbose);
		~Altera();

		void programMem();
		void program(unsigned int offset = 0) override;
		int idCode() override;
		void reset() override;
	private:
		SVF_jtag _svf;
};

#endif
