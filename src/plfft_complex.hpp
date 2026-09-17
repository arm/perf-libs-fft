/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft_util.hpp"
#include <complex>
#include <type_traits>

typedef std::complex<half> complex_half;
typedef std::complex<float> complex_float;
typedef std::complex<double> complex_double;
typedef std::complex<int8_t> complex_int8_t;
typedef std::complex<int16_t> complex_int16_t;

namespace plfft {

template<typename T>
struct is_complex : public std::false_type {};

template<typename T>
struct is_complex<std::complex<T>> : public std::true_type {};

template<typename T>
struct is_complex<const std::complex<T>> : public std::true_type {};

template<typename T>
inline constexpr bool is_complex_v = is_complex<T>::value;

template<typename T1, typename T2>
inline constexpr bool is_r2c_v = !is_complex_v<T1> && is_complex_v<T2>;
template<typename T1, typename T2>
inline constexpr bool is_c2c_v = is_complex_v<T1> && is_complex_v<T2>;
template<typename T1, typename T2>
inline constexpr bool is_c2r_v = is_complex_v<T1> && !is_complex_v<T2>;
template<typename T1, typename T2>
inline constexpr bool is_r2r_v = !is_complex_v<T1> && !is_complex_v<T2>;

enum class transform_kind { c2c, r2c, c2r };

template<typename T1, typename T2>
constexpr transform_kind transform_kind_from_io_types() {
  static_assert(!is_r2r_v<T1, T2>, "planner does not support r2r transforms");

  if constexpr (is_r2c_v<T1, T2>) {
    return transform_kind::r2c;
  } else if constexpr (is_c2r_v<T1, T2>) {
    return transform_kind::c2r;
  } else {
    return transform_kind::c2c;
  }
}

template<typename T>
struct remove_complex {
  typedef T type;
};

template<typename T>
struct remove_complex<std::complex<T>> {
  typedef T type;
};

template<typename T>
using remove_complex_t = typename remove_complex<T>::type;
template<typename T>
using add_complex_t = std::complex<remove_complex_t<T>>;

template<typename T>
inline remove_complex_t<T> real(const T x) {
  return std::real(x);
}

inline half real(half x) {
  return std::real(float(x));
}

template<bool b, typename T>
struct add_complex_if {
  using type = add_complex_t<T>;
};

template<typename T>
struct add_complex_if<false, T> {
  using type = T;
};

template<bool b, typename T>
using add_complex_if_t = typename add_complex_if<b, T>::type;

static inline half conj(half x) {
  return x;
}

static inline float conj(float x) {
  return x;
}

static inline double conj(double x) {
  return x;
}

static inline std::complex<half> conj(std::complex<half> x) {
  return std::conj(x);
}

static inline std::complex<float> conj(std::complex<float> x) {
  return std::conj(x);
}

static inline std::complex<double> conj(std::complex<double> x) {
  return std::conj(x);
}

} // end namespace plfft
