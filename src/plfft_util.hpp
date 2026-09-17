/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft_assert.hpp"
#include "plfft_attrs.hpp"

#include <complex>
#include <cstdint>
#include <limits>
#include <type_traits>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef _WIN32
#define PLFFT_NO_UNIQUE_ADDRESS msvc::no_unique_address
#else
#define PLFFT_NO_UNIQUE_ADDRESS no_unique_address
#endif

// clang-format off
#define QUOTE(name) #name
#define STR(macro) QUOTE(macro)
// clang-format on

typedef __fp16 half;

// This pattern allows disabling thread_local if the toolchain doesn't support
// it.
#ifndef THREAD_LOCAL
#define THREAD_LOCAL thread_local
#endif

namespace plfft {

namespace consts {

template<typename FloatType>
constexpr FloatType pi = FloatType(3.1415926535897932385L);

} // end namespace consts

template<typename T>
struct at_least_fp32_type {
  typedef T type;
};

template<>
struct at_least_fp32_type<std::complex<half>> {
  typedef std::complex<float> type;
};

template<>
struct at_least_fp32_type<half> {
  typedef float type;
};

template<typename T>
using at_least_fp32_t = typename at_least_fp32_type<T>::type;

/**
 * int64_t multiply with signed-overflow check
 */
inline int64_t mul_assert_no_overflow(int64_t a, int64_t b) {
  constexpr auto min = std::numeric_limits<int64_t>::min();
  constexpr auto max = std::numeric_limits<int64_t>::max();
  if (a > 0) {
    ASSERT(b > 0 ? a <= max / b : b >= min / a);
  } else if (a < 0) {
    ASSERT(b > 0 ? a >= min / b : b == 0 || a >= max / b);
  }
  return a * b;
}

/**
 * int64_t addition with signed-overflow check
 */
inline int64_t add_assert_no_overflow(int64_t a, int64_t b) {
  constexpr auto min = std::numeric_limits<int64_t>::min();
  constexpr auto max = std::numeric_limits<int64_t>::max();
  ASSERT((b <= 0 || a <= max - b) && (b >= 0 || a >= min - b));
  return a + b;
}

/**
 * Rounds n UP to the nearest multiple of r, if n is not already a multiple
 */
template<typename T1, typename T2>
PLFFT_ALWAYS_INLINE std::common_type_t<T1, T2> iround(T1 n_in, T2 r_in) {
  using type = std::common_type_t<T1, T2>;
  const type n = n_in;
  const type r = r_in;

  const type diff = n % r;

  if (diff)
    return n - diff + r;

  return n;
}

/**
 * Rounds n UP to the nearest multiple of r, then divides by r.
 */
template<typename IntType1, typename IntType2>
PLFFT_ALWAYS_INLINE std::common_type_t<IntType1, IntType2>
iround_div(IntType1 n, IntType2 r) {
  return iround(n, r) / r;
}

inline bool get_nested() {
#ifdef _OPENMP
  // General note: Levels are indexed from zero; here, return true if the
  // current level is smaller than the max.
  return omp_get_active_level() < omp_get_max_active_levels();
#else
  return false;
#endif
}

inline int get_max_threads() {
#ifdef _OPENMP
  return get_nested() ? omp_get_max_threads() : 1;
#else
  return 1;
#endif
}

/**
 * Returns the min of the specified max value and
 * get_max_threads.
 */
inline int bounded_get_max_threads(int max) {
  // let's really avoid calling get_max_threads if we can at all help it!
  if (max <= 1)
    return 1;

  auto nt = get_max_threads();
  return max < nt ? max : nt;
}

} // end namespace plfft
