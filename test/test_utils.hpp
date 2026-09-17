/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "arm_fft1d.hpp"

#include <arm_neon.h>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

template<typename Tx, typename Ty>
using Planner = plfft::fft_plan_ptr(int64_t n, int64_t howmany, int64_t istride,
                                    int64_t idist, int64_t ostride,
                                    int64_t odist, plfft_direction_t direction,
                                    plfft_io_alias_t inout_place);

template<typename T>
struct remove_complex {
  using type = T;
};

template<typename T>
struct remove_complex<std::complex<T>> {
  using type = T;
};

template<typename T>
using remove_complex_t = typename remove_complex<T>::type;

template<typename T>
inline remove_complex_t<T> real(const T &x) {
  return std::real(x);
}

inline __fp16 real(__fp16 x) {
  return std::real(float(x));
}

template<typename>
struct to_int_t {};

template<>
struct to_int_t<__fp16> {
  using t = uint16_t;
};

template<>
struct to_int_t<float> {
  using t = uint32_t;
};

template<>
struct to_int_t<double> {
  using t = uint64_t;
};

// Helpers to convert from double to other data types
template<typename ToType>
struct convert_to;

// to __fp16
template<>
struct convert_to<__fp16> {
  __fp16 operator()(const double x) const {
    return static_cast<__fp16>(x);
  }
};

// to float
template<>
struct convert_to<float> {
  float operator()(const double x) const {
    return static_cast<float>(x);
  }
};

// to double
template<>
struct convert_to<double> {
  double operator()(const double x) const {
    return x;
  }
};

// to Q0.7 (8-bit)
template<>
struct convert_to<int8_t> {
  int8_t operator()(const double x) const {
    return vqmovnh_s16(vqmovns_s32(vqmovnd_s64(vcvtd_n_s64_f64(x, 7))));
  }
};

// to Q0.15 (16-bit)
template<>
struct convert_to<int16_t> {
  int16_t operator()(const double x) const {
    return vqmovns_s32(vqmovnd_s64(vcvtd_n_s64_f64(x, 15)));
  }
};

template<typename T>
constexpr double eps() {
  union {
    typename to_int_t<T>::t ival;
    T fval;
  } val;

  val.fval = (T)1;
  ++val.ival;
  return val.fval - (T)1;
}

template<typename T>
  requires(!std::is_integral_v<T>)
T default_tol(int64_t n) {
  /*
    Machine epsilon scaled by:
    4        = flops per complex mul
    nlog2(n) = problem complexity
  */
  return eps<T>() * 4 * n * std::log2(n);
}

template<typename T>
  requires std::is_integral_v<T>
T default_tol(int64_t n) {
  /*
    A heuristic tolerance for fixed-point error, assuming:
    - n is a power of two
    - no overflow/saturation occurs
    - rounding to nearest is used
    - per-stage scaling is applied

    The tolerance is 1 LSB scaled by:
    6        = per-stage allowance for error introduced by complex muls/scaling
    log2(n)  = number of stages
  */
  assert(n > 0 && (n & (n - 1)) == 0);
  T lsb = 1;
  T log2n = 0;
  while (n >>= 1) {
    log2n++;
  }
  return 6 * lsb * log2n;
}

template<typename T>
struct result {
  bool success;
  std::conditional_t<std::is_integral_v<T>, int64_t, double> error;
};

template<typename T>
  requires(!std::is_integral_v<remove_complex_t<T>>)
result<remove_complex_t<T>> check(const T res, const T ref,
                                  remove_complex_t<T> tol) {
  // Use absolute error if reference is 0, otherwise use relative error
  const auto diff = std::abs(res - ref);
  const auto err = ref == T(0) ? diff : diff / std::abs(ref);
  return {err <= tol, err};
}

template<typename T>
  requires std::is_integral_v<remove_complex_t<T>>
result<remove_complex_t<T>> check(const T res, const T ref,
                                  remove_complex_t<T> tol) {
  // Use absolute error for fixed-point
  const auto err = std::abs(res - ref);
  return {err <= tol, err};
}

#define REQUIRE(condition, message)                                            \
  do {                                                                         \
    if (!(condition)) {                                                        \
      std::fprintf(stderr, "Requirement failed: %s\n", message);               \
      std::exit(EXIT_FAILURE);                                                 \
    }                                                                          \
  } while (0)
