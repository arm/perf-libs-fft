/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "algo.hpp"
#include "winograd.hpp"

namespace plfft::wfta {

struct nfact_data {
  std::list<expr_t> algo;
  io_ptr_t iop;
};

/**
 * Generates a length-n FFT algorithm using WFTA.
 *
 * @param nx                factors of n.
 * @param use_alternative   use alternative (e.g. split-radix) kernel instead
 *                          of WFTA.
 */
nfact_data wfta_nfact(std::vector<int> nx, bool use_alternative);

} // namespace plfft::wfta
