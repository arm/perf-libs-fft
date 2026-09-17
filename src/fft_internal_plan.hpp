/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft/fft_plan.hpp"

#include <cstdint>

namespace plfft {

class fft_internal_plan : public fft_plan {
public:
  using fft_plan::execute;

  // Temporary internal execution interface for the existing planner/runtime.
  // Remove this once all internal execution is routed through the new planner.
  virtual void execute(int64_t howmany, const void *in, void *out) const = 0;
};

using fft_internal_plan_ptr = plfft::unique_ptr<fft_internal_plan>;

} // namespace plfft
