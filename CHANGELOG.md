# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/2.0.0/).

PLFFT releases use [Calendar Versioning](https://calver.org/). The deprecated
C++ interface maintains a separate compatibility version, reported by
`plfft::api_version()`.

## [Unreleased]

## [26.09] - 2026-09

Initial public release.

### Added

- C API for planning and repeatedly executing one-dimensional discrete Fourier
  transforms (DFTs) of arbitrary length on AArch64.
- Transforms:
  - Complex DFTs in both forward and backward directions.
  - Hermitian DFTs in both real-to-complex and complex-to-real directions.
  - Real-to-real transforms including DCT and DST types I-IV, discrete Hartley
    transform, and
    [FFTW half-complex format](https://www.fftw.org/fftw3_doc/The-Halfcomplex_002dformat-DFT.html).
- Data types:
  - FP16, emulated in the JIT backend using FP32 arithmetic and conversion when
    native FP16 arithmetic is unavailable at runtime.
  - FP32 and FP64.
  - Q7 and Q15 fixed-point with the AOT backend.
- Batched transforms with configurable input and output strides, distances, and
  aliasing.
- Choice of ahead-of-time or JIT kernel backends, with runtime detection of CPU
  capabilities and automatic selection of compatible kernels.
- Opt-in SME acceleration on supported systems.
- Optional OpenMP and Windows Arm64EC builds.

### Deprecated

- The C++ interface in `arm_fft1d.hpp`. This will be removed in a future
  release.
