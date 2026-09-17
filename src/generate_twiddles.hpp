/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft.h"
#include "plfft/unique_ptr.hpp"
#include "plfft_convert.hpp"
#include "plfft_pod_vector.hpp"
#include "plfft_static_hash_map.hpp"
#include "plfft_util.hpp"
#include "r2r_buffer_helper.hpp"
#include "twiddle_layout.hpp"

#include <cstdlib>
#include <iterator>
#include <tuple>

namespace plfft {

struct twiddle_data_key {
  plfft_direction_t dir;
  int64_t n1;
  int64_t n2;
  bool want_premul_twiddles;
  int twid_interleave_factor;
  bool include_unity_row;

  bool operator==(const twiddle_data_key &r) const {
    return std::tie(dir, n1, n2, want_premul_twiddles, twid_interleave_factor,
                    include_unity_row) ==
           std::tie(r.dir, r.n1, r.n2, r.want_premul_twiddles,
                    r.twid_interleave_factor, r.include_unity_row);
  }
};

struct r2r_rot_key {
  plfft_r2r_kind_t kind;
  int n;

  bool operator==(const r2r_rot_key &r) const {
    return std::tie(kind, n) == std::tie(r.kind, r.n);
  }
};

struct twiddle_data_key_hasher {
  size_t operator()(const twiddle_data_key &k) const {
    return 2 * (size_t(k.dir) * (k.n1 * (k.n2 << 32))) +
           (k.include_unity_row ? 1 : 0);
  }
};

struct r2r_rot_key_hasher {
  size_t operator()(const r2r_rot_key &k) const {
    return size_t(k.kind) | ((int64_t)k.n << 32);
  }
};

namespace internal {
// making this odd seems to help with even
// distribution of the keys, given the hash functions above
constexpr int hash_map_size = 31;
}; // namespace internal

/**
 * Returns a vector containing the twiddle factors for a given n1 & n2
 *     auto twids = generate_twiddles(dir, dec, n1, n2);
 */
template<typename ComplexType, typename RetType = void>
const pod_vector<RetType> *
generate_twiddles(plfft_direction_t dir, int64_t n1, int64_t n2,
                  bool want_premul_twiddles, int twid_interleave_factor,
                  bool include_unity_row = false) {
  assert(n1 > 0);
  assert(n2 > 0);
  assert(twid_interleave_factor > 0);
  THREAD_LOCAL
  static_hash_map<twiddle_data_key, plfft::unique_ptr<pod_vector<RetType>>,
                  internal::hash_map_size, twiddle_data_key_hasher>
      cached_twiddles;

  twiddle_data_key k{dir,
                     n1,
                     n2,
                     want_premul_twiddles,
                     twid_interleave_factor,
                     include_unity_row};

  auto it0 = cached_twiddles.find(k);
  if (it0) {
    return it0->get();
  }

  // Calculate twiddles in double and round only when storing them. This
  // prevents large transform lengths from overflowing narrow storage types.
  using real_t = typename ComplexType::value_type;
  const int64_t n = n1 * n2;
  auto ofs_mul = want_premul_twiddles ? 2 : 1;
  auto n1_rows = include_unity_row ? n1 : (n1 - 1);
  auto n1_r = iround(n1_rows, twid_interleave_factor);
  pod_vector<ComplexType> out(ofs_mul * n1_r * (n2 - 1));
  const double base = static_cast<int>(dir) * 2.0 * consts::pi<double> / n;
  int64_t x = 0;
  const int64_t j_start = include_unity_row ? 0 : 1;
  for (int64_t j = j_start; j < n1; j += twid_interleave_factor) {
    for (int64_t i = 1; i < n2; i++) {
      for (int64_t jj = 0; jj < twid_interleave_factor; jj++) {
        // Store W[k] = (r, i)
        // Also store W[k + 1] = (-i, r) to avoid
        // fneg-ext sequence in base case assembly
        const double input = base * i * (j + jj);
        const double a = (j + jj < n1 ? std::cos(input) : 0.0);
        const double b = (j + jj < n1 ? std::sin(input) : 0.0);
        if constexpr (std::is_integral_v<real_t>) {
          auto cvt = convert_to<real_t>{};
          out[x++] = {cvt(a), cvt(b)};
          if (want_premul_twiddles) {
            out[x++] = {cvt(-b), cvt(a)};
          }
        } else {
          out[x++] = ComplexType(a, b);
          if (want_premul_twiddles) {
            out[x++] = ComplexType(-b, a);
          }
        }
      }
    }
  }

  return cached_twiddles
      .insert(k, plfft::make_unique<pod_vector<RetType>>(std::move(out)))
      ->get();
}

inline const pod_vector<void> *generate_twiddles(plfft_direction_t dir,
                                                 int64_t n1, int64_t n2,
                                                 twiddle_layout layout) {
  assert(layout.interleave_factor > 0);
  switch (layout.precision) {
  case runtime_precision::fp16:
    return generate_twiddles<std::complex<half>>(
        dir, n1, n2, layout.want_premul, layout.interleave_factor,
        layout.include_unity_row);
  case runtime_precision::fp32:
    return generate_twiddles<std::complex<float>>(
        dir, n1, n2, layout.want_premul, layout.interleave_factor,
        layout.include_unity_row);
  case runtime_precision::fp64:
    return generate_twiddles<std::complex<double>>(
        dir, n1, n2, layout.want_premul, layout.interleave_factor,
        layout.include_unity_row);
  case runtime_precision::q0_7:
    return generate_twiddles<std::complex<int8_t>>(
        dir, n1, n2, layout.want_premul, layout.interleave_factor,
        layout.include_unity_row);
  case runtime_precision::q0_15:
    return generate_twiddles<std::complex<int16_t>>(
        dir, n1, n2, layout.want_premul, layout.interleave_factor,
        layout.include_unity_row);
  }
  assert(false);
  return nullptr;
}

template<plfft_r2r_kind_t R2RKind, typename T>
const pod_vector<std::complex<T>> *generate_r2r_rotation(int n) {
  /* Note the r2r pre- and post-rotations are not twiddle factors in the
     Cooley-Tukey sense, but they are included in this file as they have similar
     form to the twiddle factors and can be cached in the same way.  */
  THREAD_LOCAL static_hash_map<r2r_rot_key,
                               plfft::unique_ptr<pod_vector<std::complex<T>>>,
                               internal::hash_map_size, r2r_rot_key_hasher>
      cached_rotations;
  r2r_rot_key key{R2RKind, n};
  auto it0 = cached_rotations.find(key);
  if (it0) {
    return it0->get();
  }

  const auto n_rotations =
      std::min(r2r_buffer_helper<R2RKind>::fft_input_size(n),
               r2r_buffer_helper<R2RKind>::fft_output_size(n));
  pod_vector<std::complex<T>> W(n_rotations);

  /* The following is only correct for pre-rotations for DCT-[23] and
     post-rotation for DCT-4. Refer to r2r_buffer_helper and UDFHT definition to
     obtain the rotations for other transforms.  */
  for (size_t k = 0; k < W.size(); k++) {
    const T m = consts::pi<T> * k / (2 * n);
    W[k] = std::complex<T>(std::cos(m), -std::sin(m));
  }

  return cached_rotations
      .insert(key,
              plfft::make_unique<pod_vector<std::complex<T>>>(std::move(W)))
      ->get();
}

} // end namespace plfft
