.. _troubleshooting:

Troubleshooting
###############

I installed openFPGALoader but it says `command not found` when I try to launch it
==================================================================================

The correct spelling of the program is *openFPGALoader* with FPGA and the "L" of "Loader" in uppercase.
Ensure the spelling of the program is correct.

Gowin device could not communicate since last bitstream flashed. (issue `#206 <https://github.com/trabucayre/openFPGALoader/issues/206>`_)
==========================================================================================================================================

Gowin's FPGA may fails to be detected if **JTAGSEL_N** (pin 08 for *GW1N-4K*) is used as a GPIO.
To recover you have to pull down this pin (before power up) to recover JTAG interface (*UG292 - JTAGSELL_N section*).

JTAG init failed
================

Avoid using USB hubs and connect it directly to your PC USB port.


Tang Primer 20k program slow and stucked (issue `#250 <https://github.com/trabucayre/openFPGALoader/issues/250>`_)
==================================================================================================================

Check your openFPGALoader version:

.. code:: bash

    openFPGALoader -V

If it is older than release then v0.9.0, install the most recent version (from commit `f5b89bff68a5e2147404a895c075773884077438 <https://github.com/trabucayre/openFPGALoader/commit/fe259fb78d185b3113661d04cd7efa9ae0232425>`_ or later).

Cannot flash Tang Nano 20k (issue `#251 <https://github.com/trabucayre/openFPGALoader/issues/511>`_)
====================================================================================================

Some firmware version cannot be flashed on Linux-based systems. Version 2024122312 is such an example. It seems this version was not published on the `SiPeed website <https://api.dl.sipeed.com/TANG/Debugger/onboard/BL616/>`_, however some boards sold have this firmware.

The cause of the problem is the debugger on the Tang Nano, specifically the firmware of this debugger. This firmware can be easily updated by following `these <https://wiki.sipeed.com/hardware/en/tang/common-doc/update_debugger.html>`_ steps.

Cannot flash Tang Nano 9k (issue `#251 <https://github.com/trabucayre/openFPGALoader/issues/251>`_)
===================================================================================================

This is a device issue, erase its Embedded Flash using Official GoWin Programmer (preferentially in Windows) and SRAM too, then you can use openFPGALoader again.

Unable to open FTDI device: -4 (usb_open() failed) (issue `#245 <https://github.com/trabucayre/openFPGALoader/issues/245>`_)
============================================================================================================================

Edit your `/etc/udev/rules.d/99-ftdi.rules` file exchanging your programming device permissions.

For more information, check the udev section from `this guide <install.rst>`_

Converter cannot be opened: `fails to open device` (issue `#626 <https://github.com/trabucayre/openFPGALoader/issues/626>`_)
============================================================================================================================

This is usually a permissions issue on Linux.

Check your current groups:

.. code:: bash

    id $USER

Verify device node access rights:

.. code:: bash

    ls -l /dev/ttyUSB* /dev/ttyACM*

Then verify udev rules are installed correctly (``70-openfpgaloader.rules`` or
``99-openfpgaloader.rules``), and that your user is in the expected group
(``dialout`` or ``plugdev``).

After changing groups or rules, reload udev rules, then unplug/replug the
converter and log out/login again.

Reference: `install guide (udev rules section) <https://trabucayre.github.io/openFPGALoader/guide/install.html#udev-rules>`_.


Unable to flash device on OpenBSD: `JTAG init failed with: DirtyJtag: fails to open device`
===========================================================================================
Certain evaluation boards may show the following error message when running openFPGAloader on OpenBSD:

.. code:: bash

    fail to read data usb bulk read failed
    JTAG init failed with: low level FTDI init failed

This issue is most likely caused by the uftdi(4) module, which has attached itself to the device, whereas openFPGALoader requires it to be accessible as a ugen(4) device.

Unfortunately, due to the security concept of OpenBSD, it is not possible to detach it without modifying the kernel and rebooting the system. However, there are two ways to resolve the issue: Either by patching and recompiling the kernel; or by deactivating the uftdi(4) module.

After COMMENTING OUT the problematic devices in `/usr/src/sys/dev/usb/uftdi.c`, the code would look like this:

.. code:: c

    { USB_VENDOR_FTDI, USB_PRODUCT_FTDI_SEMC_DSS20 },
    //{ USB_VENDOR_FTDI, USB_PRODUCT_FTDI_SERIAL_2232C },
    { USB_VENDOR_FTDI, USB_PRODUCT_FTDI_SERIAL_2232L },

With this manual patch applied, the steps in `https://www.openbsd.org/faq/faq5.html <https://www.openbsd.org/faq/faq5.html#Custom>` can be used to recompile the kernel.

Without recompilation, DEACTIVATING uftdi(4) can be achieved using the following commands:

.. code:: bash

    # doas config -e -f -o /bsd.nouftdi /bsd
    OpenBSD 7.8 (GENERIC) #54: Sun Oct 12 12:45:58 MDT 2025
        deraadt@amd64.openbsd.org:/usr/src/sys/arch/amd64/compile/GENERIC
    Enter 'help' for information
    ukc> disable uftdi*
    356 uftdi* disabled
    ukc> disable uftdi0
    ukc> disable uftdi1
    ukc> quit
    Saving modified kernel.
    # reboot

At the boot prompt, typing in

.. code:: bash

    boot> boot /bsd.nouftdi

will boot the new kernel with the disabled module. 

Either way, openFPGALoader will then be able to access the development board as a generic USB device via ugen(4).

