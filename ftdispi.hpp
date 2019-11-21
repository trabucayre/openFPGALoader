#include <ftdi.h>
#include <iostream>
#include <vector>

#include "ftdipp_mpsse.hpp"

class FtdiSpi : public FTDIpp_MPSSE {
 public:
	#define SPI_MSB_FIRST 0
	#define SPI_LSB_FIRST 1

	#define SPI_CS_AUTO   0
	#define SPI_CS_MANUAL 1


	FtdiSpi(int vid, int pid, unsigned char interface, uint32_t clkHZ,
		bool verbose);
	~FtdiSpi();

	void setMode(uint8_t mode);
	void setEndianness(unsigned char endian) {
		_endian =(endian == SPI_MSB_FIRST) ? 0 : MPSSE_LSB;
	}

	void setCSmode(uint8_t cs_mode) {_cs_mode = cs_mode;}
	void confCs(char stat);
	void setCs();
	void clearCs();

	int ft2232_spi_wr_then_rd(const uint8_t *tx_data, uint32_t tx_len,
							uint8_t *rx_data, uint32_t rx_len);
	int ft2232_spi_wr_and_rd(uint32_t writecnt,
							const uint8_t *writearr, uint8_t *readarr);

 private:
	uint8_t _cs;
	uint8_t _clk;
	uint8_t _wr_mode;
	uint8_t _rd_mode;
	unsigned char _endian;
	uint8_t _cs_mode;
 protected:
	bool _verbose;
};
