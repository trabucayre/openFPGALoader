.. _compatibility:boards:

Boards
######

.. NOTE::
  `arty` can be any of the board names from the first column.

.. code-block:: bash

    openFPGALoader -b arty bitstream.bit # Loading in SRAM (volatile)
    openFPGALoader -b arty -f bitstream.bit # Writing in flash (non-volatile)

======================= ================================================================================================================================================= ============================= ========= ========
             Board name Description                                                                                                                                       FPGA                          Memory    Flash
======================= ================================================================================================================================================= ============================= ========= ========
            acornCle215 `Acorn CLE 215+ <http://squirrelsresearch.com/acorn-cle-215/>`__                                                                                  Artix xc7a200tsbg484          OK        OK
            alchitry_au `Alchitry Au <https://alchitry.com/products/alchitry-au-fpga-development-board>`__                                                                Artix xc7a35tftg256           OK        OK
                   arty `Digilent Arty A7 <https://reference.digilentinc.com/reference/programmable-logic/arty-a7/start>`__                                               Artix xc7a35ticsg324          OK        OK
                   arty `Digilent Arty S7 <https://reference.digilentinc.com/reference/programmable-logic/arty-s7/start>`__                                               Spartan7 xc7s50csga324        OK        OK
                   arty `Digilent Analog Discovery 2 <https://reference.digilentinc.com/test-and-measurement/analog-discovery-2/start>`__                                 Spartan6 xc6slx25             OK        NT
                   arty `Digilent Digital Discovery <https://reference.digilentinc.com/test-and-measurement/digital-discovery/start>`__                                   Spartan6 xc6slx25             OK        NT
                 basys3 `Digilent Basys3 <https://reference.digilentinc.com/reference/programmable-logic/basys-3/start>`__                                                Artix xc7a35tcpg236           OK        OK
      gatemate_evb_jtag `Cologne Chip GateMate FPGA Evaluation Board (JTAG mode) <https://colognechip.com/programmable-logic/gatemate/>`__                                 Cologne Chip GateMate Series  OK        OK
       gatemate_evb_spi `Cologne Chip GateMate FPGA Evaluation Board (SPI mode) <https://colognechip.com/programmable-logic/gatemate/>`__                                  Cologne Chip GateMate Series  OK        OK
       gatemate_pgm_spi `Cologne Chip GateMate FPGA Programmer (SPI mode) <https://colognechip.com/programmable-logic/gatemate/>`__                                       Cologne Chip GateMate Series  OK        OK
             colorlight `Colorlight 5A-75B (version 7) <https://fr.aliexpress.com/item/32281130824.html>`__                                                               ECP5 LFE5U-25F-6BG256C        OK        OK
          colorlight_i5 `Colorlight I5 <https://www.colorlight-led.com/product/colorlight-i5-led-display-receiver-card.html>`__                                           ECP5 LFE5U-25F-6BG381C        OK        OK
        crosslinknx_evn `Lattice CrossLink-NX Evaluation Board <https://www.latticesemi.com/en/Products/DevelopmentBoardsAndKits/CrossLink-NXEvaluationBoard>`__          Nexus LIFCL-40                OK        OK
                cyc1000 `Trenz cyc1000 <https://shop.trenz-electronic.de/en/TEI0003-02-CYC1000-with-Cyclone-10-FPGA-8-MByte-SDRAM>`__                                     Cyclone 10 LP 10CL025YU256C8G OK        OK
                    de0 `Terasic DE0 <https://www.terasic.com.tw/cgi-bin/page/archive.pl?No=364>`__                                                                       Cyclone III EP3C16F484C6      OK        NT
                de0nano `Terasic de0nano <https://www.terasic.com.tw/cgi-bin/page/archive.pl?No=593>`__                                                                   Cyclone IV E EP4CE22F17C6     OK        OK
             de0nanoSoc `Terasic de0nanoSoc <https://www.terasic.com.tw/cgi-bin/page/archive.pl?Language=English&CategoryNo=205&No=941>`__                                Cyclone V SoC 5CSEMA4U23C6    OK
               de10nano `Terasic de10Nano <https://www.terasic.com.tw/cgi-bin/page/archive.pl?Language=English&CategoryNo=205&No=1046>`__                                 Cyclone V SoC 5CSEBA6U23I7    OK
               ecp5_evn `Lattice ECP5 5G Evaluation Board <https://www.latticesemi.com/en/Products/DevelopmentBoardsAndKits/ECP5EvaluationBoard>`__                       ECP5G LFE5UM5G-85F            OK        OK
                 ecpix5 `LambdaConcept ECPIX-5 <https://shop.lambdaconcept.com/home/46-2-ecpix-5.html#/2-ecpix_5_fpga-ecpix_5_85f>`__                                     ECP5 LFE5UM5G-85F             OK        OK
                fireant `Fireant Trion T8 <https://www.crowdsupply.com/jungle-elec/fireant>`__                                                                            Trion T8F81                   NA        AS
                   fomu `Fomu PVT <https://tomu.im/fomu.html>`__                                                                                                          iCE40UltraPlus UP5K           NA        OK
              honeycomb `honeycomb <https://github.com/Disasm/honeycomb-pcb>`__                                                                                           littleBee GW1NS-2C            OK        IF
          ice40_generic `iCEBreaker <https://1bitsquared.com/collections/fpga/products/icebreaker>`__                                                                     iCE40UltraPlus UP5K           NA        AS
       icebreaker-bitsy `iCEBreaker-bitsy <https://1bitsquared.com/collections/fpga/products/icebreaker-bitsy>`__                                                         iCE40UltraPlus UP5K           NA        OK
          ice40_generic `icestick <https://www.latticesemi.com/icestick>`__                                                                                               iCE40 HX1k                    NA        AS
          ice40_generic `iCE40-HX8K <https://www.latticesemi.com/Products/DevelopmentBoardsAndKits/iCE40HX8KBreakoutBoard.aspx>`__                                        iCE40 HX8k                    NT        AS
          ice40_generic `Olimex iCE40HX1K-EVB <https://www.olimex.com/Products/FPGA/iCE40/iCE40HX1K-EVB/open-source-hardware>`__                                          iCE40 HX1k                    NT        AS
          ice40_generic `Olimex iCE40HX8K-EVB <https://www.olimex.com/Products/FPGA/iCE40/iCE40HX8K-EVB/open-source-hardware>`__                                          iCE40 HX8k                    NT        AS
          ice40_generic `Icezum Alhambra II <https://alhambrabits.com/alhambra>`__                                                                                        iCE40 HX4k                    NT        AS
                  kc705 `Xilinx KC705 <https://www.xilinx.com/products/boards-and-kits/ek-k7-kc705-g.html>`__                                                             Kintex7 xc7k325t              OK        NT
             licheeTang `Sipeed Lichee Tang <https://tang.sipeed.com/en/hardware-overview/lichee-tang/>`__                                                                eagle s20 EG4S20BG256         OK        OK
             machXO2EVN `Lattice MachXO2 Breakout Board Evaluation Kit  <https://www.latticesemi.com/products/developmentboardsandkits/machxo2breakoutboard>`__           MachXO2 LCMXO2-7000HE         OK        OK
             machXO3EVN `Lattice MachXO3D Development Board  <https://www.latticesemi.com/products/developmentboardsandkits/machxo3d_development_board>`__                MachXO3D LCMXO3D-9400HC       OK        NT
              machXO3SK `Lattice MachXO3LF Starter Kit <https://www.latticesemi.com/en/Products/DevelopmentBoardsAndKits/MachXO3LFStarterKit>`__                          MachXO3 LCMX03LF-6900C        OK        OK
             nexysVideo `Digilent Nexys Video <https://reference.digilentinc.com/reference/programmable-logic/nexys-video/start>`__                                       Artix xc7a200tsbg484          OK        OK
             orangeCrab `Orange Crab <https://github.com/gregdavill/OrangeCrab>`__                                                                                        ECP5 LFE5U-25F-8MG285C        OK (JTAG) OK (DFU)
            pipistrello `Saanlima Pipistrello LX45 <http://pipistrello.saanlima.com/index.php?title=Welcome_to_Pipistrello>`__                                            Spartan6 xc6slx45-csg324      OK        OK
        qmtechCycloneIV `QMTech CycloneIV Core Board <https://fr.aliexpress.com/item/32949281189.html>`__                                                                 Cyclone IV EP4CE15F23C8N      OK        OK
         qmtechCycloneV `QMTech CycloneV Core Board <https://fr.aliexpress.com/i/1000006622149.html>`__                                                                   Cyclone V 5CEFA2F23I7         OK        OK
                 runber `SeeedStudio Gowin RUNBER <https://www.seeedstudio.com/Gowin-RUNBER-Development-Board-p-4779.html>`__                                             littleBee GW1N-4              OK        IF/EF
                 runber `Scarab Hardware MiniSpartan6+ <https://www.kickstarter.com/projects/1812459948/minispartan6-a-powerful-fpga-board-and-easy-to-use>`__            Spartan6 xc6slx25-3-ftg256    OK        NT
  spartanEdgeAccelBoard `SeeedStudio Spartan Edge Accelerator Board <http://wiki.seeedstudio.com/Spartan-Edge-Accelerator-Board>`__                                       Spartan7 xc7s15ftgb196        OK        NA
               tangnano `Sipeed Tang Nano <https://tangnano.sipeed.com/en/>`__                                                                                            littleBee GW1N-1              OK
             tangnano4k `Sipeed Tang Nano 4K <https://tangnano.sipeed.com/en/>`__                                                                                         littleBee GW1NSR-4C           OK        IF/EF
                tec0117 `Trenz Gowin LittleBee (TEC0117) <https://shop.trenz-electronic.de/en/TEC0117-01-FPGA-Module-with-GOWIN-LittleBee-and-8-MByte-internal-SDRAM>`__  littleBee GW1NR-9             OK        IF
                   xtrx `FairWaves XTRXPro <https://www.crowdsupply.com/fairwaves/xtrx>`__                                                                                Artix xc7a50tcpg236           OK        OK
             xyloni_spi `Efinix Xyloni <https://www.efinixinc.com/products-devkits-xyloni.html>`__                                                                        Trion T8F81                   NA        AS
  trion_t120_bga576_spi `Efinix Trion T120 BGA576 Dev Kit <https://www.efinixinc.com/products-devkits-triont120bga576.html>`__                                            Trion T120BGA576              NA        AS
    trion_ti60_f225_spi `Efinix Titanium F225 Dev Kit <https://www.efinixinc.com/products-devkits-titaniumti60f225.html>`__                                               Titanium Ti60F225             NA        AS
                   xmf3 `PLDkit XMF3 <https://pldkit.com/xilinx/xmf3>`__                                                                                                  Xilinx xc3s200ft256, xcf01s   OK        OK
               zedboard `Avnet ZedBoard <https://www.avnet.com/wps/portal/us/products/avnet-boards/avnet-board-families/zedboard/>`__                                     zynq7000 xc7z020clg484        OK        NA
======================= ================================================================================================================================================= ============================= ========= ========

* IF: Internal Flash
* EF: External Flash
* AS: Active Serial flash mode
* NA: Not Available
* NT: Not Tested
