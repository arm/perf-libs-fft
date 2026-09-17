/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft/algo_flops.hpp"
#include "plfft_assert.hpp"
#include <stddef.h>

namespace plfft {

/**
 * The pointers to the text and data memory areas for the particular kernel,
 * plus any additional metadata like flop counts.
 * This will be typically used with `F = void` inside WFTA, since we don't
 * actually care about the function signature.
 */
template<typename F>
struct kernel_registry_entry {
private:
  F *text_ptr = nullptr;

public:
  size_t text_len;
  const void *data_ptr;
  size_t data_len;

  algo_flops flops;

  kernel_registry_entry() = default;

  kernel_registry_entry(F *text_ptr_, algo_flops flops_)
    : text_ptr{text_ptr_}, text_len{0}, data_ptr{nullptr}, data_len{0},
      flops{flops_} {}

  kernel_registry_entry(F *text_ptr_, size_t text_len_, const void *data_ptr_,
                        size_t data_len_, algo_flops flops_)
    : text_ptr{text_ptr_}, text_len{text_len_}, data_ptr{data_ptr_},
      data_len{data_len_}, flops{flops_} {}

  explicit operator bool() const {
    return text_ptr != nullptr;
  }

  F *get_text_ptr() const {
    ASSERT(text_ptr != nullptr);
    return text_ptr;
  }
};

} // namespace plfft
