
cmake_minimum_required(VERSION 3.0)

# set the project name
project(openFPGALoader VERSION 0.1 LANGUAGES CXX)

## specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wextra")

include(GNUInstallDirs)
# By default: DATA_DIR="/usr/local/share"
add_definitions(-DDATA_DIR=\"${CMAKE_INSTALL_FULL_DATAROOTDIR}\")

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBFTDI REQUIRED libftdi1)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
pkg_check_modules(LIBUDEV REQUIRED libudev)

# for non glibc, argp-standalone is required and must be
# explicitly linked. This code will fail for others libc
# implementations
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
	#include <argp.h>
	int main(int argc, char **argv) {
	argp_parse(NULL, argc, argv, 0, 0, NULL);}" HAVE_ARGP)

set(OPENFPGALOADER_SOURCE
	src/spiFlash.cpp
	src/epcq.cpp
	src/svf_jtag.cpp
	src/jedParser.cpp
	src/display.cpp
	src/jtag.cpp
	src/ftdiJtagBitbang.cpp
	src/ftdiJtagMPSSE.cpp
	src/configBitstreamParser.cpp
	src/ftdipp_mpsse.cpp
	src/xilinx.cpp
	src/main.cpp
	src/latticeBitParser.cpp
	src/gowin.cpp
	src/device.cpp
	src/lattice.cpp
	src/progressBar.cpp
	src/fsparser.cpp
	src/mcsParser.cpp
	src/ftdispi.cpp
	src/altera.cpp
	src/bitparser.cpp
)

set(OPENFPGALOADER_HEADERS
	src/altera.hpp
	src/progressBar.hpp
	src/bitparser.hpp
	src/ftdiJtagBitbang.hpp
	src/ftdiJtagMPSSE.hpp
	src/jtag.hpp
	src/jtagInterface.hpp
	src/fsparser.hpp
	src/part.hpp
	src/board.hpp
	src/jedParser.hpp
	src/display.hpp
	src/mcsParser.hpp
	src/ftdipp_mpsse.hpp
	src/spiFlash.hpp
	src/epcq.hpp
	src/svf_jtag.hpp
	src/xilinx.hpp
	src/configBitstreamParser.hpp
	src/device.hpp
	src/gowin.hpp
	src/cable.hpp
	src/ftdispi.hpp
	src/lattice.hpp
	src/latticeBitParser.hpp
)

add_executable(openFPGALoader
	${OPENFPGALOADER_SOURCE}
	${OPENFPGALOADER_HEADERS}
)

include_directories(${LIBUDEV_INCLUDE_DIRS}
	${LIBUSB_INCLUDE_DIRS}
	${LIBFTDI_INCLUDE_DIRS}
)

target_link_libraries(openFPGALoader
	${LIBUDEV_LIBRARIES}
	${LIBUSB_LIBRARIES}
	${LIBFTDI_LIBRARIES}
)

if(NOT HAVE_ARGP)
	target_link_libraries(openFPGALoader /usr/lib/libargp.a)
endif()

# libftdi < 1.4 as no usb_addr
if (${LIBFTDI_VERSION} VERSION_LESS 1.4)
	set(CMAKE_CXX_FLAGS "-DOLD_FTDI_VERSION=1")
else()
	set(CMAKE_CXX_FLAGS "-DOLD_FTDI_VERSION=0")
endif()

install(TARGETS openFPGALoader DESTINATION bin)
install(FILES 
	test_sfl.svf
	spiOverJtag/spiOverJtag_xc7a35.bit
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/openFPGALoader
)
