cmake_minimum_required(VERSION 3.7)

# set the project name
project(openFPGALoader VERSION 0.1)

file(GLOB SOURCES *.hpp *.cpp)

# add the executable
add_executable(openFPGALoader ${SOURCES})

find_path(LIBUSB_INCLUDE_DIR
  NAMES libusb.h
  PATH_SUFFIXES "include" "libusb" "libusb-1.0")

find_library(LIBUSB_LIBRARY
  NAMES usb
  PATH_SUFFIXES "lib" "lib32" "lib64")

find_library(LIBUSB10_LIBRARY
  NAMES usb-1.0
  PATH_SUFFIXES "lib" "lib32" "lib64")

find_library(LIBUDEV_LIBRARY
  NAMES udev
  PATH_SUFFIXES "lib" "lib32" "lib64")

find_path(LIBFTDI1_INCLUDE_DIR
  NAMES ftdi.h
  PATH_SUFFIXES "include" "ftdi" "libftdi1")

find_library(LIBFTDI1_LIBRARY
  NAMES ftdi1
  PATH_SUFFIXES "lib" "lib32" "lib64")

find_library(LIBFTDIPP1_LIBRARY
	NAMES ftdipp1
	PATH_SUFFIXES  "lib" "lib32" "lib64")

find_package(LibFTDI1) 

target_include_directories(openFPGALoader PUBLIC ${LIBUSB_INCLUDE_DIR})
target_include_directories(openFPGALoader PUBLIC ${LIBFTDI1_INCLUDE_DIR})
target_link_libraries(openFPGALoader ${LIBUSB_LIBRARY})
target_link_libraries(openFPGALoader ${LIBFTDI1_LIBRARY})
target_link_libraries(openFPGALoader ${LIBFTDIPP1_LIBRARY})
target_link_libraries(openFPGALoader ${LIBUSB10_LIBRARY})
target_link_libraries(openFPGALoader ${LIBUDEV_LIBRARY})

link_libraries(udev usb-1.0)

#/usr/lib/x86_64-linux-gnu/libftdipp1.so
configure_file(openFPGALoader.h.in openFPGALoader.h)

target_include_directories(openFPGALoader PUBLIC "${PROJECT_BINARY_DIR}")

set(CMAKE_SOURCE_DIR openFPGALoader/)


set(CMAKE_CXX_FLAGS "-DOLD_FTDI_VERSION=1")

# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
