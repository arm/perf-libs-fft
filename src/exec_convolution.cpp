/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "exec_convolution.hpp"

#include "plfft_complex.hpp"

namespace plfft {

template<typename T>
void pointwise_multiply(T *__restrict__ work, const T *__restrict__ b,
                        int64_t n) {
  for (int64_t j = 0; j < n; ++j) {
    work[j] *= b[j];
  }
}

template void
pointwise_multiply<std::complex<half>>(std::complex<half> *work,
                                       const std::complex<half> *b, int64_t n);
template void pointwise_multiply<std::complex<float>>(
    std::complex<float> *work, const std::complex<float> *b, int64_t n);
template void pointwise_multiply<std::complex<double>>(
    std::complex<double> *work, const std::complex<double> *b, int64_t n);

template<typename T>
void pointwise_multiply(T *__restrict__ work, const T *__restrict__ b,
                        int64_t n, int64_t howmany, int64_t stride) {
  if (howmany == 1 && stride == 1) {
    pointwise_multiply(work, b, n);
    return;
  }

  for (int64_t j = 0; j < n; ++j) {
    auto b_elem = b[j];
    for (int64_t i = 0; i < howmany; ++i) {
      work[j * stride + i] *= b_elem;
    }
  }
}

template void
pointwise_multiply<std::complex<half>>(std::complex<half> *work,
                                       const std::complex<half> *b, int64_t n,
                                       int64_t howmany, int64_t stride);
template void
pointwise_multiply<std::complex<float>>(std::complex<float> *work,
                                        const std::complex<float> *b, int64_t n,
                                        int64_t howmany, int64_t stride);
template void pointwise_multiply<std::complex<double>>(
    std::complex<double> *work, const std::complex<double> *b, int64_t n,
    int64_t howmany, int64_t stride);

template<typename T>
void exec_convolution(fft_internal_plan &pf, fft_internal_plan &pb, T *work,
                      const T *b, int64_t n) {
  // do forwards FFT!
  pf.execute(work, work);

  // do pointwise multiplication by b
  pointwise_multiply(work, b, n);

  // do backwards FFT!
  pb.execute(work, work);
}

template void exec_convolution<std::complex<half>>(fft_internal_plan &pf,
                                                   fft_internal_plan &pb,
                                                   std::complex<half> *work,
                                                   const std::complex<half> *b,
                                                   int64_t n);
template void exec_convolution<std::complex<float>>(
    fft_internal_plan &pf, fft_internal_plan &pb, std::complex<float> *work,
    const std::complex<float> *b, int64_t n);
template void exec_convolution<std::complex<double>>(
    fft_internal_plan &pf, fft_internal_plan &pb, std::complex<double> *work,
    const std::complex<double> *b, int64_t n);

template<typename T>
void exec_convolution(fft_internal_plan &pf, fft_internal_plan &pb, T *work,
                      const T *b, int64_t n, int64_t howmany, int64_t stride) {
  if (howmany == 1 && stride == 1) {
    exec_convolution(pf, pb, work, b, n);
    return;
  }

  // do forwards FFT!
  pf.execute(howmany, work, work);

  // do pointwise multiplication by b
  pointwise_multiply(work, b, n, howmany, stride);

  // do backwards FFT!
  pb.execute(howmany, work, work);
}

template void exec_convolution<std::complex<half>>(
    fft_internal_plan &pf, fft_internal_plan &pb, std::complex<half> *work,
    const std::complex<half> *b, int64_t n, int64_t howmany, int64_t stride);
template void exec_convolution<std::complex<float>>(
    fft_internal_plan &pf, fft_internal_plan &pb, std::complex<float> *work,
    const std::complex<float> *b, int64_t n, int64_t howmany, int64_t stride);
template void exec_convolution<std::complex<double>>(
    fft_internal_plan &pf, fft_internal_plan &pb, std::complex<double> *work,
    const std::complex<double> *b, int64_t n, int64_t howmany, int64_t stride);

} // namespace plfft
