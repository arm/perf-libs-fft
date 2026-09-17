/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "fft_internal_plan.hpp"
#include "plfft/unique_ptr.hpp"
#include "plfft_pod_vector.hpp"
#include "r2r_buffer_helper.hpp"

namespace plfft {

/**
 * Wrapper class for real-to-real transforms such as discrete sine and cosine
 * transforms. Such transforms can be implemented by an FFT run on a permuted
 * input array, as described in the below paper.
 * @see Oraintara, S., 2002, May. The unified discrete Fourier-Hartley
 * transforms: Theory and structure. In 2002 IEEE International Symposium on
 * Circuits and Systems. Proceedings (Cat. No. 02CH37353) (Vol. 3, pp. III-III).
 * IEEE.
 * @tparam TransformType The transform to implement (for example DCT type-1)
 * @tparam T The datatype used in the transform (e.g. half, double)
 */
template<plfft_r2r_kind_t TransformType, typename T>
class r2r_plan : public fft_internal_plan {
public:
  r2r_plan() = delete;

  /**
   * @param [in]  n                 The size of the transform being represented.
   *                                This will probably be different to the size
   *                                of the FFT which is actually used
   * @param [in]  howmany           Planned batch size
   * @param [in]  istride           Stride of the input array
   * @param [in]  idist             Distance between input transforms in a batch
   * @param [in]  ostride           Stride of the output array
   * @param [in]  odist             Distance between output transforms in a
   *                                batch
   * @param [in]  target_secs_total Timing budget for plan auditioning
   * @param [in]  margin            Auditioning margin
   */
  r2r_plan(int n, int64_t howmany, int64_t istride, int64_t idist,
           int64_t ostride, int64_t odist, double target_secs_total,
           double margin);

  void execute(const void *in, void *out) const override;

  void execute(int64_t howmany, const void *in, void *out) const override;

#ifndef NO_LIBCPP
  std::string plan_to_string() const override;
#endif // NO_LIBCPP
  inline algo_flops flops() const override {
    // TODO: not implemented yet, so just return 0.
    return {};
  }

  /// The type of transform being implemented by this wrapper (for example DCT
  /// type-1).
  static constexpr plfft_r2r_kind_t transform_type{TransformType};

  /// The size of FFT being used to implement TransformType.
  int transform_size() const;

private:
  /// The size of the transform being implemented (usually different to
  /// transform_size).
  const int n_;

  // Size of the input to the FFT corresponding to transform the r2r transform.
  int fft_input_size() const;

  /// Size of the output from the FFT corresponding to the r2r transform. This
  /// may not be the same as the size of the input, for instance DCT-1 is
  /// implemented as an FFT of size <tt>2n - 2</tt>, however we only need the
  /// first <tt>n</tt> elements of this to retrieve the corresponding output for
  /// the r2r transform.
  int fft_output_size() const;

  /// The type of the input to the FFT (will either be T or std::complex<T>).
  using fft_in_t =
      typename r2r_buffer_helper<TransformType>::template fft_in_t<T>;

  /// The type of the output of the FFT (will either be T or std::complex<T>).
  using fft_out_t =
      typename r2r_buffer_helper<TransformType>::template fft_out_t<T>;

  static constexpr plfft_direction_t fft_direction =
      r2r_buffer_helper<TransformType>::fft_direction;

  const int64_t howmany_;
  const int64_t istride_;
  const int64_t idist_;
  const int64_t ostride_;
  const int64_t odist_;

  /// Plan for the FFT transform to perform. All transforms dealt with in
  /// this class are assumed to be representable as some permuted Fourier
  /// transform.
  fft_plan_ptr impl_;

