/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "kernel_data.hpp"
#include "kernel_function_pointers.hpp"
#include "plfft_complex.hpp"

using namespace plfft;

namespace plfft {

sme2_fft_func_n_t<float, complex_float, complex_float> plfft_256_sccnoh_tt_sme2;

sme2_fft_func_n_t<complex_half, complex_half, complex_half>
    plfft_16x_jjjn_tt_sme2;
sme2_fft_func_n_t<complex_half, complex_half, complex_half>
    plfft_16x_jjjn_uu_sme2;
sme2_fft_func_n_t<complex_half, complex_half, complex_half>
    plfft_16x_jjjn_tu_sme2;
sme2_fft_func_n_t<complex_half, complex_half, complex_half>
    plfft_16x_jjjn_ut_sme2;

sme2_fft_func_n_t<std::complex<float>, std::complex<float>, std::complex<float>>
    plfft_16x_cccn_tt_sme2;
sme2_fft_func_n_t<std::complex<float>, std::complex<float>, std::complex<float>>
    plfft_16x_cccn_uu_sme2;
sme2_fft_func_n_t<std::complex<float>, std::complex<float>, std::complex<float>>
    plfft_16x_cccn_tu_sme2;
sme2_fft_func_n_t<std::complex<float>, std::complex<float>, std::complex<float>>
    plfft_16x_cccn_ut_sme2;
} // namespace plfft
