/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "level_ac_t_c2r.hpp"
#include "fft_config.hpp"
#include "generate_twiddles.hpp"
#include "kernel_data.hpp"
#include "level_to_string.hpp"
#include "plfft_parallel.hpp"
#include "plfft_util.hpp"

namespace plfft {

template<typename Tx, typename Ty>
level_ac_t<Tx, Ty, std::enable_if_t<is_c2r_v<Tx, Ty>>>::level_ac_t(
    int64_t n, int64_t n1, int64_t n2, int64_t howmany, int64_t istride,
    int64_t ostride, int64_t idist, int64_t odist, plfft_direction_t dir,
    bool want_sme)
  : level_data_direct_t<Tx, Ty>(n, n1, n2),
    parallel_threshold_(fft_parallel_threshold) {
  std::tie(hm_, istride_, ostride_, idist_, odist_) =
      get_direct_t_level_params<Tx, Ty>(n, n1, n2, istride, ostride);
  const auto hm_lev = n / (n1 * n2);
  const auto n1halfhi = n1 / 2 + 1;
  n2halfhi_ = n2 / 2 + 1;

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

  kernel_n_ = k_n->ab_nbih.get_text_ptr();
  flops_ = k_n->ab_nbih.flops * hm_;
  // If n1 is even, the last element of the complex input will always be
  // at the first column because (n1 * n2 / 2) mod n2 will always be 0.
  // In that case the tbil kernel will be used. On the other hand, when
  // n1 is odd, then the tbih kernel will be used.
  if (n1 % 2 == 0) {
    ireflect_n_ = (n1halfhi - 2) * n2 * hm_lev;
    ireflect_t_ = ireflect_n_ + (n2 - 1) * hm_lev;
    kernel_t_ = k_t->ac_tbil_dif.get_text_ptr();
    flops_ += k_t->ac_tbil_dif.flops * hm_ * (n2halfhi_ - 1);
  } else {
    ireflect_n_ = (n1halfhi - 1) * n2 * hm_lev;
    ireflect_t_ = ireflect_n_ - hm_lev;
    kernel_t_ = k_t->ac_tbih_dif.get_text_ptr();
    flops_ += k_t->ac_tbih_dif.flops * hm_ * (n2halfhi_ - 1);
  }

  ofs_mul_ = k_t->twid_layout.want_premul ? 2 : 1;
}

template<typename Tx, typename Ty>
void level_ac_t<Tx, Ty, std::enable_if_t<is_c2r_v<Tx, Ty>>>::execute(
    const int64_t howmany, const void *x, const int64_t istride,
    const int64_t idist, void *y, const int64_t ostride,
    const int64_t odist) const {
  auto *X = (const Tw *)x;
  auto *Y = (Tw *)y;
  kernel_n_(X, X + ireflect_n_, Y, istride, ostride, howmany, 1, 1);
  if (this->n_ >= parallel_threshold_) {
    const auto nt = get_max_threads();
    const auto split = parallel::make_parallel_split(n2halfhi_ - 1, 1, nt);

    const auto scale_factor = ofs_mul_ * (this->n1_ - 1);
    auto exec = [this, split, X, Y, istride, idist, ostride, odist,
                 scale_factor](int thread_num) {
      const auto [start, work] = parallel::work_distribution(thread_num, split);
      for (int64_t i = 0; i < work; ++i) {
        kernel_t_(&X[(start + 1 + i) * idist],
                  &X[ireflect_t_ - (start + i) * idist],
                  &Y[(start + 1 + i) * odist], istride, ostride,
                  &(*twids_)[(i + start) * scale_factor], hm_, 1, 1);
      }
    };
    parallel::parallel_loop(split.threads, exec);
  } else {
    for (int64_t i = 1; i < n2halfhi_; ++i) {
      const auto offsetx_T = i * idist;
      const auto offsety_T = i * odist;
      const auto offsetxx_T = ireflect_t_ - (i - 1) * idist;
      const auto offsetw = (i - 1) * ofs_mul_ * (this->n1_ - 1);
      kernel_t_(X + offsetx_T, X + offsetxx_T, Y + offsety_T, istride, ostride,
                &(*twids_)[offsetw], hm_, 1, 1);
    }
  }
}

template<typename Tx, typename Ty>
algo_flops
level_ac_t<Tx, Ty, std::enable_if_t<is_c2r_v<Tx, Ty>>>::flops() const {
  return flops_;
}

template struct level_ac_t<complex_half, half>;
template struct level_ac_t<complex_float, float>;
template struct level_ac_t<complex_double, double>;
#if PLFFT_ENABLE_FIXED_POINT
template struct level_ac_t<complex_int8_t, int8_t>;
template struct level_ac_t<complex_int16_t, int16_t>;
#endif // PLFFT_ENABLE_FIXED_POINT

} // namespace plfft
