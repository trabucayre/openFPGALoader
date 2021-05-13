
cmake_minimum_required(VERSION 3.0)

# set the project name
project(openFPGALoader VERSION "0.2.6" LANGUAGES CXX)
add_definitions(-DVERSION=\"v${PROJECT_VERSION}\")

option(BUILD_STATIC "Whether or not to build with static libraries" OFF)
option(ENABLE_UDEV "use udev to search JTAG adapter from /dev/xx" ON)
option(USE_PKGCONFIG "Use pkgconfig to find libraries" ON)
option(LINK_CMAKE_THREADS "Use CMake find_package to link the threading library" OFF)

## specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wextra ${CMAKE_CXX_FLAGS_DEBUG}")

if (BUILD_STATIC)
	set(CMAKE_EXE_LINKER_FLAGS "-static-libstdc++ -static ${CMAKE_EXE_LINKER_FLAGS}")
	set(BUILD_SHARED_LIBS OFF)
endif()

include(GNUInstallDirs)
# By default: DATA_DIR="/usr/local/share"
add_definitions(-DDATA_DIR=\"${CMAKE_INSTALL_FULL_DATAROOTDIR}\")

if(USE_PKGCONFIG)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(LIBFTDI REQUIRED libftdi1)
	pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

	if(ENABLE_UDEV)
		pkg_check_modules(LIBUDEV libudev)
		if (LIBUDEV_FOUND)
			add_definitions(-DUSE_UDEV)
		else()
			message("libudev not found, disabling udev support and -D parameter")
			set(ENABLE_UDEV OFF)
		endif()
	endif()
endif()

set(OPENFPGALOADER_SOURCE
	src/anlogic.cpp
	src/anlogicBitParser.cpp
	src/anlogicCable.cpp
	src/dfuFileParser.cpp
	src/dirtyJtag.cpp
	src/efinix.cpp
	src/efinixHexParser.cpp
	src/ice40.cpp
	src/ihexParser.cpp
	src/spiFlash.cpp
	src/rawParser.cpp
	src/usbBlaster.cpp
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
	src/anlogic.hpp
	src/anlogicBitParser.hpp
	src/anlogicCable.hpp
	src/cxxopts.hpp
	src/dfuFileParser.hpp
	src/dirtyJtag.hpp
	src/efinix.hpp
	src/efinixHexParser.hpp
	src/ice40.hpp
	src/ihexParser.hpp
	src/progressBar.hpp
	src/rawParser.hpp
	src/usbBlaster.hpp
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
	target_link_libraries(openFPGALoader ${LIBFTDI1STATIC} ${LIBUSB1STATIC})
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

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	# winsock provides ntohs
	target_link_libraries(openFPGALoader ws2_32)
endif()

# libusb_attach_kernel_driver is only available on Linux.
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	add_definitions(-DATTACH_KERNEL)
endif()

if(ENABLE_UDEV)
include_directories(${LIBUDEV_INCLUDE_DIRS})
target_link_libraries(openFPGALoader ${LIBUDEV_LIBRARIES})
endif()

if (BUILD_STATIC)
	set_target_properties(openFPGALoader PROPERTIES LINK_SEARCH_END_STATIC 1)
endif()
endif()

if (LINK_CMAKE_THREADS)
	find_package(Threads REQUIRED)
	target_link_libraries(openFPGALoader Threads::Threads)
endif()


# libftdi < 1.4 as no usb_addr
# libftdi >= 1.5 as purge_buffer obsolete
string(REPLACE "." ";" VERSION_LIST ${LIBFTDI_VERSION})
list(GET VERSION_LIST 0 LIBFTDI_VERSION_MAJOR)
list(GET VERSION_LIST 1 LIBFTDI_VERSION_MINOR)
math(EXPR FTDI_VAL "${LIBFTDI_VERSION_MAJOR} * 100 + ${LIBFTDI_VERSION_MINOR}")

add_definitions(-DFTDI_VERSION=${FTDI_VAL})

install(TARGETS openFPGALoader DESTINATION bin)
install(FILES 
	test_sfl.svf
	spiOverJtag/spiOverJtag_xc7a100tfgg484.bit
	spiOverJtag/spiOverJtag_xc7a200tsbg484.bit
	spiOverJtag/spiOverJtag_xc7a35tcsg324.bit
	spiOverJtag/spiOverJtag_xc7a35tftg256.bit
	spiOverJtag/spiOverJtag_xc7a50tcpg236.bit
	spiOverJtag/spiOverJtag_xc7a75tfgg484.bit
	spiOverJtag/spiOverJtag_xc7s50csga324.bit
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/openFPGALoader
)
