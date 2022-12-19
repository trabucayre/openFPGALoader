// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "epcq2.hpp"

#define RD_STATUS_REG       0x05
#  define STATUS_REG_WEL    (0x01 << 1)
#  define STATUS_REG_WIP    (0x01 << 0)
#define RD_BYTE_REG         0x03
#define RD_DEV_ID_REG       0x9F
#define RD_SILICON_ID_REG   0xAB
#define RD_FAST_READ_REG    0x0B
/* TBD */
#define WR_ENABLE_REG       0x06
#define WR_DISABLE_REG      0x04
#define WR_STATUS_REG       0x01
#define WR_BYTES_REG        0x02
/* TBD */
#define ERASE_BULK_REG      0xC7
#define ERASE_SECTOR_REG    0xD8
#define ERASE_SUBSECTOR_REG 0x20
#define RD_SFDP_REG_REG     0x5A

#define SECTOR_SIZE			65536

/* EPCQ wait for LSB first data
 * so we simply reconstruct a new char with reverse
 */
unsigned char EPCQ::convertLSB(unsigned char src)
{
	unsigned char res = 0;

	for (int i=0; i < 8; i++)
		res = (res << 1) | ((src >> i) & 0x01);

	return res;

}

/* wait for WEL goes high by reading
 * status register in a loop
 */
#if 0
void EPCQ::wait_wel()
{
	uint8_t cmd = RD_STATUS_REG, recv;

	_spi.setCSmode(FtdiSpi::SPI_CS_MANUAL);
	_spi.clearCs();
	_spi.ft2232_spi_wr_and_rd(1, &cmd, NULL);
	do {
		_spi.ft2232_spi_wr_and_rd(1, NULL, &recv);
	} while(!(recv & STATUS_REG_WEL));
	_spi.setCs();
	_spi.setCSmode(FtdiSpi::SPI_CS_AUTO);
}
#endif

/* wait for WIP goes low by reading
 * status register in a loop
 */
#if 0
void EPCQ::wait_wip()
{
	uint8_t cmd = RD_STATUS_REG, recv;

	_spi.setCSmode( FtdiSpi::SPI_CS_MANUAL);
	_spi.clearCs();
	_spi.ft2232_spi_wr_and_rd(1, &cmd, NULL);
	do {
		_spi.ft2232_spi_wr_and_rd(1, NULL, &recv);
	} while(0x00 != (recv & STATUS_REG_WIP));
	_spi.setCs();
	_spi.setCSmode( FtdiSpi::SPI_CS_AUTO);
}
#endif

/* enable write enable */
#if 0
int EPCQ::do_write_enable()
{
	uint8_t cmd;
	cmd = WR_ENABLE_REG;
	_spi.ft2232_spi_wr_and_rd(1, &cmd, NULL);
	wait_wel();
	return 0;
}
#endif


/* currently we erase sector but it's possible to 
 * do sector + subsector to reduce erase
 */

#if 0
int EPCQ::erase_sector(char start_sector, char nb_sectors)
{
	uint8_t buffer[4] = {ERASE_SECTOR_REG, 0, 0, 0};
	uint32_t base_addr = start_sector * SECTOR_SIZE;

	/* 1. enable write
	 * 2. send opcode + address in targeted sector
	 * 3. wait for end. 
	 */

	printf("erase %d sectors\n", nb_sectors);
	for (base_addr = start_sector * SECTOR_SIZE; nb_sectors >= 0; nb_sectors--, base_addr += SECTOR_SIZE) {
		/* allow write */
		do_write_enable();
		/* send addr in the current sector */	
		buffer[1] = (base_addr >> 16) & 0xff;
		buffer[2] = (base_addr >> 8) & 0x0ff;
		buffer[3] = (base_addr) & 0x0ff;
		printf("%d %d %x %x %x %x ", nb_sectors, base_addr, buffer[0], buffer[1], buffer[2], buffer[3]);

		if (_spi.ft2232_spi_wr_and_rd(4, buffer, NULL) < 0) {
			cout << "Write error in erase_sector\n" << endl;
			return -1;
		}

		/* read status reg, wait for WIP goes low */
		wait_wip();
		printf("sector %d ok\n", nb_sectors);
	}
	printf("erase : end\n");
	return 0;
}
#endif

/* write must be do by 256bytes. Before writing next 256bytes we must
 * wait for WIP goes low
 */

