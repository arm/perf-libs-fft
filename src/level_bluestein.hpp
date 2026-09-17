/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "level_data.hpp"

namespace plfft {

template<typename Tx, typename Ty>
struct level_bluestein_n : public level_data_base<Tx, Ty> {
private:
  bluestein<Tx, Ty> bs_;

  int64_t hm_;
  int64_t istride_;
  int64_t ostride_;
  int64_t idist_;
  int64_t odist_;

public:
  level_bluestein_n(int64_t n, int64_t n1, int64_t n2, int64_t howmany,
                    int64_t istride, int64_t ostride, int64_t idist,
                    int64_t odist, decltype(bs_) bs)
    : level_data_base<Tx, Ty>(n, n1, n2), bs_(std::move(bs)) {
    std::tie(hm_, istride_, ostride_, idist_, odist_) =
        get_n_kernel_params<Tx, Ty>(n, n1, n2, howmany, istride, ostride, idist,
                                    odist);
  }

#ifndef NO_LIBCPP
  std::string level_to_string() const override {
    return bs_.plan_to_string();
  }
#endif // NO_LIBCPP

  algo_flops flops() const override {
    // TODO: not implemented yet...
    return {};
  }

  void execute(const void *x, void *y) const override {
    this->execute(hm_, x, istride_, idist_, y, ostride_, odist_);
  };

  void execute(const int64_t howmany, const void *x, const int64_t istride,
               const int64_t idist, void *y, const int64_t ostride,
               const int64_t odist) const override;
}; // struct level_bluestein_n

template<typename Tx, typename Ty>
struct level_bluestein_t : public level_data_base<Tx, Ty> {
private:
  const pod_vector<void> *twids_;
  bluestein<Tx, Ty> bs_;

  int64_t hm_;
  int64_t istride_;
  int64_t ostride_;
  int64_t idist_;
  int64_t odist_;

public:
  level_bluestein_t(int64_t n, int64_t n1, int64_t n2, int64_t howmany,
                    int64_t istride, int64_t ostride, int64_t idist,
                    int64_t odist, decltype(twids_) twids, decltype(bs_) bs)
    : level_data_base<Tx, Ty>(n, n1, n2), twids_{std::move(twids)},
      bs_(std::move(bs)) {
    std::tie(hm_, istride_, ostride_, idist_, odist_) =
        get_prime_t_level_params<Tx, Ty>(n, n1, n2, istride, ostride);
  }

#ifndef NO_LIBCPP
  std::string level_to_string() const override {
    return bs_.plan_to_string();
  }
#endif // NO_LIBCPP

  algo_flops flops() const override {
    // TODO: not implemented yet...
    return {};
  }

  void execute(const void *x, void *y) const override {
    this->execute(this->howmany_, x, istride_, idist_, y, ostride_, odist_);
  };

  void execute(const int64_t howmany, const void *x, const int64_t istride,
               const int64_t idist, void *y, const int64_t ostride,
               const int64_t odist) const override;
}; // struct level_bluestein_t

} // namespace plfft
