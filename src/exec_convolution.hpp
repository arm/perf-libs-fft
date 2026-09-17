/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "fft_internal_plan.hpp"

namespace plfft {

/**
 * Perform pointwise multiplication of a work array by b.
 */
template<typename T>
void pointwise_multiply(T *__restrict__ work, const T *__restrict__ b,
                        int64_t n, int64_t howmany, int64_t stride);

template<typename T>
void pointwise_multiply(T *__restrict__ work, const T *__restrict__ b,
                        int64_t n);

template<typename T>
void exec_convolution(fft_internal_plan &pf, fft_internal_plan &pb, T *work,
                      const T *b, int64_t n);

template<typename T>
void exec_convolution(fft_internal_plan &pf, fft_internal_plan &pb, T *work,
                      const T *b, int64_t n, int64_t howmany, int64_t stride);

} // namespace plfft
