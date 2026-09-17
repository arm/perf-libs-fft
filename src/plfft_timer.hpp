/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <cfloat>

#if defined(_WIN32)
#include <windows.h>
#else
#include <ctime>
#endif

namespace plfft {

#if !defined(BAREMETAL)

#if defined(_WIN32)

using time_point_t = LARGE_INTEGER;

static inline time_point_t timer_start() {
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return t;
}

static inline double timer_end(time_point_t start_time) {
  LARGE_INTEGER end_time, freq;
  QueryPerformanceCounter(&end_time);
  QueryPerformanceFrequency(&freq);
  return static_cast<double>(end_time.QuadPart - start_time.QuadPart) /
         freq.QuadPart;
}

#else // POSIX

using time_point_t = timespec;

static inline time_point_t timer_start() {
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t;
}

static inline double timer_end(time_point_t start_time) {
  timespec end_time;
  clock_gettime(CLOCK_MONOTONIC, &end_time);
  double seconds = static_cast<double>(end_time.tv_sec - start_time.tv_sec) +
                   (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
  return seconds;
}

#endif // _WIN32

#else // BAREMETAL

using time_point_t = int;

static inline time_point_t timer_start() {
  return 0;
}

static inline double timer_end(time_point_t /*start_time*/) {
  return DBL_MAX;
}

#endif // !BAREMETAL

} // namespace plfft
