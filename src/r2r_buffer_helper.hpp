/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft.h"
#include "plfft_complex.hpp"
#include "plfft_pod_vector.hpp"
#include "plfft_util.hpp"

namespace plfft {

namespace r2r {

template<plfft_r2r_kind_t>
struct is_r2c {
  static constexpr bool v = false;
};

template<>
struct is_r2c<PLFFT_R2R_DCT_1> {
  static constexpr bool v = true;
};

template<>
struct is_r2c<PLFFT_R2R_DCT_2> {
  static constexpr bool v = true;
};

template<>
struct is_r2c<PLFFT_R2R_DST_1> {
  static constexpr bool v = true;
};

template<>
struct is_r2c<PLFFT_R2R_DST_2> {
  static constexpr bool v = true;
};

template<>
struct is_r2c<PLFFT_R2R_R2HC> {
  static constexpr bool v = true;
};

template<plfft_r2r_kind_t>
struct is_c2r {
  static constexpr bool v = false;
};

template<>
struct is_c2r<PLFFT_R2R_DCT_3> {
  static constexpr bool v = true;
};

template<>
struct is_c2r<PLFFT_R2R_DST_3> {
  static constexpr bool v = true;
};

template<>
struct is_c2r<PLFFT_R2R_HC2R> {
  static constexpr bool v = true;
};

template<plfft_r2r_kind_t>
struct is_c2c {
  static constexpr bool v = false;
};

template<>
struct is_c2c<PLFFT_R2R_DCT_4> {
  static constexpr bool v = true;
};

template<>
struct is_c2c<PLFFT_R2R_DST_4> {
  static constexpr bool v = true;
};

template<>
struct is_c2c<PLFFT_R2R_DHT> {
  static constexpr bool v = true;
};

/// If transform TT is implemented by a complex-to-real FFT then the input to
/// the FFT needs to be complex
template<plfft_r2r_kind_t TT, typename T>
using fft_input_type = add_complex_if_t<(is_c2r<TT>::v || is_c2c<TT>::v), T>;

/// If transform TT is implemented by a real-to-complex FFT then the output from
/// the FFT needs to be complex
template<plfft_r2r_kind_t TT, typename T>
using fft_output_type = add_complex_if_t<(is_r2c<TT>::v || is_c2c<TT>::v), T>;

/// Most r2r transforms are implemented as forwards FFTs, but we need the
/// capability to specify backwards FFT for certain exceptions
template<plfft_r2r_kind_t>
struct fft_direction {
  static constexpr plfft_direction_t v = PLFFT_FORWARD;
};

template<>
struct fft_direction<PLFFT_R2R_DCT_3> {
  static constexpr plfft_direction_t v = PLFFT_BACKWARD;
};

template<>
struct fft_direction<PLFFT_R2R_DST_3> {
  static constexpr plfft_direction_t v = PLFFT_BACKWARD;
};

template<>
struct fft_direction<PLFFT_R2R_DST_4> {
  static constexpr plfft_direction_t v = PLFFT_BACKWARD;
};

template<>
struct fft_direction<PLFFT_R2R_DHT> {
  static constexpr plfft_direction_t v = PLFFT_BACKWARD;
};

template<>
struct fft_direction<PLFFT_R2R_HC2R> {
  static constexpr plfft_direction_t v = PLFFT_BACKWARD;
};

} // namespace r2r

/**
 * A helper class to manage buffers used to implement real-to-real transforms by
 * r2r_plan.
 * @tparam TT The transform being implemented (e.g. DCT type-1)
 */
template<plfft_r2r_kind_t TT>
class r2r_buffer_helper {
public:
  /**
   * The type of input to the FFT (will either be T or std::complex<T>).
   * @tparam T The type of the transform being implemented (some real
   * floating-point type)
   */
  template<typename T>
  using fft_in_t = typename r2r::fft_input_type<TT, T>;

  /**
   * The type of output of the FFT (will either be T or std::complex<T>).
   * @tparam T The type of the transform being implemented (some real
   * floating-point type)
   */
  template<typename T>
  using fft_out_t = typename r2r::fft_output_type<TT, T>;

  /// The plfft_direction_t of the FFT that implements this transform (forward
  /// by default)
  static constexpr plfft_direction_t fft_direction = r2r::fft_direction<TT>::v;

  /**
   * Size of the input to the FFT corresponding to transform TT.
   * @param [in] n Size of the r2r transform being implemented by the FFT.
   */
  static inline int fft_input_size(int n);

  /**
   * Size of the output from the FFT corresponding to transform TT. This
   * may not be the same as the size of the input, for instance DCT-1 is
   * implemented as an FFT of size <tt>2n - 2</tt>, however we only need
   * the first <tt>n</tt> elements of this to retrieve the corresponding
   * output for the r2r transform.
   * @param [in] n Size of the r2r transform being implemented by the FFT.
   */
  static inline int fft_output_size(int n);

  /**
   * Copy the input array to a buffer for passing to the FFT. This
   * will generally involve some permutation of the input
   * array specific to the transform being implemented.
   * @param [out] fft_input Pointer to the array which will be used as the
   *                        input to the FFT
   * @param [in] r2r_input Pointer to the input array to the r2r transform
   * @param [in] n Size of the r2r transform being implemented by the FFT.
   * @param [in] istride Stride of the r2r input array. This indicates
   *                     that the i-th element of the input array is
   *                     located at i * istride, starting at i = 0
   * @param [in] w Vector of pre-calculated rotations. This may be unused,
   *               where either the pre-processing step does not involve
   *               rotation or this particular optimization is not
   *               implemented.
   * @tparam T The type of the transform being implemented (some real
   *           floating-point type)
   */
  template<typename T>
  static inline void
  prepare_fft_input(fft_in_t<T> *fft_input, const T *r2r_input, int n,
                    int64_t istride, const std::complex<T> *w);

