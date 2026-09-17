#ifndef _SRC_PORTABILITY_UNISTD_HPP
#define _SRC_PORTABILITY_UNISTD_HPP

#if __has_include(<unistd.h>)
#include "portability/unistd.hpp"
#elif defined(_MSC_VER)

#include <chrono>
#include <thread>

#include <io.h>

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif

using useconds_t = unsigned long long;

static int usleep(const useconds_t usec) {
  if (usec < 0) {
    _set_errno(EINVAL);
    return -1;
  }
  if (usec == 0) {
    return 0;
  }
  std::this_thread::sleep_for(std::chrono::microseconds(usec));
  return 0;
}

static int sleep(const unsigned int seconds) {
  std::this_thread::sleep_for(std::chrono::seconds(seconds));
  return 0;
}

#endif

#endif