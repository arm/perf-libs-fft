/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft_assert.hpp"

#include <ostream>
#include <vector>

namespace plfft::wfta {

/**
 * Represents the varieties of kernel we can generate.
 * This is used in the rtype structure.
 */
enum rtype_kind { RK_HALF = 1, RK_FLOAT, RK_DOUBLE, RK_Q0_7, RK_Q0_15 };

/**
 * A utility wrapper around rtype_kind.
 * Mainly useful for the bits and is_float methods.
 */
struct rtype {
  rtype_kind kind;

  constexpr rtype(rtype_kind k) : kind(k) {}

  constexpr int bits() const {
    switch (kind) {
    case RK_Q0_7:
      return 8;
    case RK_HALF:
    case RK_Q0_15:
      return 16;
    case RK_FLOAT:
      return 32;
    case RK_DOUBLE:
      return 64;
    }
    ASSERT(false);
  }

  constexpr bool is_float() const {
    switch (kind) {
    case RK_HALF:
    case RK_FLOAT:
    case RK_DOUBLE:
      return true;
    default:
      return false;
    }
  }
};

struct kernel_types_t {
  rtype x; ///< Input element type.
  rtype y; ///< Output element type.
  rtype w; ///< Twiddle/intermediate element type.
};

static inline bool operator==(rtype a, rtype b) {
  return a.kind == b.kind;
}

static inline bool operator<(rtype a, rtype b) {
  return a.kind < b.kind;
}

static inline bool operator!=(rtype a, rtype b) {
  return !(a == b);
}

/**
 * Lookup the corresponding rtype from the template argument, i.e.
 *   rtype_from_real_type<half>    => RK_HALF
 *   rtype_from_real_type<float>   => RK_FLOAT
 *   rtype_from_real_type<double>  => RK_DOUBLE
 *   rtype_from_real_type<int8_t>  => RK_Q0_7
 *   rtype_from_real_type<int16_t> => RK_Q0_15
 */
template<typename T>
struct rtype_from_real_type;

template<>
struct rtype_from_real_type<__fp16> {
  static inline constexpr rtype value = RK_HALF;
};

template<>
struct rtype_from_real_type<float> {
  static inline constexpr rtype value = RK_FLOAT;
};

template<>
struct rtype_from_real_type<double> {
  static inline constexpr rtype value = RK_DOUBLE;
};

template<>
struct rtype_from_real_type<int8_t> {
  static inline constexpr rtype value = RK_Q0_7;
};

template<>
struct rtype_from_real_type<int16_t> {
  static inline constexpr rtype value = RK_Q0_15;
};

/// Lookup the corresponding rtype from the template argument.
template<typename T>
static inline constexpr rtype rtype_from_real_type_v =
    rtype_from_real_type<T>::value;

} // end namespace plfft::wfta
