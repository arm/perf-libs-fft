/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "bluestein.hpp"
#include "arm_fft1d_impl.hpp"

#include "exec_convolution.hpp"
#include "factorize.hpp"
#include "fft_buffers.hpp"
#include "kernel_data.hpp"
#include "level_bluestein.hpp"
#include "plfft.h"

#include "plfft_util.hpp"

namespace plfft {

// {{{ Create a bluestein object
template<typename Tx, typename Ty>
bluestein<Tx, Ty> create_bluestein(int64_t n, plfft_direction_t dir,
                                   double target_secs_total, double margin,
                                   bool want_sme) {

  using real_t = remove_complex_t<Tx>;
  using cplx_t = std::complex<real_t>;

  const auto &factors = get_kernel_ns<Tx, Ty>();
  assert(!factors.empty());
  const int64_t n_pad = next_length_factorable_by(2 * n - 1, factors);

  pod_vector<cplx_t> a(n_pad, 0);
  pod_vector<cplx_t> b(n_pad, 0);
  auto work = get_memory<cplx_t>(buffer_name::bluestein, n_pad);
  pod_vector<cplx_t> b_conj(n);

  // Populate the two vectors to be used in the convolution, a and b.
  // Also store the conjugate version of b for multiplication in
  // the last step of the algorithm
  auto pi = consts::pi<real_t>;
  for (int64_t i = 0; i < n; i++) {
    auto c = (real_t)std::cos(pi * (real_t)i * (real_t)i / (real_t)n);
    auto s = (real_t)std::sin(pi * (real_t)i * (real_t)i / (real_t)n);
    a[i] = {c, (real_t)-s}; // { cos(-pi*i*i/n), sin(-pi*i*i/n) }
    b[i] = {c, s};          // { cos( pi*i*i/n), sin( pi*i*i/n) }
    b_conj[i] = a[i];
  }
  // Make sure b is periodic
  int64_t j = n_pad - n + 1;
  for (int64_t i = n - 1; i > 0; i--) {
    b[j++] = b[i];
  }

  // Create 2 plans: forward and backward - c2c so always use DIT
  // We always call these plan with howmany = 1, i/ostride = 1. i/odist are
  // irrelevant. Bluestein is used for large primes, so these 1D plans will not
  // be direct plans.
  auto pf = make_1d_plan(n_pad, work, work, 1, 1, 0, 1, 0, PLFFT_FORWARD,
                         target_secs_total, margin, false, want_sme);
  assert(pf && "composition1 failed in bluestein - this should not happen");
  auto pb = make_1d_plan(n_pad, work, work, 1, 1, 0, 1, 0, PLFFT_BACKWARD,
                         target_secs_total, margin, false, want_sme);
  assert(pb && "composition2 failed in bluestein - this should not happen");

  // Execute fwds plan transforming series b
  pf->execute(b.data(), b.data());

  // Multiply output from FFT of b with 1/n_pad
  real_t n1 = (real_t)(1.0 / (double)n_pad);
  for (int64_t i = 0; i < n_pad; i++) {
    b[i] *= n1;
  }

  return bluestein<Tx, Ty>{.n = n,
                           .n_pad = n_pad,
                           .dir = dir,
                           .a = std::move(a),
                           .b = std::move(b),
                           .b_conj = std::move(b_conj),
                           .pf = std::move(pf),
                           .pb = std::move(pb)};
}

#define CREATE_BLUESTEIN(Tx, Ty)                                               \
  template bluestein<Tx, Ty> create_bluestein(                                 \
      int64_t n, plfft_direction_t dir, double target_secs_total,              \
      double margin, bool want_sme);

CREATE_BLUESTEIN(half, std::complex<half>)
CREATE_BLUESTEIN(std::complex<half>, half)
CREATE_BLUESTEIN(std::complex<half>, std::complex<half>)
CREATE_BLUESTEIN(float, std::complex<float>)
CREATE_BLUESTEIN(std::complex<float>, float)
CREATE_BLUESTEIN(std::complex<float>, std::complex<float>)
CREATE_BLUESTEIN(double, std::complex<double>)
CREATE_BLUESTEIN(std::complex<double>, double)
CREATE_BLUESTEIN(std::complex<double>, std::complex<double>)

#undef CREATE_BLUESTEIN

// }}}

template<typename Tx, typename Ty, typename T1, typename Tw = add_complex_t<Tx>>
static inline void multiply_a_x(const T1 *x, Tw *y, const Tw *a, int64_t n,
                                int64_t n_pad, int64_t istride) {
  if constexpr (is_c2r_v<Tx, Ty> && is_c2c_v<T1, Tw>) {
    const int64_t nhalf = n / 2 + 1;
    for (int64_t i = 0; i < nhalf; i++) {
      y[i] = a[i] * x[i * istride];
    }
    // When n is prime and not 2, it will always be odd.
    for (int64_t i = nhalf - 1, j = nhalf; j < n; i--, j++) {
      y[j] = a[j] * std::conj(x[i * istride]);
    }
  } else {
    for (int64_t i = 0; i < n; i++) {
      y[i] = a[i] * x[i * istride];
    }
  }
  for (int64_t i = n; i < n_pad; i++) {
    y[i] = Tw(0.0);
  }
}

template<typename Tx, typename Tw = add_complex_t<Tx>>
static inline void multiply_a_x_dit(const Tx *x, Tw *y, const Tw *a, int64_t n,
                                    int64_t n_pad, int64_t istride,
                                    const Tw *W) {
  y[0] = a[0] * x[0];
  for (int64_t i = 1; i < n; i++) {
    y[i] = a[i] * x[i * istride] * W[i - 1];
  }
  for (int64_t i = n; i < n_pad; i++) {
    y[i] = Tw(0.0);
  }
}

template<typename Tx, typename Ty, typename T2, typename Tw = add_complex_t<Tx>>
static inline void multiply_y_bconj(const Tw *x, T2 *y, const Tw *b_conj,
                                    int64_t n, int64_t ostride,
                                    plfft_direction_t dir, int64_t nx) {
  if constexpr (is_c2r_v<Tx, Ty> && is_c2r_v<Tw, T2>) {
    y[0] =
        (T2)(x[0].real() * b_conj[0].real() - x[0].imag() * b_conj[0].imag());
    y = y + n * ostride;
    ostride = -ostride;
    for (int64_t i = 1; i < nx; i++) {
      y[i * ostride] =
          (T2)(x[i].real() * b_conj[i].real() - x[i].imag() * b_conj[i].imag());
    }
  } else {
    y[0] = x[0] * b_conj[0];
    if (dir == PLFFT_BACKWARD) {
      y = y + n * ostride;
      ostride = -ostride;
    }
    for (int64_t i = 1; i < nx; i++) {
      y[i * ostride] = x[i] * b_conj[i];
    }
  }
}

template<typename Ty, typename Tw = add_complex_t<Ty>>
static inline void multiply_y_bconj_dif(const Tw *x, Ty *y, const Tw *b_conj,
                                        int64_t n, int64_t ostride,
                                        plfft_direction_t dir, const Tw *W) {
  if constexpr (is_complex_v<Ty>) {
    y[0] = x[0] * b_conj[0];
    if (dir == PLFFT_BACKWARD) {
      y = y + n * ostride;
      ostride = -ostride;
    }
    for (int64_t i = 1; i < n; i++) {
      y[i * ostride] = x[i] * b_conj[i] * W[n - 1 - i];
    }
  }
}

// {{{ Execute function for complex to complex
template<typename T1, typename T2, typename BS, typename Tw = add_complex_t<T2>>
void execute_vanilla(const T1 *x, T2 *y, int64_t istride, int64_t ostride,
                     const add_complex_t<T1> *W, int64_t howmany, int64_t idist,
                     int64_t odist, const BS *bs) {

  auto work = get_memory<add_complex_t<T1>>(buffer_name::bluestein, bs->n_pad);

  for (int64_t i = 0; i < howmany; i++) {
    // Multiply input by a and store in work
    if (is_dit_v<T1, T2> && W != NULL && i > 0) {
      multiply_a_x_dit(&x[i * idist], work, bs->a.data(), bs->n, bs->n_pad,
                       istride, &W[(bs->n - 1) * (i - 1)]);
    } else {
      multiply_a_x<T1, T2, T1>(&x[i * idist], work, bs->a.data(), bs->n,
                               bs->n_pad, istride);
    }

    exec_convolution(*bs->pf, *bs->pb, work, bs->b.data(), bs->n_pad);

    // Multiply by conjugate of b and store in output vector y
    if (!is_dit_v<T1, T2> && W != NULL && i > 0) {
      multiply_y_bconj_dif(work, &y[i * odist], bs->b_conj.data(), bs->n,
                           ostride, bs->dir, &W[(bs->n - 1) * (i - 1)]);
    } else {
      auto nx = is_r2c_v<T1, T2> ? bs->n / 2 + 1 : bs->n;
      multiply_y_bconj<T1, T2, T2>(work, &y[i * odist], bs->b_conj.data(),
                                   bs->n, ostride, bs->dir, nx);
    }
  }
}

// }}}

// Functions handling real input/output

// {{{ R2C exec for Complex, Complex data
// In R2C case when the input is real the templates in the class definition will
// be used. However when the input is complex (i.e. BS is being used as the
// second factor, applying twiddles) then only the first howmany/2 + 1 rows are
// provided and the remaining howmany - howmany/2 + 1 are given by conjugate
// symmetry. In this case we need a special execute function and a special
// multiply_a_x_c2r function.

template<typename T>
void execute_r2c(const std::complex<T> *x, std::complex<T> *y, int64_t istride,
                 int64_t ostride, const std::complex<T> *W, int64_t howmany,
                 int64_t idist, int64_t odist,
                 const bluestein<T, std::complex<T>> *bs) {

  auto work = get_memory<add_complex_t<T>>(buffer_name::bluestein, bs->n_pad);

  const auto n = bs->n;
  const auto hilim = howmany / 2;

  for (int64_t i = 0; i < howmany; i++) {

    // Multiply input by a and store in work - always apply twiddle factor (DIT
    // not DIF)
    if (i == 0) {
      multiply_a_x<T, std::complex<T>, std::complex<T>>(
          &x[i * idist], work, bs->a.data(), n, bs->n_pad, istride);
    } else if (i < howmany / 2 + 1) {
      multiply_a_x_dit(&x[i * idist], work, bs->a.data(), n, bs->n_pad, istride,
                       &W[(n - 1) * (i - 1)]);
    } else {

      const int64_t hm_halfhi = howmany / 2 + 1;
      const bool want_conj = i >= hm_halfhi;
      const auto ii = want_conj ? (howmany - i) : i;
      const auto xx = &x[ii * idist];

      work[0] = bs->a[0] * std::conj(xx[0]);
      for (int64_t j = 1; j < n; j++) {
        work[j] = bs->a[j] * std::conj(xx[j * istride]) *
                  W[(n - 1) * (i - 1) + j - 1];
      }
      for (int64_t j = n; j < bs->n_pad; j++) {
        work[j] = std::complex<T>(0.0);
      }
    }

    exec_convolution(*bs->pf, *bs->pb, work, bs->b.data(), bs->n_pad);

    // Multiply by conjugate of b and store in output vector y - never apply
    // twiddle factor (DIT not DIF)
    const auto nx = n / 2 + (i <= hilim ? 1 : 0);
    multiply_y_bconj<T, std::complex<T>, std::complex<T>>(
        work, &y[i * odist], bs->b_conj.data(), n, ostride, bs->dir, nx);
  }
}

// }}}

// {{{ C2R exec for Complex, Complex data
// In C2R case when BS is used as the first factor then symmetry is global over
// n*howmany. We need a special execute function to pass an extra pointer for
// the second half of the sequence
template<typename T>
void execute_c2r(const std::complex<T> *x, std::complex<T> *y, int64_t istride,
                 int64_t ostride, const std::complex<T> *W, int64_t howmany,
                 int64_t idist, int64_t odist,
                 const bluestein<std::complex<T>, T> *bs) {

  auto work = get_memory<add_complex_t<T>>(buffer_name::bluestein, bs->n_pad);

  const int64_t n1 = bs->n;
  const int64_t nendmod = howmany / 2 + 1;
  const int64_t ireflect = istride * (n1 / 2);
  const auto nhalfhi = n1 / 2 + 1;

  const auto a = bs->a.data();

  // nhalfhi reads from x[0], rest from X[ireflect] in reverse order and
  // conjugated, multiply by a
  for (int64_t j = 0; j < nhalfhi; j++) {
    work[j] = a[j] * x[j * istride];
  }
  const auto *xx = &x[ireflect];
  for (int64_t j = nhalfhi; j < n1; j++) {
    work[j] = a[j] * std::conj(xx[(j - nhalfhi) * -istride]);
  }
  for (int64_t j = n1; j < bs->n_pad; j++) {
    work[j] = std::complex<T>(0);
  }
  exec_convolution(*bs->pf, *bs->pb, work, bs->b.data(), bs->n_pad);
  multiply_y_bconj<std::complex<T>, T, std::complex<T>>(
      work, &y[0], bs->b_conj.data(), n1, ostride, bs->dir, n1);

  for (int64_t i = 1; i < nendmod; i++) {
    // Multiply input by a and store in work - never apply twiddle factor (DIF
    // not DIT) Nhalfhi reads from x[idist], remainder from x[ireflect - idist]
    for (int64_t j = 0; j < nhalfhi; j++) {
      work[j] = a[j] * x[i * idist + j * istride];
    }
    for (int64_t j = nhalfhi; j < n1; j++) {
      work[j] = a[j] * std::conj(xx[(j - nhalfhi) * -istride - i * idist]);
    }
    for (int64_t j = n1; j < bs->n_pad; j++) {
      work[j] = std::complex<T>(0);
    }

    exec_convolution(*bs->pf, *bs->pb, work, bs->b.data(), bs->n_pad);

    // Multiply by conjugate of b and store in output vector y - always apply
    // twiddle factor (DIF not DIT)
    multiply_y_bconj_dif(work, &y[i * odist], bs->b_conj.data(), n1, ostride,
                         bs->dir, &W[(n1 - 1) * (i - 1)]);
  }
}

// }}}

template struct bluestein<half, std::complex<half>>;
template struct bluestein<std::complex<half>, half>;
template struct bluestein<std::complex<half>, std::complex<half>>;
template struct bluestein<float, std::complex<float>>;
template struct bluestein<std::complex<float>, float>;
template struct bluestein<std::complex<float>, std::complex<float>>;
template struct bluestein<double, std::complex<double>>;
template struct bluestein<std::complex<double>, double>;
template struct bluestein<std::complex<double>, std::complex<double>>;

template<typename Tx, typename Ty>
void level_bluestein_n<Tx, Ty>::execute(const int64_t howmany, const void *x,
                                        const int64_t istride,
                                        const int64_t idist, void *y,
                                        const int64_t ostride,
                                        const int64_t odist) const {
  using Tw = add_complex_t<Tx>;
  auto *X = (const Tx *)x;
  auto *Y = (Ty *)y;
  execute_vanilla(X, Y, istride, ostride, (const Tw *)nullptr, howmany, idist,
                  odist, &bs_);
}

#define LEVEL_BLUESTEIN_N_EXECUTE(Tx, Ty)                                      \
  template void level_bluestein_n<Tx, Ty>::execute(                            \
      const int64_t howmany, const void *x, const int64_t istride,             \
      const int64_t idist, void *y, const int64_t ostride,                     \
      const int64_t odist) const;

LEVEL_BLUESTEIN_N_EXECUTE(half, std::complex<half>)
LEVEL_BLUESTEIN_N_EXECUTE(std::complex<half>, half)
LEVEL_BLUESTEIN_N_EXECUTE(std::complex<half>, std::complex<half>)
LEVEL_BLUESTEIN_N_EXECUTE(float, std::complex<float>)
LEVEL_BLUESTEIN_N_EXECUTE(std::complex<float>, float)
LEVEL_BLUESTEIN_N_EXECUTE(std::complex<float>, std::complex<float>)
LEVEL_BLUESTEIN_N_EXECUTE(double, std::complex<double>)
LEVEL_BLUESTEIN_N_EXECUTE(std::complex<double>, double)
LEVEL_BLUESTEIN_N_EXECUTE(std::complex<double>, std::complex<double>)

#undef LEVEL_BLUESTEIN_N_EXECUTE

template<typename Tx, typename Ty>
void level_bluestein_t<Tx, Ty>::execute(const int64_t howmany, const void *x,
                                        const int64_t istride,
                                        const int64_t idist, void *y,
                                        const int64_t ostride,
                                        const int64_t odist) const {
  using Tw = add_complex_t<Tx>;
  auto *X = (const Tw *)x;
  auto *Y = (Tw *)y;
  auto *W = (const Tw *)twids_->data();
  for (int64_t i = 0; i != howmany; ++i) {
    if constexpr (is_c2c_v<Tx, Ty>) {
      int64_t offsetx = i;
      int64_t offsety = i * (odist / howmany);
      execute_vanilla(X + offsetx, Y + offsety, istride, ostride, W, this->hm_,
                      idist, odist, &bs_);
    } else if constexpr (is_r2c_v<Tx, Ty>) {
      int64_t offsetx = i;
      int64_t offsety = i * (odist / howmany);
      execute_r2c(X + offsetx, Y + offsety, istride, ostride, W, this->hm_,
                  idist, odist, &bs_);
    } else if constexpr (is_c2r_v<Tx, Ty>) {
      int64_t offsetx = i * (idist / howmany);
      int64_t offsety = i;
      execute_c2r(X + offsetx, Y + offsety, istride, ostride, W, this->hm_,
                  idist, odist, &bs_);
    }
  }
}

#define LEVEL_BLUESTEIN_T_EXECUTE(Tx, Ty)                                      \
  template void level_bluestein_t<Tx, Ty>::execute(                            \
      const int64_t howmany, const void *x, const int64_t istride,             \
      const int64_t idist, void *y, const int64_t ostride,                     \
      const int64_t odist) const;

LEVEL_BLUESTEIN_T_EXECUTE(half, std::complex<half>)
LEVEL_BLUESTEIN_T_EXECUTE(std::complex<half>, half)
LEVEL_BLUESTEIN_T_EXECUTE(std::complex<half>, std::complex<half>)
LEVEL_BLUESTEIN_T_EXECUTE(float, std::complex<float>)
LEVEL_BLUESTEIN_T_EXECUTE(std::complex<float>, float)
LEVEL_BLUESTEIN_T_EXECUTE(std::complex<float>, std::complex<float>)
LEVEL_BLUESTEIN_T_EXECUTE(double, std::complex<double>)
LEVEL_BLUESTEIN_T_EXECUTE(std::complex<double>, double)
LEVEL_BLUESTEIN_T_EXECUTE(std::complex<double>, std::complex<double>)

#undef LEVEL_BLUESTEIN_T_EXECUTE
} // end namespace plfft
