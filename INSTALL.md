# Installing openFPGALoader

## Linux

### Arch Linux

openFPGALoader is available in the default repositories:

```
sudo pacman -S openfpgaloader
```

### Fedora

openFPGALoader is available as a Copr repository:

```
sudo dnf copr enable mobicarte/openFPGALoader
sudo dnf install openFPGALoader
```

### From source (Debian, Ubuntu)

This application uses **libftdi1**, so this library must be installed (and,
depending of the distribution, headers too)
```bash
apt-get install libftdi1-2 libftdi1-dev libhidapi-hidraw0 libhidapi-dev libudev-dev \
	zlib1g-dev cmake pkg-config make g++
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

## Udev rules

By default, users have no access to converters. A rule file
(*99-openfpgaloader.rules*) for *udev* is provided at the root directory
of this repository. These rules set access right and group (*plugdev*)
when a converter is plugged.

```bash
$ sudo cp 99-openfpgaloader.rules /etc/udev/rules.d/
$ sudo udevadm control --reload-rules && sudo udevadm trigger # force udev to take new rule
$ sudo usermod -a $USER -G plugdev # add user to plugdev group
```
After that you need to unplug and replug your device.

## macOS

openFPGALoader is available as a [Homebrew](https://brew.sh) formula:

```
brew install openfpgaloader
```

## Windows

## Common

Bitstreams for **XC2C (coolrunner-II)** needs to be remapped using `.map` shipped with
**ISE**. **ISE** path is set at configure time using

```
-DISE_PATH=/somewhere/Xilinx/ISE_VERS/
```

default: `/opt/Xilinx/14.7`
