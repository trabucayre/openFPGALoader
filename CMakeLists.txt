cmake_minimum_required(VERSION 3.0)

# set the project name
project(openFPGALoader VERSION "0.10.0" LANGUAGES CXX)
add_definitions(-DVERSION=\"v${PROJECT_VERSION}\")

option(BUILD_STATIC "Whether or not to build with static libraries" OFF)
if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(ENABLE_UDEV OFF)
else()
	option(ENABLE_UDEV "use udev to search JTAG adapter from /dev/xx" ON)
endif()
option(ENABLE_CMSISDAP "enable cmsis DAP interface (requires hidapi)" ON)
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	option(ENABLE_LIBGPIOD "enable libgpiod bitbang driver (requires libgpiod)" ON)
else()
	set(ENABLE_LIBGPIOD OFF)
endif()
option(USE_PKGCONFIG "Use pkgconfig to find libraries" ON)
option(LINK_CMAKE_THREADS "Use CMake find_package to link the threading library" OFF)
set(ISE_PATH "/opt/Xilinx/14.7" CACHE STRING "ise root directory (default: /opt/Xilinx/14.7)")

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

add_definitions(-DISE_DIR=\"${ISE_PATH}\")

if (USE_PKGCONFIG)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(LIBFTDI REQUIRED libftdi1)
	pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
	pkg_check_modules(HIDAPI hidapi-hidraw)
	# if hidraw not found try with libusb
	if (NOT HIDAPI_FOUND)
		pkg_check_modules(HIDAPI hidapi-libusb)
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

set(OPENFPGALOADER_SOURCE
	src/anlogic.cpp
	src/anlogicBitParser.cpp
	src/anlogicCable.cpp
	src/ch552_jtag.cpp
	src/dfu.cpp
	src/dfuFileParser.cpp
	src/dirtyJtag.cpp
	src/efinix.cpp
	src/efinixHexParser.cpp
	src/fx2_ll.cpp
	src/ice40.cpp
	src/ihexParser.cpp
	src/pofParser.cpp
	src/rawParser.cpp
	src/spiFlash.cpp
	src/spiInterface.cpp
	src/usbBlaster.cpp
	src/epcq.cpp
	src/svf_jtag.cpp
	src/jedParser.cpp
	src/feaparser.cpp
	src/display.cpp
	src/jtag.cpp
	src/ftdiJtagBitbang.cpp
	src/ftdiJtagMPSSE.cpp
	src/configBitstreamParser.cpp
	src/ftdipp_mpsse.cpp
	src/main.cpp
	src/latticeBitParser.cpp
	src/libusb_ll.cpp
	src/gowin.cpp
	src/device.cpp
	src/jlink.cpp
	src/lattice.cpp
	src/progressBar.cpp
	src/fsparser.cpp
	src/mcsParser.cpp
	src/ftdispi.cpp
	src/altera.cpp
	src/bitparser.cpp
	src/xilinx.cpp
	src/xilinxMapParser.cpp
	src/colognechip.cpp
	src/colognechipCfgParser.cpp
)

set(OPENFPGALOADER_HEADERS
	src/altera.hpp
	src/anlogic.hpp
	src/anlogicBitParser.hpp
	src/anlogicCable.hpp
	src/ch552_jtag.hpp
	src/cxxopts.hpp
	src/dfu.hpp
	src/dfuFileParser.hpp
	src/dirtyJtag.hpp
	src/efinix.hpp
	src/efinixHexParser.hpp
	src/fx2_ll.hpp
	src/ice40.hpp
	src/ihexParser.hpp
	src/pofParser.hpp
	src/progressBar.hpp
	src/rawParser.hpp
	src/usbBlaster.hpp
	src/bitparser.hpp
	src/ftdiJtagBitbang.hpp
	src/ftdiJtagMPSSE.hpp
	src/jlink.hpp
	src/jtag.hpp
	src/jtagInterface.hpp
	src/libusb_ll.hpp
	src/fsparser.hpp
	src/part.hpp
	src/board.hpp
	src/jedParser.hpp
	src/feaparser.hpp
	src/display.hpp
	src/mcsParser.hpp
	src/ftdipp_mpsse.hpp
	src/spiFlash.hpp
	src/spiFlashdb.hpp
	src/epcq.hpp
	src/spiInterface.hpp
	src/svf_jtag.hpp
	src/configBitstreamParser.hpp
	src/device.hpp
	src/gowin.hpp
	src/cable.hpp
	src/ftdispi.hpp
	src/lattice.hpp
	src/latticeBitParser.hpp
	src/xilinx.hpp
	src/xilinxMapParser.hpp
	src/colognechip.hpp
	src/colognechipCfgParser.hpp
)

link_directories(
	${LIBUSB_LIBRARY_DIRS}
	${LIBFTDI_LIBRARY_DIRS}
)

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

if (ENABLE_LIBGPIOD)
	link_directories(${LIBGPIOD_LIBRARY_DIRS})
	include_directories(${LIBGPIOD_INCLUDE_DIRS})
	target_link_libraries(openFPGALoader ${LIBGPIOD_LIBRARIES})
	add_definitions(-DENABLE_LIBGPIOD=1)
	target_sources(openFPGALoader PRIVATE src/libgpiodJtagBitbang.cpp)
	list (APPEND OPENFPGALOADER_HEADERS src/libgpiodJtagBitbang.hpp)
	message("libgpiod support enabled")
endif(ENABLE_LIBGPIOD)

if (ENABLE_JETSONNANOGPIO)
	add_definitions(-DENABLE_JETSONNANOGPIO=1)
	target_sources(openFPGALoader PRIVATE src/jetsonNanoJtagBitbang.cpp)
	list (APPEND OPENFPGALOADER_HEADERS src/jetsonNanoJtagBitbang.hpp)
	message("Jetson Nano GPIO support enabled")
endif(ENABLE_JETSONNANOGPIO)

if (ENABLE_UDEV OR ENABLE_LIBGPIOD OR ENABLE_JETSONNANOGPIO)
	add_definitions(-DUSE_DEVICE_ARG)
endif(ENABLE_UDEV OR ENABLE_LIBGPIOD OR ENABLE_JETSONNANOGPIO)

if (BUILD_STATIC)
	set_target_properties(openFPGALoader PROPERTIES LINK_SEARCH_END_STATIC 1)
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

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	add_definitions(-DENABLE_XVC=1)
	target_sources(openFPGALoader PRIVATE src/xvc_client.cpp src/xvc_server.cpp)
	list (APPEND OPENFPGALOADER_HEADERS src/xvc_client.hpp src/xvc_server.cpp)
	set(CMAKE_EXE_LINKER_FLAGS "-pthread ${CMAKE_EXE_LINKER_FLAGS}")
	message("Xilinx Virtual Server support enabled")
else()
		message("Xilinx Virtual Server support disabled")
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

# libftdi < 1.4 as no usb_addr
# libftdi >= 1.5 as purge_buffer obsolete
string(REPLACE "." ";" VERSION_LIST ${LIBFTDI_VERSION})
list(GET VERSION_LIST 0 LIBFTDI_VERSION_MAJOR)
list(GET VERSION_LIST 1 LIBFTDI_VERSION_MINOR)
math(EXPR FTDI_VAL "${LIBFTDI_VERSION_MAJOR} * 100 + ${LIBFTDI_VERSION_MINOR}")

add_definitions(-DFTDI_VERSION=${FTDI_VAL})

install(TARGETS openFPGALoader DESTINATION bin)
file(GLOB BITS_FILES spiOverJtag/spiOverJtag_*.bit)
file(GLOB RBF_FILES spiOverJtag/spiOverJtag_*.rbf)
file(GLOB GZ_FILES spiOverJtag/spiOverJtag_*.*.gz)
install(FILES 
	${BITS_FILES}
	${RBF_FILES}
	${GZ_FILES}
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/openFPGALoader
)
