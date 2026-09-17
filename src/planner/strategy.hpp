/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft/fft_plan.hpp"

#include <cassert>

namespace plfft {

class planner;
struct problem;

template<typename Tx, typename Ty>
class strategy {
public:
  virtual ~strategy() = default;
  virtual fft_plan_ptr make_plan(const problem &p,
                                 plfft::planner &planner) const = 0;

  void operator delete(void *) noexcept {
    // Define our own operator delete for polymorphic types to avoid
    // a dependency on it coming from the C++ runtime lib
    assert(false &&
           "Operator delete in strategy called. This should not be possible");
  }
};

} // namespace plfft
