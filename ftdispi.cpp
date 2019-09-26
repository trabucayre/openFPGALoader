
#include <stdio.h>
#include <stdlib.h>
#include <ftdi.h>
#include <unistd.h>
#include <string.h>
#include "ftdipp_mpsse.hpp"
#include "ftdispi.hpp"
//#include "ftdi_handle.h"

/*
 * SCLK -> ADBUS0
 * MOSI -> ADBUS1
 * MISO -> ADBUS2
 * CS	-> ADBUS3
 */
#define SPI_CLK (1 << 0)
#define cs_bits 0x08
#define pindir 0x0b


//uint8_t buffer[1024];
//int num = 0;

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
		_clk = 0;
		_wr_mode = MPSSE_WRITE_NEG;
		_rd_mode = 0;
		break;
	case 1:
		_clk = 0;
		_wr_mode = 0;
		_rd_mode = MPSSE_READ_NEG;
		break;
	case 2:
		_clk = SPI_CLK;
		_wr_mode = 0; //POS
		_rd_mode = MPSSE_READ_NEG;
		break;
	case 3:
		_clk = SPI_CLK;
		_wr_mode = MPSSE_WRITE_NEG;
		_rd_mode = 0;
		break;
	}
}

static FTDIpp_MPSSE::mpsse_bit_config bit_conf =
	{0x08, 0x0B, 0x08, 0x0B};

FtdiSpi::FtdiSpi(int vid, int pid, unsigned char interface, uint32_t clkHZ):
	FTDIpp_MPSSE(vid, pid, interface, clkHZ)
{
	setCSmode(SPI_CS_AUTO);
	setEndianness(SPI_MSB_FIRST);

	init(1, 0x00, bit_conf);
}
FtdiSpi::~FtdiSpi()
{
}

#if 0
#define CLOCK 0x08
#define LATENCY 16
#define TIMEOUT 0
#define SIZE 65536
#define TX_BUFS (60000/8-3)

int ftdi_spi_init_internal(struct ftdi_spi *spi, uint32_t clk_freq_hz);

int ftdi_spi_init_by_name(struct ftdi_spi *spi, char *devname,
			  uint8_t interface, uint32_t clk_freq_hz)
{
	spi->ftdic = open_device_by_name(devname, interface, 115200);
	if (spi->ftdic == NULL) {
		printf("erreur d'ouverture\n");
		return EXIT_FAILURE;
	}
	return ftdi_spi_init_internal(spi, clk_freq_hz);
}

