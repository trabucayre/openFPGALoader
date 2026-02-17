# CMake Toolchain file for cross-compiling to Windows x64 using Clang + MinGW-w64
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-x86_64-w64-mingw32-clang.cmake ..

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(WINDOWS_TARGET_TRIPLE "x86_64-w64-mingw32")

find_program(CMAKE_C_COMPILER NAMES clang)
find_program(CMAKE_CXX_COMPILER NAMES clang++)
find_program(CMAKE_RC_COMPILER NAMES llvm-rc x86_64-w64-mingw32-windres)

if(NOT CMAKE_C_COMPILER OR NOT CMAKE_CXX_COMPILER)
    message(FATAL_ERROR "clang/clang++ not found. Please install clang.")
endif()

set(CMAKE_C_COMPILER_TARGET ${WINDOWS_TARGET_TRIPLE})
set(CMAKE_CXX_COMPILER_TARGET ${WINDOWS_TARGET_TRIPLE})

if(CMAKE_RC_COMPILER AND CMAKE_RC_COMPILER MATCHES "llvm-rc$")
    set(CMAKE_RC_COMPILER_TARGET ${WINDOWS_TARGET_TRIPLE})
endif()

# Keep target selection compatible with older CMake versions.
set(CMAKE_C_FLAGS_INIT "--target=${WINDOWS_TARGET_TRIPLE}")
set(CMAKE_CXX_FLAGS_INIT "--target=${WINDOWS_TARGET_TRIPLE}")

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
set(ENABLE_XILINX_VIRTUAL_CABLE OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC ON CACHE BOOL "" FORCE)
