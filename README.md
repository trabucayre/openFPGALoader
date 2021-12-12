# openFPGALoader

<p align="right">
  <a title="Documentation" href="https://trabucayre.github.io/openFPGALoader"><img src="https://img.shields.io/website.svg?label=trabucayre.github.io%2FopenFPGALoader&longCache=true&style=flat-square&url=http%3A%2F%2Ftrabucayre.github.io%2FopenFPGALoader%2Findex.html&logo=GitHub"></a><!--
  -->
  <a title="'Test' workflow Status" href="https://github.com/trabucayre/openFPGALoader/actions?query=workflow%3ATest"><img alt="'Test' workflow Status" src="https://img.shields.io/github/workflow/status/trabucayre/openFPGALoader/Test?longCache=true&style=flat-square&label=Test&logo=github%20actions&logoColor=fff"></a><!--
  -->
  <a title="Releases" href="https://github.com/trabucayre/openFPGALoader/releases"><img src="https://img.shields.io/github/commits-since/trabucayre/openFPGALoader/latest.svg?longCache=true&style=flat-square&logo=git&logoColor=fff"></a>
</p>

<p align="center">
  <strong><a href="https://trabucayre.github.io/openFPGALoader/guide/first-steps.html">First steps</a> • <a href="https://trabucayre.github.io/openFPGALoader/guide/install.html">Install</a> • <a href="https://trabucayre.github.io/openFPGALoader/guide/troubleshooting.html">Troubleshooting</a></strong> • <a href="https://trabucayre.github.io/openFPGALoader/guide/advanced.html">Advanced usage</a>
</p>

Universal utility for programming FPGAs. Compatible with many boards, cables and FPGA from major manufacturers (Xilinx, Altera/Intel, Lattice, Gowin, Efinix, Anlogic, Cologne Chip). openFPGALoader works on Linux, Windows and macOS.

Not sure if your hardware is supported? Check the hardware compatibility lists:

 * [FPGA compatibility list](https://trabucayre.github.io/openFPGALoader/compatibility/fpga.html)
 * [Board compatibility list](https://trabucayre.github.io/openFPGALoader/compatibility/board.html)
 * [Cable compatibility list](https://trabucayre.github.io/openFPGALoader/compatibility/cable.html)

Also checkout the vendor-specific documentation:
[Anlogic](https://trabucayre.github.io/openFPGALoader/vendors/anlogic.html),
[Cologne Chip](https://trabucayre.github.io/openFPGALoader/vendors/colognechip.html),
[Efinix](https://trabucayre.github.io/openFPGALoader/vendors/efinix.html),
[Gowin](https://trabucayre.github.io/openFPGALoader/vendors/gowin.html),
[Intel/Altera](https://trabucayre.github.io/openFPGALoader/vendors/intel.html),
[Lattice](https://trabucayre.github.io/openFPGALoader/vendors/lattice.html),
[Xilinx](https://trabucayre.github.io/openFPGALoader/vendors/xilinx.html).

## Quick Usage

`arty` in the example below is one of the many FPGA board configurations listed [here](https://trabucayre.github.io/openFPGALoader/compatibility/board.html).

```bash
openFPGALoader -b arty arty_bitstream.bit # Loading in SRAM
openFPGALoader -b arty -f arty_bitstream.bit # Writing in flash
```

You can also specify a JTAG cable model (complete list [here](https://trabucayre.github.io/openFPGALoader/compatibility/cable.html)) instead of the board model:

```bash
openFPGALoader -c cmsisdap fpga_bitstream.bit
```

## Usage

```
openFPGALoader --help
Usage: openFPGALoader [OPTION...] BIT_FILE
openFPGALoader -- a program to flash FPGA

      --altsetting arg      DFU interface altsetting (only for DFU mode)
      --bitstream arg       bitstream
  -b, --board arg           board name, may be used instead of cable
  -c, --cable arg           jtag interface
      --vid arg             probe Vendor ID
      --pid arg             probe Product ID
      --ftdi-serial arg     FTDI chip serial number
      --ftdi-channel arg    FTDI chip channel number (channels 0-3 map to
                            A-D)
  -d, --device arg          device to use (/dev/ttyUSBx)
      --detect              detect FPGA
      --dfu                 DFU mode
      --dump-flash          Dump flash mode
      --external-flash      select ext flash for device with internal and
                            external storage
      --file-size arg       provides size in Byte to dump, must be used with
                            dump-flash
      --file-type arg       provides file type instead of let's deduced by
                            using extension
      --fpga-part arg       fpga model flavor + package
      --freq arg            jtag frequency (Hz)
  -f, --write-flash         write bitstream in flash (default: false)
      --index-chain arg     device index in JTAG-chain
      --list-boards         list all supported boards
      --list-cables         list all supported cables
      --list-fpga           list all supported FPGA
  -m, --write-sram          write bitstream in SRAM (default: true)
  -o, --offset arg          start offset in EEPROM
      --pins arg            pin config (only for ft232R) TDI:TDO:TCK:TMS
      --probe-firmware arg  firmware for JTAG probe (usbBlasterII)
      --quiet               Produce quiet output (no progress bar)
  -r, --reset               reset FPGA after operations
      --spi                 SPI mode (only for FTDI in serial mode)
  -v, --verbose             Produce verbose output
      --verbose-level arg   verbose level -1: quiet, 0: normal, 1:verbose,
                            2:debug
  -h, --help                Give this help list
      --verify              Verify write operation (SPI Flash only)
  -V, --Version             Print program version

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.

Report bugs to <gwenhael.goavec-merou@trabucayre.com>.
```
