/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "level_ab_t_r2c.hpp"
#include "fft_config.hpp"
#include "generate_twiddles.hpp"
#include "kernel_data.hpp"
#include "level_to_string.hpp"
#include "plfft_parallel.hpp"
#include "plfft_util.hpp"

namespace plfft {

template<typename Tx, typename Ty>
level_ab_t<Tx, Ty, std::enable_if_t<is_r2c_v<Tx, Ty>>>::level_ab_t(
    int64_t n, int64_t n1, int64_t n2, int64_t howmany, int64_t istride,
    int64_t ostride, int64_t idist, int64_t odist, plfft_direction_t dir,
    bool want_sme)
  : level_data_direct_t<Tx, Ty>(n, n1, n2),
    parallel_threshold_(fft_parallel_threshold) {
  std::tie(hm_, istride_, ostride_, idist_, odist_) =
      get_direct_t_level_params<Tx, Ty>(n, n1, n2, istride, ostride);
  n2halflo_ = (n2 + 1) / 2;
  yend_ = (n1 / 2 - 1) * ostride_ + n2 * odist_;

  const int strategy = config::get_kernel_strategy();

  // We know that howmany = 1 for the non-twiddled call. So, use uun kernel.
  auto k_n = get_kernel_data<Tx, Ty>(n1, 1, istride, ostride, 1, 1, dir,
                                     strategy, order_kind::ORDER_NA, want_sme);
  auto k_tfj = get_kernel_data<Tx, Ty>(n1, std::nullopt, istride, ostride,
                                       idist_, odist_, dir, strategy,
                                       order_kind::ORDER_AB, want_sme);
  // For the third call processing n2/2 + 1 column when n2 is even, howmany = 1
  // as well.
  auto k_tfh =
      get_kernel_data<Tx, Ty>(n1, 1, istride, ostride, 1, 1, dir, strategy,
                              order_kind::ORDER_AB, want_sme);
  assert(k_n);
  assert(k_tfj);
  assert(k_tfh);
  assert(k_tfj->twid_layout.interleave_factor > 0);
  assert(k_tfh->twid_layout.precision == k_tfj->twid_layout.precision);
  assert(k_tfh->twid_layout.interleave_factor > 0);

  // Parallel split partitions only the loop over tfj - tfol uses separate
  // twiddle table and computes offset independently.
  twiddle_interleave_factor_ = k_tfj->twid_layout.interleave_factor;
  twiddle_interleave_factor_tfh_ = k_tfh->twid_layout.interleave_factor;
  twids_tfj_ = generate_twiddles(dir, n2, n1, k_tfj->twid_layout);
  twids_tfh_ = generate_twiddles(dir, n2, n1, k_tfh->twid_layout);

  // We know that howmany = 1 for the non-twiddled call. So, use uun kernel.
  kernel_nfoh_ = k_n->ab_nfoh.get_text_ptr();
  kernel_tfj_ = k_tfj->ab_tfj_dit.get_text_ptr();
  flops_ = k_n->ab_nfoh.flops;
  flops_ += k_tfj->ab_tfj_dit.flops * (n2halflo_ - 1);
  if (hm_ % 2 == 0) {
    kernel_tfol_ = k_tfh->ab_tfol_dit.get_text_ptr();
    flops_ += k_tfh->ab_tfol_dit.flops;
  }

  ofs_mul_tfj_ = k_tfj->twid_layout.want_premul ? 2 : 1;
  ofs_mul_tfh_ = k_tfh->twid_layout.want_premul ? 2 : 1;
}

template<typename Tx, typename Ty>
void level_ab_t<Tx, Ty, std::enable_if_t<is_r2c_v<Tx, Ty>>>::execute(
    const int64_t howmany, const void *x, const int64_t istride,
    const int64_t idist, void *y, const int64_t ostride,
    const int64_t odist) const {
  auto *X = (const Tw *)x;
  auto *Y = (Tw *)y;
  const auto offsetx_tfj = idist;
  const auto offsety_tfj = odist;
  const auto offsetyy_tfj = yend_ - odist;

  kernel_nfoh_(X, Y, istride, ostride, 1, 0, 0);

  if (this->n_ >= parallel_threshold_) {
    const auto access_scale_factor = (this->n1_ - 1) * ofs_mul_tfj_;
    const auto nt = get_max_threads();
    const auto split = parallel::make_parallel_split(
        n2halflo_ - 1, twiddle_interleave_factor_, nt);

    auto exec = [this, split, X, Y, istride, ostride, idist, odist,
                 access_scale_factor, offsetyy_tfj](int thread_num) {
      const auto [start, work] = parallel::work_distribution(thread_num, split);
      kernel_tfj_(&X[(start + 1) * idist], &Y[(start + 1) * odist],
                  &Y[offsetyy_tfj - start * odist], istride, ostride,
                  &(*twids_tfj_)[start * access_scale_factor], work, idist,
                  odist);
    };
    parallel::parallel_loop(split.threads, exec);
  } else {
    kernel_tfj_(X + offsetx_tfj, Y + offsety_tfj, Y + offsetyy_tfj, istride,
                ostride, twids_tfj_->data(), n2halflo_ - 1, idist, odist);
  }

  if (kernel_tfol_) {
    const auto offsetx_tfh = n2halflo_ * idist;
    const auto offsety_tfh = n2halflo_ * odist;
    const auto twiddle_row_tfh = n2halflo_ - 1;
    // Convert the logical final row into the physical row/block position for
    // whichever twiddle layout the tfol kernel selected.
    const auto twiddle_block_tfh =
        twiddle_row_tfh / twiddle_interleave_factor_tfh_;
    const auto twiddle_block_row_tfh =
        twiddle_row_tfh % twiddle_interleave_factor_tfh_;
    const auto offsetw_tfh =
        ofs_mul_tfh_ *
        (twiddle_block_tfh * twiddle_interleave_factor_tfh_ * (idist - 1) +
         twiddle_block_row_tfh);
    kernel_tfol_(X + offsetx_tfh, Y + offsety_tfh, istride, ostride,
                 &(*twids_tfh_)[offsetw_tfh], 1, 0, 0);
  }
}

template<typename Tx, typename Ty>
algo_flops
level_ab_t<Tx, Ty, std::enable_if_t<is_r2c_v<Tx, Ty>>>::flops() const {
  return flops_;
}

template struct level_ab_t<half, complex_half>;
template struct level_ab_t<float, complex_float>;
template struct level_ab_t<double, complex_double>;
#if PLFFT_ENABLE_FIXED_POINT
template struct level_ab_t<int8_t, complex_int8_t>;
template struct level_ab_t<int16_t, complex_int16_t>;
#endif // PLFFT_ENABLE_FIXED_POINT

} // namespace plfft
