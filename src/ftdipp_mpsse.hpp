#ifndef _FTDIPP_MPSSE_H
#define _FTDIPP_MPSSE_H
#include <ftdi.h>
#include <string>

class FTDIpp_MPSSE {
	public:
		typedef struct {
			int vid;
			int pid;
			int interface;
			int bit_low_val;
			int bit_low_dir;
			int bit_high_val;
			int bit_high_dir;
		} mpsse_bit_config;

		FTDIpp_MPSSE(const mpsse_bit_config &cable, const std::string &dev,
			const std::string &serial, uint32_t clkHZ, bool verbose = false);
		~FTDIpp_MPSSE();

		int init(unsigned char latency, unsigned char bitmask_mode, unsigned char mode,
				mpsse_bit_config &bit_conf);
		int setClkFreq(uint32_t clkHZ);
		int setClkFreq(uint32_t clkHZ, char use_divide_by_5);

		int vid() {return _vid;}
		int pid() {return _pid;}

	protected:
		void open_device(const std::string &serial, unsigned int baudrate);
		void ftdi_usb_close_internal();
		int close_device();
		int mpsse_write();
		int mpsse_read(unsigned char *rx_buff, int len);
		int mpsse_store(unsigned char c);
		int mpsse_store(unsigned char *c, int len);
		int mpsse_get_buffer_size() {return _buffer_size;}
		unsigned int udevstufftoint(const char *udevstring, int base);
		bool search_with_dev(const std::string &device);
		bool _verbose;
	private:
		int _vid;
		int _pid;
		int _bus;
		int _addr;
		char _product[64];
		unsigned char _interface;
	protected:
		int _clkHZ;
		struct ftdi_context *_ftdi;
		int _buffer_size;
		int _num;
	private:
		unsigned char *_buffer;
};

#endif
