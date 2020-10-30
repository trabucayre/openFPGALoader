
#include <stdio.h>
#include <stdlib.h>
#include <ftdi.h>
#include <unistd.h>
#include <string.h>
#include "board.hpp"
#include "ftdipp_mpsse.hpp"
#include "ftdispi.hpp"

/*
 * SCLK -> ADBUS0
 * MOSI -> ADBUS1
 * MISO -> ADBUS2
 * CS	-> ADBUS3
 */

/* GGM: Faut aussi definir l'etat des broches par defaut */
/* necessaire en mode0 et 1, ainsi qu'entre 2 et 3
 */
/* Rappel :
 * Mode0 : clk idle low, ecriture avant le premier front
 * 			ie lecture sur le premier front (montant)
 * Mode1 : clk idle low, ecriture sur le premier front (montant)
 * 			lecture sur le second front (descendant)
 * Mode2 : clk idle high, ecriture avant le premier front
 * 			lecture sur le premier front (descendant)
 * Mode3 : clk idle high, ecriture sur le premier front (descendant)
 * 			lecture sur le second front (montant)
 */
void FtdiSpi::setMode(uint8_t mode)
{
	switch (mode) {
	case 0:
		_clk_idle = 0;
		_wr_mode = MPSSE_WRITE_NEG;
		_rd_mode = 0;
		break;
	case 1:
		_clk_idle = 0;
		_wr_mode = 0;
		_rd_mode = MPSSE_READ_NEG;
		break;
	case 2:
		_clk_idle = _clk;
		_wr_mode = 0; //POS
		_rd_mode = MPSSE_READ_NEG;
		break;
	case 3:
		_clk_idle = _clk;
		_wr_mode = MPSSE_WRITE_NEG;
		_rd_mode = 0;
		break;
	}
	/* for clk pin in idle state */
	if (_clk_idle)
		gpio_set(_clk, true);
	else
		gpio_clear(_clk, true);
}

static FTDIpp_MPSSE::mpsse_bit_config bit_conf =
	{0x403, 0x6010, INTERFACE_B, 0x08, 0x0B, 0x08, 0x0B};

FtdiSpi::FtdiSpi(int vid, int pid, unsigned char interface, uint32_t clkHZ,
	bool verbose):
	FTDIpp_MPSSE(bit_conf, "", "", clkHZ, verbose)
{
	setMode(0);
	setCSmode(SPI_CS_AUTO);
	setEndianness(SPI_MSB_FIRST);

	init(1, 0x00, BITMODE_MPSSE);
}

FtdiSpi::FtdiSpi(const FTDIpp_MPSSE::mpsse_bit_config &conf,
		spi_pins_conf_t spi_config,
		uint32_t clkHZ, bool verbose):
		FTDIpp_MPSSE(conf, "", "", clkHZ, verbose),
		_cs_bits(1 << 3), _clk(1 << 0)
{
	/* if cs not provided use pin 4 */
	/*if (_cs == 0)
		_cs = (1<<4);
	_cable.low_dir |= _cs;*/

	printf("cs pin : %d\n", spi_config.cs_pin);
	if (spi_config.cs_pin)
		_cs_bits = spi_config.cs_pin;
	if (spi_config.sck_pin)
		_clk = spi_config.sck_pin;

	/* clk is fixed by MPSSE engine
	 * but CS is free -> update bit direction
	 */

	gpio_set_output(_cs_bits, true);
	gpio_set(_cs_bits, true);

	setMode(0);
	setCSmode(SPI_CS_AUTO);
	setEndianness(SPI_MSB_FIRST);

	init(1, 0x00, BITMODE_MPSSE);
}

FtdiSpi::~FtdiSpi()
{
}

/* send two consecutive cs configuration */
bool FtdiSpi::confCs(char stat)
{
	bool ret;
	if (stat == 0) {
		ret = gpio_clear(_cs_bits, true);
		ret |= gpio_clear(_cs_bits, true);
	} else {
		ret = gpio_set(_cs_bits, true);
		ret |= gpio_set(_cs_bits, true);
	}
	if (!ret)
		printf("Error: CS update\n");
	return ret;
}

bool FtdiSpi::setCs()
{
	_cs = _cs_bits;
	return confCs(_cs);
}

bool FtdiSpi::clearCs()
{
	_cs = 0x00;
	return confCs(_cs);
}

