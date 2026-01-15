cmake_minimum_required(VERSION 3.10)

# set the project name
project(openFPGALoader VERSION "1.0.0" LANGUAGES CXX)
add_definitions(-DVERSION=\"v${PROJECT_VERSION}\")

if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
	set(windows_crosscompile ON)
else()
	set(windows_crosscompile OFF)
endif()

####################################################################################################
# Generics Options
####################################################################################################

option(ENABLE_OPTIM       "Enable build with -O3 optimization level"             ON)
option(BUILD_STATIC       "Whether or not to build with static libraries"        OFF)
option(USE_PKGCONFIG      "Use pkgconfig to find libraries"                      ON)
option(LINK_CMAKE_THREADS "Use CMake find_package to link the threading library" OFF)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(ENABLE_UDEV OFF)
else()
	option(ENABLE_UDEV "use udev to search JTAG adapter from /dev/xx" ON)
endif()

option(ENABLE_USB_SCAN "Enable USB Scan option" ON)

####################################################################################################
# CABLES Options
####################################################################################################

# set all cable on by default
option(ENABLE_CABLE_ALL        "Enable all cables"                            ON)

option(ENABLE_ANLOGIC_CABLE    "enable Anlogic cable (requires libUSB)"       ${ENABLE_CABLE_ALL})
option(ENABLE_CH347            "enable CH347 cable (requires libUSB)"         ${ENABLE_CABLE_ALL})
# CMSIS-DAP requires hidapi which is complex to cross-compile, disable by default for cross-compilation
if (NOT windows_crosscompile)
	option(ENABLE_CMSISDAP     "enable cmsis DAP interface (requires hidapi)" ${ENABLE_CABLE_ALL})
else()
	option(ENABLE_CMSISDAP     "enable cmsis DAP interface (requires hidapi)" OFF)
endif()
option(ENABLE_DIRTYJTAG        "enable dirtyJtag cable (requires libUSB)"     ${ENABLE_CABLE_ALL})
option(ENABLE_ESP_USB          "enable ESP32S3 cable (requires libUSB)"       ${ENABLE_CABLE_ALL})
option(ENABLE_JLINK            "enable JLink cable (requires libUSB)"         ${ENABLE_CABLE_ALL})
option(ENABLE_DFU              "enable DFU support (requires libUSB)"         ${ENABLE_CABLE_ALL})
option(ENABLE_FTDI_BASED_CABLE "enable cables based on FTDI (requires libFTDI" ${ENABLE_CABLE_ALL})
option(ENABLE_GOWIN_GWU2X      "enable Gowin GWU2X interface"                 ${ENABLE_CABLE_ALL})
option(ENABLE_SVF_JTAG         "enable SVF support"                           ${ENABLE_CABLE_ALL})
option(ENABLE_USB_BLASTERI     "enable Altera USB Blaster I support"          ${ENABLE_CABLE_ALL})
option(ENABLE_USB_BLASTERII    "enable Altera USB Blaster II support"         ${ENABLE_CABLE_ALL})

set(BLASTERII_PATH           "" CACHE STRING "usbBlasterII firmware directory")
set(ISE_PATH "/opt/Xilinx/14.7" CACHE STRING "ise root directory (default: /opt/Xilinx/14.7)")

# Libgpiod is only available on Linux OS.
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	option(ENABLE_LIBGPIOD "enable libgpiod bitbang driver (requires libgpiod)" ${ENABLE_CABLE_ALL})
else()
	set(ENABLE_LIBGPIOD OFF)
endif()

# XVC and RemoteBitbang are not available on Windows OS.
if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	option(ENABLE_REMOTEBITBANG        "enable remote bitbang driver"              ${ENABLE_CABLE_ALL})
	option(ENABLE_XILINX_VIRTUAL_CABLE "enable Xilinx Virtual Cable (XVC) support" ${ENABLE_CABLE_ALL})
else()
	set(ENABLE_REMOTEBITBANG        OFF)
	set(ENABLE_XILINX_VIRTUAL_CABLE OFF)
endif()

####################################################################################################
# Cross-compilation support
####################################################################################################

# Detect cross-compilation for Windows from Linux/macOS
if(windows_crosscompile)
	set(CROSS_COMPILING_WINDOWS TRUE)
	message(STATUS "Cross-compiling for Windows from ${CMAKE_HOST_SYSTEM_NAME}")

	# Option to automatically download and build cross-compile dependencies
	option(CROSS_COMPILE_DEPS "Download and build Windows dependencies for cross-compilation" ON)

	if(CROSS_COMPILE_DEPS)
		# Include the cross-compilation module
		list(INSERT CMAKE_MODULE_PATH 0 ${PROJECT_SOURCE_DIR}/cmake/Modules)
		include(CrossCompileWindows)

		# Setup the dependencies (downloads libusb, builds libftdi)
		setup_windows_cross_compile_deps()

		# Update pkg-config path for the dependencies
		set(ENV{PKG_CONFIG_PATH} "${CROSS_DEPS_INSTALL_DIR}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")

		# Add the cross-compiled dependencies to the search path
		list(APPEND CMAKE_PREFIX_PATH ${CROSS_DEPS_INSTALL_DIR})
		include_directories(${CROSS_DEPS_INSTALL_DIR}/include)
		link_directories(${CROSS_DEPS_INSTALL_DIR}/lib)
	endif()
else()
	set(CROSS_COMPILING_WINDOWS FALSE)
endif()

####################################################################################################
# VENDORS Options
####################################################################################################

# set all vendor on by default
option(ENABLE_VENDORS_ALL         "Enable all vendors support"                             ON)

option(ENABLE_ALTERA_SUPPORT      "enable Altera FPGAs Support"                            ${ENABLE_VENDORS_ALL})
option(ENABLE_ANLOGIC_SUPPORT     "enable Anlogic FPGAs Support"                           ${ENABLE_VENDORS_ALL})
option(ENABLE_COLOGNECHIP_SUPPORT "enable CologneChip FPGAs Support (requires libftdi)"    ${ENABLE_VENDORS_ALL})
option(ENABLE_EFINIX_SUPPORT      "enable Efinix FPGAs Support (requires libftdi)"         ${ENABLE_VENDORS_ALL})
option(ENABLE_GOWIN_SUPPORT       "enable Gowin FPGAs Support"                             ${ENABLE_VENDORS_ALL})
option(ENABLE_ICE40_SUPPORT       "enable Ice40 FPGAs Support (requires libftdi)"          ${ENABLE_VENDORS_ALL})
option(ENABLE_LATTICE_SUPPORT     "enable Lattice FPGAs Support"                           ${ENABLE_VENDORS_ALL})
option(ENABLE_LATTICESSPI_SUPPORT "enable Lattice FPGAs Support (SSPI / requires libftdi)" ${ENABLE_VENDORS_ALL})
option(ENABLE_XILINX_SUPPORT      "enable Xilinx FPGAs Support"                            ${ENABLE_VENDORS_ALL})

####################################################################################################
# Variables
####################################################################################################

# set dependencies
set(USE_FX2_LL    OFF)
set(USE_LIBFTDI   OFF)
set(USE_LIBUSB    OFF)
set(USE_LIBUSB_LL OFF)

# Only adds libftdi as dependency when a cable
# need this library.
if (ENABLE_FTDI_BASED_CABLE OR ENABLE_USB_BLASTERI OR ENABLE_XILINX_VIRTUAL_CABLE)
	set(USE_LIBFTDI ON)
else()
	message("disabled all cables based on FTDI devices")
endif(ENABLE_FTDI_BASED_CABLE OR ENABLE_USB_BLASTERI OR ENABLE_XILINX_VIRTUAL_CABLE)

# Only adds libusb as dependency when a cable need this library
if (ENABLE_DFU OR ENABLE_ANLOGIC_CABLE OR ENABLE_CH347 OR ENABLE_DIRTYJTAG
		OR ENABLE_ESP_USB OR ENABLE_JLINK OR ENABLE_GOWIN_GWU2X OR ENABLE_USB_BLASTERII OR ENABLE_USB_SCAN)
	set(USE_LIBUSB ON)
endif()

if (ENABLE_USB_SCAN OR ENABLE_GOWIN_GWU2X)
	set(USE_LIBUSB_LL ON)
endif()

# Only enable fx2_ll when cable using it
if (ENABLE_USB_BLASTERII)
	set(USE_FX2_LL ON)
endif()

####################################################################################################
# Build options
####################################################################################################

## specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wextra ${CMAKE_CXX_FLAGS_DEBUG}")
if(ENABLE_OPTIM AND NOT(CMAKE_BUILD_TYPE STREQUAL "Debug"))
	set(CMAKE_CXX_FLAGS "-O3 ${CMAKE_CXX_FLAGS}")
endif()

if (BUILD_STATIC)
	set(CMAKE_EXE_LINKER_FLAGS "-static-libstdc++ -static ${CMAKE_EXE_LINKER_FLAGS}")
	set(BUILD_SHARED_LIBS OFF)
endif()

include(GNUInstallDirs)
# By default: DATA_DIR="/usr/local/share"
add_definitions(-DDATA_DIR=\"${CMAKE_INSTALL_FULL_DATAROOTDIR}\")

add_definitions(-DISE_DIR=\"${ISE_PATH}\")

####################################################################################################
# Dependencies check/search
####################################################################################################

if (USE_LIBFTDI)
	# Try to find the LibFTDI1 with cmake
	find_package(LibFTDI1 QUIET)
endif()

if (USE_PKGCONFIG)
	find_package(PkgConfig REQUIRED)

	# Backward compat when the libftdi is not found
	# by using cmake
	if (USE_LIBFTDI)
		if (NOT LIBFTDI_FOUND)
			pkg_check_modules(LIBFTDI REQUIRED libftdi1)
			string(REPLACE "." ";" VERSION_LIST ${LIBFTDI_VERSION})
			list(GET VERSION_LIST 0 LIBFTDI_VERSION_MAJOR)
			list(GET VERSION_LIST 1 LIBFTDI_VERSION_MINOR)
		endif()
	else()
		set(LIBFTDI_LIBRARY_DIRS "")
		set(LIBFTDI_INCLUDE_DIRS "")
		set(LIBFTDI_LIBRARIES    "")
	endif(USE_LIBFTDI)

	if (USE_LIBUSB)
		pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
	else()
		set(LIBUSB_LIBRARY_DIRS "")
		set(LIBUSB_INCLUDE_DIRS "")
		set(LIBUSB_LIBRARIES    "")
	endif(USE_LIBUSB)

	if(ENABLE_CMSISDAP)
		pkg_check_modules(HIDAPI hidapi-libusb)
		# if libusb not found try with hidraw
		if (NOT HIDAPI_FOUND)
			pkg_check_modules(HIDAPI hidapi-hidraw)
		endif()
		if (NOT HIDAPI_FOUND)
			pkg_check_modules(HIDAPI hidapi)
		endif()
	endif()

	# zlib support (gzip)
	pkg_check_modules(ZLIB zlib)
	if (NOT ZLIB_FOUND)
		# try zlib-ng
		pkg_check_modules(ZLIB zlib-ng)
		if (ZLIB_FOUND)
			add_definitions(-DHAS_ZLIBNG)
		else()
			message(FATAL_ERROR "Could find ZLIB")
		endif()
	endif(NOT ZLIB_FOUND)

	if (ENABLE_UDEV)
		pkg_check_modules(LIBUDEV libudev)
		if (LIBUDEV_FOUND)
			add_definitions(-DUSE_UDEV)
		else()
			message("libudev not found, disabling udev support and -D parameter")
			set(ENABLE_UDEV OFF)
		endif()
	endif()

	if (ENABLE_LIBGPIOD)
		pkg_check_modules(LIBGPIOD libgpiod)
		if (NOT LIBGPIOD_FOUND)
			message("libgpiod not found, disabling gpiod support")
			set(ENABLE_LIBGPIOD OFF)
		endif()
	endif()
endif()

####################################################################################################
# FILES
####################################################################################################

# ===========================
# Core Classes
# ===========================
set(OPENFPGALOADER_SOURCE
	src/common.cpp
	src/configBitstreamParser.cpp
	src/device.cpp
	src/display.cpp
	src/main.cpp
	src/progressBar.cpp
)

set(OPENFPGALOADER_HEADERS
	src/board.hpp
	src/cable.hpp
	src/common.hpp
	src/configBitstreamParser.hpp
	src/cxxopts.hpp
	src/device.hpp
	src/display.hpp
	src/part.hpp
	src/progressBar.hpp
)

# ===========================
# Parsers classes
# ===========================
list(APPEND OPENFPGALOADER_SOURCE  src/rawParser.cpp)
list(APPEND OPENFPGALOADER_HEADERS src/rawParser.hpp)

if (${ENABLE_LATTICE_SUPPORT} OR ${ENABLE_XILINX_SUPPORT})
	list(APPEND OPENFPGALOADER_SOURCE
		src/jedParser.cpp
		src/mcsParser.cpp
	)

	list(APPEND OPENFPGALOADER_HEADERS
		src/jedParser.hpp
		src/mcsParser.hpp
	)
endif()

# ===========================
# To be sorted
# ===========================
list(APPEND OPENFPGALOADER_SOURCE
	src/spiFlash.cpp
	src/spiInterface.cpp
	src/epcq.cpp
	src/jtag.cpp
)

list(APPEND OPENFPGALOADER_HEADERS
	src/jtag.hpp
	src/jtagInterface.hpp
	src/spiFlash.hpp
	src/spiFlashdb.hpp
	src/epcq.hpp
	src/spiInterface.hpp
)

# FTDI Based cables
if (${USE_LIBFTDI})
	add_definitions(-DUSE_LIBFTDI)

	list(APPEND OPENFPGALOADER_SOURCE
		src/ch552_jtag.cpp
		src/ftdiJtagBitbang.cpp
		src/ftdiJtagMPSSE.cpp
		src/ftdipp_mpsse.cpp
		src/ftdispi.cpp
	)

	list(APPEND OPENFPGALOADER_HEADERS
		src/ch552_jtag.hpp
		src/ftdiJtagBitbang.hpp
		src/ftdiJtagMPSSE.hpp
		src/ftdipp_mpsse.hpp
		src/ftdispi.hpp
	)

	# CologneChip Drivers / Files parsers.
	if (ENABLE_COLOGNECHIP_SUPPORT)
		list(APPEND OPENFPGALOADER_SOURCE
			src/colognechip.cpp
			src/colognechipCfgParser.cpp
		)

		list(APPEND OPENFPGALOADER_HEADERS
			src/colognechip.hpp
			src/colognechipCfgParser.hpp
		)
		add_definitions(-DENABLE_COLOGNECHIP_SUPPORT=1)
		message("CologneChip support enabled")
	else()
		message("CologneChip support disabled")
	endif(ENABLE_COLOGNECHIP_SUPPORT)

	# Efinix Drivers / Files parsers.
	if (ENABLE_EFINIX_SUPPORT)
		list(APPEND OPENFPGALOADER_SOURCE
			src/efinix.cpp
			src/efinixHexParser.cpp
		)

		list(APPEND OPENFPGALOADER_HEADERS
			src/efinix.hpp
			src/efinixHexParser.hpp
		)
		add_definitions(-DENABLE_EFINIX_SUPPORT=1)
		message("Efinix support enabled")
	else()
		message("Efinix support disabled")
	endif(ENABLE_EFINIX_SUPPORT)

	# ICE40 Driver.
	if (ENABLE_ICE40_SUPPORT)
		list(APPEND OPENFPGALOADER_SOURCE  src/ice40.cpp)
		list(APPEND OPENFPGALOADER_HEADERS src/ice40.hpp)
		add_definitions(-DENABLE_ICE40_SUPPORT=1)
		message("ICE40 support enabled")
	else()
		message("ICE40 support disabled")
	endif(ENABLE_ICE40_SUPPORT)

	# Lattice SSPI Driver.
	if (ENABLE_LATTICESSPI_SUPPORT)
		list(APPEND OPENFPGALOADER_SOURCE  src/latticeSSPI.cpp)
		list(APPEND OPENFPGALOADER_HEADERS src/latticeSSPI.hpp)
		add_definitions(-DENABLE_LATTICESSPI_SUPPORT=1)
		message("Lattice SSPI support enabled")
	else()
		message("Lattice SSPI support disabled")
	endif(ENABLE_LATTICESSPI_SUPPORT)
endif()

# Altera Drivers / Files parsers.
if (ENABLE_ALTERA_SUPPORT)
	list(APPEND OPENFPGALOADER_SOURCE
		src/altera.cpp
		src/pofParser.cpp
	)

	list(APPEND OPENFPGALOADER_HEADERS
		src/altera.hpp
		src/pofParser.hpp
	)
	add_definitions(-DENABLE_ALTERA_SUPPORT=1)
	message("Altera support enabled")
else()
	message("Altera support disabled")
endif(ENABLE_ALTERA_SUPPORT)

# Anlogic Drivers / Files parsers.
if (ENABLE_ANLOGIC_SUPPORT)
	list(APPEND OPENFPGALOADER_SOURCE
		src/anlogic.cpp
		src/anlogicBitParser.cpp
	)

	list(APPEND OPENFPGALOADER_HEADERS
		src/anlogic.hpp
		src/anlogicBitParser.hpp
	)
	add_definitions(-DENABLE_ANLOGIC_SUPPORT=1)
	message("Anlogic support enabled")
else()
	message("Anlogic support disabled")
endif(ENABLE_ANLOGIC_SUPPORT)

# Gowin Drivers / Files parsers.
if (ENABLE_GOWIN_SUPPORT)
	list(APPEND OPENFPGALOADER_SOURCE
		src/gowin.cpp
		src/fsparser.cpp
	)

	list(APPEND OPENFPGALOADER_HEADERS
		src/gowin.hpp
		src/fsparser.hpp
	)
	add_definitions(-DENABLE_GOWIN_SUPPORT=1)
	message("Gowin support enabled")
else()
	message("Gowin support disabled")
endif(ENABLE_GOWIN_SUPPORT)

# Xilinx Drivers / Files parsers.
if (ENABLE_XILINX_SUPPORT)
	list(APPEND OPENFPGALOADER_SOURCE
		src/bitparser.cpp
		src/xilinx.cpp
		src/xilinxMapParser.cpp
	)

	list(APPEND OPENFPGALOADER_HEADERS
		src/bitparser.hpp
		src/xilinx.hpp
		src/xilinxMapParser.hpp
	)
	add_definitions(-DENABLE_XILINX_SUPPORT=1)
	message("Xilinx support enabled")
else()
	message("Xilinx support disabled")
endif(ENABLE_XILINX_SUPPORT)

# Lattice Drivers / Files parsers.
if (ENABLE_LATTICE_SUPPORT)
	list(APPEND OPENFPGALOADER_SOURCE
		src/lattice.cpp
		src/feaparser.cpp
		src/latticeBitParser.cpp
	)

	list(APPEND OPENFPGALOADER_HEADERS
		src/lattice.hpp
		src/feaparser.hpp
		src/latticeBitParser.hpp
	)
	add_definitions(-DENABLE_LATTICE_SUPPORT=1)
	message("Lattice support enabled")
else()
	message("Lattice support disabled")
endif(ENABLE_LATTICE_SUPPORT)

# Cables select

# DFU
if (ENABLE_DFU)
list(APPEND OPENFPGALOADER_SOURCE
	src/dfu.cpp
	src/dfuFileParser.cpp
)

list(APPEND OPENFPGALOADER_HEADERS
	src/dfu.hpp
	src/dfuFileParser.hpp
)
endif(ENABLE_DFU)

# Anlogic Cable
if (ENABLE_ANLOGIC_CABLE)
list(APPEND OPENFPGALOADER_SOURCE  src/anlogicCable.cpp)
list(APPEND OPENFPGALOADER_HEADERS src/anlogicCable.hpp)
endif()

# CH347
if (ENABLE_CH347)
list(APPEND OPENFPGALOADER_SOURCE  src/ch347jtag.cpp)
list(APPEND OPENFPGALOADER_HEADERS src/ch347jtag.hpp)
endif()

# dirtyJtag
if (ENABLE_DIRTYJTAG)
list(APPEND OPENFPGALOADER_SOURCE  src/dirtyJtag.cpp)
list(APPEND OPENFPGALOADER_HEADERS src/dirtyJtag.hpp)
endif()

# ESP32S3
if (ENABLE_ESP_USB)
list(APPEND OPENFPGALOADER_SOURCE  src/esp_usb_jtag.cpp)
list(APPEND OPENFPGALOADER_HEADERS src/esp_usb_jtag.hpp)
endif()

# Gowin GWU2X JTAG interface
if(ENABLE_GOWIN_GWU2X)
list(APPEND OPENFPGALOADER_SOURCE  src/gwu2x_jtag.cpp)
list(APPEND OPENFPGALOADER_HEADERS src/gwu2x_jtag.hpp)
endif()

# Jetson Nano (libGPIO based)
if (ENABLE_JETSONNANOGPIO)
list(APPEND OPENFPGALOADER_SOURCE  src/jetsonNanoJtagBitbang.cpp)
list(APPEND OPENFPGALOADER_HEADERS src/jetsonNanoJtagBitbang.hpp)
endif()

# JLINK
if (ENABLE_JLINK)
list(APPEND OPENFPGALOADER_SOURCE  src/jlink.cpp)
list(APPEND OPENFPGALOADER_HEADERS src/jlink.hpp)
endif()

# libGPIOD support
if (ENABLE_LIBGPIOD)
list(APPEND OPENFPGALOADER_SOURCE  src/libgpiodJtagBitbang.cpp)
list(APPEND OPENFPGALOADER_HEADERS src/libgpiodJtagBitbang.hpp)
endif()

# RemoteBitbang
if (ENABLE_REMOTEBITBANG)
list (APPEND OPENFPGALOADER_SOURCE  src/remoteBitbang_client.cpp)
list (APPEND OPENFPGALOADER_HEADERS src/remoteBitbang_client.hpp)
endif()

# SVF JTAG file type support
if (ENABLE_SVF_JTAG)
list (APPEND OPENFPGALOADER_SOURCE  src/svf_jtag.cpp)
list (APPEND OPENFPGALOADER_HEADERS src/svf_jtag.hpp)
add_definitions(-DENABLE_SVF_JTAG)
endif()

# Xilinx Virtual Cable
if (ENABLE_XILINX_VIRTUAL_CABLE)
list (APPEND OPENFPGALOADER_SOURCE  src/xvc_client.cpp src/xvc_server.cpp)
list (APPEND OPENFPGALOADER_HEADERS src/xvc_client.hpp src/xvc_server.hpp)
endif()

# Altera USB Blaster (I & II).
if (ENABLE_USB_BLASTERI OR ENABLE_USB_BLASTERII)
	add_definitions(-DENABLE_USBBLASTER)

	if (ENABLE_USB_BLASTERI)
		add_definitions(-DENABLE_USB_BLASTERI)
	endif()

	if (ENABLE_USB_BLASTERII)
		add_definitions(-DBLASTERII_DIR=\"${BLASTERII_PATH}\")
		add_definitions(-DENABLE_USB_BLASTERII)
	endif (ENABLE_USB_BLASTERII)

	list(APPEND OPENFPGALOADER_SOURCE  src/usbBlaster.cpp)
	list(APPEND OPENFPGALOADER_HEADERS src/usbBlaster.hpp)
endif(ENABLE_USB_BLASTERI OR ENABLE_USB_BLASTERII)

link_directories(
	${LIBUSB_LIBRARY_DIRS}
	${LIBFTDI_LIBRARY_DIRS}
)

if (ENABLE_LIBGPIOD)
	link_directories(${LIBGPIOD_LIBRARY_DIRS})
endif()

if (ENABLE_CMSISDAP AND HIDAPI_FOUND)
	link_directories(${HIDAPI_LIBRARY_DIRS})
endif()

if (USE_LIBUSB_LL)
list(APPEND OPENFPGALOADER_SOURCE  src/libusb_ll.cpp)
list(APPEND OPENFPGALOADER_HEADERS src/libusb_ll.hpp)
endif()

if (USE_FX2_LL)
list(APPEND OPENFPGALOADER_SOURCE  src/fx2_ll.cpp src/ihexParser.cpp)
list(APPEND OPENFPGALOADER_HEADERS src/fx2_ll.hpp src/ihexParser.hpp)
endif()

add_executable(openFPGALoader
	${OPENFPGALOADER_SOURCE}
	${OPENFPGALOADER_HEADERS}
)

include_directories(
	${LIBUSB_INCLUDE_DIRS}
	${LIBFTDI_INCLUDE_DIRS}
)

target_link_libraries(openFPGALoader
	${LIBUSB_LIBRARIES}
	${LIBFTDI_LIBRARIES}
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	# winsock provides ntohs
	target_link_libraries(openFPGALoader ws2_32)

	target_sources(openFPGALoader PRIVATE src/pathHelper.cpp)
	list(APPEND OPENFPGALOADER_HEADERS src/pathHelper.hpp)
endif()

# libusb_attach_kernel_driver is only available on Linux.
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	add_definitions(-DATTACH_KERNEL)
endif()

if (ENABLE_UDEV)
	include_directories(${LIBUDEV_INCLUDE_DIRS})
	target_link_libraries(openFPGALoader ${LIBUDEV_LIBRARIES})
endif()

if (ENABLE_UDEV OR ENABLE_LIBGPIOD OR ENABLE_JETSONNANOGPIO)
	add_definitions(-DUSE_DEVICE_ARG)
endif(ENABLE_UDEV OR ENABLE_LIBGPIOD OR ENABLE_JETSONNANOGPIO)

if (BUILD_STATIC)
	set_target_properties(openFPGALoader PROPERTIES LINK_SEARCH_END_STATIC 1)
endif()

# Anlogic Cable
if (ENABLE_ANLOGIC_CABLE)
	add_definitions(-DENABLE_ANLOGIC_CABLE=1)
	message("Anlogic Cable support enabled")
else()
	message("Anlogic Cable support disabled")
endif()

# CH347
if (ENABLE_CH347)
	add_definitions(-DENABLE_CH347=1)
	message("CH347 support enabled")
else()
	message("CH347 support disabled")
endif()

if (ENABLE_CMSISDAP)
	if (HIDAPI_FOUND)
		include_directories(${HIDAPI_INCLUDE_DIRS})
		target_link_libraries(openFPGALoader ${HIDAPI_LIBRARIES})
		add_definitions(-DENABLE_CMSISDAP=1)
		target_sources(openFPGALoader PRIVATE src/cmsisDAP.cpp)
		list (APPEND OPENFPGALOADER_HEADERS src/cmsisDAP.hpp)
		message("cmsis_dap support enabled")
	else()
		message("hidapi-libusb not found: cmsis_dap support disabled")
	endif()
endif(ENABLE_CMSISDAP)

# DFU
if (ENABLE_DFU)
	add_definitions(-DENABLE_DFU=1)
	message("DFU support enabled")
else()
	message("DFU support disabled")
endif()

# dirtyJtag
if (ENABLE_DIRTYJTAG)
	add_definitions(-DENABLE_DIRTYJTAG=1)
	message("dirtyJtag support enabled")
else()
	message("dirtyJtag support disabled")
endif()

# ESP32S3
if (ENABLE_ESP_USB)
	add_definitions(-DENABLE_ESP_USB=1)
	message("ESP32S3 support enabled")
else()
	message("ESP32S3 support disabled")
endif()

# Gowin GWU2X JTAG interface
if(ENABLE_GOWIN_GWU2X)
	add_definitions(-DENABLE_GOWIN_GWU2X=1)
	message("Gowin GWU2X support enabled")
else()
	message("Gowin GWU2X support disabled")
endif()

# Jetson Nano (libGPIO based)
if (ENABLE_JETSONNANOGPIO)
	add_definitions(-DENABLE_JETSONNANOGPIO=1)
	message("Jetson Nano GPIO support enabled")
endif(ENABLE_JETSONNANOGPIO)

# JLINK
if (ENABLE_JLINK)
	add_definitions(-DENABLE_JLINK=1)
	message("JLink support enabled")
else()
	message("JLink support disabled")
endif()

# libGPIOD support
if (ENABLE_LIBGPIOD)
	include_directories(${LIBGPIOD_INCLUDE_DIRS})
	target_link_libraries(openFPGALoader ${LIBGPIOD_LIBRARIES})
	add_definitions(-DENABLE_LIBGPIOD=1)
	if (LIBGPIOD_VERSION VERSION_GREATER_EQUAL 2)
		message("libgpiod v2 support enabled")
		add_definitions(-DGPIOD_APIV2)
	else()
		message("libgpiod v1 support enabled")
	endif()
endif(ENABLE_LIBGPIOD)

if (ENABLE_REMOTEBITBANG)
	add_definitions(-DENABLE_REMOTEBITBANG=1)
	message("Remote bitbang client support enabled")
else()
	message("Remote bitbang client support disabled")
endif()

if (ENABLE_XILINX_VIRTUAL_CABLE)
	add_definitions(-DENABLE_XVC=1)
	set(CMAKE_EXE_LINKER_FLAGS "-pthread ${CMAKE_EXE_LINKER_FLAGS}")
	message("Xilinx Virtual Server support enabled")
else()
	message("Xilinx Virtual Server support disabled")
endif()

# USB Devices Scan
if (ENABLE_USB_SCAN)
	add_definitions("-DENABLE_USB_SCAN=1")
endif()

if (ZLIB_FOUND)
	include_directories(${ZLIB_INCLUDE_DIRS})
	target_link_libraries(openFPGALoader ${ZLIB_LIBRARIES})
	add_definitions(-DHAS_ZLIB=1)
else()
	message("zlib library not found: can't flash intel/altera devices")
endif()

if (LINK_CMAKE_THREADS)
	find_package(Threads REQUIRED)
	target_link_libraries(openFPGALoader Threads::Threads)
endif()

if (USE_LIBFTDI)
	# libftdi < 1.4 as no usb_addr
	# libftdi >= 1.5 as purge_buffer obsolete
	math(EXPR FTDI_VAL "${LIBFTDI_VERSION_MAJOR} * 100 + ${LIBFTDI_VERSION_MINOR}")
	add_definitions(-DFTDI_VERSION=${FTDI_VAL})
endif()


install(TARGETS openFPGALoader DESTINATION bin)

####################################################################################################
# SPIOverJtag bitstreams install
####################################################################################################

file(GLOB GZ_FILES spiOverJtag/spiOverJtag_*.*.gz)

# Compress rbf and bit files present into repository
# TODO: test compat with Windows and MacOS
list(INSERT CMAKE_MODULE_PATH 0 ${PROJECT_SOURCE_DIR}/cmake/Modules)
include(FindGZIP)

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows" AND GZIP_PRG)
	set(SPIOVERJTAG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/spiOverJtag")
	file(GLOB BITS_FILES RELATIVE ${SPIOVERJTAG_DIR} spiOverJtag/spiOverJtag_*.bit)
	file(GLOB RBF_FILES RELATIVE ${SPIOVERJTAG_DIR} spiOverJtag/spiOverJtag_*.rbf)

	STRING(REGEX REPLACE ".bit" ".bit.gz" BIT_GZ_FILES "${BITS_FILES}")
	STRING(REGEX REPLACE ".rbf" ".rbf.gz" RBF_GZ_FILES "${RBF_FILES}")

	FOREACH(bit ${BITS_FILES} ${RBF_FILES})
		ADD_CUSTOM_COMMAND(OUTPUT ${bit}.gz
			COMMAND ${GZIP_PRG} -9 -c ${bit} > ${CMAKE_CURRENT_BINARY_DIR}/${bit}.gz
			DEPENDS ${SPIOVERJTAG_DIR}/${bit}
			WORKING_DIRECTORY ${SPIOVERJTAG_DIR}
			COMMENT "Building ${bit}.gz")
		list(APPEND GZ_FILES ${CMAKE_CURRENT_BINARY_DIR}/${bit}.gz)
	ENDFOREACH(bit)

	ADD_CUSTOM_TARGET(bit ALL DEPENDS ${BIT_GZ_FILES} ${RBF_GZ_FILES})
else()
	file(GLOB BITS_FILES spiOverJtag/spiOverJtag_*.bit)
	file(GLOB RBF_FILES spiOverJtag/spiOverJtag_*.rbf)
	install(FILES
		${BITS_FILES}
		${RBF_FILES}
		DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/openFPGALoader
	)
endif()

install(FILES
	${GZ_FILES}
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/openFPGALoader
)
