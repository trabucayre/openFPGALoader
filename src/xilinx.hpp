#ifndef XILINX_HPP
#define XILINX_HPP

#include <string>

#include "configBitstreamParser.hpp"
#include "device.hpp"
#include "jtag.hpp"
#include "spiInterface.hpp"

class Xilinx: public Device, SPIInterface {
	public:
		Xilinx(Jtag *jtag, const std::string &filename,
				bool flash_wr, bool sram_wr, int8_t verbose);
		~Xilinx();

		void program(unsigned int offset = 0) override;
		void program_spi(ConfigBitstreamParser * bit, unsigned int offset = 0);
		void program_mem(ConfigBitstreamParser *bitfile);
		int idCode() override;
		void reset() override;

		/* spi interface */
		int spi_put(uint8_t cmd, uint8_t *tx, uint8_t *rx,
				uint32_t len) override;
		int spi_put(uint8_t *tx, uint8_t *rx, uint32_t len) override;
		int spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
				uint32_t timeout, bool verbose = false) override;
};

#endif
