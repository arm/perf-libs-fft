/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "level_ac_t_r2c.hpp"
#include "fft_config.hpp"
#include "generate_twiddles.hpp"
#include "kernel_data.hpp"
#include "level_to_string.hpp"
#include "plfft_parallel.hpp"
#include "plfft_util.hpp"

namespace plfft {

template<typename Tx, typename Ty>
level_ac_t<Tx, Ty, std::enable_if_t<is_r2c_v<Tx, Ty>>>::level_ac_t(
    int64_t n, int64_t n1, int64_t n2, int64_t howmany, int64_t istride,
    int64_t ostride, int64_t idist, int64_t odist, plfft_direction_t dir,
    bool want_sme)
  : level_data_direct_t<Tx, Ty>(n, n1, n2),
    parallel_threshold_(fft_parallel_threshold) {
  std::tie(hm_, istride_, ostride_, idist_, odist_) =
      get_direct_t_level_params<Tx, Ty>(n, n1, n2, istride, ostride);

  n2halflo_ = (n2 + 1) / 2;
  // the yy pointer starts at the bottom right of halflo data
  // and moves backwards along n2 (odist). The offset is one
  // less than n1/2 since we're zero indexed.
  yend_ = (n1 / 2 - 1) * ostride_ + n2 * odist_;

  const int strategy = config::get_kernel_strategy();

  // ac kernels are vectorized along hm_lev dimension. idist/odist is 1 and
  // hm_lev > 1 for mid-levels.
  auto k_n =
      get_kernel_data<Tx, Ty>(n1, std::nullopt, istride, ostride, 1, 1, dir,
                              strategy, order_kind::ORDER_NA, want_sme);
  auto k_t =
      get_kernel_data<Tx, Ty>(n1, std::nullopt, istride, ostride, 1, 1, dir,
                              strategy, order_kind::ORDER_AC, want_sme);
  assert(k_n);
  assert(k_t);
  assert(k_t->twid_layout.interleave_factor == 1);
  twids_ = generate_twiddles(dir, n2, n1, k_t->twid_layout);

  kernel_nfoh_ = k_n->ab_nfoh.get_text_ptr();
  kernel_tfj_ = k_t->ac_tfj_dit.get_text_ptr();
  flops_ = k_n->ab_nfoh.flops * hm_;
  flops_ += k_t->ac_tfj_dit.flops * hm_ * (n2halflo_ - 1);
  if (n2 % 2 == 0) {
    kernel_tfol_ = k_t->ac_tfol_dit.get_text_ptr();
    flops_ += k_t->ac_tfol_dit.flops * hm_;
  }

  ofs_mul_ = k_t->twid_layout.want_premul ? 2 : 1;
}

template<typename Tx, typename Ty>
void level_ac_t<Tx, Ty, std::enable_if_t<is_r2c_v<Tx, Ty>>>::execute(
    const int64_t howmany, const void *x, const int64_t istride,
    const int64_t idist, void *y, const int64_t ostride,
    const int64_t odist) const {
  auto *X = (const Tw *)x;
  auto *Y = (Tw *)y;

  kernel_nfoh_(X, Y, istride, ostride, howmany, 1, 1);
  if (this->n_ >= parallel_threshold_) {
    const auto nt = get_max_threads();
    const auto split = parallel::make_parallel_split(n2halflo_ - 1, 1, nt);
    const auto access_scale_factor = ofs_mul_ * (this->n1_ - 1);

    auto exec = [this, split, X, Y, istride, ostride, idist, odist,
                 access_scale_factor](int thread_num) {
      const auto [start, work] = parallel::work_distribution(thread_num, split);
      for (int64_t i = 0; i < work; ++i) {
        kernel_tfj_(&X[(start + 1 + i) * idist], &Y[(start + 1 + i) * odist],
                    &Y[yend_ - (start + i + 1) * odist], istride, ostride,
                    &(*twids_)[(i + start) * access_scale_factor], hm_, 1, 1);
      }
    };
    parallel::parallel_loop(split.threads, exec);
  } else {
    for (int64_t i = 1; i < n2halflo_; ++i) {
      const auto offsetx = i * idist;
      const auto offsety = i * odist;
      const auto offsetyy = yend_ - i * odist;
      const auto offsetw = (i - 1) * ofs_mul_ * (this->n1_ - 1);
      kernel_tfj_(X + offsetx, Y + offsety, Y + offsetyy, istride, ostride,
                  &(*twids_)[offsetw], hm_, 1, 1);
    }
  }

  if (kernel_tfol_) {
    const auto offsetx = n2halflo_ * idist;
    const auto offsety = n2halflo_ * odist;
    const auto offsetw = (n2halflo_ - 1) * ofs_mul_ * (this->n1_ - 1);
    kernel_tfol_(X + offsetx, Y + offsety, istride, ostride,
                 &(*twids_)[offsetw], howmany, 1, 1);
  }
}

template<typename Tx, typename Ty>
algo_flops
level_ac_t<Tx, Ty, std::enable_if_t<is_r2c_v<Tx, Ty>>>::flops() const {
  return flops_;
}

template struct level_ac_t<half, complex_half>;
template struct level_ac_t<float, complex_float>;
template struct level_ac_t<double, complex_double>;
#if PLFFT_ENABLE_FIXED_POINT
template struct level_ac_t<int8_t, complex_int8_t>;
template struct level_ac_t<int16_t, complex_int16_t>;
#endif // PLFFT_ENABLE_FIXED_POINT

} // namespace plfft
