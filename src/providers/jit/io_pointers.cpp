/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "io_pointers.hpp"

#include "expr.hpp"
#include "plfft_assert.hpp"

#include <iostream>

namespace plfft::wfta {

io_pointers::io_pointers(size_t in_begin, size_t in_end, size_t out_begin,
                         size_t out_end, size_t local_end)
  : in_begin(in_begin), in_end(in_end), out_begin(out_begin), out_end(out_end),
    local_end(local_end) {}

void io_pointers::print() const {
  std::cout << "in begin = " << in_begin << std::endl;
  std::cout << "in end = " << in_end << std::endl;
  std::cout << "out begin = " << out_begin << std::endl;
  std::cout << "out end = " << out_end << std::endl;
}

static bool is_in_ptr(const io_pointers &ioptr, size_t x) {
  return x >= ioptr.in_begin && x < ioptr.in_end;
}

static bool is_out_ptr(const io_pointers &ioptr, size_t x) {
  return x >= ioptr.out_begin && x < ioptr.out_end;
}

static bool is_inout_ptr(const io_pointers &ioptr, size_t x) {
  return is_in_ptr(ioptr, x) || is_out_ptr(ioptr, x);
}

bool is_in_ptr(const io_pointers &ioptr, const atom &x) {
  return x.kind == RT_PTR && is_in_ptr(ioptr, x.ival);
}

bool is_out_ptr(const io_pointers &ioptr, const atom &x) {
  return x.kind == RT_PTR && is_out_ptr(ioptr, x.ival);
}

bool is_local_ptr(const io_pointers &ioptr, const atom &x) {
  return x.kind == RT_PTR && !is_inout_ptr(ioptr, x.ival);
}

static int64_t get_in_ofs(const io_pointers &ioptr, size_t x) {
  ASSERT(is_in_ptr(ioptr, x));
  return x - ioptr.in_begin;
}

static int64_t get_out_ofs(const io_pointers &ioptr, size_t x) {
  ASSERT(is_out_ptr(ioptr, x));
  return x - ioptr.out_begin;
}

int64_t get_in_ofs(const io_pointers &ioptr, const atom &x) {
  ASSERT(x.kind == RT_PTR);
  return get_in_ofs(ioptr, x.ival);
}

int64_t get_out_ofs(const io_pointers &ioptr, const atom &x) {
  ASSERT(x.kind == RT_PTR);
  return get_out_ofs(ioptr, x.ival);
}

int64_t get_in_size_elems(const io_pointers &ioptr) {
  return ioptr.in_end - ioptr.in_begin;
}

int64_t get_out_size_elems(const io_pointers &ioptr) {
  return ioptr.out_end - ioptr.out_begin;
}

} // end namespace plfft::wfta
