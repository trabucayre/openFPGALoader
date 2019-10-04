# cycloader
Utility for programming Intel/Altera Cyclone and Xilinx Serie 7

Current support:
* Trenz cyc1000 Cyclone 10 LP 10CL025 (memory and spi flash)
* Digilent arty 35T (memory)

## compile and install

This application uses libftdi1, so this library must be installed (and,
depending of the distribution, headers too)
```bash
apt-get install libftdi1-2 libftdi1-dev libftdipp1-3 libftdipp1-dev
```

To build the app:
```bash
$ make
```
To install
```bash
$ sudo make install
```
Currently, the install path is hardcoded to /usr/local

## Usage

```bash
cycloader --help
Usage: cycloader [OPTION...] BIT_FILE
cycloader -- a program to flash cyclone10 LP FPGA

  -b, --board=BOARD          board name, may be used instead of cable
  -c, --cable=CABLE          jtag interface
  -d, --display              display FPGA and EEPROM model
  -o, --offset=OFFSET        start offset in EEPROM
  -r, --reset                reset FPGA after operations
  -v, --verbose              Produce verbose output
  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version

```
To have complete help

### CYC1000

#### loading in memory:

sof to svf generation:
```bash
quartus_cpf -c -q -g 3.3 -n 12.0MHz p project_name.sof project_name.svf
```
file load:
```bash
cycloader -b cyc1000 project_name.svf
```

#### SPI flash:
sof to rpd:
```bash
quartus_cpf -o auto_create_rpd=on -c -d EPCQ16A -s 10CL025YU256C8G project_name.svf project_name.jic
```
file load:
```bash
cycloader -b cyc1000 -r project_name_auto.rpd
```

**Note about SPI flash:
svf file used to write in flash is just a bridge between FT2232 interfaceB
configured in SPI mode and sfl primitive used to access EPCQ SPI flash.**
