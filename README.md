# openFPGALoader

<p align="center">
  <a title="'Test' workflow Status" href="https://github.com/trabucayre/openFPGALoader/actions?query=workflow%3ATest"><img alt="'Test' workflow Status" src="https://img.shields.io/github/workflow/status/trabucayre/openFPGALoader/Test?longCache=true&style=flat-square&label=Test&logo=github%20actions&logoColor=fff"></a><!--
  -->
</p>

Universal utility for programming FPGAs.

__Current supported kits:__

|     board name      | description | FPGA | memory | flash |
|--------------------:|:------------|------|--------|-------|
| **acornCle215**     | [Acorn CLE 215+](http://squirrelsresearch.com/acorn-cle-215/) | Artix</br>xc7a200tsbg484 | OK | OK |
| **alchitry_au**     | [Alchitry Au](https://alchitry.com/products/alchitry-au-fpga-development-board) | Artix</br>xc7a35tftg256 | OK | OK |
| **arty**            | [Digilent Arty A7](https://reference.digilentinc.com/reference/programmable-logic/arty-a7/start) | Artix</br>xc7a35ticsg324 | OK | OK |
|                     | [Digilent Arty S7](https://reference.digilentinc.com/reference/programmable-logic/arty-s7/start) | Spartan7</br>xc7s50csga324 | OK | OK |
|                     | [Digilent Analog Discovery 2](https://reference.digilentinc.com/test-and-measurement/analog-discovery-2/start) | Spartan6</br>xc6slx25 | OK | NT |
|                     | [Digilent Digital Discovery](https://reference.digilentinc.com/test-and-measurement/digital-discovery/start) | Spartan6</br>xc6slx25 | OK | NT |
| **basys3**          | [Digilent Basys3](https://reference.digilentinc.com/reference/programmable-logic/basys-3/start) | Artix</br>xc7a35tcpg236 | OK | OK |
| **colorlight**      | [Colorlight 5A-75B (version 7)](https://fr.aliexpress.com/item/32281130824.html) | ECP5</br>LFE5U-25F-6BG256C | OK | OK |
| **colorlight_i5**   | [Colorlight I5](https://www.colorlight-led.com/product/colorlight-i5-led-display-receiver-card.html) |ECP5</br>LFE5U-25F-6BG381C| OK | OK |
| **crosslinknx_evn** | [Lattice CrossLink-NX Evaluation Board](https://www.latticesemi.com/en/Products/DevelopmentBoardsAndKits/CrossLink-NXEvaluationBoard)| Nexus</br>LIFCL-40 | OK | OK |
| **cyc1000**         | [Trenz cyc1000](https://shop.trenz-electronic.de/en/TEI0003-02-CYC1000-with-Cyclone-10-FPGA-8-MByte-SDRAM) | Cyclone 10 LP</br>10CL025YU256C8G | OK | OK |
| **de0**             | [Terasic DE0](https://www.terasic.com.tw/cgi-bin/page/archive.pl?No=364) | Cyclone III</br>EP3C16F484C6 | OK | NT |
| **de0nano**         | [Terasic de0nano](https://www.terasic.com.tw/cgi-bin/page/archive.pl?No=593) | Cyclone IV E</br>EP4CE22F17C6 | OK | OK |
| **de0nanoSoc**      | [Terasic de0nanoSoc](https://www.terasic.com.tw/cgi-bin/page/archive.pl?Language=English&CategoryNo=205&No=941) | Cyclone V SoC</br>5CSEMA4U23C6| OK | |
| **de10nano**        | [Terasic de10Nano](https://www.terasic.com.tw/cgi-bin/page/archive.pl?Language=English&CategoryNo=205&No=1046) | Cyclone V SoC</br>5CSEBA6U23I7 | OK | |
| **ecp5_evn**        | [Lattice ECP5 5G Evaluation Board](https://www.latticesemi.com/en/Products/DevelopmentBoardsAndKits/ECP5EvaluationBoard) | ECP5G</br>LFE5UM5G-85F | OK | OK |
| **ecpix5**          | [LambdaConcept ECPIX-5](https://shop.lambdaconcept.com/home/46-2-ecpix-5.html#/2-ecpix_5_fpga-ecpix_5_85f) | ECP5</br>LFE5UM5G-85F | OK | OK |
| **fireant**         | [Fireant Trion T8](https://www.crowdsupply.com/jungle-elec/fireant) | Trion</br>T8F81 | NA | AS |
| **fomu**            | [Fomu PVT](https://tomu.im/fomu.html) | iCE40UltraPlus</br>UP5K | NA | OK |
| **honeycomb**       | [honeycomb](https://github.com/Disasm/honeycomb-pcb) | littleBee</br>GW1NS-2C | OK | IF |
| **ice40_generic**   | [iCEBreaker](https://1bitsquared.com/collections/fpga/products/icebreaker) | iCE40UltraPlus</br>UP5K | NA | AS |
| **ice40_generic**   | [icestick](https://www.latticesemi.com/icestick) | iCE40</br>HX1k | NA | AS |
| **ice40_generic**   | [iCE40-HX8K](https://www.latticesemi.com/Products/DevelopmentBoardsAndKits/iCE40HX8KBreakoutBoard.aspx) | iCE40</br>HX8k | NT | AS |
| **ice40_generic**   | [Olimex iCE40HX1K-EVB](https://www.olimex.com/Products/FPGA/iCE40/iCE40HX1K-EVB/open-source-hardware) | iCE40</br>HX1k | NT | AS |
| **ice40_generic**   | [Olimex iCE40HX8K-EVB](https://www.olimex.com/Products/FPGA/iCE40/iCE40HX8K-EVB/open-source-hardware) | iCE40</br>HX8k | NT | AS |
| **kc705**           | [Xilinx KC705](https://www.xilinx.com/products/boards-and-kits/ek-k7-kc705-g.html) | Kintex7</br>xc7k325t | OK | NT |
| **licheeTang**      | [Sipeed Lichee Tang](https://tang.sipeed.com/en/hardware-overview/lichee-tang/) | eagle s20</br>EG4S20BG256 | OK | OK |
| **machXO2EVN**      | [Lattice MachXO2 Breakout Board Evaluation Kit ](https://www.latticesemi.com/products/developmentboardsandkits/machxo2breakoutboard) | MachXO2</br>LCMXO2-7000HE | OK | OK |
| **machXO3EVN**      | [Lattice MachXO3D Development Board ](https://www.latticesemi.com/products/developmentboardsandkits/machxo3d_development_board) | MachXO3D</br>LCMXO3D-9400HC | OK | NT |
| **machXO3SK**       | [Lattice MachXO3LF Starter Kit](https://www.latticesemi.com/en/Products/DevelopmentBoardsAndKits/MachXO3LFStarterKit) | MachXO3</br>LCMX03LF-6900C | OK | OK |
| **nexysVideo**      | [Digilent Nexys Video](https://reference.digilentinc.com/reference/programmable-logic/nexys-video/start) | Artix</br>xc7a200tsbg484 | OK | OK |
| **orangeCrab**      | [Orange Crab](https://github.com/gregdavill/OrangeCrab) | ECP5</br>LFE5U-25F-8MG285C | OK (JTAG) | OK (DFU) |
| **pipistrello**     | [Saanlima Pipistrello LX45](http://pipistrello.saanlima.com/index.php?title=Welcome_to_Pipistrello) | Spartan6</br>xc6slx45-csg324 | OK | OK |
| **qmtechCycloneV**  | [QMTech CycloneV Core Board](https://fr.aliexpress.com/i/1000006622149.html) | Cyclone V</br>5CEFA2F23I7 | OK | OK |
| **runber**          | [SeeedStudio Gowin RUNBER](https://www.seeedstudio.com/Gowin-RUNBER-Development-Board-p-4779.html) | littleBee</br>GW1N-4 | OK | IF |
|                     | [Scarab Hardware MiniSpartan6+](https://www.kickstarter.com/projects/1812459948/minispartan6-a-powerful-fpga-board-and-easy-to-use) | Spartan6</br>xc6slx25-3-ftg256 | OK | NT |
| **spartanEdgeAccelBoard** | [SeeedStudio Spartan Edge Accelerator Board](http://wiki.seeedstudio.com/Spartan-Edge-Accelerator-Board) | Spartan7</br>xc7s15ftgb196 | OK | NA |
| **tangnano**        | [Sipeed Tang Nano](https://tangnano.sipeed.com/en/) | littleBee</br>GW1N-1 | OK | |
| **tec0117**         | [Trenz Gowin LittleBee (TEC0117)](https://shop.trenz-electronic.de/en/TEC0117-01-FPGA-Module-with-GOWIN-LittleBee-and-8-MByte-internal-SDRAM) | littleBee</br>GW1NR-9 | OK | IF |
| **xtrx**            | [FairWaves XTRXPro](https://www.crowdsupply.com/fairwaves/xtrx) | Artix</br>xc7a50tcpg236 | OK | OK |
| **xyloni_spi**      | [Efinix Xyloni](https://www.efinixinc.com/products-devkits-xyloni.html) | Trion</br>T8F81 | NA | AS |
| **zedboard**        | [Avnet ZedBoard](https://www.avnet.com/wps/portal/us/products/avnet-boards/avnet-board-families/zedboard/) | zynq7000</br>xc7z020clg484 | OK | NA |

- *IF* Internal Flash
- *AS* Active Serial flash mode
- *NA* Not Available
- *NT* Not Tested

__Supported (tested) FPGA:__

| vendor  | model | memory | flash |
|--------:|-------|--------|-------|
| Anlogic | [EG4S20](http://www.anlogic.com/prod_view.aspx?TypeId=10&Id=168&FId=t3:10:3)                                                     | OK | AS |
| Efinix  | [Trion T8](https://www.efinixinc.com/products-trion.html)                                                                        | NA | OK |
| Gowin   | [GW1N (GW1N-1, GW1N-4, GW1NR-9, GW1NS-2C)](https://www.gowinsemi.com/en/product/detail/2/)                                       | OK | IF |
| Intel   | Cyclone III [EP3C16](https://www.intel.com/content/www/us/en/programmable/products/fpga/cyclone-series/cyclone-iii/support.html) | OK | OK |
|         | Cyclone IV CE [EP4CE22](https://www.intel.com/content/www/us/en/products/programmable/fpga/cyclone-iv/features.html)             | OK | OK |
|         | Cyclone V E [5CEA2, 5CEBA4](https://www.intel.com/content/www/us/en/products/programmable/fpga/cyclone-v.html)                   | OK | OK |
|         | Cyclone 10 LP [10CL025](https://www.intel.com/content/www/us/en/products/programmable/fpga/cyclone-10.html)                      | OK | OK |
| Lattice | [CrossLink-NX (LIFCL-40)](https://www.latticesemi.com/en/Products/FPGAandCPLD/CrossLink-NX)                                      | OK | OK |
|         | [ECP5 (25F, 5G 85F](http://www.latticesemi.com/Products/FPGAandCPLD/ECP5)                                                        | OK | OK |
|         | [iCE40 (HX1K,HX8K, UP5K)](https://www.latticesemi.com/en/Products/FPGAandCPLD/iCE40)                                             | OK | NT |
|         | [MachXO2](https://www.latticesemi.com/en/Products/FPGAandCPLD/MachXO2)                                                           | OK | OK |
|         | [MachXO3D](http://www.latticesemi.com/en/Products/FPGAandCPLD/MachXO3D.aspx)                                                     | OK | OK |
|         | [MachXO3LF](http://www.latticesemi.com/en/Products/FPGAandCPLD/MachXO3.aspx)                                                     | OK | OK |
| Xilinx  | Artix 7 [xc7a35ti, xc7a50t, xc7a75t, xc7a100t, xc7a200t](https://www.xilinx.com/products/silicon-devices/fpga/artix-7.html)      | OK | OK |
|         | Kintex 7 [xc7k325t](https://www.xilinx.com/products/silicon-devices/fpga/kintex-7.html#productTable)                             | OK | NT |
|         | Spartan 6 [xc6slx9, xc6slx16, xc6slx25, xc6slx45](https://www.xilinx.com/products/silicon-devices/fpga/spartan-6.html)           | OK | OK |
|         | Spartan 7 [xc7s15, xc7s25, xc7s50](https://www.xilinx.com/products/silicon-devices/fpga/spartan-7.html)                          | OK | OK |

- *IF* Internal Flash
- *AS* Active Serial flash mode
- *NA* Not Available
- *NT* Not Tested

__Supported cables:__

* anlogic JTAG adapter
* [digilent_hs2](https://store.digilentinc.com/jtag-hs2-programming-cable/): jtag programmer cable from digilent
* [cmsisdap](https://os.mbed.com/docs/mbed-os/v6.11/debug-test/daplink.html): ARM CMSIS DAP protocol interface (hid only)
* [Orbtrace](https://github.com/orbcode/orbtrace): Open source FPGA-based debug and trace interface
* [DFU (Device Firmware Upgrade)](http://www.usb.org/developers/docs/devclass_docs/DFU_1.1.pdf): USB device compatible with DFU protocol
* [DirtyJTAG](https://github.com/jeanthom/DirtyJTAG): JTAG probe firmware for STM32F1
  (Best to use release (1.4 or newer) or limit the --freq to 600000 with older releases. New version https://github.com/jeanthom/DirtyJTAG/tree/dirtyjtag2 is also supported)
* Intel USB Blaster I & II : jtag programmer cable from intel/altera
* JTAG-HS3: jtag programmer cable from digilent
* FT2232: generic programmer cable based on Ftdi FT2232
* FT232RL and FT231X: generic USB<->UART converters in bitbang mode
* [Tang Nano USB-JTAG interface](https://github.com/diodep/ch55x_jtag): FT2232C clone based on CH552 microcontroler
  (with some limitations and workaround)
* [Tigard](https://www.crowdsupply.com/securinghw/tigard): SWD/JTAG/UART/SPI programmer based on Ftdi FT2232HQ
* [honeycomb USB-JTAG interface](https://github.com/Disasm/f042-ftdi): FT2232C clone based on STM32F042 microcontroler

# Contents

- [Compile and install](#compile-and-install)
- [Access Right](#access-right)
- [Usage](#usage)
  - [Generic usage](#generic-usage)
  - [display FPGA](#display-fpga)
  - [Reset device](#reset-device)
  - [Load bistream](#load-bitstream-device)
  - [Bypass file type detection](#automatic-file-type-detection-bypass)
  - [Bitbang mode and pins configuration](#bitbang-mode-and-pins-configuration)
- [Altera](#altera)
- [Xilinx](#xilinx)
- [Lattice machXO](#lattice-machxo)
- [Lattice ECP5 and Nexus](#lattice-ecp5-nexus)
- [Gowin](#gowin)
- [Anlogic](#anlogic)
- [Efinix](#efinix)
- [ice40](#ice40)

## compile and install

This application uses **libftdi1**, so this library must be installed (and,
depending of the distribution, headers too)
```bash
apt-get install libftdi1-2 libftdi1-dev libhidapi-libusb0 libhidapi-dev libudev-dev cmake pkg-config make g++
```
**libudev-dev** is optional, may be replaced by **eudev-dev** or just not installed.

By default, **(e)udev** support is enabled (used to open a device by his */dev/xx*
node). If you don't want this option, use:

```bash
-DENABLE_UDEV=OFF
```

By default, **cmsisdap** support is enabled (used for colorlight I5).
If you don't want this option, use:

```bash
-DENABLE_CMSISDAP=OFF
```

Alternatively you can manually specify the location of **libusb** and **libftdi1**:

```bash
-DUSE_PKGCONFIG=OFF -DLIBUSB_LIBRARIES=<path_to_libusb> -DLIBFTDI_LIBRARIES=<path_to_libftdi> -DLIBFTDI_VERSION=<version> -DCMAKE_CXX_FLAGS="-I<libusb_include_dir> -I<libftdi1_include_dir>"
```

You may also need to add this if you see link errors between **libusb** and **pthread**:

```bash
-DLINK_CMAKE_THREADS=ON
```

To build the app:
```bash
$ mkdir build
$ cd build
$ cmake ../ # add -DBUILD_STATIC=ON to build a static version
            # add -DENABLE_UDEV=OFF to disable udev support and -d /dev/xxx
            # add -DENABLE_CMSISDAP=OFF to disable CMSIS DAP support
$ cmake --build .
or
$ make -j$(nproc)
```
To install
```bash
$ sudo make install
```
The default install path is `/usr/local`, to change it, use
`-DCMAKE_INSTALL_PREFIX=myInstallDir` in cmake invokation.

## access right

By default, users have no access to converters. A rule file
(*99-openfpgaloader.rules*) for *udev* is provided at the root directory
of this repository. These rules set access right and group (*plugdev*)
when a converter is plugged.

```bash
$ sudo cp 99-openfpgaloader.rules /etc/udev/rules.d/
$ sudo udevadm control --reload-rules && sudo udevadm trigger # force udev to take new rule
$ sudo usermod -a YourUserName -G plugdev # add user to plugdev group
```
After that you need to unplug and replug your device.

## Usage

```bash
openFPGALoader --help
Usage: openFPGALoader [OPTION...] BIT_FILE
openFPGALoader -- a program to flash FPGA

      --bitstream arg       bitstream
  -b, --board arg           board name, may be used instead of cable
  -c, --cable arg           jtag interface
      --ftdi-serial arg     FTDI chip serial number
      --ftdi-channel arg    FTDI chip channel number (channels 0-3 map to
                            A-D)
  -d, --device arg          device to use (/dev/ttyUSBx)
      --detect              detect FPGA
      --dfu                 DFU mode
      --dump-flash          Dump flash mode
      --file-size arg       provides size in Byte to dump, must be used with
                            dump-flash
      --file-type arg       provides file type instead of let's deduced by
                            using extension
      --fpga-part arg       fpga model flavor + package
      --freq arg            jtag frequency (Hz)
  -f, --write-flash         write bitstream in flash (default: false, only
                            for Gowin and ECP5 devices)
      --index-chain arg     device index in JTAG-chain
      --list-boards         list all supported boards
      --list-cables         list all supported cables
      --list-fpga           list all supported FPGA
  -m, --write-sram          write bitstream in SRAM (default: true, only for
                            Gowin and ECP5 devices)
  -o, --offset arg          start offset in EEPROM
      --pins arg            pin config (only for ft232R) TDI:TDO:TCK:TMS
      --probe-firmware arg  firmware for JTAG probe (usbBlasterII)
      --quiet               Produce quiet output (no progress bar)
  -r, --reset               reset FPGA after operations
      --spi                 SPI mode (only for FTDI in serial mode)
  -v, --verbose             Produce verbose output
  -h, --help                Give this help list
      --verify              Verify write operation (SPI Flash only)
  -V, --Version             Print program version

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.

Report bugs to <gwenhael.goavec-merou@trabucayre.com>.
```
To have complete help

### Generic usage

- when a bitstream file is compatible with both memory load and FLASH write, the default behavior is to load bitstream in memory
- with FPGA using an external SPI flash (*xilinx*, *lattice ECP5/nexus/ice40*, *anlogic*, *efinix*) option  **-o** allows one to write raw binary file to an arbitrary adress in FLASH.

#### display FPGA

With board name:
```bash
openFPGALoader -b theBoard
```
(see `openFPGALoader --list-boards`)

With cable:
```bash
openFPGALoader -c theCable
```
(see `openFPGALoader --list-cables`)

With device node:
```bash
openFPGALoader -d /dev/ttyUSBX
```

**Note:** for some cable (like *digilent* adapters) signals from the converter
are not just directly to the FPGA. For this case, the *-c* must be added.

**Note:** when -d is not provided, *openFPGALoader* will opens the first *ftdi*
found, if more than one converter is connected to the computer,
the *-d* option is the better solution

#### Reset device

```bash
openFPGALoader [options] -r
```

#### load bitstream device (memory or flash)

```bash
openFPGALoader [options] /path/to/bitstream.ext
```

##### Using pipe

```bash
cat /path/to/bitstream.ext | openFPGALoader --file-type ext [options]
```

`--file-type` is required to detect file type

Note: It's possible to load a bitstream through network:

```bash
# FPGA side
nc -lp port | openFPGALoader --filetype xxx [option
# Bitstream side
nc -q 0 host port < /path/to/bitstream.ext
```

#### Automatic file type detection bypass

Default behavior is to use file extension to determine file parser. To avoid
this mecanism `--file-type type` must be used.

#### bitbang mode and pins configuration

*FT232R* and *ft231X* may be used as JTAG programmer. JTAG communications are
emulated in bitbang mode.

To use these devices user needs to provides both the cable and the pin mapping:
```bash
openFPGALoader [options] -cft23XXX --pins=TDI:TDO:TCK:TMS /path/to/bitstream.ext
```
where:
* ft23XXX may be **ft232RL** or **ft231X**
* TDI:TDO:TCK:TMS may be the pin ID (0 <= id <= 7) or string value

allowed values are:

| value | ID |
|-------|----|
|  TXD  | 0  |
|  RXD  | 1  |
|  RTS  | 2  |
|  CTS  | 3  |
|  DTR  | 4  |
|  DSR  | 5  |
|  DCD  | 6  |
|  RI   | 7  |


<a id='altera'></a>
### <span style="text-decoration:underline">Intel/Altera: CYC1000, DE0, de0nano</span>

#### loading in memory:

`SVF` and `RBF` files are supported:

sof to svf generation:
```bash
quartus_cpf -c -q -g 3.3 -n 12.0MHz p project_name.sof project_name.svf
```
sof to rbf generation:
```bash
quartus_cpf  --option=bitstream_compression=off -c project_name.sof project_name.rbf
```

<span style="color:red">**Warning: as mentionned in `cyclone` handbooks, real-time decompression is not
supported by FPGA in JTAG mode. Keep in mind to disable this option.**</span>

file load:
```bash
openFPGALoader -b boardname project_name.svf
# or
openFPGALoader -b boardname project_name.rbf
```
with `boardname` = `de0`, `cyc1000`, `de0nano`, `de0nanoSoc` or `qmtechCycloneV`

#### SPI flash:

`RPD` and `RBF` are supported

sof to rpd:
```bash
quartus_cpf -o auto_create_rpd=on -c -d EPCQ16A -s 10CL025YU256C8G project_name.svf project_name.jic
```
file load:
```bash
openFPGALoader -b cyc1000 -r project_name_auto.rpd
# or
openFPGALoader -b cyc1000 -r project_name.rbf
```

<a id='xilinx'></a>
### <span style="text-decoration:underline">Xilinx based boards</span>

To simplify further explanations, we consider the project is generated in the
current directory.

**Note:**
1. Spartan Edge Accelerator Board has only pinheader, so the cable must be provided
2. a *JTAG* <-> *SPI* bridge (used to write bitstream in FLASH) is available for some device, see
[spiOverJtag](https://github.com/trabucayre/openFPGALoader/tree/master/spiOverJtag) to check if your model is supported
3. board provides the device/package model, but if the targeted board is not
   officially supported but the FPGA yes, you can use --fpga-part to provides
   model

<span style="color:red">**Warning** *.bin* may be loaded in memory or in flash, but this extension is a classic extension
for CPU firmware and, by default, *openFPGALoader* load file in memory, double check
*-m* / *-f* when you want to use a firmware for a softcore
(or anything, other than a bitstream) to write somewhere in the FLASH device).</span>

*.bit* file is the default format generated by *vivado*, so nothing special
task must be done to generates this bitstream.

*.bin* is not, by default, produces. To have access to this file you need to configure the tool:
- **GUI**: *Tools* -> *Settings* -> *Bitstreams* -> check *-bin_file*
- **TCL**: append your *TCL* file with `set_property STEPS.WRITE_BITSTREAM.ARGS.BIN_FILE true [get_runs impl_1]`

<span style="color:red">**Warning: for alchitry board the bitstream must be configured with a buswidth of 1 or 2. Quad mode can't be used with alchitry's FLASH**</span>

#### loading in memory:

<span style="text-decoration:underline">*.bit* and *.bin* are allowed to be loaded in memory.</span>

__file load:__
```bash
openFPGALoader [-m] -b arty *.runs/impl_1/*.bit (or *.bin)
```
or
```bash
openFPGALoader [-m] -b spartanEdgeAccelBoard -c digilent_hs2 *.runs/impl_1/*.bit (or *.bin)
```

#### SPI flash:

<span style="text-decoration:underline">*.bit*, *.bin*, and *.mcs* are supported for FLASH.</span>

.mcs must be generates through vivado with a tcl script like
```tcl
set project [lindex $argv 0]

set bitfile "${project}.runs/impl_1/${project}.bit"
set mcsfile "${project}.runs/impl_1/${project}.mcs"

write_cfgmem -format mcs -interface spix4 -size 16 \
    -loadbit "up 0x0 $bitfile" -loaddata "" \
    -file $mcsfile -force

```
**Note:
*-interface spix4* and *-size 16* depends on SPI flash capability and size.**

The tcl script is used with:
```bash
vivado -nolog -nojournal -mode batch -source script.tcl -tclargs myproject
```

__file load:__
```bash
openFPGALoader [--fpga-part xxxx] -f -b arty *.runs/impl_1/*.mcs (or .bit / .bin)
```
**Note: *-f* is required to write bitstream (without them *.bit* and *.bin* are loaded in memory)**

Note: "--fpga-part" is only required if this information is not provided at
board.hpp level or if the board is not officially supported. device/packagee
format is something like xc7a35tcsg324 (arty model). See src/board.hpp, or
spiOverJtag directory for examples.

<a id='lattice-machxo'></a>
### MachXO2/MachXO3 Starter Kit

#### Flash memory:

*.jed* file is the default format generated by *Lattice Diamond*, so nothing
special must be done to generates this file.

__file load__:
```bash
openFPGALoader [-b yourboard] impl1/*.jed
```
where *yourboard* may be:
* *machX02EVN*
* *machXO3SK*

#### SRAM:

To generates *.bit* file **Bitstream file** must be checked under **Exports Files** in *Lattice Diamond* left panel.

__file load__:
```bash
openFPGALoader [-b yourboard] impl1/*.bit
```
where *yourboard* may be:
* *machX02EVN*
* *machXO3SK*

<a id='lattice-ecp5-nexus'></a>
### Lattice ECP5 (Colorlight 5A-75b, Lattice ECP5 5G Evaluation board, ULX3S) CrossLink-NX

#### SRAM:

```bash
openFPGALoader [-b yourBoard] [-c yourCable] -m project_name/*.bit
```

**By default, openFPGALoader load bitstream in memory, so the '-m' argument is optional**

#### SPI Flash:

##### bit

```bash
openFPGALoader [-b yourBoard] [-c yourCable] -f project_name/*.bit # or *.bin
```

##### mcs

To generates *.mcs* file **PROM File** must be checked under **Exports Files** in *Lattice Diamond* left panel.

```bash
openFPGALoader [-b yourBoard] [-c yourCable] project_name/*.mcs
```

<a id='gowin'></a>
### GOWIN GW1N (Trenz TEC0117, Sipeed Tang Nano, Honeycomb and RUNBER)

*.fs* file is the default format generated by *Gowin IDE*, so nothing
special must be done to generates this file.

Since the same file is used for SRAM and Flash a CLI argument is used to
specify the destination.

#### Flash SRAM:

with **-m**

```bash
openFPGALoader -m -b BOARD_NAME impl/pnr/*.fs
```
where *BOARD_NAME* is:
- *tec0117*
- *tangnano*
- *runber*

#### Flash (only with Trenz TEC0117 and runber):

with **-f**

__file load__:
```bash
openFPGALoader -f -b BOARD_NAME impl/pnr/*.fs
```
where *BOARD_NAME* is:
- **tec0117**
- **runber**

<a id='anlogic'></a>
### Sipeed Lichee Tang

For this target, *openFPGALoader* support *svf* and *bit*

__bit file load (memory)__

```bash
openFPGALoader -m -b licheeTang /somewhere/project/prj/*.bit
```

Since *-m* is the default, this argument is optional

__bit file load (spi flash)__

```bash
openFPGALoader -f -b licheeTang /somewhere/project/prj/*.bit
```

__svf file load__

It's possible to produce this file by using *TD*:
* Tools->Device Chain
* Add your bit file
* Option : Create svf

or by using [prjtang project](https://github.com/mmicko/prjtang)

```bash
mkdir build
cd build
cmake ../
make
```

Now a file called *tangbit* is present in current directory and has to be used as
follow:
```bash
tangbit --input /somewhere.bit --svf bitstream.svf
```

```bash
openFPGALoader -b licheeTang /somewhere/*.svf
```

<a id='efinix'></a>
### Firant and Xyloni boards (efinix trion T8)

*.hex* file is the default format generated by *Efinity IDE*, so nothing
special must be done to generates this file.

*openFPGALoader* support only active mode (SPI) (*JTAG* is WIP).

__hex file load__

```bash
openFPGALoader -b fireant /somewhere/project/outflow/*.hex
```
or, for xyloni board
```bash
openFPGALoader -b xyloni_spi /somewhere/project/outflow/*.hex
```

Since openFPGALoader access the flash directly in SPI mode the *-b fireant*, *-b xyloni_spi* is required (no autodetection possible)

<a id='ice40'></a>
### ice40 boards (icestick, iCE40-HX8K, iCEBreaker, iCE40HX1K-EVB, iCE40HX8K-EVB)

*.bin* is the default format generated by *nextpnr*, so nothing special
must be done.

Since most ice40 boards uses the same pinout between *FTDI* and *SPI flash* a generic *ice40_generic* board is provided.

For the specific case of the *iCE40HXXK-EVB* where no onboard programmer is present, please use this:

|     FTDI     | iCE40HXXK-EVB        |
|--------------|----------------------|
| SCK (ADBUS0) | Pin 9                |
| SI  (ADBUS1) | Pin 8                |
| SO  (ADBUS2) | Pin 7                |
| CS  (ABDUS4) | Pin 10               |
| RST (ADBUS6  | Pin 6                |
| DONE (ADBUS7)| Pin 5                |

__bin file load__
```bash
openFPGALoader -b ice40_generic /somewhere/*.bin
```

Since it's a direct access to the flash (SPI) the *-b* option is required.
