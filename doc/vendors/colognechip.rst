.. _colognechip:

Cologne Chip notes
##################

Supported Boards/Cables
=======================

* GateMate Evaluation Board using board parameters `-b gatemate_evb_jtag` or `-b gatemate_evb_spi`
* GateMate Programmer using cable parameter `-c gatemate_pgm`

Programming Modes
=================

Supported configuration files are bitfiles `*.bit` and it's ASCII equivalents `*.cfg`.

JTAG Configuration
------------------

Performs an active hardware reset and writes the configuration into the FPGA latches via JTAG. The configuration mode pins CFG_MD[3:0] must be set to 0xF0 (JTAG).

* Program using Evaluation Board:

    openFPGALoader -b gatemate_evb_jtag <bitfile>.cfg.bit

* Program using Programmer Cable:

    openFPGALoader -c gatemate_pgm <bitfile>.cfg.bit

SPI Configuration
-----------------

Performs an active hardware reset and writes the configuration into the FPGA latches via SPI. The configuration mode pins CFG_MD[3:0] must be set to 0x40 (SPI passive).

* Program using Evaluation Board:

    openFPGALoader -b gatemate_evb_spi <bitfile>.cfg.bit

* Program using Programmer Cable:

    openFPGALoader -b gatemate_pgm_spi <bitfile>.cfg.bit

JTAG Flash Access
-----------------

It is possible to access external flashes via the internal JTAG-SPI-bypass. The configuration mode pins CFG_MD[3:0] must be set to 0xF0 (JTAG). Note that the FPGA will not start automatically.

* Write to flash using Evaluation Board:

    openFPGALoader -b gatemate_evb_jtag <bitfile>.cfg.bit

* Write to flash using Programmer Cable:

    openFPGALoader -c gatemate_pgm -f <bitfile>.cfg.bit

The `offset` parameter can be used to store data at any point in the flash, e.g.:

    openFPGALoader -b gatemate_evb_jtag -o <offset> <bitfile>.cfg.bit

SPI Flash Access
----------------

If the programming device and FPGA share the same SPI signals, it is possible to hold the FPGA in reset and write data to the flash. The configuration mode can be set as desired. If the FPGA should start from the external memory after reset, the configuration mode pins CFG_MD[3:0] set to 0x00 (SPI active).

* Write to flash using Evaluation Board:

    openFPGALoader -b gatemate_evb_spi -f <bitfile>.cfg.bit

* Write to flash using Programmer Cable:

    openFPGALoader -b gatemate_pgm_spi -f <bitfile>.cfg.bit

The `offset` parameter can be used to store data at any point in the flash, e.g.:

    openFPGALoader -b gatemate_evb_spi -o <offset> <bitfile>.cfg.bit