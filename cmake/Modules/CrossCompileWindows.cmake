# CrossCompileWindows.cmake
# Downloads and builds dependencies for cross-compiling openFPGALoader to Windows.

include(ExternalProject)
include(FetchContent)

set(LIBUSB_VERSION "1.0.27" CACHE STRING "libusb version")
set(LIBFTDI_VERSION "1.5" CACHE STRING "libftdi version")

set(LIBUSB_URL "https://github.com/libusb/libusb/releases/download/v${LIBUSB_VERSION}/libusb-${LIBUSB_VERSION}.7z")
set(LIBFTDI_URL "https://www.intra2net.com/en/developer/libftdi/download/libftdi1-${LIBFTDI_VERSION}.tar.bz2")

if(NOT DEFINED CROSS_DEPS_DIR)
    set(CROSS_DEPS_DIR "${CMAKE_BINARY_DIR}/cross-deps")
endif()

set(CROSS_DEPS_INSTALL_DIR "${CROSS_DEPS_DIR}/install")
set(CROSS_DEPS_BUILD_DIR "${CROSS_DEPS_DIR}/build")
set(CROSS_DEPS_SRC_DIR "${CROSS_DEPS_DIR}/src")

file(MAKE_DIRECTORY ${CROSS_DEPS_DIR})
file(MAKE_DIRECTORY ${CROSS_DEPS_INSTALL_DIR})
file(MAKE_DIRECTORY ${CROSS_DEPS_INSTALL_DIR}/include)
file(MAKE_DIRECTORY ${CROSS_DEPS_INSTALL_DIR}/lib)
file(MAKE_DIRECTORY ${CROSS_DEPS_INSTALL_DIR}/lib/pkgconfig)
file(MAKE_DIRECTORY ${CROSS_DEPS_BUILD_DIR})
file(MAKE_DIRECTORY ${CROSS_DEPS_SRC_DIR})

find_program(SEVENZIP_EXECUTABLE NAMES 7z 7za p7zip)
if(NOT SEVENZIP_EXECUTABLE)
    message(FATAL_ERROR "7z/p7zip not found. Please install p7zip or 7zip.")
endif()

