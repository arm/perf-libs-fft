/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft/algo_flops.hpp"
#include "plfft/unique_ptr.hpp"
#ifndef NO_LIBCPP
#include <string>
#endif // NO_LIBCPP
#include <utility>

#include <cassert>

namespace plfft {

class fft_plan {
public:
  virtual ~fft_plan() = default;
  virtual void execute(const void *in, void *out) const = 0;

#ifndef NO_LIBCPP
  virtual std::string plan_to_string() const = 0;
#endif // NO_LIBCPP
  virtual algo_flops flops() const = 0;

  void operator delete(void *ptr) noexcept {
    // Define our own operator delete for polymorphic types to avoid
    // a dependency on it coming from the C++ runtime lib
    assert(false &&
           "Operator delete in fft_plan called. This should not be possible");
  }
};

using fft_plan_ptr = unique_ptr<fft_plan>;

} // namespace plfft
