/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <cstdint>
#include <memory>

namespace plfft::wfta {

typedef struct io_pointers {
  size_t in_begin;
  size_t in_end;
  size_t out_begin;
  size_t out_end;
  size_t local_end;

  io_pointers() = default;
  io_pointers(size_t in_begin, size_t in_end, size_t out_begin, size_t out_end,
              size_t local_end);
  void print() const;
} io_ptr_t;

struct atom;

bool is_in_ptr(const io_pointers &, const atom &);
bool is_out_ptr(const io_pointers &, const atom &);
bool is_local_ptr(const io_pointers &, const atom &);
int64_t get_in_ofs(const io_pointers &ioptr, const atom &x);
int64_t get_out_ofs(const io_pointers &ioptr, const atom &x);
int64_t get_in_size_elems(const io_pointers &ioptr);
int64_t get_out_size_elems(const io_pointers &ioptr);

} // namespace plfft::wfta