function(setup_libusb_windows)
    set(LIBUSB_ARCHIVE "${CROSS_DEPS_SRC_DIR}/libusb-${LIBUSB_VERSION}.7z")
    set(LIBUSB_EXTRACT_DIR "${CROSS_DEPS_SRC_DIR}/libusb-${LIBUSB_VERSION}")

    if(NOT EXISTS ${LIBUSB_ARCHIVE})
        message(STATUS "Downloading libusb ${LIBUSB_VERSION}...")
        file(DOWNLOAD ${LIBUSB_URL} ${LIBUSB_ARCHIVE} SHOW_PROGRESS STATUS DOWNLOAD_STATUS)
        list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
        if(NOT STATUS_CODE EQUAL 0)
            message(FATAL_ERROR "Failed to download libusb: ${DOWNLOAD_STATUS}")
        endif()
    endif()

    if(NOT EXISTS "${LIBUSB_EXTRACT_DIR}/MinGW64")
        message(STATUS "Extracting libusb...")
        file(MAKE_DIRECTORY ${LIBUSB_EXTRACT_DIR})
        execute_process(
            COMMAND ${SEVENZIP_EXECUTABLE} x -y -o${LIBUSB_EXTRACT_DIR} ${LIBUSB_ARCHIVE}
            WORKING_DIRECTORY ${CROSS_DEPS_SRC_DIR}
            RESULT_VARIABLE EXTRACT_RESULT
        )
        if(NOT EXTRACT_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to extract libusb")
        endif()
    endif()

    message(STATUS "Installing libusb headers and libraries...")
    file(MAKE_DIRECTORY "${CROSS_DEPS_INSTALL_DIR}/include/libusb-1.0")
    file(COPY "${LIBUSB_EXTRACT_DIR}/include/libusb.h"
         DESTINATION "${CROSS_DEPS_INSTALL_DIR}/include/libusb-1.0/")
    file(COPY "${LIBUSB_EXTRACT_DIR}/MinGW64/static/libusb-1.0.a"
         DESTINATION "${CROSS_DEPS_INSTALL_DIR}/lib/")

    file(WRITE "${CROSS_DEPS_INSTALL_DIR}/lib/pkgconfig/libusb-1.0.pc"
"prefix=${CROSS_DEPS_INSTALL_DIR}
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include/libusb-1.0

Name: libusb-1.0
Description: C API for USB device access from Windows
Version: ${LIBUSB_VERSION}
Libs: -L\${libdir} -lusb-1.0
Cflags: -I\${includedir}
")

    set(LIBUSB_FOUND TRUE PARENT_SCOPE)
    set(LIBUSB_INCLUDE_DIRS "${CROSS_DEPS_INSTALL_DIR}/include/libusb-1.0" PARENT_SCOPE)
    set(LIBUSB_LIBRARIES "${CROSS_DEPS_INSTALL_DIR}/lib/libusb-1.0.a" PARENT_SCOPE)
    set(LIBUSB_VERSION ${LIBUSB_VERSION} PARENT_SCOPE)
endfunction()

function(setup_libftdi_windows)
    set(LIBFTDI_ARCHIVE "${CROSS_DEPS_SRC_DIR}/libftdi1-${LIBFTDI_VERSION}.tar.bz2")
    set(LIBFTDI_SRC_DIR "${CROSS_DEPS_SRC_DIR}/libftdi1-${LIBFTDI_VERSION}")
    set(LIBFTDI_BUILD_DIR "${CROSS_DEPS_BUILD_DIR}/libftdi1")

    if(NOT EXISTS ${LIBFTDI_ARCHIVE})
        message(STATUS "Downloading libftdi ${LIBFTDI_VERSION}...")
        file(DOWNLOAD ${LIBFTDI_URL} ${LIBFTDI_ARCHIVE} SHOW_PROGRESS STATUS DOWNLOAD_STATUS)
        list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
        if(NOT STATUS_CODE EQUAL 0)
            message(FATAL_ERROR "Failed to download libftdi: ${DOWNLOAD_STATUS}")
        endif()
    endif()

    if(NOT EXISTS ${LIBFTDI_SRC_DIR})
        message(STATUS "Extracting libftdi...")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xjf ${LIBFTDI_ARCHIVE}
            WORKING_DIRECTORY ${CROSS_DEPS_SRC_DIR}
            RESULT_VARIABLE EXTRACT_RESULT
        )
        if(NOT EXTRACT_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to extract libftdi")
        endif()
    endif()

    if(NOT EXISTS "${CROSS_DEPS_INSTALL_DIR}/lib/libftdi1.a")
        message(STATUS "Building libftdi for Windows...")
        file(MAKE_DIRECTORY ${LIBFTDI_BUILD_DIR})

        execute_process(
            COMMAND ${CMAKE_COMMAND}
                -DCMAKE_SYSTEM_NAME=Windows
                -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                -DCMAKE_INSTALL_PREFIX=${CROSS_DEPS_INSTALL_DIR}
                -DCMAKE_PREFIX_PATH=${CROSS_DEPS_INSTALL_DIR}
                -DLIBUSB_INCLUDE_DIRS=${CROSS_DEPS_INSTALL_DIR}/include/libusb-1.0
                -DLIBUSB_LIBRARIES=${CROSS_DEPS_INSTALL_DIR}/lib/libusb-1.0.a
                -DFTDIPP=OFF -DBUILD_TESTS=OFF -DDOCUMENTATION=OFF
                -DEXAMPLES=OFF -DFTDI_EEPROM=OFF -DPYTHON_BINDINGS=OFF
                -DSTATICLIBS=ON
                ${LIBFTDI_SRC_DIR}
            WORKING_DIRECTORY ${LIBFTDI_BUILD_DIR}
            RESULT_VARIABLE CONFIG_RESULT
        )
        if(NOT CONFIG_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to configure libftdi")
        endif()

        execute_process(
            COMMAND ${CMAKE_COMMAND} --build . --parallel
            WORKING_DIRECTORY ${LIBFTDI_BUILD_DIR}
            RESULT_VARIABLE BUILD_RESULT
        )
        if(NOT BUILD_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to build libftdi")
        endif()

        execute_process(
            COMMAND ${CMAKE_COMMAND} --install .
            WORKING_DIRECTORY ${LIBFTDI_BUILD_DIR}
            RESULT_VARIABLE INSTALL_RESULT
        )
        if(NOT INSTALL_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to install libftdi")
        endif()

        file(WRITE "${CROSS_DEPS_INSTALL_DIR}/lib/pkgconfig/libftdi1.pc"
"prefix=${CROSS_DEPS_INSTALL_DIR}
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include/libftdi1

Name: libftdi1
Description: Library to program and control the FTDI USB controller
Requires: libusb-1.0
Version: ${LIBFTDI_VERSION}
Libs: -L\${libdir} -lftdi1
Cflags: -I\${includedir}
")
    endif()

    set(LIBFTDI_FOUND TRUE PARENT_SCOPE)
    set(LIBFTDI_INCLUDE_DIRS "${CROSS_DEPS_INSTALL_DIR}/include/libftdi1" PARENT_SCOPE)
    set(LIBFTDI_LIBRARIES "${CROSS_DEPS_INSTALL_DIR}/lib/libftdi1.a" PARENT_SCOPE)
    set(LIBFTDI_VERSION ${LIBFTDI_VERSION} PARENT_SCOPE)
endfunction()

function(setup_windows_cross_compile_deps)
    message(STATUS "Setting up Windows cross-compilation dependencies...")
    message(STATUS "  Dependencies directory: ${CROSS_DEPS_DIR}")
    message(STATUS "  Install directory: ${CROSS_DEPS_INSTALL_DIR}")

    setup_libusb_windows()
    setup_libftdi_windows()

    list(APPEND CMAKE_PREFIX_PATH ${CROSS_DEPS_INSTALL_DIR})
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} PARENT_SCOPE)
    set(ENV{PKG_CONFIG_PATH} "${CROSS_DEPS_INSTALL_DIR}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")

    set(LIBUSB_FOUND TRUE PARENT_SCOPE)
    set(LIBUSB_INCLUDE_DIRS "${CROSS_DEPS_INSTALL_DIR}/include/libusb-1.0" PARENT_SCOPE)
    set(LIBUSB_LIBRARIES "${CROSS_DEPS_INSTALL_DIR}/lib/libusb-1.0.a" PARENT_SCOPE)

    set(LIBFTDI_FOUND TRUE PARENT_SCOPE)
    set(LIBFTDI_INCLUDE_DIRS "${CROSS_DEPS_INSTALL_DIR}/include/libftdi1" PARENT_SCOPE)
    set(LIBFTDI_LIBRARIES "${CROSS_DEPS_INSTALL_DIR}/lib/libftdi1.a" PARENT_SCOPE)

    set(CROSS_DEPS_INSTALL_DIR ${CROSS_DEPS_INSTALL_DIR} PARENT_SCOPE)

    message(STATUS "Windows cross-compilation dependencies ready!")
endfunction()
