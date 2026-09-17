/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "rader.hpp"
#include "arm_fft1d_impl.hpp"

#include "exec_convolution.hpp"
#include "fft_buffers.hpp"
#include "level_rader.hpp"
#include "plfft_pod_vector.hpp"
#include "rader_generator.hpp"

namespace plfft {

template<typename Tx, typename Ty>
std::optional<rader<Tx, Ty>>
create_rader(int64_t n, int64_t howmany, plfft_direction_t dir,
             double target_secs_total, double margin, bool use_direct_kernel,
             bool want_sme) {
  using real_t = remove_complex_t<Tx>;
  using cplx_t = add_complex_t<Tx>;

  auto work = get_memory<cplx_t>(buffer_name::rader, n - 1);

  auto g = find_group_generator(n);
  if (!g) {
    return std::nullopt;
  }

  // try to plan recursive calls, but do not allow recursive use of Rader's
  // algorithm since that tends to be slower than just using Bluestein.
  //
  // When we use direct kernels: the convolution subplans are vectorized over
  // the planned Rader batch count in an interleaved in-place work buffer:
  //   istride = ostride = howmany
  //   idist = odist = 1
  // During multi-threaded execution, runtime howmany may differ from planned
  // howmany, so the `rader` class stores the work buffer stride separately.
  //
  // When we use non-direct kernels: the calls are not vectorized,
  //   i.e. istride = ostride = 1, idist = odist = 0.
  auto work_stride = howmany;
  auto pf =
      use_direct_kernel
          ? make_1d_direct_plan(n - 1, work, work, howmany, work_stride, 1,
                                work_stride, 1, PLFFT_FORWARD, want_sme)
          : make_1d_plan(n - 1, work, work, 1, 1, 0, 1, 0, PLFFT_FORWARD,
                         target_secs_total, margin, false, want_sme);
  auto pb =
      use_direct_kernel
          ? make_1d_direct_plan(n - 1, work, work, howmany, work_stride, 1,
                                work_stride, 1, PLFFT_BACKWARD, want_sme)
          : make_1d_plan(n - 1, work, work, 1, 1, 0, 1, 0, PLFFT_BACKWARD,
                         target_secs_total, margin, false, want_sme);
  if (!pf || !pb) {
    return std::nullopt;
  }

  // fill out permutation vectors to avoid needing to do expensive
  // modulus operations in the actual execute call.
  pod_vector<int64_t> gmul_fw_perm(n - 1);
  pod_vector<int64_t> ginvmul_fw_perm(n - 1);
  pod_vector<int64_t> ginvmul_bw_perm(n - 1);
  // permuting by ginvmul_fw_perm followed by ginvmul_bw_perm is the identity
  for (int64_t j = 0, gmul = 1, m = n - 1; j < m; ++j) {
    auto ginv_idx = j == 0 ? 0 : m - j; // (m - j) % m;
    gmul_fw_perm[j] = gmul;
    ginvmul_fw_perm[ginv_idx] = gmul;
    ginvmul_bw_perm[gmul - 1] = ginv_idx + 1;
    gmul = (gmul * g) % n;
  }
  // Populate the vector b to be used in the convolution
  pod_vector<cplx_t> b(n - 1);
  auto pi = consts::pi<real_t>;
  for (int64_t i = 0; i < n - 1; i++) {
    double x = (double)ginvmul_fw_perm[i];
    double in =
        ((2. * pi * x) / (double)n) * (dir == PLFFT_FORWARD ? -1.0 : 1.0);
    b[i] = {(real_t)std::cos(in), (real_t)std::sin(in)};
  }
  auto b_plan = make_1d_plan(
      n - 1, b.data(), b.data(), 1, 1, 0, 1, 0, static_cast<int>(PLFFT_FORWARD),
      static_cast<plfft_r2r_kind_t>(0), 0.0, 0.0, want_sme);
  if (!b_plan) {
    return std::nullopt;
  }
  b_plan->execute(b.data(), b.data());

  // Multiply output from FFT of b with 1/n_pad
  real_t n1 = (real_t)(1.0 / (double)(n - 1));
  for (int64_t i = 0; i < n - 1; i++) {
    b[i] *= n1;
  }

  return rader<Tx, Ty>{.n = n,
                       .g = g,
                       .pf = std::move(pf),
                       .pb = std::move(pb),
                       .use_direct_kernel = use_direct_kernel,
                       .work_stride = work_stride,
                       .b = std::move(b),
                       .gmul_fw_perm = std::move(gmul_fw_perm),
                       .ginvmul_fw_perm = std::move(ginvmul_fw_perm),
                       .ginvmul_bw_perm = std::move(ginvmul_bw_perm)};
}

#define CREATE_RADER(Tx, Ty)                                                   \
  template std::optional<rader<Tx, Ty>> create_rader(                          \
      int64_t n, int64_t howmany, plfft_direction_t dir,                       \
      double target_secs_total, double margin, bool use_direct_kernel,         \
      bool want_sme);

CREATE_RADER(half, std::complex<half>)
CREATE_RADER(std::complex<half>, half)
CREATE_RADER(std::complex<half>, std::complex<half>)
CREATE_RADER(float, std::complex<float>)
CREATE_RADER(std::complex<float>, float)
CREATE_RADER(std::complex<float>, std::complex<float>)
CREATE_RADER(double, std::complex<double>)
CREATE_RADER(std::complex<double>, double)
CREATE_RADER(std::complex<double>, std::complex<double>)

#undef CREATE_RADER

template<typename To, typename From>
static inline To downcast_to(From x) {
  if constexpr (std::is_same_v<add_complex_t<From>, To>) {
    return x;
  } else {
    static_assert(std::is_same_v<remove_complex_t<From>, To>);
    return real(x);
  }
}

template<bool IsVec, bool IsTwiddle, typename T>
static void c2c_permute_x_into_work(const int64_t i, const int64_t istride,
                                    const int64_t idist, int64_t work_stride,
                                    const int64_t nm1,
                                    const pod_vector<int64_t> &gmul_fw_perm,
                                    const T *W, const T *x, T *work_ptr,
                                    const T &x0, T &x0i, T &y0) {

  // calculate y0 (sum)
  x0i = x0;
  T y0i = x0;

  // permute x into work and finish calculating y0.
  for (int64_t j = 0; j < nm1; ++j) {
    auto xidx = i * idist + gmul_fw_perm[j] * istride;
    auto xi = x[xidx];
    if constexpr (IsTwiddle) {
      if (i > 0) {
        auto idx = nm1 * (i - 1) + gmul_fw_perm[j] - 1;
        xi *= W[idx];
      }
    }
    y0i += xi;
    if (IsVec) {
      work_ptr[i + j * work_stride] = xi;
    } else {
      work_ptr[j] = xi;
    }
  }
  y0 = y0i;
}

template<bool IsVec, bool IsTwiddle, typename T>
static void c2c_permute_work_into_y(const int64_t nm1,
                                    const int64_t work_stride, const int64_t i,
                                    const int64_t odist, const int64_t ostride,
                                    const pod_vector<int64_t> &ginvmul_fw_perm,
                                    T *y, const T *work_ptr, const T &y0,
                                    const T &x0) {
  y[i * odist] = downcast_to<T>(y0);
  for (int64_t j = 0; j < nm1; ++j) {
    const auto idx = IsVec ? i + j * work_stride : j;
    const auto yelem = work_ptr[idx] + x0;
    y[i * odist + ginvmul_fw_perm[j] * ostride] = downcast_to<T>(yelem);
  }
}

template<bool IsTwiddle, typename Tx, typename Ty, typename T1, typename T2,
         typename Tw = add_complex_t<Tx>>
static void execute_c2c_vec(const rader<Tx, Ty> &r, const T1 *x, T2 *y,
                            int64_t istride, int64_t ostride, const Tw *W,
                            int64_t howmany, int64_t idist, int64_t odist) {
  static_assert(is_c2c_v<Tx, Ty>, "");
  static_assert(is_c2c_v<T1, T2>, "");

  auto nm1 = r.n - 1;
  auto work_stride = r.work_stride;
  auto work_ptr = get_memory<Tw>(buffer_name::rader, (nm1 + 2) * work_stride);
  auto x0 = work_ptr + nm1 * work_stride;
  auto y0 = x0 + work_stride;
  constexpr bool IsVec = true;

  for (int64_t i = 0; i < howmany; ++i) {
    // calculate y0 (sum)
    c2c_permute_x_into_work<IsVec, IsTwiddle>(
        i, istride, idist, work_stride, nm1, r.gmul_fw_perm, W, x, work_ptr,
        x[i * idist], x0[i], y0[i]);
  }
  exec_convolution(*r.pf, *r.pb, work_ptr, r.b.data(), nm1, howmany,
                   work_stride);

  for (int64_t i = 0; i < howmany; ++i) {
    // permute work into y, add x0
    c2c_permute_work_into_y<IsVec, IsTwiddle>(nm1, work_stride, i, odist,
                                              ostride, r.ginvmul_fw_perm, y,
                                              work_ptr, y0[i], x0[i]);
  }
}

template<bool IsTwiddle, typename Tx, typename Ty, typename T1, typename T2,
         typename Tw = add_complex_t<Tx>>
static void execute_c2c_novec(const rader<Tx, Ty> &r, const T1 *x, T2 *y,
                              int64_t istride, int64_t ostride, const Tw *W,
                              int64_t howmany, int64_t idist, int64_t odist) {
  static_assert(is_c2c_v<Tx, Ty>, "");
  static_assert(is_c2c_v<T1, T2>, "");

  auto nm1 = r.n - 1;
  auto work_ptr = get_memory<Tw>(buffer_name::rader, nm1);
  constexpr bool IsVec = false;
  for (int64_t i = 0; i < howmany; i++) {
    Tw x0, y0;
    c2c_permute_x_into_work<IsVec, IsTwiddle>(i, istride, idist, howmany, nm1,
                                              r.gmul_fw_perm, W, x, work_ptr,
                                              x[i * idist], x0, y0);

    exec_convolution(*r.pf, *r.pb, work_ptr, r.b.data(), nm1);

    c2c_permute_work_into_y<IsVec, IsTwiddle>(nm1, howmany, i, odist, ostride,
                                              r.ginvmul_fw_perm, y, work_ptr,
                                              y0, x0);
  }
}

template<bool IsTwiddle, typename Tx, typename Ty, typename T1, typename T2,
         typename Tw = add_complex_t<Tx>>
static void execute_c2c(const rader<Tx, Ty> &r, const T1 *x, T2 *y,
                        int64_t istride, int64_t ostride, const Tw *W,
                        int64_t howmany, int64_t idist, int64_t odist) {
  if (r.use_direct_kernel) {
    execute_c2c_vec<IsTwiddle>(r, x, y, istride, ostride, W, howmany, idist,
                               odist);
  } else {
    execute_c2c_novec<IsTwiddle>(r, x, y, istride, ostride, W, howmany, idist,
                                 odist);
  }
}

template<bool IsVec, bool IsTwiddle, typename T, typename Tw = add_complex_t<T>>
inline static void
r2c_permute_x_into_work(const int64_t i, const int64_t nm1,
                        const int64_t howmany, const int64_t idist,
                        const int64_t istride, const int64_t work_stride,
                        const pod_vector<int64_t> &gmul_fw_perm, Tw *work_ptr,
                        const Tw *W, Tw &x0, Tw &y0, const T *x) {

  const int64_t hm_halfhi = howmany / 2 + 1;
  const bool want_conj = IsTwiddle && i >= hm_halfhi;

  // calculate y0 (sum)
  const auto ii = want_conj ? (howmany - i) : i;
  x0 = want_conj ? conj(x[ii * idist]) : x[ii * idist];
  Tw y0i = x0;

  // permute x into work and finish calculating y0.
  for (int64_t j = 0; j < nm1; ++j) {
    const auto xidx = ii * idist + gmul_fw_perm[j] * istride;
    Tw xi = want_conj ? conj(x[xidx]) : x[xidx];
    if (IsTwiddle && i > 0) {
      auto idx = nm1 * (i - 1) + gmul_fw_perm[j] - 1;
      xi *= W[idx];
    }
    y0i += xi;

    if constexpr (IsVec) {
      work_ptr[i + j * work_stride] = xi;
    } else {
      work_ptr[j] = xi;
    }
  }
  y0 = y0i;
}

template<bool IsVec, bool IsTwiddle, typename T, typename Tw = add_complex_t<T>>
inline static void
r2c_permute_work_into_y(const int64_t i, const int64_t n, const int64_t howmany,
                        const int64_t work_stride, const int64_t ostride,
                        const int64_t odist,
                        const pod_vector<int64_t> &ginvmul_bw_perm,
                        const Tw *work_ptr, const Tw x0, T *y, const Tw y0) {
  // permute work into y, add x0
  const auto hilim = howmany / 2;
  const auto ylim = n / 2 + (!IsTwiddle || i <= hilim ? 1 : 0);
  y[i * odist] = downcast_to<T>(y0);
  for (int64_t j = 1; j < ylim; ++j) {
    const auto idx = IsVec ? i + (ginvmul_bw_perm[j - 1] - 1) * work_stride
                           : ginvmul_bw_perm[j - 1] - 1;
    const auto yelem = work_ptr[idx] + x0;
    y[i * odist + j * ostride] = downcast_to<T>(yelem);
  }
}

template<bool IsTwiddle, typename Tx, typename Ty, typename T1, typename T2,
         typename Tw = add_complex_t<Tx>>
static void execute_r2c_vec(const rader<Tx, Ty> &r, const T1 *x, T2 *y,
                            int64_t istride, int64_t ostride, const Tw *W,
                            int64_t howmany, int64_t idist, int64_t odist) {
  auto nm1 = r.n - 1;
  auto work_stride = r.work_stride;
  auto work_ptr = get_memory<Tw>(buffer_name::rader, (nm1 + 2) * work_stride);
  auto x0 = work_ptr + nm1 * work_stride;
  auto y0 = x0 + work_stride;
  constexpr bool IsVec = true;

  for (int64_t i = 0; i < howmany; ++i) {
    r2c_permute_x_into_work<IsVec, IsTwiddle>(i, nm1, howmany, idist, istride,
                                              work_stride, r.gmul_fw_perm,
                                              work_ptr, W, x0[i], y0[i], x);
  }

  exec_convolution(*r.pf, *r.pb, work_ptr, r.b.data(), nm1, howmany,
                   work_stride);

  // permute work into y, add x0
  for (int64_t i = 0; i < howmany; ++i) {
    r2c_permute_work_into_y<IsVec, IsTwiddle>(i, r.n, howmany, work_stride,
                                              ostride, odist, r.ginvmul_bw_perm,
                                              work_ptr, x0[i], y, y0[i]);
  }
}

template<bool IsTwiddle, typename Tx, typename Ty, typename T1, typename T2,
         typename Tw = add_complex_t<Tx>>
static void execute_r2c_novec(const rader<Tx, Ty> &r, const T1 *x, T2 *y,
                              int64_t istride, int64_t ostride, const Tw *W,
                              int64_t howmany, int64_t idist, int64_t odist) {
  auto nm1 = r.n - 1;
  auto work_ptr = get_memory<Tw>(buffer_name::rader, nm1);
  constexpr bool IsVec = false;

  for (int64_t i = 0; i < howmany; i++) {
    Tw x0, y0;
    r2c_permute_x_into_work<IsVec, IsTwiddle>(i, nm1, howmany, idist, istride,
                                              howmany, r.gmul_fw_perm, work_ptr,
                                              W, x0, y0, x);

    exec_convolution(*r.pf, *r.pb, work_ptr, r.b.data(), nm1);
    r2c_permute_work_into_y<IsVec, IsTwiddle>(i, r.n, howmany, howmany, ostride,
                                              odist, r.ginvmul_bw_perm,
                                              work_ptr, x0, y, y0);
  }
}

template<bool IsTwiddle, typename Tx, typename Ty, typename T1, typename T2,
         typename Tw = add_complex_t<Tx>>
static void execute_r2c(const rader<Tx, Ty> &r, const T1 *x, T2 *y,
                        int64_t istride, int64_t ostride, const Tw *W,
                        int64_t howmany, int64_t idist, int64_t odist) {
  if (r.use_direct_kernel) {
    execute_r2c_vec<IsTwiddle>(r, x, y, istride, ostride, W, howmany, idist,
                               odist);
  } else {
    execute_r2c_novec<IsTwiddle>(r, x, y, istride, ostride, W, howmany, idist,
                                 odist);
  }
}

template<bool IsVec, bool IsTwiddle, typename T>
inline static void
c2r_permute_x_into_work(const int64_t i, const int64_t n, const int64_t nm1,
                        const int64_t howmany, const int64_t work_stride,
                        const int64_t idist, const int64_t istride, const T *x,
                        T *y0, T *work_ptr, T &y0i,
                        const pod_vector<int64_t> &gmul_fw_perm) {

  const auto nhalf = n / 2 + 1;
  for (int64_t j = 0; j < nm1; ++j) {
    T xi;
    if (gmul_fw_perm[j] >= nhalf) {
      if (!IsTwiddle || i == 0) {
        xi = conj(x[i * idist + (n - gmul_fw_perm[j]) * istride]);
      } else {
        xi = conj(x[(howmany - i) * idist + (nm1 - gmul_fw_perm[j]) * istride]);
      }
    } else {
      xi = x[i * idist + gmul_fw_perm[j] * istride];
    }

    y0i += xi;

    if (IsVec || i == 0) {
      work_ptr[i + j * work_stride] = xi;
    } else {
      work_ptr[j] = xi;
    }
  }
}

template<bool IsVec, bool IsTwiddle, typename T, typename Tw = add_complex_t<T>>
inline static void
c2r_permute_work_into_y(const int64_t i, const int64_t n, const int64_t nm1,
                        const int64_t work_stride, const int64_t odist,
                        const int64_t ostride,
                        const pod_vector<int64_t> &ginvmul_bw_perm,
                        const Tw *work_ptr, const Tw *W, const Tw *x0, T *y) {
  for (int64_t j = 1; j < n; ++j) {

    // Calculate indexes. Compile-time optimization for IsVec case.
    const auto work_idx = IsVec ? i + (ginvmul_bw_perm[j - 1] - 1) * work_stride
                                : ginvmul_bw_perm[j - 1] - 1;
    const auto x0_idx = IsVec ? i : 0;

    auto yelem = work_ptr[work_idx] + x0[x0_idx];
    if (IsTwiddle && i > 0) {
      auto idx = nm1 * (i - 1) + j - 1;
      yelem *= W[idx];
    }
    y[i * odist + j * ostride] = downcast_to<T>(yelem);
  }
}

template<bool IsTwiddle, typename Tx, typename Ty, typename T1, typename T2,
         typename Tw = add_complex_t<Tx>>
static void execute_c2r_vec(const rader<Tx, Ty> &r, const T1 *x, T2 *y,
                            int64_t istride, int64_t ostride, const Tw *W,
                            int64_t howmany, int64_t idist, int64_t odist) {

  auto nm1 = r.n - 1;

  auto hm_lim = IsTwiddle ? howmany / 2 + 1 : howmany;
  auto work_stride = r.work_stride;
  auto work_ptr = get_memory<Tw>(buffer_name::rader, (nm1 + 2) * work_stride);
  auto x0 = work_ptr + nm1 * work_stride;
  auto y0 = x0 + work_stride;

  constexpr bool IsVec = true;

  for (int64_t i = 0; i < hm_lim; ++i) {
    // calculate y0 (sum)
    x0[i] = x[i * idist];
    Tw y0i = x0[i];
    c2r_permute_x_into_work<IsVec, IsTwiddle>(i, r.n, nm1, howmany, work_stride,
                                              idist, istride, x, y0, work_ptr,
                                              y0i, r.gmul_fw_perm);
    y0[i] = y0i;
  }
  exec_convolution(*r.pf, *r.pb, work_ptr, r.b.data(), nm1, hm_lim,
                   work_stride);

  // permute work into y, add x0
  for (int64_t i = 0; i < hm_lim; ++i) {
    y[i * odist] = downcast_to<T2>(y0[i]);
    c2r_permute_work_into_y<IsVec, IsTwiddle>(i, r.n, nm1, work_stride, odist,
                                              ostride, r.ginvmul_bw_perm,
                                              work_ptr, W, x0, y);
  }
}

template<bool IsTwiddle, typename Tx, typename Ty, typename T1, typename T2,
         typename Tw = add_complex_t<Tx>>
static void execute_c2r_novec(const rader<Tx, Ty> &r, const T1 *x, T2 *y,
                              int64_t istride, int64_t ostride, const Tw *W,
                              int64_t howmany, int64_t idist, int64_t odist) {

  auto nm1 = r.n - 1;
  auto work_ptr = get_memory<Tw>(buffer_name::rader, nm1);
  auto hm_lim = IsTwiddle ? howmany / 2 + 1 : howmany;
  constexpr bool IsVec = false;

  for (int64_t i = 0; i < hm_lim; i++) {

    // calculate y0 (sum)
    Tw x0 = x[i * idist];

    // Assigning x0 to y0 gives:
    // error: ‘y0.std::complex<float>::_M_value’ may be used uninitialized
    // [-Werror=maybe-uninitialized] So we get it from the source.
    Tw y0 = x[i * idist];

    // permute x into work and finish calculating y0.
    c2r_permute_x_into_work<IsVec, IsTwiddle>(i, r.n, nm1, howmany, 1, idist,
                                              istride, x, &y0, work_ptr, y0,
                                              r.gmul_fw_perm);

    exec_convolution(*r.pf, *r.pb, work_ptr, r.b.data(), nm1);

    y[i * odist] = downcast_to<T2>(y0);
    c2r_permute_work_into_y<IsVec, IsTwiddle>(i, r.n, nm1, hm_lim, odist,
                                              ostride, r.ginvmul_bw_perm,
                                              work_ptr, W, &x0, y);
  }
}

template<bool IsTwiddle, typename Tx, typename Ty, typename T1, typename T2,
         typename Tw = add_complex_t<Tx>>
static void execute_c2r(const rader<Tx, Ty> &r, const T1 *x, T2 *y,
                        int64_t istride, int64_t ostride, const Tw *W,
                        int64_t howmany, int64_t idist, int64_t odist) {
  if (r.use_direct_kernel) {
    execute_c2r_vec<IsTwiddle>(r, x, y, istride, ostride, W, howmany, idist,
                               odist);
  } else {
    execute_c2r_novec<IsTwiddle>(r, x, y, istride, ostride, W, howmany, idist,
                                 odist);
  }
}

template struct rader<half, std::complex<half>>;
template struct rader<std::complex<half>, half>;
template struct rader<std::complex<half>, std::complex<half>>;
template struct rader<float, std::complex<float>>;
template struct rader<std::complex<float>, float>;
template struct rader<std::complex<float>, std::complex<float>>;
template struct rader<double, std::complex<double>>;
template struct rader<std::complex<double>, double>;
template struct rader<std::complex<double>, std::complex<double>>;

template<typename Tx, typename Ty>
void level_rader_n<Tx, Ty>::execute(const int64_t howmany, const void *x,
                                    const int64_t istride, const int64_t idist,
                                    void *y, const int64_t ostride,
                                    const int64_t odist) const {
  using Tw = add_complex_t<Tx>;
  auto *X = (const Tx *)x;
  auto *Y = (Ty *)y;
  if constexpr (is_r2c_v<Tx, Ty>) {
    execute_r2c<false>(r_, X, Y, istride, ostride, (const Tw *)nullptr, howmany,
                       idist, odist);
  } else if constexpr (is_c2r_v<Tx, Ty>) {
    execute_c2r<false>(r_, X, Y, istride, ostride, (const Tw *)nullptr, howmany,
                       idist, odist);
  } else {
    execute_c2c<false>(r_, X, Y, istride, ostride, (const Tw *)nullptr, howmany,
                       idist, odist);
  }
}

#define LEVEL_RADER_N_EXECUTE(Tx, Ty)                                          \
  template void level_rader_n<Tx, Ty>::execute(                                \
      const int64_t howmany, const void *x, const int64_t istride,             \
      const int64_t idist, void *y, const int64_t ostride,                     \
      const int64_t odist) const;

LEVEL_RADER_N_EXECUTE(half, std::complex<half>)
LEVEL_RADER_N_EXECUTE(std::complex<half>, half)
LEVEL_RADER_N_EXECUTE(std::complex<half>, std::complex<half>)
LEVEL_RADER_N_EXECUTE(float, std::complex<float>)
LEVEL_RADER_N_EXECUTE(std::complex<float>, float)
LEVEL_RADER_N_EXECUTE(std::complex<float>, std::complex<float>)
LEVEL_RADER_N_EXECUTE(double, std::complex<double>)
LEVEL_RADER_N_EXECUTE(std::complex<double>, double)
LEVEL_RADER_N_EXECUTE(std::complex<double>, std::complex<double>)

#undef LEVEL_RADER_N_EXECUTE

template<typename Tx, typename Ty>
void level_rader_t<Tx, Ty>::execute(const int64_t howmany, const void *x,
                                    const int64_t istride, const int64_t idist,
                                    void *y, const int64_t ostride,
                                    const int64_t odist) const {
  using Tw = add_complex_t<Tx>;
  auto *X = (const Tw *)x;
  auto *Y = (Tw *)y;
  auto *W = (const Tw *)twids_->data();
  for (int64_t i = 0; i != howmany; ++i) {
    if constexpr (is_r2c_v<Tx, Ty>) {
      int64_t offsetx = i;
      int64_t offsety = i * (odist / howmany);
      execute_r2c<true>(r_, X + offsetx, Y + offsety, istride, ostride, W,
                        this->hm_, idist, odist);
    } else if constexpr (is_c2r_v<Tx, Ty>) {
      int64_t offsetx = i * (idist / howmany);
      int64_t offsety = i;
      execute_c2r<true>(r_, X + offsetx, Y + offsety, istride, ostride, W,
                        this->hm_, idist, odist);
    } else if constexpr (is_c2c_v<Tx, Ty>) {
      int64_t offsetx = i;
      int64_t offsety = i * (odist / howmany);
      execute_c2c<true>(r_, X + offsetx, Y + offsety, istride, ostride, W,
                        this->hm_, idist, odist);
    }
  }
}

#define LEVEL_RADER_T_EXECUTE(Tx, Ty)                                          \
  template void level_rader_t<Tx, Ty>::execute(const void *x, void *y) const;

LEVEL_RADER_T_EXECUTE(half, std::complex<half>)
LEVEL_RADER_T_EXECUTE(std::complex<half>, half)
LEVEL_RADER_T_EXECUTE(std::complex<half>, std::complex<half>)
LEVEL_RADER_T_EXECUTE(float, std::complex<float>)
LEVEL_RADER_T_EXECUTE(std::complex<float>, float)
LEVEL_RADER_T_EXECUTE(std::complex<float>, std::complex<float>)
LEVEL_RADER_T_EXECUTE(double, std::complex<double>)
LEVEL_RADER_T_EXECUTE(std::complex<double>, double)
LEVEL_RADER_T_EXECUTE(std::complex<double>, std::complex<double>)

#undef LEVEL_RADER_T_EXECUTE

#define LEVEL_RADER_T_EXECUTE(Tx, Ty)                                          \
  template void level_rader_t<Tx, Ty>::execute(                                \
      const int64_t howmany, const void *x, const int64_t istride,             \
      const int64_t idist, void *y, const int64_t ostride,                     \
      const int64_t odist) const;

LEVEL_RADER_T_EXECUTE(half, std::complex<half>)
LEVEL_RADER_T_EXECUTE(std::complex<half>, half)
LEVEL_RADER_T_EXECUTE(std::complex<half>, std::complex<half>)
LEVEL_RADER_T_EXECUTE(float, std::complex<float>)
LEVEL_RADER_T_EXECUTE(std::complex<float>, float)
LEVEL_RADER_T_EXECUTE(std::complex<float>, std::complex<float>)
LEVEL_RADER_T_EXECUTE(double, std::complex<double>)
LEVEL_RADER_T_EXECUTE(std::complex<double>, double)
LEVEL_RADER_T_EXECUTE(std::complex<double>, std::complex<double>)

#undef LEVEL_RADER_T_EXECUTE

} // namespace plfft
