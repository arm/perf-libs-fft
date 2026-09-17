/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "level_ab_n.hpp"
#include "fft_config.hpp"
#include "kernel_data.hpp"
#include "level_to_string.hpp"
#include "plfft_parallel.hpp"
#include "plfft_util.hpp"

namespace plfft {

template<typename Tx, typename Ty>
level_ab_n<Tx, Ty>::level_ab_n(int64_t n, int64_t n1, int64_t n2,
                               int64_t howmany, int64_t istride,
                               int64_t ostride, int64_t idist, int64_t odist,
                               plfft_direction_t dir, bool want_sme)
  : level_data_base<Tx, Ty>(n, n1, n2),
    parallel_threshold_(fft_parallel_threshold) {
  std::tie(hm_, istride_, ostride_, idist_, odist_) =
      get_n_kernel_params<Tx, Ty>(n, n1, n2, howmany, istride, ostride, idist,
                                  odist);
  const int strategy = config::get_kernel_strategy();
  const auto kernels = get_kernel_data<Tx, Ty>(
      n1, std::nullopt, istride_, ostride_, idist_, odist_, dir, strategy,
      order_kind::ORDER_NA, want_sme);
  assert(kernels);
  kernel_n_ = kernels->ab_n.get_text_ptr();
  flops_ = kernels->ab_n.flops * hm_;
}

#ifndef NO_LIBCPP
template<typename Tx, typename Ty>
std::string level_ab_n<Tx, Ty>::level_to_string() const {
  return level_to_string_direct(this->n1_, this->n2_, this->howmany_);
}
#endif // NO_LIBCPP

template<typename Tx, typename Ty>
void level_ab_n<Tx, Ty>::execute(const int64_t howmany, const void *x,
                                 const int64_t istride, const int64_t idist,
                                 void *y, const int64_t ostride,
                                 const int64_t odist) const {
  auto *X = (const Tx *)x;
  auto *Y = (Ty *)y;

  if (this->n_ >= parallel_threshold_) {
    const auto nt = get_max_threads();
    const auto split = parallel::make_parallel_split(howmany, 1, nt);

    auto exec = [this, split, X, Y, istride, idist, ostride,
                 odist](int thread_num) {
      const auto [start, work] = parallel::work_distribution(thread_num, split);
      kernel_n_(&X[start * idist], &Y[start * odist], istride, ostride, work,
                idist, odist);
    };

    parallel::parallel_loop(split.threads, exec);
  } else {
    kernel_n_(X, Y, istride, ostride, howmany, idist, odist);
  }
}

template<typename Tx, typename Ty>
algo_flops level_ab_n<Tx, Ty>::flops() const {
  return flops_;
};

template struct level_ab_n<half, complex_half>;
template struct level_ab_n<complex_half, half>;
template struct level_ab_n<complex_half, complex_half>;
template struct level_ab_n<float, complex_float>;
template struct level_ab_n<complex_float, float>;
template struct level_ab_n<complex_float, complex_float>;
template struct level_ab_n<double, complex_double>;
template struct level_ab_n<complex_double, double>;
template struct level_ab_n<complex_double, complex_double>;
#if PLFFT_ENABLE_FIXED_POINT
template struct level_ab_n<int8_t, complex_int8_t>;
template struct level_ab_n<complex_int8_t, int8_t>;
template struct level_ab_n<complex_int8_t, complex_int8_t>;
template struct level_ab_n<int16_t, complex_int16_t>;
template struct level_ab_n<complex_int16_t, int16_t>;
template struct level_ab_n<complex_int16_t, complex_int16_t>;
#endif // PLFFT_ENABLE_FIXED_POINT

} // namespace plfft
