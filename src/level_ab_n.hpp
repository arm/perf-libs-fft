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
struct level_ab_n : public level_data_base<Tx, Ty> {
private:
  fft_func_n_t<Tx, Ty> *kernel_n_ = nullptr;
  algo_flops flops_;

  int64_t hm_;
  int64_t istride_;
  int64_t ostride_;
  int64_t idist_;
  int64_t odist_;
  const int64_t parallel_threshold_;

public:
  level_ab_n(int64_t n, int64_t n1, int64_t n2, int64_t howmany,
             int64_t istride, int64_t ostride, int64_t idist, int64_t odist,
             plfft_direction_t dir, bool want_sme);

#ifndef NO_LIBCPP
  std::string level_to_string() const override;
#endif // NO_LIBCPP
  algo_flops flops() const override;

  void execute(const void *x, void *y) const override {
    this->execute(hm_, x, istride_, idist_, y, ostride_, odist_);
  };

  void execute(const int64_t howmany, const void *x, const int64_t istride,
               const int64_t idist, void *y, const int64_t ostride,
               const int64_t odist) const override;
}; // struct level_ab_n

} // namespace plfft
