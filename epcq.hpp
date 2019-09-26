#include <iostream>
#include <vector>
#include <ftdispi.hpp>
using namespace std;

class EPCQ {
 public:
 	EPCQ(int vid, int pid, unsigned char interface, uint32_t clkHZ);
	~EPCQ();

	short detect();

	void program(unsigned int start_offet, string filename, bool reverse=true);
	int erase_sector(char start_sector, char nb_sectors);
	void dumpflash(char *dest_file, int size);

	private:
		unsigned char convertLSB(unsigned char src);
		void wait_wel();
		void wait_wip();
		int do_write_enable();

		/* trash */
		void dumpJICFile(char *jic_file, char *out_file, size_t max_len);

		//struct ftdi_spi *_spi;
		FtdiSpi _spi;

		unsigned char _device_id;
		unsigned char _silicon_id;

#if 0
	uint32_t _freq_hz;
	int _enddr;
	int _endir;
	int _run_state;
	int _end_state;
	svf_XYR hdr;
	svf_XYR hir;
	svf_XYR sdr;
	svf_XYR sir;
	svf_XYR tdr;
	svf_XYR tir;
#endif
};