#if 0
void EPCQ::program(unsigned int start_offset, string filename, bool reverse)
{
	FILE *fd;
	int file_size, nb_sect, i, ii;
	unsigned char buffer[256 + 4], rd_buffer[256], start_sector;
	int nb_iter, len, nb_read, offset = start_offset;
	/* 1. we need to know the size of the bistream
	 * 2. according to the same we compute number of sector needed
	 * 3. we erase sectors
	 * 4. we write new content
	 */
	fd = fopen(filename.c_str(), "r");
	if (!fd) {
		cout << "Error opening " << filename << endl;
		return;
	}
	fseek(fd, 0, SEEK_END);
	file_size = ftell(fd);
	fseek(fd, 0, SEEK_SET);

	/* compute number of sector used */
	nb_sect = file_size / SECTOR_SIZE;
	nb_sect +=  ((file_size % SECTOR_SIZE) ? 1 : 0);
	/* compute number of iterations */
	nb_iter = file_size / 256;
	nb_iter +=  ((file_size % 256) ? 1 : 0);
	len = file_size;
	/* compute start sector */
	start_sector = start_offset / SECTOR_SIZE;

	printf("erase %d sectors starting at 0x%x (sector %d)\n", nb_sect, offset, start_sector);
	erase_sector(start_sector, (char)nb_sect);

	/* now start programming */
	if (_verbose) {
		printf("program in ");
		if (reverse)
			printf("reverse mode\n");
		else
			printf("direct mode\n");
	}
	buffer[0] = WR_BYTES_REG;
	for (i= 0; i < nb_iter; i++) {
		do_write_enable();

		nb_read = fread(rd_buffer, 1, 256, fd);
		if (nb_read == 0) {
			printf("problem reading the source file\n");
			break;
		}
		buffer[1] = (offset >> 16) & 0xff;
		buffer[2] = (offset >> 8) & 0xff;
		buffer[3] = offset & 0xff;
		memcpy(&buffer[4], rd_buffer, nb_read);
		for (ii= 0; ii < nb_read; ii++)
			buffer[ii+4] = (reverse) ? convertLSB(rd_buffer[ii]):rd_buffer[ii];
		_spi.ft2232_spi_wr_and_rd(nb_read+4, buffer, NULL);
		wait_wip();
		len -= nb_read;
		offset += nb_read;
		if ((i % 10) == 0)
			printf("%s sector done len %d %d %d\n", __func__, len, i, nb_iter);
	}

	fclose(fd);
}
#endif


void EPCQ::dumpJICFile(char *jic_file, char *out_file, size_t max_len)
{
	int offset = 0xA1;
	unsigned char c;
	size_t i=0;

	FILE *jic = fopen(jic_file, "r");
	fseek(jic, offset, SEEK_SET);
	FILE *out = fopen(out_file, "w");
	for (i=0; i < max_len && (1 == fread(&c, 1, 1, jic)); i++) {
		fprintf(out, "%zx %x\n", i, c);
	}
	fclose(jic);
	fclose(out);
}

#if 0
void EPCQ::dumpflash(char *dest_file, int size)
{
	(void)size;
	(void)dest_file;
	int i;
	unsigned char tx_buf[5] = {RD_FAST_READ_REG, 0, 0, 0, 0};

	/* 1 byte cmd + 3 byte addr + 8 dummy clk cycle -> 1 byte */
	int realByteToRead = 2097380;
	realByteToRead =  0x1FFFFF;
	realByteToRead = 718569;
	unsigned char big_buf[realByteToRead];

	_spi.ft2232_spi_wr_then_rd(tx_buf, 5, big_buf, realByteToRead);

	FILE *fd = fopen("flash_dump.dd", "w");
	FILE *fd_txt = fopen("flash_dump.txt", "w");
	unsigned char c;
	for (i=0; i<realByteToRead; i++) {
		c = convertLSB(big_buf[i]);
		fwrite(&c, 1, 1, fd);
		fprintf(fd_txt, "%x %x\n", i, c);
	}
	fclose(fd);
	fclose(fd_txt);
}
#endif

void EPCQ::read_id()
{
	unsigned char tx_buf[5];
	unsigned char rx_buf[5];
	/* read EPCQ device id */
	tx_buf[0] = 0x9f;
	/* 2 dummy_byte + 1byte */
	_spi->spi_put(0x9F, NULL, rx_buf, 3);
	_device_id = rx_buf[2];
	if (_verbose)
		printf("device id 0x%x expected 0x15\n", _device_id);
	/* read EPCQ silicon id */
	//tx_buf[0] = 0xAB;
	/* 3 dummy_byte + 1 byte*/
	_spi->spi_put(0xAB, NULL, rx_buf, 4);
	_silicon_id = rx_buf[3];
	if (_verbose)
		printf("silicon id 0x%x expected 0x14\n", _silicon_id);
}

EPCQ::EPCQ(SPIInterface *spi, int8_t verbose):SPIFlash(spi, verbose)
{}

EPCQ::~EPCQ()
{}
