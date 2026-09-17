/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "kernel_data.hpp"
#include "plfft_kernels.hpp"

namespace plfft {

template<typename Tx, typename Ty>
struct acle_sme2_direct_kernel_data {
  using Tw = add_complex_t<Tx>;

  kernel_registry_entry<sme2_fft_func_n_t<Tx, Ty, Tw>> kernel;
  dist_types dist;
};

class acle_provider {

public:
  template<typename Tx, typename Ty>
  static std::optional<acle_sme2_direct_kernel_data<Tx, Ty>>
  get_sme2_direct_kernel_data(int64_t n, int64_t howmany, int64_t istride,
                              int64_t ostride, int64_t idist, int64_t odist);
};

} // namespace plfft
