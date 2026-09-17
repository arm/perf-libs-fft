/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "bluestein.hpp"
#include "level_to_string.hpp"
#include "plfft.h"
#include "plfft/algo_flops.hpp"
#include "plfft/unique_ptr.hpp"
#include "rader.hpp"

#include <cassert>

namespace plfft {

/*
 * This default value has been set based on
 * performance analysis for c2c transforms
 * on N1, V1, V2.
 *
 * Threshold detection for parallelism in FFT
 * could be moved to planning stage in the future.
 */
static constexpr int64_t fft_parallel_threshold = 10000;

enum class level_type {
  SINGLE = 1,
  INPUT,
  INTERNAL,
  OUTPUT,
};

// Return { howmany, istride, ostride, idist, odist } for use in execute
// function.
template<typename Tx, typename Ty>
static std::tuple<int64_t, int64_t, int64_t, int64_t, int64_t>
get_n_kernel_params(const int64_t n, const int64_t n1, const int64_t n2,
                    const int64_t howmany, const int64_t istride,
                    const int64_t ostride, const int64_t idist,
                    const int64_t odist) {
  if (n == n1 && n2 == 1) {
    // This is single-level rader/bluestein.
    // Use original howmany/strides/dists.
    return {howmany, istride, ostride, idist, odist};
  } else {
    // For non-twiddled level in multi-level transforms, we merge n2 with
    // howmany level into single call. hm = hm_lev * n2 = n / (n1 * n2) * n2
    auto hm = n / n1;
    if constexpr (is_dit_v<Tx, Ty>) {
      return {hm, istride * hm, hm, istride, 1};
    } else {
      return {hm, hm, ostride * hm, 1, ostride};
    }
  }
}

// Return { howmany, istride, ostride, idist, odist } for use in execute
// function for direct level.
template<typename Tx, typename Ty>
static std::tuple<int64_t, int64_t, int64_t, int64_t, int64_t>
get_direct_t_level_params(const int64_t n, const int64_t n1, const int64_t n2,
                          const int64_t istride, const int64_t ostride) {
  const auto hm_lev = n / (n1 * n2);
  if (hm_lev != 1) {
    // This is an internal level. ac kernels are used.
    if constexpr (is_dit_v<Tx, Ty>) {
      return {hm_lev, hm_lev, hm_lev * n2, hm_lev * n1, hm_lev};
    } else {
      return {hm_lev, hm_lev * n2, hm_lev, hm_lev, hm_lev * n1};
    }
  } else {
    // Input or output twiddled levels use ab kernels.
    if constexpr (is_dit_v<Tx, Ty>) {
      return {n2, 1, ostride * n2, n1, ostride};
    } else {
      // We have (n2 / 2 + 1) columns we need to compute for twiddled level.
      const auto n2halfhi = n2 / 2 + 1;
      return {n2halfhi, istride * n2, 1, istride, n1};
    }
  }
}

// Return { howmany, istride, ostride, idist, odist } for use in execute
// function for rader or bluestein level.
template<typename Tx, typename Ty>
static std::tuple<int64_t, int64_t, int64_t, int64_t, int64_t>
get_prime_t_level_params(const int64_t n, const int64_t n1, const int64_t n2,
                         const int64_t istride, const int64_t ostride) {
  const auto hm_lev = n / (n1 * n2);
  if (hm_lev != 1) {
    // Internal twiddled levels derive their strides from the packed
    // intermediate buffer layout, not from caller strides.
    if constexpr (is_dit_v<Tx, Ty>) {
      return {n2, hm_lev, hm_lev * n2, hm_lev * n1, hm_lev};
    } else {
      return {n2, hm_lev * n2, hm_lev, hm_lev, hm_lev * n1};
    }
  } else {
    // Input or output twiddled levels use caller strides.
    if constexpr (is_dit_v<Tx, Ty>) {
      return {n2, hm_lev, ostride * hm_lev * n2, hm_lev * n1, hm_lev * ostride};
    } else {
      return {n2, istride * hm_lev * n2, hm_lev, hm_lev * istride, hm_lev * n1};
    }
  }
}

/**
 * Struct containing the values used in each level of the fft.
 *
 * For example if n=30, there will be a level for 5,3,2.
 *
 * All precomputed values unique to a level, such as twiddle factors, are
 * stored in this struct.
 */
template<typename Tx, typename Ty>
struct level_data_base {
protected:
  int64_t n_;
  int64_t n1_;
  int64_t n2_;
  int64_t howmany_; // howmany of this level = n / (n1 * n2)

public:
  level_data_base() = default;
  virtual ~level_data_base() = default;
  level_data_base(const level_data_base &) = delete;
  level_data_base(level_data_base &&) = default;
  level_data_base &operator=(const level_data_base &) = delete;
  level_data_base &operator=(level_data_base &&) = default;

  level_data_base(decltype(n_) n, decltype(n1_) n1, decltype(n2_) n2)
    : n_{n}, n1_{n1}, n2_{n2}, howmany_{n / (n1 * n2)} {}

#ifndef NO_LIBCPP
  virtual std::string level_to_string() const = 0;
#endif // NO_LIBCPP
  virtual algo_flops flops() const = 0;
  virtual void execute(const void *x, void *y) const = 0;
  virtual void execute(const int64_t howmany, const void *x,
                       const int64_t istride, const int64_t idist, void *y,
                       const int64_t ostride, const int64_t odist) const = 0;

  void operator delete(void *ptr) noexcept {
    // Define our own operator delete for polymorphic types to avoid
    // a dependency on it coming from the C++ runtime lib
    assert(false && "Operator delete in level_data_base called. This should "
                    "not be possible");
  }

}; // struct level_data_base

template<typename Tx, typename Ty>
using level_data_ptr = plfft::unique_ptr<level_data_base<Tx, Ty>>;

template<typename Tx, typename Ty>
struct level_data_direct_t : public level_data_base<Tx, Ty> {
  level_data_direct_t(int64_t n, int64_t n1, int64_t n2)
    : level_data_base<Tx, Ty>(n, n1, n2) {}

#ifndef NO_LIBCPP
  std::string level_to_string() const override {
    return level_to_string_direct(this->n1_, this->n2_, this->howmany_);
  }
#endif // NO_LIBCPP
}; // struct level_data_direct_t

template<typename Tx, typename Ty, typename = void>
struct level_ab_t;

template<typename Tx, typename Ty, typename = void>
struct level_abx_t;

template<typename Tx, typename Ty, typename = void>
struct level_ac_t;

struct level_data_info {
  bool used_rader;
};

} // namespace plfft
