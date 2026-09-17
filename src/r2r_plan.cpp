/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "r2r_plan.hpp"
#include "arm_fft1d.hpp"
#include "fft_buffers.hpp"
#include "generate_twiddles.hpp"
#include "planner/plfft_planner.hpp"

namespace plfft {

template<plfft_r2r_kind_t TransformType, typename T>
r2r_plan<TransformType, T>::r2r_plan(const int n, const int64_t howmany,
                                     const int64_t istride, const int64_t idist,
                                     const int64_t ostride, const int64_t odist,
                                     double target_secs_total, double margin)
  : n_{n}, howmany_{howmany}, istride_{istride}, idist_{idist},
    ostride_{ostride}, odist_{odist},
#ifdef _OPENMP
    impl_{make_batched_1d_plan<fft_in_t, fft_out_t>(
        transform_size(), nullptr, nullptr, 1, 1, 0, 1, 0,
        static_cast<int>(fft_direction), static_cast<plfft_r2r_kind_t>(0),
        target_secs_total, margin)} {
#else
    impl_{make_planner_batched_1d_plan<fft_in_t, fft_out_t>(
        transform_size(), nullptr, nullptr, 1, 1, 0, 1, 0, fft_direction,
        target_secs_total, margin, false)} {
#endif
  if constexpr (TransformType == PLFFT_R2R_DCT_2 ||
                TransformType == PLFFT_R2R_DCT_3 ||
                TransformType == PLFFT_R2R_DCT_4) {
    // Only DCTs 2-4 have the optimization where rotation is cached
    precalculated_rotations_ =
        generate_r2r_rotation<TransformType, T>(n)->data();
  }
}

template<plfft_r2r_kind_t TT, typename T>
int r2r_plan<TT, T>::transform_size() const {
  return std::max(fft_input_size(), fft_output_size());
}

template<plfft_r2r_kind_t TT, typename T>
int r2r_plan<TT, T>::fft_input_size() const {
  return r2r_buffer_helper<TT>::fft_input_size(n_);
}

template<plfft_r2r_kind_t TT, typename T>
int r2r_plan<TT, T>::fft_output_size() const {
  return r2r_buffer_helper<TT>::fft_output_size(n_);
}

template<plfft_r2r_kind_t TransformType, typename T>
void r2r_plan<TransformType, T>::execute(const void *in, void *out) const {
  execute(howmany_, in, out);
}

template<plfft_r2r_kind_t TransformType, typename T>
void r2r_plan<TransformType, T>::execute(int64_t howmany, const void *in,
                                         void *out) const {
  auto fft_in = get_memory<fft_in_t>(buffer_name::r2r_in, fft_input_size());
  auto fft_out = get_memory<fft_out_t>(
      buffer_name::r2r_out,
      r2r_buffer_helper<TransformType>::fft_output_size(n_));
  auto in_i = (const T *)in;
  auto out_i = (T *)out;
  for (int64_t i = 0; i < howmany; i++) {
    r2r_buffer_helper<TransformType>::prepare_fft_input(
        fft_in, in_i, n_, istride_, precalculated_rotations_);
    impl_->execute(fft_in, fft_out);
    r2r_buffer_helper<TransformType>::extract_output_from_fft(
        fft_out, out_i, n_, ostride_, precalculated_rotations_);
    in_i += idist_;
    out_i += odist_;
  }
}

#ifndef NO_LIBCPP
template<plfft_r2r_kind_t TransformType, typename T>
std::string r2r_plan<TransformType, T>::plan_to_string() const {
  if constexpr (TransformType == PLFFT_R2R_DCT_1) {
    return "(r2r-dct-1 " + impl_->plan_to_string() + ")";
  } else if constexpr (TransformType == PLFFT_R2R_DCT_2) {
    return "(r2r-dct-2 " + impl_->plan_to_string() + ")";
  } else if constexpr (TransformType == PLFFT_R2R_DCT_3) {
    return "(r2r-dct-3 " + impl_->plan_to_string() + ")";
  } else if constexpr (TransformType == PLFFT_R2R_DCT_4) {
    return "(r2r-dct-4 " + impl_->plan_to_string() + ")";
  } else if constexpr (TransformType == PLFFT_R2R_DST_1) {
    return "(r2r-dst-1 " + impl_->plan_to_string() + ")";
  } else if constexpr (TransformType == PLFFT_R2R_DST_2) {
    return "(r2r-dst-2 " + impl_->plan_to_string() + ")";
  } else if constexpr (TransformType == PLFFT_R2R_DST_3) {
    return "(r2r-dst-3 " + impl_->plan_to_string() + ")";
  } else if constexpr (TransformType == PLFFT_R2R_DST_4) {
    return "(r2r-dst-4 " + impl_->plan_to_string() + ")";
  } else if constexpr (TransformType == PLFFT_R2R_DHT) {
    return "(r2r-dht " + impl_->plan_to_string() + ")";
  } else if constexpr (TransformType == PLFFT_R2R_R2HC) {
    return "(r2r-r2hc " + impl_->plan_to_string() + ")";
  } else if constexpr (TransformType == PLFFT_R2R_HC2R) {
    return "(r2r-hc2r " + impl_->plan_to_string() + ")";
  } else
    assert(false);
}
#endif // NO_LIBCPP

template class r2r_plan<PLFFT_R2R_DCT_1, half>;
template class r2r_plan<PLFFT_R2R_DCT_1, float>;
template class r2r_plan<PLFFT_R2R_DCT_1, double>;

template class r2r_plan<PLFFT_R2R_DCT_2, half>;
template class r2r_plan<PLFFT_R2R_DCT_2, float>;
template class r2r_plan<PLFFT_R2R_DCT_2, double>;

template class r2r_plan<PLFFT_R2R_DCT_3, half>;
template class r2r_plan<PLFFT_R2R_DCT_3, float>;
template class r2r_plan<PLFFT_R2R_DCT_3, double>;

template class r2r_plan<PLFFT_R2R_DCT_4, half>;
template class r2r_plan<PLFFT_R2R_DCT_4, float>;
template class r2r_plan<PLFFT_R2R_DCT_4, double>;

template class r2r_plan<PLFFT_R2R_DST_1, half>;
template class r2r_plan<PLFFT_R2R_DST_1, float>;
template class r2r_plan<PLFFT_R2R_DST_1, double>;

template class r2r_plan<PLFFT_R2R_DST_2, half>;
template class r2r_plan<PLFFT_R2R_DST_2, float>;
template class r2r_plan<PLFFT_R2R_DST_2, double>;

template class r2r_plan<PLFFT_R2R_DST_3, half>;
template class r2r_plan<PLFFT_R2R_DST_3, float>;
template class r2r_plan<PLFFT_R2R_DST_3, double>;

template class r2r_plan<PLFFT_R2R_DST_4, half>;
template class r2r_plan<PLFFT_R2R_DST_4, float>;
template class r2r_plan<PLFFT_R2R_DST_4, double>;

template class r2r_plan<PLFFT_R2R_DHT, half>;
template class r2r_plan<PLFFT_R2R_DHT, float>;
template class r2r_plan<PLFFT_R2R_DHT, double>;

template class r2r_plan<PLFFT_R2R_R2HC, half>;
template class r2r_plan<PLFFT_R2R_R2HC, float>;
template class r2r_plan<PLFFT_R2R_R2HC, double>;

template class r2r_plan<PLFFT_R2R_HC2R, half>;
template class r2r_plan<PLFFT_R2R_HC2R, float>;
template class r2r_plan<PLFFT_R2R_HC2R, double>;

} // namespace plfft
