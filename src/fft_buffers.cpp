/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "fft_buffers.hpp"
#include "plfft_complex.hpp"
#include <array>

namespace plfft {

// If a prime (Bluestein/Rader) plan is part of a larger composite plan
// and the prime plan itself involves a composite sub-plan, then we may
// need 2 active buffers, but no more.
THREAD_LOCAL std::array<buffer_type, 2> buffer_compositor;
THREAD_LOCAL int comp_buf_active = -1;

static buffer_type &get_buffer(buffer_name id) {
  switch (id) {
  case buffer_name::compositor:
    return buffer_compositor[comp_buf_active];
  case buffer_name::bluestein:
    THREAD_LOCAL buffer_type buffer_bluestein;
    return buffer_bluestein;
  case buffer_name::rader:
    THREAD_LOCAL buffer_type buffer_rader;
    return buffer_rader;
  case buffer_name::r2r_in:
    THREAD_LOCAL buffer_type buffer_r2r_in;
    return buffer_r2r_in;
  case buffer_name::r2r_out:
    THREAD_LOCAL buffer_type buffer_r2r_out;
    return buffer_r2r_out;
  case buffer_name::batched:
    THREAD_LOCAL buffer_type buffer_batched;
    return buffer_batched;
  case buffer_name::sme2_direct:
    THREAD_LOCAL buffer_type buffer_sme2_direct;
    return buffer_sme2_direct;
  }
  assert(false);
  __builtin_unreachable();
}

void release_compositor_buf() {
  comp_buf_active--;
}

template<typename T>
T *get_memory(buffer_name id, std::size_t i) {
  if (id == buffer_name::compositor) {
    comp_buf_active++;
    assert(comp_buf_active == 0 || comp_buf_active == 1);
  }

  // reference to THREAD_LOCAL buffer
  auto &buffer = get_buffer(id);

  i *= sizeof(T);

  if (std::size(buffer) < i) {
    // assign rather than resize as we want to maintain the alignment -
    // shouldn't be called often
    buffer = buffer_type(i);
  }
  return reinterpret_cast<T *>(std::data(buffer));
}

#define GET_MEMORY(T) template T *get_memory(buffer_name id, std::size_t i)

GET_MEMORY(half);
GET_MEMORY(std::complex<half>);
GET_MEMORY(float);
GET_MEMORY(std::complex<float>);
GET_MEMORY(double);
GET_MEMORY(std::complex<double>);
#if PLFFT_ENABLE_FIXED_POINT
GET_MEMORY(int8_t);
GET_MEMORY(std::complex<int8_t>);
GET_MEMORY(int16_t);
GET_MEMORY(std::complex<int16_t>);
#endif // PLFFT_ENABLE_FIXED_POINT

#undef GET_MEMORY

void free_buffer(buffer_name id) {
  if (id == buffer_name::compositor) {
    assert(comp_buf_active == -1);
    for (auto &buf : buffer_compositor) {
      buffer_type().swap(buf);
    }
  } else {
    buffer_type().swap(get_buffer(id));
  }
}

} // end namespace plfft