int FtdiSpi::ft2232_spi_wr_then_rd(
						const uint8_t *tx_data, uint32_t tx_len,
						uint8_t *rx_data, uint32_t rx_len)
{
	setCSmode(SPI_CS_MANUAL);
	clearCs();
	uint32_t ret = ft2232_spi_wr_and_rd(tx_len, tx_data, NULL);
	if (ret != 0) {
		printf("%s : write error %d %d\n", __func__, ret, tx_len);
	} else {
		ret = ft2232_spi_wr_and_rd(rx_len, NULL, rx_data);
		if (ret != 0) {
			printf("%s : read error\n", __func__);
		}
	}
	setCs();
	setCSmode(SPI_CS_AUTO);
	return ret;
}

/* Returns 0 upon success, a negative number upon errors. */
int FtdiSpi::ft2232_spi_wr_and_rd(//struct ftdi_spi *spi,
			    uint32_t writecnt,
			    const uint8_t * writearr, uint8_t * readarr)
{
#define TX_BUF (60000/8-3)
	uint8_t buf[TX_BUF+3];//65536+9];
	int i = 0;
	int ret = 0;

	uint8_t *rx_ptr = readarr;
	uint8_t *tx_ptr = (uint8_t *)writearr;
	uint32_t len = writecnt;
	uint32_t xfer;

	if (_cs_mode == SPI_CS_AUTO) {
		clearCs();
	}
	mpsse_write();

	/*
	 * Minimize USB transfers by packing as many commands as possible
	 * together. If we're not expecting to read, we can assert CS#, write,
	 * and deassert CS# all in one shot. If reading, we do three separate
	 * operations.
	 */
	while (len > 0) {
		xfer = (len > TX_BUF) ? TX_BUF: len;

		buf[i++] = ((readarr) ? (MPSSE_DO_READ | _rd_mode) : 0) |
					((writearr) ? (MPSSE_DO_WRITE | _wr_mode) : 0);
		buf[i++] = (xfer - 1) & 0xff;
		buf[i++] = ((xfer - 1) >> 8) & 0xff;
		if (writearr) {
			memcpy(buf + i, tx_ptr, xfer);
			tx_ptr += xfer;
			i += xfer;
		}

		ret = mpsse_store(buf, i);
		if (ret)
			printf("send_buf failed before read: %i %s\n", ret, ftdi_get_error_string(_ftdi));
		i = 0;
		if (readarr) {
			//if (ret == 0) {
			ret = mpsse_read(rx_ptr, xfer);
			if ((uint32_t)ret != xfer)
				printf("get_buf failed: %i\n", ret);
			//}
			rx_ptr += xfer;
		} else {
			ret = mpsse_write();
			if ((uint32_t)ret != xfer+3)
				printf("error %d %d\n", ret, i);
		}
		len -= xfer;

	}

	if (_cs_mode == SPI_CS_AUTO) {
		if (!setCs())
			printf("send_buf failed at write %d\n", ret);
	}

	return 0;
}

/* method spiInterface::spi_put */
int FtdiSpi::spi_put(uint8_t cmd, uint8_t *tx, uint8_t *rx, uint32_t len)
{
	uint32_t xfer_len = len + 1;
	uint8_t jtx[xfer_len];
	uint8_t jrx[xfer_len];

	jtx[0] = cmd;
	if (tx != NULL)
		memcpy(jtx+1, tx, len);

	/* send first alreay stored cmd,
	 * in the same time store each byte
	 * to next
	 */
	ft2232_spi_wr_and_rd(xfer_len, jtx, (rx != NULL)?jrx:NULL);

	if (rx != NULL)
		memcpy(rx, jrx+1, len);

	return 0;
}

/* method spiInterface::spi_put */
int FtdiSpi::spi_put(uint8_t *tx, uint8_t *rx, uint32_t len)
{
	return ft2232_spi_wr_and_rd(len, tx, rx);
}

/* method spiInterface::spi_wait
 */
int FtdiSpi::spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
			uint32_t timeout, bool verbose)
{
	uint8_t rx;
	uint32_t count = 0;

	setCSmode(SPI_CS_MANUAL);
	clearCs();
	ft2232_spi_wr_and_rd(1, &cmd, NULL);
	do {
		ft2232_spi_wr_and_rd(1, NULL, &rx);
		count ++;
		if (count == timeout) {
			printf("timeout: %2x %d\n", rx, count);
			break;
		}

		if (verbose) {
			printf("%02x %02x %02x %02x\n", rx, mask, cond, count);
		}
	} while((rx & mask) != cond);
	setCs();
	setCSmode(SPI_CS_AUTO);

	if (count == timeout) {
		printf("%x\n", rx);
		std::cout << "wait: Error" << std::endl;
		return -ETIME;
	} else
		return 0;
}
