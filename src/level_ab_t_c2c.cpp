/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "level_ab_t_c2c.hpp"
#include "fft_config.hpp"
#include "generate_twiddles.hpp"
#include "kernel_data.hpp"
#include "level_to_string.hpp"
#include "plfft_parallel.hpp"
#include "plfft_util.hpp"

namespace plfft {

template<typename Tx, typename Ty>
level_ab_t<Tx, Ty, std::enable_if_t<is_c2c_v<Tx, Ty>>>::level_ab_t(
    int64_t n, int64_t n1, int64_t n2, int64_t howmany, int64_t istride,
    int64_t ostride, int64_t idist, int64_t odist, plfft_direction_t dir,
    bool want_sme)
  : level_data_direct_t<Tx, Ty>(n, n1, n2),
    parallel_threshold_(fft_parallel_threshold) {
  std::tie(hm_, istride_, ostride_, idist_, odist_) =
      get_direct_t_level_params<Tx, Ty>(n, n1, n2, istride, ostride);

  const int strategy = config::get_kernel_strategy();

  // We know that howmany = 1 for the non-twiddled call. So, use uun kernel.
  auto k_n =
      get_kernel_data<Tx, Ty>(n1, 1, istride, ostride, idist_, odist_, dir,
                              strategy, order_kind::ORDER_NA, want_sme);
  auto k_t = get_kernel_data<Tx, Ty>(n1, std::nullopt, istride, ostride, idist_,
                                     odist_, dir, strategy,
                                     order_kind::ORDER_AB, want_sme);
  assert(k_n);
  assert(k_t);
  assert(k_t->twid_layout.interleave_factor > 0);
  want_premult_twiddles_ = k_t->twid_layout.want_premul;
  twiddle_interleave_factor_ = k_t->twid_layout.interleave_factor;
  twids_ = generate_twiddles(dir, n2, n1, k_t->twid_layout);

  kernel_n_ = k_n->ab_n.get_text_ptr();
  kernel_t_ = k_t->ab_t_dit.get_text_ptr();
  flops_ = k_n->ab_n.flops;
  flops_ += k_t->ab_t_dit.flops * (hm_ - 1);
}

template<typename Tx, typename Ty>
void level_ab_t<Tx, Ty, std::enable_if_t<is_c2c_v<Tx, Ty>>>::execute(
    const int64_t howmany, const void *x, const int64_t istride,
    const int64_t idist, void *y, const int64_t ostride,
    const int64_t odist) const {
  auto *X = (const Tw *)x;
  auto *Y = (Tw *)y;

  kernel_n_(X, Y, istride, ostride, 1, 0, 0);

  if (this->n_ >= parallel_threshold_) {
    const int premult_size_factor = this->want_premult_twiddles_ ? 2 : 1;
    const auto access_scale_factor = (this->n1_ - 1) * premult_size_factor;

    const auto nt = get_max_threads();
    const auto split = parallel::make_parallel_split(
        howmany - 1, this->twiddle_interleave_factor_, nt);

    auto exec = [this, split, X, Y, istride, idist, ostride, odist,
                 access_scale_factor](int thread_num) {
      const auto [start, work] = parallel::work_distribution(thread_num, split);
      kernel_t_(&X[(start + 1) * idist], &Y[(start + 1) * odist], istride,
                ostride, &(*twids_)[start * access_scale_factor], work, idist,
                odist);
    };

    parallel::parallel_loop(split.threads, exec);
  } else {
    kernel_t_(X + idist, Y + odist, istride, ostride, twids_->data(),
              howmany - 1, idist, odist);
  }
}

template<typename Tx, typename Ty>
algo_flops
level_ab_t<Tx, Ty, std::enable_if_t<is_c2c_v<Tx, Ty>>>::flops() const {
  return flops_;
}

template struct level_ab_t<complex_half, complex_half>;
template struct level_ab_t<complex_float, complex_float>;
template struct level_ab_t<complex_double, complex_double>;
#if PLFFT_ENABLE_FIXED_POINT
template struct level_ab_t<complex_int8_t, complex_int8_t>;
template struct level_ab_t<complex_int16_t, complex_int16_t>;
#endif // PLFFT_ENABLE_FIXED_POINT

} // namespace plfft
