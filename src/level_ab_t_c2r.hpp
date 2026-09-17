/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "kernel_data.hpp"
#include "level_data.hpp"
#include "plfft_util.hpp"

namespace plfft {

template<typename Tx, typename Ty>
struct level_ab_t<Tx, Ty, std::enable_if_t<is_c2r_v<Tx, Ty>>>
  : public level_data_direct_t<Tx, Ty> {
  using Tw = add_complex_t<Tx>;

private:
  fft_func_in_t<Tw> *kernel_n_ = nullptr;
  fft_func_it_t<Tw> *kernel_t_ = nullptr;
  const pod_vector<void> *twids_;
  algo_flops flops_;

  int64_t hm_;
  int64_t istride_;
  int64_t ostride_;
  int64_t idist_;
  int64_t odist_;

  bool want_premult_twiddles_;
  int twiddle_interleave_factor_;
  int64_t parallel_threshold_;

  // ireflect_n is the offset to the conjugated input element that the nbih
  // kernel will read from in reverse order.
  int64_t ireflect_n_;
  // ireflect_t is the offset to the conjugated input element that the tbih or
  // tbil kernel will read from in reverse order. This can be easily derived
  // from ireflect_n.
  int64_t ireflect_t_;
  int64_t n2halfhi_;

public:
  level_ab_t(int64_t n, int64_t n1, int64_t n2, int64_t howmany,
             int64_t istride, int64_t ostride, int64_t idist, int64_t odist,
             plfft_direction_t dir, bool want_sme);

  algo_flops flops() const override;

  void execute(const void *x, void *y) const override {
    this->execute(hm_, x, istride_, idist_, y, ostride_, odist_);
  };

  void execute(const int64_t howmany, const void *x, const int64_t istride,
               const int64_t idist, void *y, const int64_t ostride,
               const int64_t odist) const override;
}; // struct level_ab_t c2r

} // namespace plfft
