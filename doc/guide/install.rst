.. _install:

Installing openFPGALoader
#########################

Linux
=====

Debian/Ubuntu
-------------

openFPGALoader is available in the default repositories:

.. code-block:: bash
    
    sudo apt install openfpgaloader

Guix
----------

openFPGALoader is available in the default repositories:

.. code-block:: bash

    guix install openfpgaloader

To use openFPGALoader under GuixSystem without root privileges it is necessary to install the necessary udev rules. This can be done by extending ``udev-service-type`` in the ``operating-system`` configuration file with this package

.. code-block:: bash

    (udev-rules-service 'openfpgaloader openfpgaloader #:groups '(\"plugdev\")

Additionally, ``plugdev`` group should be registered in the ``supplementary-groups`` field of your ``user-account``declaration.  Refer to ``Base Services`` section in the manual for examples.

Arch Linux
----------

openFPGALoader is available in the default repositories:

.. code-block:: bash

    sudo pacman -S openfpgaloader

Alternatively, you could build from source. First: install required libraries:

.. code-block:: bash

   sudo pacman -S git cmake make gcc pkgconf libftdi libusb zlib hidapi gzip

Build step is similar as Debian

Fedora
------

openFPGALoader is available as a Copr repository:

.. code-block:: bash

    sudo dnf copr enable mobicarte/openFPGALoader
    sudo dnf install openFPGALoader

From source 
----------------------------

This application uses ``libftdi1``, so this library must be installed (and, depending on the distribution, headers too):

.. code-block:: bash

    sudo apt install \
      git \
      gzip \
      libftdi1-2 \
      libftdi1-dev \
      libhidapi-hidraw0 \
      libhidapi-dev \
      libudev-dev \
      zlib1g-dev \
      cmake \
      pkg-config \
      make \
      g++

.. HINT::
  ``libudev-dev`` is optional, may be replaced by ``eudev-dev`` or just not installed.

By default, ``(e)udev`` support is enabled (used to open a device by his ``/dev/xx`` node).
If you don't want this option, use:

.. code-block:: bash

    -DENABLE_UDEV=OFF

By default, ``cmsisdap`` support is enabled (used for colorlight I5, I9).
If you don't want this option, use:

.. code-block:: bash

    -DENABLE_CMSISDAP=OFF

Alternatively you can manually specify the location of ``libusb`` and ``libftdi1``:

.. code-block:: bash

    -DUSE_PKGCONFIG=OFF \
    -DLIBUSB_LIBRARIES=<path_to_libusb> \
    -DLIBFTDI_LIBRARIES=<path_to_libftdi> \
    -DLIBFTDI_VERSION=<version> \
    -DCMAKE_CXX_FLAGS="-I<libusb_include_dir> -I<libftdi1_include_dir>"

You may also need to add this if you see link errors between ``libusb`` and ``pthread``:

.. code-block:: bash

    -DLINK_CMAKE_THREADS=ON

By default, ``libgpiod`` support is enabled
If you don't want this option, use:

.. code-block:: bash

    -DENABLE_LIBGPIOD=OFF

Additionaly you have to install ``libgpiod``

To build the app:

.. code-block:: bash

    git clone https://github.com/trabucayre/openFPGALoader
    cd openFPGALoader
    mkdir build
    cd build
    cmake .. # add -DBUILD_STATIC=ON to build a static version
             # add -DENABLE_UDEV=OFF to disable udev support and -d /dev/xxx
             # add -DENABLE_CMSISDAP=OFF to disable CMSIS DAP support
    cmake --build .
    # or
    make -j$(nproc)

To install

.. code-block:: bash

    $ sudo make install

The default install path is ``/usr/local``, to change it, use ``-DCMAKE_INSTALL_PREFIX=myInstallDir`` in cmake invokation.

Udev rules
----------

By default, users have no access to converters.
A rule file (:ghsrc:`99-openfpgaloader.rules <99-openfpgaloader.rules>`) for ``udev`` is provided at the root directory
of this repository.
These rules set access right and group (``plugdev``) when a converter is plugged.

.. code-block:: bash

    sudo cp 99-openfpgaloader.rules /etc/udev/rules.d/
    sudo udevadm control --reload-rules && sudo udevadm trigger # force udev to take new rule
    sudo usermod -a $USER -G plugdev # add user to plugdev group

After that you need to unplug and replug your device.

.. HINT::
   ``usermod`` is used to add ``$USER`` as a member of ``plugdev`` group.
   However this update is not taken into account immediately: it's required to
   logout from current session and login again.
   Check, by using ``id $USER``, if ``plugdev`` is mentioned after ``groups=``.
   An alternate (and temporary) solution is to use ``sudo - $USER`` to have
   your user seen as a member of ``plugdev`` group (works only for the current terminal).

macOS
=====

openFPGALoader is available as a `Homebrew <https://brew.sh>`__ formula:

.. code-block:: bash

    brew install openfpgaloader

Alternatively, if you want to build it by hand:

.. code-block:: bash

    brew install --only-dependencies openfpgaloader
    brew install cmake pkg-config zlib gzip
    git clone https://github.com/trabucayre/openFPGALoader
    cd openFPGALoader
    mkdir build
    cd build
    cmake ..
    make -j

Windows
=======

MSYS2 (Native Build)
--------------------

openFPGALoader can be installed via MSYS2:

.. code-block:: bash

    pacman -S mingw-w64-ucrt-x86_64-openFPGALoader

Cross-compilation from Linux
----------------------------

openFPGALoader can be cross-compiled for Windows from Linux using MinGW-w64
toolchains (GCC or Clang). The build system will automatically download and
build the required dependencies (libusb, libftdi).

**Prerequisites (Debian/Ubuntu):**

.. code-block:: bash

    sudo apt install \
      mingw-w64 \
      libz-mingw-w64-dev \
      clang \
      lld \
      cmake \
      pkg-config \
      p7zip-full

**Prerequisites (Fedora/RHEL/Rocky):**

.. code-block:: bash

    sudo dnf install \
      mingw64-gcc \
      mingw64-gcc-c++ \
      mingw64-zlib \
      mingw64-zlib-static \
      clang \
      lld \
      cmake \
      p7zip \
      p7zip-plugins

**Build (shared steps):**

.. code-block:: bash

    git clone https://github.com/trabucayre/openFPGALoader
    cd openFPGALoader
    mkdir build-win64
    cd build-win64

**Configure + build with GCC (MinGW-w64):**

.. code-block:: bash

    cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchain-x86_64-w64-mingw32.cmake ..
    cmake --build . --parallel

**Configure + build with Clang (MinGW-w64 target):**

.. code-block:: bash

    cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchain-x86_64-w64-mingw32-clang.cmake ..
    cmake --build . --parallel

The resulting ``openFPGALoader.exe`` will be a statically-linked executable
that only depends on standard Windows system DLLs (KERNEL32, msvcrt, WS2_32).

.. NOTE::
   ``zlib`` for the Windows target is required. Configuration fails if it is
   missing (install ``libz-mingw-w64-dev`` on Debian/Ubuntu or
   ``mingw64-zlib`` on Fedora/RHEL/Rocky).
   ``zlib`` is linked statically by default on Windows builds
   (``-DWINDOWS_STATIC_ZLIB=ON``).

**Optional: Strip debug symbols to reduce size:**

.. code-block:: bash

    x86_64-w64-mingw32-strip openFPGALoader.exe
    # or
    llvm-strip openFPGALoader.exe

**Cross-compilation options:**

- ``-DCROSS_COMPILE_DEPS=OFF`` - Disable automatic dependency download (use system libraries)
- ``-DENABLE_CMSISDAP=ON`` - Enable CMSIS-DAP support (requires manually providing hidapi)
- ``-DWINDOWS_STATIC_ZLIB=OFF`` - Allow dynamic zlib linkage (produces ``zlib1.dll`` runtime dependency)

Common
======

Bitstreams for *XC2C (coolrunner-II)* needs to be remapped using ``.map`` shipped with *ISE*.
*ISE* path is set at configure time using:

.. code-block:: bash

    -DISE_PATH=/somewhere/Xilinx/ISE_VERS/

default: ``/opt/Xilinx/14.7``.

Disabling/Enabling Cable or Vendor Drivers
------------------------------------------

With the default ``cmake ..`` configuration, openFPGALoader enables
``ENABLE_CABLE_ALL=ON`` and ``ENABLE_VENDORS_ALL=ON``. This means all cable
and vendor drivers are enabled by default, then filtered only by OS
capabilities and available dependencies.

To reduce binary size, speed up the build, or keep support limited to selected
cables/vendors, you can explicitly enable or disable options.

These commands are equivalent:

.. code-block:: bash

    cmake -DENABLE_CABLE_ALL=ON -DENABLE_VENDORS_ALL=ON ..
    # and
    cmake ..  # Implicit default values

To disable all cable and vendor support:

.. code-block:: bash

    cmake -DENABLE_CABLE_ALL=OFF -DENABLE_VENDORS_ALL=OFF ..

This produces an **openFPGALoader** binary with no cable/vendor support.
Then re-enable only what you need by adding one or more options below.

Each item in the following lists is a CMake option name. Use them with
``-D<OPTION>=ON`` to enable or ``-D<OPTION>=OFF`` to disable.

.. note::

   The default value for each option is provided by ``ENABLE_CABLE_ALL`` and
   ``ENABLE_VENDORS_ALL``.

Example (enable FTDI-based cables and Xilinx devices only):

.. code-block:: bash

    cmake \
      -DENABLE_CABLE_ALL=OFF \
      -DENABLE_VENDORS_ALL=OFF \
      -DENABLE_FTDI_BASED_CABLE=ON \
      -DENABLE_XILINX_SUPPORT=ON \
      ..

**Cable options**

- ``ENABLE_USB_SCAN``: Enable USB cable discovery/scanning support.
- ``ENABLE_ANLOGIC_CABLE``: Enable Anlogic cable support (requires libUSB).
- ``ENABLE_CH347``: Enable CH347 cable support (requires libUSB).
- ``ENABLE_CMSISDAP``: Enable CMSIS-DAP interface support (requires hidapi).
- ``ENABLE_DIRTYJTAG``: Enable DirtyJTAG cable support (requires libUSB).
- ``ENABLE_ESP_USB``: Enable ESP32-S3 USB-JTAG cable support (requires libUSB).
- ``ENABLE_JLINK``: Enable J-Link cable support (requires libUSB).
- ``ENABLE_DFU``: Enable DFU-based cable support (requires libUSB).
- ``ENABLE_FTDI_BASED_CABLE``: Enable FTDI-based cable drivers (requires libftdi).
- ``ENABLE_GOWIN_GWU2X``: Enable Gowin GWU2X interface support.
- ``ENABLE_SVF_JTAG``: Enable SVF JTAG playback support.
- ``ENABLE_USB_BLASTERI``: Enable Altera USB-Blaster I support.
- ``ENABLE_USB_BLASTERII``: Enable Altera USB-Blaster II support.
- ``ENABLE_LIBGPIOD``: Enable libgpiod bitbang driver support (Linux only).
- ``ENABLE_REMOTEBITBANG``: Enable remote-bitbang driver support.
- ``ENABLE_XILINX_VIRTUAL_CABLE``: Enable Xilinx Virtual Cable (XVC) support.

**Vendor options**

- ``ENABLE_ALTERA_SUPPORT``: Enable Altera/Intel device family support.
- ``ENABLE_ANLOGIC_SUPPORT``: Enable Anlogic device family support.
- ``ENABLE_COLOGNECHIP_SUPPORT``: Enable Cologne Chip device family support (requires libftdi).
- ``ENABLE_EFINIX_SUPPORT``: Enable Efinix device family support (requires libftdi).
- ``ENABLE_GOWIN_SUPPORT``: Enable Gowin device family support.
- ``ENABLE_ICE40_SUPPORT``: Enable Lattice iCE40 device family support (requires libftdi).
- ``ENABLE_LATTICE_SUPPORT``: Enable Lattice device family support.
- ``ENABLE_LATTICESSPI_SUPPORT``: Enable Lattice SSPI support (requires libftdi).
- ``ENABLE_XILINX_SUPPORT``: Enable Xilinx device family support.

.. note::

   SPI support is hardcoded to FTDI. When FTDI support is disabled, some
   vendor drivers are also disabled (*iCE40*, *Cologne Chip*, *Efinix*, and
   *Lattice SSPI*).
