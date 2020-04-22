
cmake_minimum_required(VERSION 3.0)

# set the project name
project(openFPGALoader VERSION 0.1 LANGUAGES CXX)

option(BUILD_STATIC "Whether or not to build with static libraries" OFF)
option(ENABLE_UDEV "use udev to search JTAG adapter from /dev/xx" ON)

## specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wextra")

if (BUILD_STATIC)
	 set(CMAKE_EXE_LINKER_FLAGS "-static-libstdc++ -static")
	 set(BUILD_SHARED_LIBS OFF)
endif()

include(GNUInstallDirs)
# By default: DATA_DIR="/usr/local/share"
add_definitions(-DDATA_DIR=\"${CMAKE_INSTALL_FULL_DATAROOTDIR}\")

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBFTDI REQUIRED libftdi1)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

if(ENABLE_UDEV)
	pkg_check_modules(LIBUDEV libudev)
	if (LIBUDEV_FOUND)
		add_definitions(-DUSE_UDEV)
	else()
		message("libudev not found, disabling udev support and -d parameter")
		set(ENABLE_UDEV OFF)
	endif()
endif()

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
	src/spiInterface.hpp
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

include_directories(
	${LIBUSB_INCLUDE_DIRS}
	${LIBFTDI_INCLUDE_DIRS}
)


if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	find_library(LIBFTDI1STATIC libftdi1.a REQUIRED)
	find_library(LIBUSB1STATIC libusb-1.0.a REQUIRED)
	find_library(LIBARGPSTATIC libargp.a REQUIRED)
	target_link_libraries(openFPGALoader ${LIBFTDI1STATIC} ${LIBUSB1STATIC} ${LIBARGPSTATIC})
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreFoundation -framework IOKit")
	link_directories(/usr/local/lib)
	target_include_directories(openFPGALoader PRIVATE /usr/local/include)
	set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
	set_target_properties(openFPGALoader PROPERTIES LINK_SEARCH_END_STATIC 1)
else()
target_link_libraries(openFPGALoader
	${LIBUSB_LIBRARIES}
	${LIBFTDI_LIBRARIES}
)

# libusb_attach_kernel_driver is only available on Linux.
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	add_definitions(-DATTACH_KERNEL)
endif()

if(ENABLE_UDEV)
include_directories(${LIBUDEV_INCLUDE_DIRS})
target_link_libraries(openFPGALoader ${LIBUDEV_LIBRARIES})
endif()

if(NOT HAVE_ARGP)
	find_library(LIBARGPSTATIC libargp.a REQUIRED)
	target_link_libraries(openFPGALoader ${LIBARGPSTATIC})
endif()

if (BUILD_STATIC)
	set_target_properties(openFPGALoader PROPERTIES LINK_SEARCH_END_STATIC 1)
endif()
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
