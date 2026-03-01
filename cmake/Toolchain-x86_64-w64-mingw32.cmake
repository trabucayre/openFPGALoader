# CMake Toolchain file for cross-compiling to Windows x64 using MinGW-w64
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-x86_64-w64-mingw32.cmake ..

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

find_program(CMAKE_C_COMPILER NAMES x86_64-w64-mingw32-gcc)
find_program(CMAKE_CXX_COMPILER NAMES x86_64-w64-mingw32-g++)
find_program(CMAKE_RC_COMPILER NAMES x86_64-w64-mingw32-windres)

if(NOT CMAKE_C_COMPILER)
    message(FATAL_ERROR "x86_64-w64-mingw32-gcc not found. Please install mingw-w64 toolchain.")
endif()

option(CROSS_COMPILE_DEPS "Download and build Windows dependencies for cross-compilation" ON)

set(CROSS_DEPS_DIR "${CMAKE_BINARY_DIR}/cross-deps" CACHE PATH "Directory for cross-compiled dependencies")

if(EXISTS "/usr/x86_64-w64-mingw32/sys-root/mingw")
    set(MINGW_SYSROOT "/usr/x86_64-w64-mingw32/sys-root/mingw")
elseif(EXISTS "/usr/x86_64-w64-mingw32")
    set(MINGW_SYSROOT "/usr/x86_64-w64-mingw32")
else()
    set(MINGW_SYSROOT "")
endif()

set(CMAKE_FIND_ROOT_PATH ${CROSS_DEPS_DIR} ${MINGW_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -static-libgcc -static-libstdc++")

set(ENABLE_UDEV OFF CACHE BOOL "" FORCE)
set(ENABLE_LIBGPIOD OFF CACHE BOOL "" FORCE)
set(ENABLE_REMOTEBITBANG OFF CACHE BOOL "" FORCE)
set(ENABLE_XILINX_VIRTUAL_CABLE_CLIENT OFF CACHE BOOL "" FORCE)
set(ENABLE_XILINX_VIRTUAL_CABLE_SERVER OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC ON CACHE BOOL "" FORCE)