int ftdi_spi_init(struct ftdi_spi *spi, uint32_t vid, uint32_t pid,
		  uint8_t interface, uint32_t clk_freq_hz)
{
	spi->ftdic = open_device(vid, pid, interface, 115200);
	if (spi->ftdic == NULL) {
		printf("erreur d'ouverture\n");
		return EXIT_FAILURE;
	}
	return ftdi_spi_init_internal(spi, clk_freq_hz);
}
#endif
#if 0
int ftdi_spi_init_internal(struct ftdi_spi *spi, uint32_t clock_freq_hz)
{
	setCSmode(spi, SPI_CS_AUTO);
	setEndianness(spi, SPI_MSB_FIRST);
	spi->tx_buff = (uint8_t *)malloc(sizeof(uint8_t) * TX_BUFS);

	if (ftdi_usb_reset(spi->ftdic) != 0) {
		printf("erreur de reset\n");
		return -1;
	}
	if (ftdi_usb_purge_rx_buffer(spi->ftdic) != 0) {
		printf("erreur de reset\n");
		return -1;
	}
	if (ftdi_usb_purge_tx_buffer(spi->ftdic) != 0) {
		printf("erreur de reset\n");
		return -1;
	}
	if (ftdi_read_data_set_chunksize(spi->ftdic, SIZE) != 0) {
		printf("erreur de reset\n");
		return -1;
	}
	if (ftdi_write_data_set_chunksize(spi->ftdic, SIZE) != 0) {
		printf("erreur de reset\n");
		return -1;
	}
	if (ftdi_set_latency_timer(spi->ftdic, LATENCY) != 0) {
		printf("erreur de reset\n");
		return -1;
	}
	if (ftdi_set_event_char(spi->ftdic, 0x00, 0) != 0) {
		printf("erreur de reset\n");
		return -1;
	}
	if (ftdi_set_error_char(spi->ftdic, 0x00, 0) != 0) {
		printf("erreur de reset\n");
		return -1;
	}
	// set the read timeouts in ms for the ft2232H
	spi->ftdic->usb_read_timeout = TIMEOUT;
	// set the write timeouts in ms for the ft2232H
	spi->ftdic->usb_write_timeout = 5000;
	if (ftdi_set_bitmode(spi->ftdic, 0x00, 0x00) != 0) {	// reset controller 
		printf("erreur de reset\n");
		return -1;
	}

	if (ftdi_set_bitmode(spi->ftdic, 0x00, 0x02) != 0) {	// enable mpsse mode
		printf("erreur de reset\n");
		return -1;
	}

	if (ftdi_setClock(spi->ftdic, /*0x08,*/ clock_freq_hz) < 0)
		return -1;
	spi->tx_size = 0;
	
	spi->tx_buff[spi->tx_size++] = 0x97;	// disable adaptive clocking
	// devrait etre 8C pour enable et non 8D
	spi->tx_buff[spi->tx_size++] = 0x8d;	//disable tri phase data clocking
	if (ftdi_write_data(spi->ftdic, spi->tx_buff, spi->tx_size) != spi->tx_size) {
		printf("erreur de write pour dis clock, adaptive, tri phase\n");
		return -1;
	}
	spi->tx_size = 0;
	spi->tx_buff[spi->tx_size++] = 0x85;	// disable loopback
	if (ftdi_write_data(spi->ftdic, spi->tx_buff, spi->tx_size) != spi->tx_size) {
		printf("erreur disable loopback\n");
		return -1;
	}

	spi->tx_size = 0;
	spi->tx_buff[spi->tx_size++] = 0x80;
	spi->tx_buff[spi->tx_size++] = 0x08;
	spi->tx_buff[spi->tx_size++] = 0x0B;
	if (ftdi_write_data(spi->ftdic, spi->tx_buff, spi->tx_size) != spi->tx_size) {
		printf("erreur de write pour set bit\n");
		return -1;
	}
	spi->tx_size = 0;
	return 0;
}
#endif

//FtdiSpi::~FtdiSpi()
//int ftdi_spi_close(struct ftdi_spi *spi)
//{
	//struct ftdi_context *ftdic = spi->ftdic;
	//free(spi->tx_buff);
	//return close_device(ftdic);
//}

// mpsse_write
/*static int send_buf(struct ftdi_context *ftdic, const unsigned char *buf,
		    int size)
{
	int r;
	r = ftdi_write_data(ftdic, (unsigned char *)buf, size);
	if (r < 0) {
		printf("ftdi_write_data: %d, %s\n", r,
			 ftdi_get_error_string(ftdic));
		return 1;
	}
	return 0;
}

static int ft_flush_buffer(struct ftdi_spi *spi)
{
	int ret = 0;
	if (spi->tx_size != 0) {
		ret = send_buf(spi->ftdic, spi->tx_buff, spi->tx_size);
		spi->tx_size = 0;
	}
	return ret;
}*/

// mpsse_store
/*static int ft_store_char(struct ftdi_spi *spi, uint8_t c)
{
	int ret = 0;
	if (spi->tx_size == TX_BUFS)
		ret = ft_flush_buffer(spi);
	spi->tx_buff[spi->tx_size] = c;
	spi->tx_size++;
	return ret;
}

static int ft_store_star_char(struct ftdi_spi *spi, uint8_t *buff, int len)
{
	int ret = 0;
	if (spi->tx_size + len + 1 == TX_BUFS)
		ret = ft_flush_buffer(spi);
	memcpy(spi->tx_buff + spi->tx_size, buff, len);
	spi->tx_size += len;
	return ret;
}*/