  /**
   * Extract the output of the real-to-real transform from the output of
   * the FFT. This will generally involve some permutation of the output
   * buffer specific to the type of transform being implemented.
   * @param [in] fft_res Pointer to the array at which the result of the
   *                     FFT will be stored
   * @param [out] r2r_res Pointer to the caller's output array
   * @param [in] n Size of the r2r transform being implemented by the FFT.
   * @param [in] ostride Stride of the output array. This indicates that
   *                     the i-th element of the output should be located
   *                     at at i * ostride, starting at i = 0
   * @param [in] w Vector of pre-calculated rotations. This may be unused,
   *               where either the post-processing step does not involve
   *               rotation or this particular optimization is not
   *               implemented.
   * @tparam T The type of the transform being implemented (some real
   *           floating-point type)
   */
  template<typename T>
  static inline void extract_output_from_fft(const fft_out_t<T> *fft_res,
                                             T *r2r_res, int n, int64_t ostride,
                                             const std::complex<T> *w);
};

/**
 * Default length for the input to the FFT is the same as the input to the r2r
 * if the r2r is implemented using a full (c2c) FFT, and half the length of the
 * input to the r2r (rounded up) otherwise. This is used for all transforms
 * implemented using the fast UDFHT approach.
 * @param [in] n Size of the r2r transform being implemented by the FFT
 * @tparam TT The transform being implemented (e.g. DCT type-1)
 */
template<plfft_r2r_kind_t TT>
inline int r2r_buffer_helper<TT>::fft_input_size(int n) {
  switch (TT) {
  case PLFFT_R2R_DCT_1:
    return std::max(2 * n - 2, 0);
  case PLFFT_R2R_DST_1:
    return 2 * n + 2;
  case PLFFT_R2R_DCT_2:
  case PLFFT_R2R_DST_2:
  case PLFFT_R2R_DCT_4:
  case PLFFT_R2R_DST_4:
  case PLFFT_R2R_DHT:
  case PLFFT_R2R_R2HC:
    return n;
  case PLFFT_R2R_DCT_3:
  case PLFFT_R2R_DST_3:
  case PLFFT_R2R_HC2R:
    return n / 2 + 1;
  }
}

/**
 * Default length for the FFT output is the same as the size of the r2r
 * transform. This is the case for the UDFHT and DCT-type-1.
 * @param [in] n Size of the r2r transform being implemented by the FFT
 * @tparam TT The transform being implemented (e.g. DCT type-1)
 */
template<plfft_r2r_kind_t TT>
inline int r2r_buffer_helper<TT>::fft_output_size(int n) {
  switch (TT) {
  case PLFFT_R2R_DST_1:
    return 2 * n + 2;
  case PLFFT_R2R_DCT_2:
  case PLFFT_R2R_DST_2:
    return n / 2 + 1;
  default:
    return n;
  }
}

/**
 * DCT-1 of a sequence ABCDE is equivalent to FFT on the sequence ABCDEDCB.
 * @param [out] fft_input Pointer to the array which will be used as the
 *                        input to the FFT
 * @param [in] r2r_input Pointer to the input array to the r2r transform
 * @param [in] n Size of the r2r transform being implemented by the FFT.
 * @param [in] istride Stride of the r2r input array. This indicates
 *                     that the i-th element of the input array is located at
 *                     i * istride, starting at i = 0
 * @tparam T The type of the transform being implemented (some real
 *           floating-point type)
 */
template<>
template<typename T>
inline void r2r_buffer_helper<PLFFT_R2R_DCT_1>::prepare_fft_input(
    fft_in_t<T> *fft_input, const T *r2r_input, int n, int64_t istride,
    const std::complex<T> *) {
  static_assert(!is_complex_v<T>);
  static_assert(!is_complex_v<fft_in_t<T>>);

  for (int i = 0; i < n; i++) {
    fft_input[i] = r2r_input[i * istride];
  }
  for (int j = n - 2, i = n; j > 0; j--, i++) {
    fft_input[i] = r2r_input[j * istride];
  }
}

/**
 * Pre-process the input array according to step 1 of the FCT procedure
 * described in "A Fast Cosine Transform in One and Two Dimensions", Makhoul
 * J., 1980, such that the length-N DCT can be computed by a length-N DFT and
 * some post-processing. This algorithm is exactly equivalent to using
 * the UDFHT, but expresses the transform in a slightly more compact way.
 *
 * @param [out] fft_input Pointer to the array which will be used as the
 *                        input to the FFT
 * @param [in] r2r_input Pointer to the input array to the r2r transform
 * @param [in] n Size of the r2r transform being implemented by the FFT.
 * @param [in] istride Stride of the r2r input array. This indicates
 *                     that the i-th element of the input array is located at
 *                     i * istride, starting at i = 0
 * @tparam T The type of the transform being implemented (some real
 *           floating-point type)
 */
template<>
template<typename T>
inline void r2r_buffer_helper<PLFFT_R2R_DCT_2>::prepare_fft_input(
    fft_in_t<T> *fft_input, const T *r2r_input, int n, int64_t istride,
    const std::complex<T> *) {
  static_assert(!is_complex_v<T>);
  static_assert(!is_complex_v<fft_in_t<T>>);

  int i = 0;
  for (; i < (n + 1) / 2; i++)
    // Even elements go to first half
    fft_input[i] = r2r_input[i * 2 * istride];

  for (; i < n; i++)
    // Odd elements get reversed and go to second half
    fft_input[i] = r2r_input[(2 * (n - i) - 1) * istride];
}

/**
 * Pre-process the input array according to step 1 of the IFCT procedure
 * described in "A Fast Cosine Transform in One and Two Dimensions", Makhoul
 * J., 1980, such that the length-N DCT can be computed by a length-N DFT and
 * some post-processing. This algorithm is exactly equivalent to using
 * the UDFHT up to scale factor of 1/2. DCT-3 is the example given in Oraintara,
 * a slight difference here is that we do not need to multiply input by
 * reciprocal root 2 since FFTW produces unnormalized output.
 * @param [out] fft_input Pointer to the array which will be used as the
 *                        input to the FFT
 * @param [in] r2r_input Pointer to the input array to the r2r transform
 * @param [in] n Size of the r2r transform being implemented by the FFT.
 * @param [in] istride Stride of the r2r input array. This indicates
 *                     that the i-th element of the input array is located at
 *                     i * istride, starting at i = 0
 * @param [in] w Pre-calculated vector of rotations
 * @tparam T The type of the transform being implemented (some real
 *           floating-point type)
 */
template<>
template<typename T>
inline void r2r_buffer_helper<PLFFT_R2R_DCT_3>::prepare_fft_input(
    fft_in_t<T> *fft_input, const T *r2r_input, int n, int64_t istride,
    const std::complex<T> *w) {
  static_assert(!is_complex_v<T>);
  static_assert(is_complex_v<fft_in_t<T>>);

  for (int k = 0; k < fft_input_size(n); k++) {
    const fft_in_t<T> j(r2r_input[k * istride],
                        k == 0 ? 0 : r2r_input[(n - k) * istride]);
    // Note Makhoul has a factor of 0.5 here, which to be consistent with FFTW
    // is not used
    fft_input[k] = j * w[k];
  }
}

/**
 * DCT-4 is implemented using the Unified Discrete Fourier-Hartley Transform,
 * with parameters <tt>A = 0.5, B = 0.5, k_0 = 0.25</tt> and <tt>n_0 = 0.5</tt>.
 * See Oraintara, S., 2002, May. The unified discrete Fourier-Hartley
 * transforms: Theory and structure. In 2002 IEEE International Symposium on
 * Circuits and Systems. Proceedings (Cat. No. 02CH37353) (Vol. 3, pp. III-III).
 * IEEE.
 * @param [out] fft_input Pointer to the array which will be used as the
 *                        input to the FFT
 * @param [in] r2r_input Pointer to the input array to the r2r transform
 * @param [in] n Size of the r2r transform being implemented by the FFT.
 * @param [in] istride Stride of the r2r input array. This indicates
 *                     that the i-th element of the input array is located at
 *                     i * istride, starting at i = 0
 * @param [in] w Pre-calculated vector of rotations
 * @tparam T The type of the transform being implemented (some real
 *           floating-point type)
 */
template<>
template<typename T>
inline void r2r_buffer_helper<PLFFT_R2R_DCT_4>::prepare_fft_input(
    fft_in_t<T> *fft_input, const T *r2r_input, int n, int64_t istride,
    const std::complex<T> *w) {
  static_assert(!is_complex_v<T>);
  static_assert(is_complex_v<fft_out_t<T>>);

  for (int i = 0; i < n; i++) {
    fft_input[i] = std::complex<T>(r2r_input[i * istride],
                                   r2r_input[(n - i - 1) * istride]) *
                   w[i];
  }
}

/**
 * DST-1 of a sequence ABC is equivalent to FFT on the sequence 0 A B C 0 -C -B
 * -A.
 * @param [out] fft_input Pointer to the array which will be used as the
 *                        input to the FFT
 * @param [in] r2r_input Pointer to the input array to the r2r transform
 * @param [in] n Size of the r2r transform being implemented by the FFT.
 * @param [in] istride Stride of the r2r input array. This indicates
 *                     that the i-th element of the input array is located at
 *                     i * istride, starting at i = 0
 * @tparam T The type of the transform being implemented (some real
 *           floating-point type)
 */
template<>
template<typename T>
inline void r2r_buffer_helper<PLFFT_R2R_DST_1>::prepare_fft_input(
    fft_in_t<T> *fft_input, const T *r2r_input, int n, int64_t istride,
    const std::complex<T> *) {
  static_assert(!is_complex_v<T>);
  static_assert(!is_complex_v<fft_in_t<T>>);

  fft_input[0] = 0.0;
  for (int i = 1; i < n + 1; i++) {
    fft_input[i] = r2r_input[(i - 1) * istride];
  }
  fft_input[n + 1] = 0.0;
  for (int i = n + 2, j = n - 1; j >= 0; j--, i++) {
    fft_input[i] = -r2r_input[j * istride];
  }
}

/**
 * DST-2 cannot be implemented directly using the UDFHT, because the fast UDFHT
 * approach is only defined for n_0 = p/2, where p is some integer, and DST-2 is
 * equivalent to UDFHT with n_0 = 1/4 (see the below paper for details of the
 * fast UDFHT). However DST-2 is the inverse of DST-3, so it is implemented by
 * reversing the steps for that.
 * See Oraintara, S., 2002, May. The unified discrete Fourier-Hartley
 * transforms: Theory and structure. In 2002 IEEE International Symposium on
 * Circuits and Systems. Proceedings (Cat. No. 02CH37353) (Vol. 3, pp. III-III).
 * IEEE. Section 3.
 * @see r2r_buffer_helper<PLFFT_R2R_DST_2>::extract_output_from_fft
 * @param [out] fft_input Pointer to the array which will be used as the
 *                        input to the FFT
 * @param [in] r2r_input Pointer to the input array to the r2r transform
 * @param [in] n Size of the r2r transform being implemented by the FFT.
 * @param [in] istride Stride of the r2r input array. This indicates
 *                     that the i-th element of the input array is located at
 *                     i * istride, starting at i = 0
 * @tparam T The type of the transform being implemented (some real
 *           floating-point type)
 */
template<>
template<typename T>
inline void r2r_buffer_helper<PLFFT_R2R_DST_2>::prepare_fft_input(
    fft_in_t<T> *fft_input, const T *r2r_input, int n, int64_t istride,
    const std::complex<T> *) {
  static_assert(!is_complex_v<T>);
  static_assert(!is_complex_v<fft_in_t<T>>);

  for (int i = 0; i < n; i++) {
    const int j = i % 2 ? n - (i + 1) / 2 : i / 2;
    // Permutation matrix is P_1, so we have sign changes
    const T sign = i % 2 ? -1 : 1;
    fft_input[j] = (T)(r2r_input[i * istride] * sign);
  }
}

/**
 * DST-3 is implemented using the Unified Discrete Fourier-Hartley Transform.
 * Copy the input array to the input buffer, using the permutation described in
 * the below paper.
 * See Oraintara, S., 2002, May. The unified discrete Fourier-Hartley
 * transforms: Theory and structure. In 2002 IEEE International Symposium on
 * Circuits and Systems. Proceedings (Cat. No. 02CH37353) (Vol. 3, pp. III-III).
 * IEEE. Section 2.1.
 * @param [out] fft_input Pointer to the array which will be used as the
 *                        input to the FFT
 * @param [in] r2r_input Pointer to the input array to the r2r transform
 * @param [in] n Size of the r2r transform being implemented by the FFT.
 * @param [in] istride Stride of the r2r input array. This indicates
 *                     that the i-th element of the input array is located at
 *                     i * istride, starting at i = 0
 * @tparam T The type of the transform being implemented (some real
 *           floating-point type)
 */
template<>
template<typename T>
inline void r2r_buffer_helper<PLFFT_R2R_DST_3>::prepare_fft_input(
    fft_in_t<T> *fft_input, const T *r2r_input, int n, int64_t istride,
    const std::complex<T> *) {
  static_assert(!is_complex_v<T>);
  static_assert(is_complex_v<fft_in_t<T>>);

  fft_input[0] = std::complex(T(0), r2r_input[(n - 1) * istride]);
  // First form the complex sequence. In this case we shift the input one
  // place so that, in UDFHT terms n_0 = 0 (which means we do no
  // multiplication on output).
  for (int i = 1; i < fft_input_size(n); i++) {
    fft_input[i] = std::complex(r2r_input[(i - 1) * istride],
                                r2r_input[(n - i - 1) * istride]);
  }
  // Second, pre-multiply the input (since k_0 = 0.25 for this case in UDFHT)
  for (int i = 0; i < fft_input_size(n); i++) {
    const T pre_mul_coeff =
        (T)(0.5 * (double)i * consts::pi<double> / (double)n);
    fft_input[i] *=
        std::complex((T)sin(pre_mul_coeff), (T)(-cos(pre_mul_coeff)));
  }
}

/**
 * DST-4 is implemented using the Unified Discrete Fourier-Hartley Transform,
 * with parameters <tt>A = i/2, B = -i/2, k_0 = 0.25</tt> and <tt>n_0 =
 * 0.5</tt>. See Oraintara, S., 2002, May. The unified discrete Fourier-Hartley
 * transforms: Theory and structure. In 2002 IEEE International Symposium on
 * Circuits and Systems. Proceedings (Cat. No. 02CH37353) (Vol. 3, pp. III-III).
 * IEEE.
 * @param [out] fft_input Pointer to the array which will be used as the
 *                        input to the FFT
 * @param [in] r2r_input Pointer to the input array to the r2r transform
 * @param [in] n Size of the r2r transform being implemented by the FFT.
 * @param [in] istride Stride of the r2r input array. This indicates
 *                     that the i-th element of the input array is located at
 *                     i * istride, starting at i = 0
 * @tparam T The type of the transform being implemented (some real
 *           floating-point type)
 */
template<>
template<typename T>
inline void r2r_buffer_helper<PLFFT_R2R_DST_4>::prepare_fft_input(
    fft_in_t<T> *fft_input, const T *r2r_input, int n, int64_t istride,
    const std::complex<T> *) {
  static_assert(!is_complex_v<T>);
  static_assert(is_complex_v<fft_in_t<T>>);

  for (int i = 0; i < n; i++) {
    const T pre_mul_coeff =
        (T)(0.5 * (double)i * consts::pi<double> / (double)n);
    fft_input[i] =
        std::complex(r2r_input[i * istride], r2r_input[(n - i - 1) * istride]) *
        std::complex<T>((T)sin(pre_mul_coeff), (T)(-cos(pre_mul_coeff)));
  }
}

/**
 * DHT is implemented using the Unified Discrete Fourier-Hartley Transform,
 * with parameters <tt>A = (1+i)/2, B = (1-i)/2, k_0 = 0</tt> and <tt>n_0 =
 * 0</tt>. See Oraintara, S., 2002, May. The unified discrete Fourier-Hartley
 * transforms: Theory and structure. In 2002 IEEE International Symposium on
 * Circuits and Systems. Proceedings (Cat. No. 02CH37353) (Vol. 3, pp. III-III).
 * IEEE.
 * @param [out] fft_input Pointer to the array which will be used as the
 *                        input to the FFT
 * @param [in] r2r_input Pointer to the input array to the r2r transform
 * @param [in] n Size of the r2r transform being implemented by the FFT.
 * @param [in] istride Stride of the r2r input array. This indicates
 *                     that the i-th element of the input array is located at
 *                     i * istride, starting at i = 0
 * @tparam T The type of the transform being implemented (some real
 *           floating-point type)
 */
template<>
template<typename T>
inline void r2r_buffer_helper<PLFFT_R2R_DHT>::prepare_fft_input(
    fft_in_t<T> *fft_input, const T *r2r_input, int n, int64_t istride,
    const std::complex<T> *) {
  static_assert(!is_complex_v<T>);
  static_assert(is_complex_v<fft_in_t<T>>);

  fft_input[0] = r2r_input[0];
  for (int i = 1; i < n; i++) {
    fft_input[i] =
        std::complex(r2r_input[i * istride], r2r_input[(n - i) * istride]);
  }
}

/**
 * R2HC is implemented using a R2C FFT, so preparing the input to the FFT just
 * requires copying the input array across (excluding strided elements).
 * @param [out] fft_input Pointer to the array which will be used as the
 *                        input to the FFT
 * @param [in] r2r_input Pointer to the input array to the r2r transform
 * @param [in] n Size of the r2r transform being implemented by the FFT.
 * @param [in] istride Stride of the r2r input array. This indicates
 *                     that the i-th element of the input array is located at
 *                     i * istride, starting at i = 0
 * @tparam T The type of the transform being implemented (some real
 *           floating-point type)
 */
template<>
template<typename T>
inline void r2r_buffer_helper<PLFFT_R2R_R2HC>::prepare_fft_input(
    fft_in_t<T> *fft_input, const T *r2r_input, int n, int64_t istride,
    const std::complex<T> *) {
  static_assert(!is_complex_v<T>);
  static_assert(!is_complex_v<fft_in_t<T>>);

  for (int i = 0; i < n; i++) {
    fft_input[i] = r2r_input[i * istride];
  }
}

/**
 * HC2R is implemented using a C2R FFT, so we need to convert the input from
 * half-complex format to an array of std::complex<T>. See <a
 * href="http://www.fftw.org/fftw3_doc/The-Halfcomplex_002dformat-DFT.html">FFTW
 * docs</a> for more detail on the half-complex format. A sequence \c a of
 * length \c n in half-complex format corresponds to a complex sequence of
 * length \c n: \f[ a_0 + 0i, a_1 + a_{n-1}i, a_2 + a_{n-2}i, ... ,
 * a_{\frac{n}{2}-1} + a_{\frac{n}{2}+1}i, a_{\frac{n}{2}} + 0i, 0 + 0i, ... , 0
 * + 0i \f]
 * @param [out] fft_input Pointer to the array which will be used as the
 *                        input to the FFT
 * @param [in] r2r_input Pointer to the input array to the r2r transform
 * @param [in] n Size of the r2r transform being implemented by the FFT.
 * @param [in] istride Stride of the r2r input array. This indicates
 *                     that the i-th element of the input array is located at
 *                     i * istride, starting at i = 0
 * @tparam T The type of the transform being implemented (some real
 *           floating-point type)
 */
template<>
template<typename T>
inline void r2r_buffer_helper<PLFFT_R2R_HC2R>::prepare_fft_input(
    fft_in_t<T> *fft_input, const T *r2r_input, int n, int64_t istride,
    const std::complex<T> *) {
  static_assert(!is_complex_v<T>);
  static_assert(is_complex_v<fft_in_t<T>>);

  fft_input[0] = r2r_input[0];
  for (int i = 1; i < n / 2; i++) {
    fft_input[i] =
        std::complex(r2r_input[i * istride], r2r_input[(n - i) * istride]);
  }
  fft_input[n / 2] =
      std::complex<T>(r2r_input[(n / 2) * istride],
                      n % 2 ? r2r_input[(n - n / 2) * istride] : 0);
}

/**
 * Extract the result of a DCT-1 from the output of an FFT. The length N DCT
 * result corresponds to the real components of the first N elements of an FFT.
 * @param [in] fft_res Pointer to the array at which the result of the FFT will
 * be stored
 * @param [out] r2r_res Pointer to the caller's output array
 * @param [in] n Size of the r2r transform being implemented by the FFT.
 * @param [in] ostride Stride of the output array. This indicates that the i-th
 *                     element of the output should be located at at i *
 * ostride, starting at i = 0
 * @tparam T The type of the transform being implemented (some real
 *           floating-point type)
 */
template<>
template<typename T>
inline void r2r_buffer_helper<PLFFT_R2R_DCT_1>::extract_output_from_fft(
    const fft_out_t<T> *fft_res, T *r2r_res, int n, int64_t ostride,
    const std::complex<T> *) {
  static_assert(!is_complex_v<T>);
  static_assert(is_complex_v<fft_out_t<T>>);

  for (int i = 0; i < n; i++) {
    r2r_res[i * ostride] = fft_res[i].real();
  }
}

/**
 * Post-process the result of the length-N complex-to-real FFT, according to
 * step 3 of the FCT procedure described in "A Fast Cosine Transform in One and
 * Two Dimensions", Makhoul J., 1980. This algorithm is exactly equivalent to
 * using the UDFHT, but expresses the transform in a slightly more compact way.
 *
 * @param [in] fft_res Pointer to the array at which the result of the FFT is
 * stored
 * @param [out] r2r_res Pointer to the caller's output array
 * @param [in] n Size of the r2r transform being implemented by the FFT.
 * @param [in] ostride Stride of the output array. This indicates that the i-th
 *                     element of the output should be located at at i *
 * ostride, starting at i = 0
 * @param [in] w Pre-calculated vector of rotations
 * @tparam T The type of the transform being implemented (some real
 * floating-point type)
 */
template<>
template<typename T>
inline void r2r_buffer_helper<PLFFT_R2R_DCT_2>::extract_output_from_fft(
    const fft_out_t<T> *fft_res, T *r2r_res, int n, int64_t ostride,
    const std::complex<T> *w) {
  static_assert(!is_complex_v<T>);
  static_assert(is_complex_v<fft_out_t<T>>);

  // k = 0:
  r2r_res[0] = fft_res[0].real() * 2;

  for (int k = 1; k < fft_output_size(n); k++) {
    const fft_out_t<T> j = fft_res[k] * w[k] * (T)2;
    r2r_res[k * ostride] = j.real();
    r2r_res[(n - k) * ostride] = -j.imag();
  }
}

/**
 * Post-process the result of the length-N complex-to-real FFT, according to
 * step 3 of the IFCT procedure described in "A Fast Cosine Transform in One and
 * Two Dimensions", Makhoul J., 1980.
 * @param [in] fft_res Pointer to the array at which the result of the FFT will
 * be stored
 * @param [out] r2r_res Pointer to the caller's output array
 * @param [in] n Size of the r2r transform being implemented by the FFT.
 * @param [in] ostride Stride of the output array. This indicates that the i-th
 *                     element of the output should be located at at i *
 * ostride, starting at i = 0
 * @tparam T The type of the transform being implemented (some real
 *           floating-point type)
 */
template<>
template<typename T>
inline void r2r_buffer_helper<PLFFT_R2R_DCT_3>::extract_output_from_fft(
    const fft_out_t<T> *fft_res, T *r2r_res, int n, int64_t ostride,
    const std::complex<T> *) {
  static_assert(!is_complex_v<T>);
  static_assert(!is_complex_v<fft_out_t<T>>);

  r2r_res[0] = fft_res[0];
  // Mapping from v to x, as in Makhoul equation 20. Note indexing into fft_res
  // is inverted because the FFT was a backwards transform
  int i = 1;
  for (; i < (n + 1) / 2; i++)
    // Retrieve even elements from first half
    r2r_res[(i * 2) * ostride] = fft_res[n - i];

  for (; i < n; i++)
    // Retrieve odd elements from second half and reverse their order
    r2r_res[(2 * (n - i) - 1) * ostride] = fft_res[n - i];
}

/**
 * Extract the result of a DCT-4 from the output of an FFT. DCT-4 is implemented
 * using the Unified Discrete Fourier-Hartley Transform.
 * See Oraintara, S., 2002, May. The unified discrete Fourier-Hartley
 * transforms: Theory and structure. In 2002 IEEE International Symposium on
 * Circuits and Systems. Proceedings (Cat. No. 02CH37353) (Vol. 3, pp. III-III).
 * IEEE. Section 2.1.
 * @param [in] fft_res Pointer to the array at which the result of the FFT will
 * be stored
 * @param [out] r2r_res Pointer to the caller's output array
 * @param [in] n Size of the r2r transform being implemented by the FFT.
 * @param [in] ostride Stride of the output array. This indicates that the i-th
 *                     element of the output should be located at at i *
 * ostride, starting at i = 0
 * @tparam T The type of the transform being implemented (some real
 *           floating-point type)
 */
template<>
template<typename T>
inline void r2r_buffer_helper<PLFFT_R2R_DCT_4>::extract_output_from_fft(
    const fft_out_t<T> *fft_res, T *r2r_res, int n, int64_t ostride,
    const std::complex<T> *) {
  static_assert(!is_complex_v<T>);
  static_assert(is_complex_v<fft_out_t<T>>);

  pod_vector<fft_out_t<T>> tmp(n);
  for (int i = 0; i < n; i++) {
    const T post_mul_coeff =
        (T)((0.25 + (double)i) * consts::pi<double> / (double)n);
    tmp[i] = fft_res[i] *
             std::complex<T>((T)cos(post_mul_coeff), (T)sin(-post_mul_coeff));
  }
  // Permute the output with sign change
  for (int i = 0; i < n; i += 2) {
    int j = i / 2;
    r2r_res[i * ostride] = tmp[j].real();
  }
  for (int i = 1; i < n; i += 2) {
    int j = n - (i + 1) / 2;
    r2r_res[i * ostride] = -tmp[j].real();
  }
}

/**
 * Extract the result of a DST-1 from the output of an FFT. The length N DCT
 * result corresponds to the imaginary components of elements 1 to N of the FFT
 * shifted left.
 * @param [in] fft_res Pointer to the array at which the result of the FFT will
 * be stored
 * @param [out] r2r_res Pointer to the caller's output array
 * @param [in] n Size of the r2r transform being implemented by the FFT.
 * @param [in] ostride Stride of the output array. This indicates that the i-th
 *                     element of the output should be located at at i *
 * ostride, starting at i = 0
 * @tparam T The type of the transform being implemented (some real
 *           floating-point type)
 */
template<>
template<typename T>
inline void r2r_buffer_helper<PLFFT_R2R_DST_1>::extract_output_from_fft(
    const fft_out_t<T> *fft_res, T *r2r_res, int n, int64_t ostride,
    const std::complex<T> *) {
  static_assert(!is_complex_v<T>);
  static_assert(is_complex_v<fft_out_t<T>>);

  for (int i = 0; i < n; i++) {
    r2r_res[i * ostride] = -fft_res[i + 1].imag();
  }
}

/**
 * For reasons described in
 * r2r_buffer_helper<PLFFT_R2R_DST_2>::prepare_fft_input, DST-2 is
 * implemented by reversing the steps of DST-3. This means that extracting the
 * result of a DST-2 from the output of the FFT is equivalent to reversing
 * r2r_buffer_helper<PLFFT_R2R_DST_3>::prepare_fft_input, with the caveat
 * that we multiply the final output by 2 to obtain unnormalized output, as
 * expected by FFTW.
 * @see r2r_buffer_helper<PLFFT_R2R_DST_3>::prepare_fft_input
 * @param [in] fft_res Pointer to the array at which the result of the FFT will
 *                     be stored
 * @param [out] r2r_res Pointer to the caller's output array
 * @param [in] n Size of the r2r transform being implemented by the FFT.
 * @param [in] ostride Stride of the output array. This indicates that the i-th
 *                     element of the output should be located at at i *
 * ostride, starting at i = 0
 * @tparam T The type of the transform being implemented (some real
 *           floating-point type)
 */
template<>
template<typename T>
inline void r2r_buffer_helper<PLFFT_R2R_DST_2>::extract_output_from_fft(
    const fft_out_t<T> *fft_res, T *r2r_res, int n, int64_t ostride,
    const std::complex<T> *) {
  static_assert(!is_complex_v<T>);
  static_assert(is_complex_v<fft_out_t<T>>);

  // Note that indexing here incorporates the right-shift matching the
  // left-shift in r2r_buffer_helper<PLFFT_R2R_DST_3>::prepare_fft_input
  fft_out_t<at_least_fp32_t<T>> tmp;
  for (int i = 1; i < n; i++) {
    tmp = i < fft_output_size(n) ? fft_res[i] : std::conj(fft_res[n - i]);
    const at_least_fp32_t<T> post_mul_coeff =
        (at_least_fp32_t<T>)(0.5 * consts::pi<double> * (double)i / (double)n);
    // when T = __fp16, / operator may not be supported
    // so use at_least_fp32_t<T> instead (compiler would have cast to float
    // anyway)
    r2r_res[(i - 1) * ostride] =
        (T)((tmp / std::complex<at_least_fp32_t<T>>(
                       (at_least_fp32_t<T>)sin(post_mul_coeff),
                       (at_least_fp32_t<T>)(-cos(post_mul_coeff))))
                .real() *
            2);
  }
  r2r_res[(n - 1) * ostride] = (T)(fft_res[0].real() * (T)2);
}

/**
 * Extract the result of a DST-3 from the output of an FFT. DST-3 is implemented
 * using the Unified Discrete Fourier-Hartley Transform. The permutation
 * required to extract the DST output from the FFT is described in the below
 * paper.
 * See Oraintara, S., 2002, May. The unified discrete Fourier-Hartley
 * transforms: Theory and structure. In 2002 IEEE International Symposium on
 * Circuits and Systems. Proceedings (Cat. No. 02CH37353) (Vol. 3, pp. III-III).
 * IEEE. Section 2.1.
 * @param [in] fft_res Pointer to the array at which the result of the FFT will
 * be stored
 * @param [out] r2r_res Pointer to the caller's output array
 * @param [in] n Size of the r2r transform being implemented by the FFT.
 * @param [in] ostride Stride of the output array. This indicates that the i-th
 *                     element of the output should be located at at i *
 * ostride, starting at i = 0
 * @tparam T The type of the transform being implemented (some real
 *           floating-point type)
 */
template<>
template<typename T>
inline void r2r_buffer_helper<PLFFT_R2R_DST_3>::extract_output_from_fft(
    const fft_out_t<T> *fft_res, T *r2r_res, int n, int64_t ostride,
    const std::complex<T> *) {
  static_assert(!is_complex_v<T>);
  static_assert(!is_complex_v<fft_out_t<T>>);

  // In this case we permute the output with a change of sign for odd indices.
  for (int i = 0; i < n; i++) {
    int j = i % 2 ? n - (i + 1) / 2 : i / 2;
    T sign = i % 2 ? -1 : 1;
    r2r_res[i * ostride] = (T)(sign * fft_res[j]);
  }
}

/**
 * Extract the result of a DST-4 from the output of an FFT. DST-4 is implemented
 * using the Unified Discrete Fourier-Hartley Transform.
 * See Oraintara, S., 2002, May. The unified discrete Fourier-Hartley
 * transforms: Theory and structure. In 2002 IEEE International Symposium on
 * Circuits and Systems. Proceedings (Cat. No. 02CH37353) (Vol. 3, pp. III-III).
 * IEEE. Section 2.1.
 * @param [in] fft_res Pointer to the array at which the result of the FFT will
 * be stored
 * @param [out] r2r_res Pointer to the caller's output array
 * @param [in] n Size of the r2r transform being implemented by the FFT.
 * @param [in] ostride Stride of the output array. This indicates that the i-th
 *                     element of the output should be located at at i *
 * ostride, starting at i = 0
 * @tparam T The type of the transform being implemented (some real
 *           floating-point type)
 */
template<>
template<typename T>
inline void r2r_buffer_helper<PLFFT_R2R_DST_4>::extract_output_from_fft(
    const fft_out_t<T> *fft_res, T *r2r_res, int n, int64_t ostride,
    const std::complex<T> *) {
  static_assert(!is_complex_v<T>);
  static_assert(is_complex_v<fft_out_t<T>>);

  pod_vector<fft_out_t<T>> tmp(n);
  for (int i = 0; i < n; i++) {
    const T post_mul_coeff =
        (T)((0.25 + (double)i) * consts::pi<double> / (double)n);
    tmp[i] = fft_res[i] *
             std::complex<T>((T)cos(post_mul_coeff), (T)sin(post_mul_coeff));
  }

  for (int i = 0; i < n; i++) {
    int j = i % 2 ? n - (i + 1) / 2 : i / 2;
    r2r_res[i * ostride] = tmp[j].real();
  }
}

/**
 * Extract the result of a DHT from the output of an FFT. DHT is implemented
 * using the Unified Discrete Fourier-Hartley Transform.
 * See Oraintara, S., 2002, May. The unified discrete Fourier-Hartley
 * transforms: Theory and structure. In 2002 IEEE International Symposium on
 * Circuits and Systems. Proceedings (Cat. No. 02CH37353) (Vol. 3, pp. III-III).
 * IEEE. Section 2.1.
 * @param [in] fft_res Pointer to the array at which the result of the FFT will
 * be stored
 * @param [out] r2r_res Pointer to the caller's output array
 * @param [in] n Size of the r2r transform being implemented by the FFT.
 * @param [in] ostride Stride of the output array. This indicates that the i-th
 *                     element of the output should be located at at i *
 * ostride, starting at i = 0
 * @tparam T The type of the transform being implemented (some real
 *           floating-point type)
 */
template<>
template<typename T>
inline void r2r_buffer_helper<PLFFT_R2R_DHT>::extract_output_from_fft(
    const fft_out_t<T> *fft_res, T *r2r_res, int n, int64_t ostride,
    const std::complex<T> *) {
  static_assert(!is_complex_v<T>);
  static_assert(is_complex_v<fft_out_t<T>>);

  for (int i = 0; i < n; i++) {
    r2r_res[i * ostride] = fft_res[i].real();
  }
}

/**
 * R2HC is implemented using a R2C FFT, so we need to convert the input to
 half-complex format from an array
 * of std::complex<T>. See <a
 href="http://www.fftw.org/fftw3_doc/The-Halfcomplex_002dformat-DFT.html">FFTW
 docs</a>
 * for more detail on the half-complex format. A complex sequence of length \c
 n:
 * \f[ r_0 + i_0, ... , r_{n-1} + i_{n-1} \f]
 * corresponds to a half-complex sequence of length n:
 * \f[
 r_0, r_1, r_2, ... , r_{\frac{n}{2}}, i_{\frac{n+1}{2}-1}, ... , i_2, i_1
 * \f]
 * @param [in] fft_res Pointer to the array at which the result of the FFT will
 be stored
 * @param [out] r2r_res Pointer to the caller's output array
 * @param [in] n Size of the r2r transform being implemented by the FFT.
 * @param [in] ostride Stride of the output array. This indicates that the i-th
 *                     element of the output should be located at at i *
 ostride, starting at i = 0
 * @tparam T The type of the transform being implemented (some real
 *           floating-point type)
 */
template<>
template<typename T>
inline void r2r_buffer_helper<PLFFT_R2R_R2HC>::extract_output_from_fft(
    const fft_out_t<T> *fft_res, T *r2r_res, int n, int64_t ostride,
    const std::complex<T> *) {
  static_assert(!is_complex_v<T>);
  static_assert(is_complex_v<fft_out_t<T>>);

  for (int i = 0; i < n / 2 + 1; i++) {
    r2r_res[i * ostride] = fft_res[i].real();
  }

  for (int i = n / 2 + 1, j = (n - 1) / 2; i < n; i++, j--) {
    r2r_res[i * ostride] = fft_res[j].imag();
  }
}

/**
 * HC2R is implemented using a C2R FFT, so we just need to copy the FFT output
 * straight to the caller's output array, taking into account the stride.
 * @param [in] fft_res Pointer to the array at which the result of the FFT will
 * be stored
 * @param [out] r2r_res Pointer to the caller's output array
 * @param [in] n Size of the r2r transform being implemented by the FFT.
 * @param [in] ostride Stride of the output array. This indicates that the i-th
 *                     element of the output should be located at at i *
 * ostride, starting at i = 0
 * @tparam T The type of the transform being implemented (some real
 *           floating-point type)
 */
template<>
template<typename T>
inline void r2r_buffer_helper<PLFFT_R2R_HC2R>::extract_output_from_fft(
    const fft_out_t<T> *fft_res, T *r2r_res, int n, int64_t ostride,
    const std::complex<T> *) {
  static_assert(!is_complex_v<T>);
  static_assert(!is_complex_v<fft_out_t<T>>);

  for (int i = 0; i < n; i++) {
    r2r_res[i * ostride] = fft_res[i];
  }
}

} // namespace plfft
