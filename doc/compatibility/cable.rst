.. _compatibility:cables:

Cables
######

* anlogic JTAG adapter
* `digilent_hs2 <https://store.digilentinc.com/jtag-hs2-programming-cable/>`__: jtag programmer cable from digilent
* `cmsisdap <https://os.mbed.com/docs/mbed-os/v6.11/debug-test/daplink.html>`__: ARM CMSIS DAP protocol interface (hid only)
* `Orbtrace <https://github.com/orbcode/orbtrace>`__: Open source FPGA-based debug and trace interface
* `DFU (Device Firmware Upgrade) <http://www.usb.org/developers/docs/devclass_docs/DFU_1.1.pdf>`__: USB device compatible with DFU protocol
* `DirtyJTAG <https://github.com/jeanthom/DirtyJTAG>`__: JTAG probe firmware for STM32F1
  (Best to use release (1.4 or newer) or limit the --freq to 600000 with older releases.
  New version `dirtyjtag2 <https://github.com/jeanthom/DirtyJTAG/tree/dirtyjtag2>`__ is also supported)
* Intel USB Blaster I & II : jtag programmer cable from intel/altera
* JTAG-HS3: jtag programmer cable from digilent
* FT2232: generic programmer cable based on Ftdi FT2232
* FT232RL and FT231X: generic USB<->UART converters in bitbang mode
* `Tang Nano USB-JTAG interface <https://github.com/diodep/ch55x_jtag>`__: FT2232C clone based on CH552 microcontroler
  (with some limitations and workaround)
* `Tang Nano 4k USB-JTAG interface <https://github.com/sipeed/RV-Debugger-BL702>`__: USB-JTAG/UART debugger based on BL702 microcontroler.
* `Tigard <https://www.crowdsupply.com/securinghw/tigard>`__: SWD/JTAG/UART/SPI programmer based on Ftdi FT2232HQ
* `honeycomb USB-JTAG interface <https://github.com/Disasm/f042-ftdi>`__: FT2232C clone based on STM32F042 microcontroler
* `Cologne Chip GateMate FPGA Programmer <https://colognechip.com/programmable-logic/gatemate/>`__: FT232H-based JTAG/SPI programmer cable
