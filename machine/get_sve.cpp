/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "cpu_features.hpp"
#include "plfft_util.hpp"
#include <complex>

namespace plfft {

template<typename T1, typename T2>
bool get_sve() {
  // This is redundant with jit_kernel_data
  return get_cpu_features().sve;
};

#define GET_SVE(Tx, Ty) template bool get_sve<Tx, Ty>();

GET_SVE(half, std::complex<half>)
GET_SVE(std::complex<half>, half)
GET_SVE(std::complex<half>, std::complex<half>)
GET_SVE(float, std::complex<float>)
GET_SVE(std::complex<float>, float)
GET_SVE(std::complex<float>, std::complex<float>)
GET_SVE(double, std::complex<double>)
GET_SVE(std::complex<double>, double)
GET_SVE(std::complex<double>, std::complex<double>)
GET_SVE(int8_t, std::complex<int8_t>)
GET_SVE(std::complex<int8_t>, int8_t)
GET_SVE(std::complex<int8_t>, std::complex<int8_t>)
GET_SVE(int16_t, std::complex<int16_t>)
GET_SVE(std::complex<int16_t>, int16_t)
GET_SVE(std::complex<int16_t>, std::complex<int16_t>)

#undef GET_SVE

} // namespace plfft