  /// W-values calculated during plan creation (this is the value referred
  /// to in step 3 of FCT procedure in Makhoul. All trig transforms have
  /// some rotation that could be cached, but not all currently implement
  /// this optimization.
  const std::complex<T> *precalculated_rotations_;
};

template<typename T>
inline fft_internal_plan_ptr
make_r2r_plan_1d(int64_t n, plfft_r2r_kind_t kind, int64_t howmany,
                 int64_t istride, int64_t idist, int64_t ostride, int64_t odist,
                 double target_secs_total, double margin) {
  switch (kind) {
  case PLFFT_R2R_DCT_1:
    return plfft::make_unique<r2r_plan<PLFFT_R2R_DCT_1, T>>(
        n, howmany, istride, idist, ostride, odist, target_secs_total, margin);
  case PLFFT_R2R_DCT_2:
    return plfft::make_unique<r2r_plan<PLFFT_R2R_DCT_2, T>>(
        n, howmany, istride, idist, ostride, odist, target_secs_total, margin);
  case PLFFT_R2R_DCT_3:
    return plfft::make_unique<r2r_plan<PLFFT_R2R_DCT_3, T>>(
        n, howmany, istride, idist, ostride, odist, target_secs_total, margin);
  case PLFFT_R2R_DCT_4:
    return plfft::make_unique<r2r_plan<PLFFT_R2R_DCT_4, T>>(
        n, howmany, istride, idist, ostride, odist, target_secs_total, margin);
  case PLFFT_R2R_DST_1:
    return plfft::make_unique<r2r_plan<PLFFT_R2R_DST_1, T>>(
        n, howmany, istride, idist, ostride, odist, target_secs_total, margin);
  case PLFFT_R2R_DST_2:
    return plfft::make_unique<r2r_plan<PLFFT_R2R_DST_2, T>>(
        n, howmany, istride, idist, ostride, odist, target_secs_total, margin);
  case PLFFT_R2R_DST_3:
    return plfft::make_unique<r2r_plan<PLFFT_R2R_DST_3, T>>(
        n, howmany, istride, idist, ostride, odist, target_secs_total, margin);
  case PLFFT_R2R_DST_4:
    return plfft::make_unique<r2r_plan<PLFFT_R2R_DST_4, T>>(
        n, howmany, istride, idist, ostride, odist, target_secs_total, margin);
  case PLFFT_R2R_DHT:
    return plfft::make_unique<r2r_plan<PLFFT_R2R_DHT, T>>(
        n, howmany, istride, idist, ostride, odist, target_secs_total, margin);
  case PLFFT_R2R_R2HC:
    return plfft::make_unique<r2r_plan<PLFFT_R2R_R2HC, T>>(
        n, howmany, istride, idist, ostride, odist, target_secs_total, margin);
  case PLFFT_R2R_HC2R:
    return plfft::make_unique<r2r_plan<PLFFT_R2R_HC2R, T>>(
        n, howmany, istride, idist, ostride, odist, target_secs_total, margin);
  // nothing stopping the user from passing an invalid kind, we shouldn't assert
  // here.
  default:
    return nullptr;
  }
}

template<typename T>
using dct_1_plan = r2r_plan<PLFFT_R2R_DCT_1, T>;

template<typename T>
using dct_2_plan = r2r_plan<PLFFT_R2R_DCT_2, T>;

template<typename T>
using dct_3_plan = r2r_plan<PLFFT_R2R_DCT_3, T>;

template<typename T>
using dct_4_plan = r2r_plan<PLFFT_R2R_DCT_4, T>;

template<typename T>
using dst_1_plan = r2r_plan<PLFFT_R2R_DST_1, T>;

template<typename T>
using dst_2_plan = r2r_plan<PLFFT_R2R_DST_2, T>;

template<typename T>
using dst_3_plan = r2r_plan<PLFFT_R2R_DST_3, T>;

template<typename T>
using dst_4_plan = r2r_plan<PLFFT_R2R_DST_4, T>;

template<typename T>
using dht_plan = r2r_plan<PLFFT_R2R_DHT, T>;

template<typename T>
using r2hc_plan = r2r_plan<PLFFT_R2R_R2HC, T>;

template<typename T>
using hc2r_plan = r2r_plan<PLFFT_R2R_HC2R, T>;

} // namespace plfft
