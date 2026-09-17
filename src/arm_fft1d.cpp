/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "arm_fft1d_impl.hpp"
#include "plfft_util.hpp"

namespace plfft {

#define MAKE_BATCHED_1D_PLAN(Tx, Ty)                                           \
  template fft_plan_ptr make_batched_1d_plan<Tx, Ty>(                          \
      int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int,               \
      plfft_io_alias_t, plfft_r2r_kind_t, double, double);                     \
  template fft_plan_ptr make_batched_1d_plan<Tx, Ty>(                          \
      int64_t, const Tx *, Ty *, int64_t, int64_t, int64_t, int64_t, int64_t,  \
      int, plfft_r2r_kind_t, double, double);                                  \
  template fft_plan_ptr make_batched_1d_plan<Tx, Ty>(                          \
      int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, plfft_direction_t, \
      plfft_io_alias_t);
MAKE_BATCHED_1D_PLAN(half, half)
MAKE_BATCHED_1D_PLAN(half, std::complex<half>)
MAKE_BATCHED_1D_PLAN(std::complex<half>, half)
MAKE_BATCHED_1D_PLAN(std::complex<half>, std::complex<half>)
MAKE_BATCHED_1D_PLAN(float, float)
MAKE_BATCHED_1D_PLAN(float, std::complex<float>)
MAKE_BATCHED_1D_PLAN(std::complex<float>, float)
MAKE_BATCHED_1D_PLAN(std::complex<float>, std::complex<float>)
MAKE_BATCHED_1D_PLAN(double, double)
MAKE_BATCHED_1D_PLAN(double, std::complex<double>)
MAKE_BATCHED_1D_PLAN(std::complex<double>, double)
MAKE_BATCHED_1D_PLAN(std::complex<double>, std::complex<double>)
#if PLFFT_ENABLE_FIXED_POINT
MAKE_BATCHED_1D_PLAN(int8_t, std::complex<int8_t>)
MAKE_BATCHED_1D_PLAN(std::complex<int8_t>, int8_t)
MAKE_BATCHED_1D_PLAN(std::complex<int8_t>, std::complex<int8_t>)
MAKE_BATCHED_1D_PLAN(int16_t, std::complex<int16_t>)
MAKE_BATCHED_1D_PLAN(std::complex<int16_t>, int16_t)
MAKE_BATCHED_1D_PLAN(std::complex<int16_t>, std::complex<int16_t>)
#endif // PLFFT_ENABLE_FIXED_POINT
#undef MAKE_BATCHED_1D_PLAN

#ifdef PLFFT_ENABLE_SME
#define MAKE_BATCHED_1D_PLAN_SME(Tx, Ty)                                       \
  template fft_plan_ptr make_batched_1d_plan_sme<Tx, Ty>(                      \
      int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, plfft_direction_t, \
      plfft_io_alias_t);
MAKE_BATCHED_1D_PLAN_SME(half, std::complex<half>)
MAKE_BATCHED_1D_PLAN_SME(std::complex<half>, half)
MAKE_BATCHED_1D_PLAN_SME(std::complex<half>, std::complex<half>)
MAKE_BATCHED_1D_PLAN_SME(float, std::complex<float>)
MAKE_BATCHED_1D_PLAN_SME(std::complex<float>, float)
MAKE_BATCHED_1D_PLAN_SME(std::complex<float>, std::complex<float>)
MAKE_BATCHED_1D_PLAN_SME(double, std::complex<double>)
MAKE_BATCHED_1D_PLAN_SME(std::complex<double>, double)
MAKE_BATCHED_1D_PLAN_SME(std::complex<double>, std::complex<double>)
#undef MAKE_BATCHED_1D_PLAN_SME
#endif // PLFFT_ENABLE_SME

#define MAKE_BATCHED_1D_R2R_PLAN(T)                                            \
  template fft_plan_ptr make_batched_1d_r2r_plan<T>(                           \
      int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, plfft_r2r_kind_t,  \
      plfft_io_alias_t);
MAKE_BATCHED_1D_R2R_PLAN(half)
MAKE_BATCHED_1D_R2R_PLAN(float)
MAKE_BATCHED_1D_R2R_PLAN(double)
#undef MAKE_BATCHED_1D_R2R_PLAN

} // end namespace plfft