// mpsse read
/*static int get_buf(struct ftdi_spi *spi, const unsigned char *buf,
		   int size)
{
	int r;
	ft_store_char(spi, SEND_IMMEDIATE);
	ft_flush_buffer(spi);

	while (size > 0) {
		r = ftdi_read_data(spi->ftdic, (unsigned char *)buf, size);
		if (r < 0) {
			printf("ftdi_read_data: %d, %s\n", r,
				 ftdi_get_error_string(spi->ftdic));
			return 1;
		}
		buf += r;
		size -= r;
	}
	return 0;
}*/

/* send two consecutive cs configuration */
void FtdiSpi::confCs(char stat)
{
	uint8_t tx_buf[6] = {SET_BITS_LOW, _clk, pindir,
						 SET_BITS_LOW, _clk, pindir};

	tx_buf[1] |= (stat) ? cs_bits : 0;
	tx_buf[4] |= (stat) ? cs_bits : 0;

	if (mpsse_store(tx_buf, 6) != 0)
		printf("erreur\n");
}

void FtdiSpi::setCs()
{
	_cs = cs_bits;
	confCs(_cs);
}

void FtdiSpi::clearCs()
{
	_cs = 0x00;
	confCs(_cs);
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
	//struct ftdi_context *ftdic = spi->ftdic;
	uint8_t buf[TX_BUF+3];//65536+9];
	/* failed is special. We use bitwise ops, but it is essentially bool. */
	int i = 0, failed = 0;
	int ret = 0;

	uint8_t *rx_ptr = readarr;
	uint8_t *tx_ptr = (uint8_t *)writearr;
	int len = writecnt;
	int xfer;

	if (_cs_mode == SPI_CS_AUTO) {
		buf[i++] = SET_BITS_LOW;
		buf[i++] = (0 & ~cs_bits) | _clk;	/* assertive */
		buf[i++] = pindir;
		mpsse_store(buf, i);
		i=0;
	}

	/*
	 * Minimize USB transfers by packing as many commands as possible
	 * together. If we're not expecting to read, we can assert CS#, write,
	 * and deassert CS# all in one shot. If reading, we do three separate
	 * operations.
	 */
	while (len > 0) {
		xfer = (len > TX_BUF) ? TX_BUF: len;

		buf[i++] = /*(spi->endian == SPI_MSB_FIRST) ? 0 : MPSSE_LSB |*/
					((readarr) ? (MPSSE_DO_READ | _rd_mode) : 0) |
					((writearr) ? (MPSSE_DO_WRITE | _wr_mode) : 0);// |
					/*MPSSE_DO_WRITE |*/// spi->wr_mode | spi->rd_mode;
		buf[i++] = (xfer - 1) & 0xff;
		buf[i++] = ((xfer - 1) >> 8) & 0xff;
		if (writearr) {
			memcpy(buf + i, tx_ptr, xfer);
			tx_ptr += xfer;
			i += xfer;
		}

		ret = mpsse_store(buf, i);
		failed = ret;
		if (ret)
			printf("send_buf failed before read: %i %s\n", ret, "plop");// ftdi_get_error_string(ftdic));
		i = 0;
		if (readarr) {
			//if (ret == 0) {
			ret = mpsse_read(rx_ptr, xfer);
			failed = ret;
			if (ret != xfer)
				printf("get_buf failed: %i\n", ret);
			//}
			rx_ptr += xfer;
		}
		len -= xfer;

	}

	if (_cs_mode == SPI_CS_AUTO) {
		buf[i++] = SET_BITS_LOW;
		buf[i++] = cs_bits | _clk;
		buf[i++] = pindir;
		ret = mpsse_store(buf, i);
		failed |= ret;
		if (ret)
			printf("send_buf failed at end: %i\n", ret);
	}

	return 0;//failed ? -1 : 0;
}
