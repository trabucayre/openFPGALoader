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

openFPGALoader can be cross-compiled for Windows from Linux using MinGW-w64.
The build system will automatically download and build the required dependencies
(libusb, libftdi).

**Prerequisites (Debian/Ubuntu):**

.. code-block:: bash

    sudo apt install \
      mingw-w64 \
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
      cmake \
      p7zip \
      p7zip-plugins

**Build:**

.. code-block:: bash

    git clone https://github.com/trabucayre/openFPGALoader
    cd openFPGALoader
    mkdir build-win64
    cd build-win64
    cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchain-x86_64-w64-mingw32.cmake ..
    cmake --build . --parallel

The resulting ``openFPGALoader.exe`` will be a statically-linked executable
that only depends on standard Windows system DLLs (KERNEL32, msvcrt, WS2_32).

**Optional: Strip debug symbols to reduce size:**

.. code-block:: bash

    x86_64-w64-mingw32-strip openFPGALoader.exe

**Cross-compilation options:**

- ``-DCROSS_COMPILE_DEPS=OFF`` - Disable automatic dependency download (use system libraries)
- ``-DENABLE_CMSISDAP=ON`` - Enable CMSIS-DAP support (requires manually providing hidapi)

Common
======

Bitstreams for *XC2C (coolrunner-II)* needs to be remapped using ``.map`` shipped with *ISE*.
*ISE* path is set at configure time using:

.. code-block:: bash

    -DISE_PATH=/somewhere/Xilinx/ISE_VERS/

default: ``/opt/Xilinx/14.7``.
