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
struct level_ac_t<Tx, Ty, std::enable_if_t<is_r2c_v<Tx, Ty>>>
  : public level_data_direct_t<Tx, Ty> {
  using Tw = add_complex_t<Tx>;

private:
  fft_func_n_t<Tw, Tw> *kernel_nfoh_ = nullptr;
  fft_func_j_t<Tw> *kernel_tfj_ = nullptr;
  fft_func_t_t<Tw> *kernel_tfol_ = nullptr;
  const pod_vector<void> *twids_;
  algo_flops flops_;

  int64_t hm_;
  int64_t istride_;
  int64_t ostride_;
  int64_t idist_;
  int64_t odist_;
  const int parallel_threshold_;

  // n2halflo is the number of rows excluding the last outputted by non-twiddled
  // level. This equals to howmany for nfoh (which is 1) plus howmany for tfj
  // kernels.
  int64_t n2halflo_;
  // yend is the offset to the last output element of the nfoh kernel call (for
  // the first row). This will be used for calculating the offset for yy pointer
  // for outputting conjugated elements in reverse order.
  int64_t yend_;

  int64_t ofs_mul_;

public:
  level_ac_t(int64_t n, int64_t n1, int64_t n2, int64_t howmany,
             int64_t istride, int64_t ostride, int64_t idist, int64_t odist,
             plfft_direction_t dir, bool want_sme);

  algo_flops flops() const override;

  void execute(const void *x, void *y) const override {
    this->execute(this->howmany_, x, istride_, idist_, y, ostride_, odist_);
  };

  void execute(const int64_t howmany, const void *x, const int64_t istride,
               const int64_t idist, void *y, const int64_t ostride,
               const int64_t odist) const override;
}; // struct level_ac_t r2c

} // namespace plfft
