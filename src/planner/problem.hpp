/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft.h"
#include "plfft_complex.hpp"
#include "runtime_precision.hpp"
#include <cstdint>
#include <type_traits>

// Avoid macOS `howmany(x, y)` macro collision.
#ifdef howmany
#undef howmany
#endif // howmany

namespace plfft {

struct problem {
  transform_kind kind;
  runtime_precision precision;
  int64_t n;
  int64_t istride;
  int64_t ostride;
  int64_t howmany;
  int64_t idist;
  int64_t odist;
  plfft_direction_t dir;

  bool operator==(const problem &other) const {
    // clang-format off
    return kind      == other.kind      &&
           precision == other.precision &&
           n         == other.n         &&
           istride   == other.istride   &&
           ostride   == other.ostride   &&
           howmany   == other.howmany   &&
           idist     == other.idist     &&
           odist     == other.odist     &&
           dir       == other.dir;
    // clang-format on
  }
};

template<typename Tx, typename Ty = Tx>
constexpr problem make_problem(int64_t n, int64_t istride, int64_t ostride,
                               int64_t howmany, int64_t idist, int64_t odist,
                               plfft_direction_t dir) {
  static_assert(std::is_same_v<remove_complex_t<Tx>, remove_complex_t<Ty>>,
                "mixed-precision problems are unsupported");
  using T = remove_complex_t<Tx>;
  constexpr auto kind = transform_kind_from_io_types<Tx, Ty>();
  constexpr auto prec = runtime_precision_from_real_type<T>();
  return {kind, prec, n, istride, ostride, howmany, idist, odist, dir};
}

} // namespace plfft
