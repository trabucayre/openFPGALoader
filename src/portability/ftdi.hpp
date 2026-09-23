#ifndef _SRC_PORTABILITY_FTDI_HPP_
#define _SRC_PORTABILITY_FTDI_HPP_
// libftdi uses `timeval`; it attempts to access this by pulling in
// <sys/time.h> on win32. This is incorrect, but generally works - however,
// with `WIN32_LEAN_AND_MEAN` (which prevents unrelated win32 root namespace
// pollution, e.g. a `byte` type), the correct include is needed
#if __has_include(<WinSock2.h>)
#include <WinSock2.h>
#endif

#include <ftdi.h>

#endif