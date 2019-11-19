#ifndef _FTDIPP_MPSSE_H
#define _FTDIPP_MPSSE_H
#include <ftdi.h>
#include <string>

class FTDIpp_MPSSE {
	public:
		FTDIpp_MPSSE(const std::string &dev, unsigned char interface, uint32_t clkHZ);
		FTDIpp_MPSSE(int vid, int pid, unsigned char interface, uint32_t clkHZ);
		~FTDIpp_MPSSE();

		typedef struct {
			int vid;
			int pid;
			int bit_low_val;
			int bit_low_dir;
			int bit_high_val;
			int bit_high_dir;
		} mpsse_bit_config;

		int init(unsigned char latency, unsigned char bitmask_mode, mpsse_bit_config &bit_conf);
		int setClkFreq(uint32_t clkHZ);
		int setClkFreq(uint32_t clkHZ, char use_divide_by_5);

		int vid() {return _vid;}
		int pid() {return _pid;}

	protected:
		void open_device(unsigned int baudrate);
		void ftdi_usb_close_internal();
		int close_device();
		int mpsse_write();
		int mpsse_read(unsigned char *rx_buff, int len);
		int mpsse_store(unsigned char c);
		int mpsse_store(unsigned char *c, int len);
		int mpsse_get_buffer_size() {return _buffer_size;}
		unsigned int udevstufftoint(const char *udevstring, int base);
		bool search_with_dev(const std::string &device);

	private:
		int _vid;
		int _pid;
		int _bus;
		int _addr;
		char _product[64];
		unsigned char _interface;
		int _clkHZ;
		int _buffer_size;
		int _num;
		bool _verbose;
		unsigned char *_buffer;
		struct ftdi_context *_ftdi;
};

#endif
