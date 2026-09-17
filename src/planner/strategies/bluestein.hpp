/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "cooley_tukey.hpp"
#include "decimation.hpp"
#include "exec_convolution.hpp"
#include "factorize.hpp"
#include "fft_buffers.hpp"
#include "generate_twiddles.hpp"
#include "kernel_data.hpp"
#include "planner/planner.hpp"
#include "planner/problem.hpp"
#include "planner/strategy.hpp"
#include "plfft/fft_plan.hpp"
#include "plfft_complex.hpp"
#include "plfft_pod_vector.hpp"
#include "plfft_util.hpp"

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <cmath>
#include <complex>
#include <utility>

#ifndef NO_LIBCPP
#include <sstream>
#endif // NO_LIBCPP

namespace plfft {

template<typename T>
static void zero_tail(int64_t n, int64_t howmany, int64_t n_pad, T *a) {
  std::fill(a + n * howmany, a + n_pad * howmany, T{});
}

template<typename Tx, typename Ty>
static int64_t bluestein_convolution_length(int64_t n) {
  const auto &factors = get_kernel_ns<Tx, Ty>();
  assert(!factors.empty());
  return next_length_factorable_by(2 * n - 1, factors);
}

template<typename T>
// Compute convolution: a <- ifft(fft(a) .* fft(b)).
static void do_bluestein_convolution(int64_t n, int64_t howmany, T *a,
                                     const pod_vector<T> &b_fft,
                                     const fft_plan_ptr &fwd_plan,
                                     const fft_plan_ptr &bwd_plan) {
  const int64_t astride = howmany;

  // a is a packed batch of howmany interleaved transforms:
  // a[k * howmany + h] is element k of transform h.
  fwd_plan->execute(a, a);

  pointwise_multiply(a, b_fft.data(), n, howmany, astride);

  bwd_plan->execute(a, a);
}

template<typename T>
static void prepare_w(int64_t n, plfft_direction_t dir, pod_vector<T> &w) {
  // w[k] = exp(i*dir*pi*k^2/n), with k^2 reduced mod 2n:
  // same complex value, smaller angle argument for better float accuracy.
  constexpr double PI = consts::pi<double>;
  const double angle = static_cast<int>(dir) * PI / n;
  const int64_t two_n = mul_assert_no_overflow(2, n);

  for (int64_t k = 0; k < n; k++) {
    const int64_t k2_mod_2n = mul_assert_no_overflow(k, k) % two_n;
    const double theta = angle * k2_mod_2n;
    w[k] = T(std::cos(theta), std::sin(theta));
  }
}

template<typename T>
static void prepare_b(int64_t n, int64_t n_pad, const pod_vector<T> &w,
                      pod_vector<T> &b) {
  using scalar_t = typename T::value_type;

  std::fill(b.begin(), b.end(), T{});

  // Store one wrapped period of the convolution kernel:
  // positive offsets at k, negative offsets at n_pad - k.
  // Scale here so the inverse plan does not need a separate divide.
  const scalar_t recip_n_pad = 1.0 / n_pad;
  b[0] = std::conj(w[0]) * recip_n_pad;
  for (int64_t k = 1; k < n; k++) {
    const auto x = std::conj(w[k]) * recip_n_pad;
    b[k] = x;
    b[n_pad - k] = x;
  }
}

template<typename T>
static void prepare_tables(int64_t n, plfft_direction_t dir, int64_t n_pad,
                           pod_vector<T> &w, pod_vector<T> &b,
                           fft_plan_ptr &b_plan) {
  prepare_w(n, dir, w);
  prepare_b(n, n_pad, w, b);

  // precompute fft(b) and then destroy b_plan as it is no longer needed
  b_plan->execute(b.data(), b.data());
  b_plan = fft_plan_ptr{};
}

template<typename Tx, typename Ty>
struct bluestein_plan : public fft_plan {
  using Tw = add_complex_t<Tx>;

  bluestein_plan(int64_t n_, int64_t istride_, int64_t ostride_,
                 int64_t howmany_, int64_t idist_, int64_t odist_,
                 plfft_direction_t dir_, int64_t n_pad_, fft_plan_ptr b_plan_,
                 fft_plan_ptr fwd_plan_, fft_plan_ptr bwd_plan_)
    : n(n_), istride(istride_), ostride(ostride_), howmany(howmany_),
      idist(idist_), odist(odist_), n_pad(n_pad_),
      fwd_plan(std::move(fwd_plan_)), bwd_plan(std::move(bwd_plan_)), w(n),
      b(n_pad) {
    prepare_tables(n, dir_, n_pad, w, b, b_plan_);
  }

  void execute(const void *in, void *out) const override {
    const auto *x = static_cast<const Tx *>(in);
    auto *y = static_cast<Ty *>(out);
    const int64_t astride = howmany;
    const int64_t buffer_size = n_pad * howmany;
    auto *a = get_memory<Tw>(buffer_name::bluestein, buffer_size);

    // Bluestein requires zeros on the padded tail [n, n_pad)
    zero_tail(n, howmany, n_pad, a);

    // compute a <- x .* w
    pack_input(x, a, astride);

    // compute a <- ifft(fft(a) .* fft(b))
    // b <- fft(b) is precomputed
    do_bluestein_convolution(n_pad, howmany, a, b, fwd_plan, bwd_plan);

    // compute y <- a .* w
    unpack_output(y, a, astride);
  }

#ifndef NO_LIBCPP
  std::string plan_to_string() const override {
    std::ostringstream sstm;
    sstm << "(bluestein " << n << ' ';
    sstm << fwd_plan->plan_to_string() << ' ';
    sstm << bwd_plan->plan_to_string() << ')';
    return std::move(sstm).str();
  }
#endif // NO_LIBCPP

  algo_flops flops() const override {
    // TODO: not implemented yet...
    return {};
  }

private:
  void pack_input(const Tx *x, Tw *a, int64_t astride) const
    requires(!is_c2r_v<Tx, Ty>)
  {
    for (int64_t k = 0; k < n; k++) {
      const auto ak = a + k * astride;
      const auto wk = w[k];
      for (int64_t h = 0; h < howmany; h++) {
        const auto xh = x + h * idist;
        ak[h] = xh[k * istride] * wk;
      }
    }
  }

  void pack_input(const Tx *x, Tw *a, int64_t astride) const
    requires is_c2r_v<Tx, Ty>
  {
    const int64_t nhalfhi = n / 2 + 1;

    for (int64_t k = 0; k < nhalfhi; k++) {
      const auto ak = a + k * astride;
      const auto wk = w[k];
      for (int64_t h = 0; h < howmany; h++) {
        const auto xh = x + h * idist;
        ak[h] = xh[k * istride] * wk;
      }
    }

    for (int64_t k = nhalfhi; k < n; k++) {
      const auto ak = a + k * astride;
      const auto wk = w[k];
      for (int64_t h = 0; h < howmany; h++) {
        const auto xh = x + h * idist;
        ak[h] = conj(xh[(n - k) * istride]) * wk;
      }
    }
  }

  void unpack_output(Ty *y, const Tw *a, int64_t astride) const
    requires(!is_c2r_v<Tx, Ty>)
  {
    const int64_t nhalfhi = n / 2 + 1;
    const int64_t nout = is_r2c_v<Tx, Ty> ? nhalfhi : n;

    for (int64_t k = 0; k < nout; k++) {
      const auto ak = a + k * astride;
      const auto wk = w[k];
      for (int64_t h = 0; h < howmany; h++) {
        const auto yh = y + h * odist;
        yh[k * ostride] = ak[h] * wk;
      }
    }
  }

  void unpack_output(Ty *y, const Tw *a, int64_t astride) const
    requires is_c2r_v<Tx, Ty>
  {
    for (int64_t k = 0; k < n; k++) {
      const auto ak = a + k * astride;
      const auto wk = w[k];
      for (int64_t h = 0; h < howmany; h++) {
        const auto yh = y + h * odist;
        yh[k * ostride] = (ak[h] * wk).real();
      }
    }
  }

  const int64_t n;
  const int64_t istride, ostride;
  const int64_t howmany, idist, odist;

  const int64_t n_pad;
  fft_plan_ptr fwd_plan;
  fft_plan_ptr bwd_plan;

  pod_vector<Tw> w;
  pod_vector<Tw> b;
};

// bluestein_plan specialized for howmany == 1.
template<typename Tx, typename Ty>
struct bluestein_single_plan : public fft_plan {
  using Tw = add_complex_t<Tx>;

  bluestein_single_plan(int64_t n_, int64_t istride_, int64_t ostride_,
                        plfft_direction_t dir_, int64_t n_pad_,
                        fft_plan_ptr b_plan_, fft_plan_ptr fwd_plan_,
                        fft_plan_ptr bwd_plan_)
    : n(n_), istride(istride_), ostride(ostride_), n_pad(n_pad_),
      fwd_plan(std::move(fwd_plan_)), bwd_plan(std::move(bwd_plan_)), w(n),
      b(n_pad) {
    prepare_tables(n, dir_, n_pad, w, b, b_plan_);
  }

  void execute(const void *in, void *out) const override {
    const auto *x = static_cast<const Tx *>(in);
    auto *y = static_cast<Ty *>(out);
    auto *a = get_memory<Tw>(buffer_name::bluestein, n_pad);

    // compute a <- x .* w and zero the padded tail [n, n_pad)
    std::fill(a + n, a + n_pad, Tw{});

    pack_input(x, a);

    // compute a <- ifft(fft(a) .* fft(b))
    // b <- fft(b) is precomputed
    fwd_plan->execute(a, a);
    for (int64_t k = 0; k < n_pad; k++) {
      a[k] *= b[k];
    }
    bwd_plan->execute(a, a);

    // compute y <- a .* w
    unpack_output(y, a);
  }

#ifndef NO_LIBCPP
  std::string plan_to_string() const override {
    std::ostringstream sstm;
    sstm << "(bluestein-single " << n << ' ';
    sstm << fwd_plan->plan_to_string() << ' ';
    sstm << bwd_plan->plan_to_string() << ')';
    return std::move(sstm).str();
  }
#endif // NO_LIBCPP

  algo_flops flops() const override {
    // TODO: not implemented yet...
    return {};
  }

private:
  void pack_input(const Tx *x, Tw *a) const
    requires(!is_c2r_v<Tx, Ty>)
  {
    for (int64_t k = 0; k < n; k++) {
      a[k] = x[k * istride] * w[k];
    }
  }

  void pack_input(const Tx *x, Tw *a) const
    requires is_c2r_v<Tx, Ty>
  {
    const int64_t nhalfhi = n / 2 + 1;

    for (int64_t k = 0; k < nhalfhi; k++) {
      a[k] = x[k * istride] * w[k];
    }
    for (int64_t k = nhalfhi; k < n; k++) {
      a[k] = conj(x[(n - k) * istride]) * w[k];
    }
  }

  void unpack_output(Ty *y, const Tw *a) const
    requires(!is_c2r_v<Tx, Ty>)
  {
    const int64_t nout = is_r2c_v<Tx, Ty> ? n / 2 + 1 : n;
    for (int64_t k = 0; k < nout; k++) {
      y[k * ostride] = a[k] * w[k];
    }
  }

  void unpack_output(Ty *y, const Tw *a) const
    requires is_c2r_v<Tx, Ty>
  {
    for (int64_t k = 0; k < n; k++) {
      y[k * ostride] = real(a[k] * w[k]);
    }
  }

  const int64_t n;
  const int64_t istride, ostride;

  const int64_t n_pad;
  fft_plan_ptr fwd_plan;
  fft_plan_ptr bwd_plan;

  pod_vector<Tw> w;
  pod_vector<Tw> b;
};

template<typename Tx, typename Ty,
         transform_kind = transform_kind_from_io_types<Tx, Ty>()>
struct bluestein_twid_plan;

template<typename Tx, typename Ty>
struct bluestein_twid_plan<Tx, Ty, transform_kind::c2c> : public fft_plan {
  using Tw = add_complex_t<Ty>;
  static constexpr twiddle_layout twid_layout{
      runtime_precision_from_real_type<remove_complex_t<Tw>>(),
      false,
      1,
  };

  bluestein_twid_plan(int64_t n_, int64_t istride_, int64_t ostride_,
                      int64_t howmany0_, int64_t idist0_, int64_t odist0_,
                      int64_t howmany1_, plfft_direction_t dir_, int64_t n_pad_,
                      fft_plan_ptr b_plan_, fft_plan_ptr fwd_plan_,
                      fft_plan_ptr bwd_plan_)
    : n(n_), istride(istride_), ostride(ostride_), howmany0(howmany0_),
      idist0(idist0_), odist0(odist0_), howmany1(howmany1_), n_pad(n_pad_),
      w(n), b(n_pad),
      twiddles(generate_twiddles(dir_, howmany0_, n, twid_layout)),
      fwd_plan(std::move(fwd_plan_)), bwd_plan(std::move(bwd_plan_)) {
    static_assert(is_c2c_v<Tx, Ty>);
    prepare_tables(n, dir_, n_pad, w, b, b_plan_);
  }

  void execute(const void *in, void *out) const override {
    const auto *x = static_cast<const Ty *>(in);
    auto *y = static_cast<Ty *>(out);
    const int64_t astride = howmany0;
    const auto buffer_size = n_pad * astride;
    auto *a = get_memory<Tw>(buffer_name::bluestein, buffer_size);

    for (int64_t h1 = 0; h1 < howmany1; h1++) {
      // Bluestein requires zeros on the padded tail [n, n_pad)
      zero_tail(n, howmany0, n_pad, a);

      // compute a <- x .* w
      pack_input(x + h1, a, astride);

      // compute a <- ifft(fft(a) .* fft(b))
      // b <- fft(b) is precomputed
      do_bluestein_convolution(n_pad, howmany0, a, b, fwd_plan, bwd_plan);

      // compute y <- a .* w
      unpack_output(y + h1, a, astride);
    }
  }

#ifndef NO_LIBCPP
  std::string plan_to_string() const override {
    std::ostringstream sstm;
    sstm << "(bluestein-twid-c2c " << n << ' ';
    sstm << fwd_plan->plan_to_string() << ' ';
    sstm << bwd_plan->plan_to_string() << ')';
    return std::move(sstm).str();
  }
#endif // NO_LIBCPP

  algo_flops flops() const override {
    // TODO: not implemented yet...
    return {};
  }

private:
  void pack_input(const Ty *x, Tw *a, int64_t astride) const {
    const auto twids = static_cast<const Tw *>(twiddles->data());
    for (int64_t k = 0; k < n; k++) {
      const auto ak = a + k * astride;
      const auto wk = w[k];
      for (int64_t h = 0; h < howmany0; h++) {
        const auto xh = x + h * idist0;
        auto xk = xh[k * istride] * wk;
        if (k > 0 && h > 0) {
          xk *= twids[(h - 1) * (n - 1) + (k - 1)];
        }
        ak[h] = xk;
      }
    }
  }

  void unpack_output(Ty *y, const Tw *a, int64_t astride) const {
    for (int64_t k = 0; k < n; k++) {
      const auto ak = a + k * astride;
      const auto wk = w[k];
      for (int64_t h = 0; h < howmany0; h++) {
        const auto yh = y + h * odist0;
        yh[k * ostride] = ak[h] * wk;
      }
    }
  }

  const int64_t n;
  const int64_t istride, ostride;
  // outer batch dimension parameters
  const int64_t howmany0, idist0, odist0;
  // inner dimension parameters
  const int64_t howmany1;

  const int64_t n_pad;
  pod_vector<Tw> w;
  pod_vector<Tw> b;
  const pod_vector<void> *twiddles;
  fft_plan_ptr fwd_plan;
  fft_plan_ptr bwd_plan;
};

template<typename Tx, typename Ty>
struct bluestein_twid_plan<Tx, Ty, transform_kind::r2c> : public fft_plan {
  using Tw = add_complex_t<Ty>;
  static constexpr twiddle_layout twid_layout{
      runtime_precision_from_real_type<remove_complex_t<Tw>>(),
      false,
      1,
  };

  bluestein_twid_plan(int64_t n_, int64_t istride_, int64_t ostride_,
                      int64_t howmany0_, int64_t idist0_, int64_t odist0_,
                      int64_t howmany1_, plfft_direction_t dir_, int64_t n_pad_,
                      fft_plan_ptr b_plan_, fft_plan_ptr fwd_plan_,
                      fft_plan_ptr bwd_plan_)
    : n(n_), istride(istride_), ostride(ostride_), howmany0(howmany0_),
      idist0(idist0_), odist0(odist0_), howmany1(howmany1_), n_pad(n_pad_),
      w(n), b(n_pad),
      twiddles(generate_twiddles(dir_, howmany0, n, twid_layout)),
      fwd_plan(std::move(fwd_plan_)), bwd_plan(std::move(bwd_plan_)) {
    static_assert(is_r2c_v<Tx, Ty>);
    prepare_tables(n, dir_, n_pad, w, b, b_plan_);
  }

  void execute(const void *in, void *out) const override {
    // The twid plan consumes complex data, while Tx and Ty refer to the types
    // of the full transform; in the r2c case, Tx is real.
    const auto *x = static_cast<const Ty *>(in);
    auto *y = static_cast<Ty *>(out);
    const int64_t astride = howmany0;
    const auto buffer_size = n_pad * howmany0;
    auto *a = get_memory<Tw>(buffer_name::bluestein, buffer_size);

    for (int64_t h1 = 0; h1 < howmany1; h1++) {
      // Bluestein requires zeros on the padded tail [n, n_pad)
      zero_tail(n, howmany0, n_pad, a);

      // compute a <- x .* w
      pack_input(x + h1, a, astride);

      // compute a .* b = ifft(fft(a) .* fft(b))
      // b <- fft(b) is precomputed
      do_bluestein_convolution(n_pad, howmany0, a, b, fwd_plan, bwd_plan);

      // compute y <- a .* w
      unpack_output(y + h1, a, astride);
    }
  }

#ifndef NO_LIBCPP
  std::string plan_to_string() const override {
    std::ostringstream sstm;
    sstm << "(bluestein-twid-r2c " << n << ' ';
    sstm << fwd_plan->plan_to_string() << ' ';
    sstm << bwd_plan->plan_to_string() << ')';
    return std::move(sstm).str();
  }
#endif // NO_LIBCPP

  algo_flops flops() const override {
    // TODO: not implemented yet...
    return {};
  }

private:
  void pack_input(const Ty *x, Tw *a, int64_t astride) const {
    const int64_t halfhi = howmany0 / 2 + 1;
    const auto twids = static_cast<const Tw *>(twiddles->data());

    {
      const auto ak = a;
      const auto wk = w[0];
      for (int64_t h = 0; h < halfhi; h++) {
        const auto xh = x + h * idist0;
        ak[h] = xh[0] * wk;
      }
      for (int64_t h = halfhi; h < howmany0; h++) {
        const auto xh = x + (howmany0 - h) * idist0;
        ak[h] = conj(xh[0]) * wk;
      }
    }

    for (int64_t k = 1; k < n; k++) {
      const auto ak = a + k * astride;
      const auto wk = w[k];
      ak[0] = x[k * istride] * wk;
      for (int64_t h = 1; h < halfhi; h++) {
        const auto xh = x + h * idist0;
        const auto twk = twids[(h - 1) * (n - 1) + (k - 1)];
        ak[h] = xh[k * istride] * wk * twk;
      }
      for (int64_t h = halfhi; h < howmany0; h++) {
        const auto xh = x + (howmany0 - h) * idist0;
        const auto twk = twids[(h - 1) * (n - 1) + (k - 1)];
        ak[h] = conj(xh[k * istride]) * wk * twk;
      }
    }
  }

  void unpack_output(Ty *y, const Tw *a, int64_t astride) const {
    const int64_t halfhi = n / 2 + 1;
    const int64_t hlast = n % 2 == 0 ? 1 : howmany0 / 2 + 1;

    for (int64_t k = 0; k < halfhi - 1; k++) {
      const auto ak = a + k * astride;
      const auto wk = w[k];
      for (int64_t h = 0; h < howmany0; h++) {
        const auto yh = y + h * odist0;
        yh[k * ostride] = ak[h] * wk;
      }
    }

    const auto ak = a + (halfhi - 1) * astride;
    const auto wk = w[halfhi - 1];
    for (int64_t h = 0; h < hlast; h++) {
      const auto yh = y + h * odist0;
      yh[(halfhi - 1) * ostride] = ak[h] * wk;
    }
  }

  const int64_t n;
  const int64_t istride, ostride;
  // outer batch dimension parameters
  const int64_t howmany0, idist0, odist0;
  // inner dimension parameters
  const int64_t howmany1;

  const int64_t n_pad;
  pod_vector<Tw> w;
  pod_vector<Tw> b;
  const pod_vector<void> *twiddles;
  fft_plan_ptr fwd_plan;
  fft_plan_ptr bwd_plan;
};

template<typename Tx, typename Ty>
struct bluestein_twid_plan<Tx, Ty, transform_kind::c2r> : public fft_plan {
  using Tw = add_complex_t<Tx>;
  static constexpr twiddle_layout twid_layout{
      runtime_precision_from_real_type<remove_complex_t<Tw>>(),
      false,
      1,
  };

  bluestein_twid_plan(int64_t n_, int64_t istride_, int64_t ostride_,
                      int64_t howmany0_, int64_t idist0_, int64_t odist0_,
                      int64_t howmany1_, plfft_direction_t dir_, int64_t n_pad_,
                      fft_plan_ptr b_plan_, fft_plan_ptr fwd_plan_,
                      fft_plan_ptr bwd_plan_)
    : n(n_), istride(istride_), ostride(ostride_), howmany0(howmany0_),
      idist0(idist0_), odist0(odist0_), howmany1(howmany1_), n_pad(n_pad_),
      w(n), b(n_pad),
      twiddles(generate_twiddles(dir_, howmany0, n, twid_layout)),
      fwd_plan(std::move(fwd_plan_)), bwd_plan(std::move(bwd_plan_)) {
    static_assert(is_c2r_v<Tx, Ty>);
    prepare_tables(n, dir_, n_pad, w, b, b_plan_);
  }

  void execute(const void *in, void *out) const override {
    // The twid plan consumes complex data, while Tx and Ty refer to the types
    // of the full transform; in the c2r case, Ty is real.
    const auto *x = static_cast<const Tx *>(in);
    auto *y = static_cast<Tx *>(out);
    const int64_t astride = howmany0 / 2 + 1;
    const auto buffer_size = n_pad * astride;
    auto *a = get_memory<Tw>(buffer_name::bluestein, buffer_size);

    for (int64_t h1 = 0; h1 < howmany1; h1++) {
      // Bluestein requires zeros on the padded tail [n, n_pad)
      zero_tail(n, astride, n_pad, a);

      // compute a <- x .* w
      pack_input(x + h1, a, astride);

      // compute a .* b = ifft(fft(a) .* fft(b))
      // b <- fft(b) is precomputed
      do_bluestein_convolution(n_pad, astride, a, b, fwd_plan, bwd_plan);

      // compute y <- a .* w
      unpack_output(y + h1, a, astride);
    }
  }

#ifndef NO_LIBCPP
  std::string plan_to_string() const override {
    std::ostringstream sstm;
    sstm << "(bluestein-twid-c2r " << n << ' ';
    sstm << fwd_plan->plan_to_string() << ' ';
    sstm << bwd_plan->plan_to_string() << ')';
    return std::move(sstm).str();
  }
#endif // NO_LIBCPP

  algo_flops flops() const override {
    // TODO: not implemented yet...
    return {};
  }

private:
  void pack_input(const Tx *x, Tw *a, int64_t astride) const {
    const int64_t total_n = n * howmany0;
    const int64_t nhalfhi = total_n / 2 + 1;
    const int64_t k_split = nhalfhi / howmany0;
    const int64_t h_split = nhalfhi % howmany0;

    for (int64_t k = 0; k < k_split; k++) {
      const auto ak = a + k * astride;
      const auto wk = w[k];
      for (int64_t h = 0; h < astride; h++) {
        const auto global_k = k * howmany0 + h;
        ak[h] = x[global_k * idist0] * wk;
      }
    }

    if (k_split < n && h_split != 0) {
      const auto ak = a + k_split * astride;
      const auto wk = w[k_split];
      for (int64_t h = 0; h < h_split; h++) {
        const auto global_k = k_split * howmany0 + h;
        ak[h] = x[global_k * idist0] * wk;
      }
      for (int64_t h = h_split; h < astride; h++) {
        const auto global_k = k_split * howmany0 + h;
        ak[h] = conj(x[(total_n - global_k) * idist0]) * wk;
      }
    }

    for (int64_t k = k_split + (h_split != 0); k < n; k++) {
      const auto ak = a + k * astride;
      const auto wk = w[k];
      for (int64_t h = 0; h < astride; h++) {
        const auto global_k = k * howmany0 + h;
        ak[h] = conj(x[(total_n - global_k) * idist0]) * wk;
      }
    }
  }

  void unpack_output(Tx *y, const Tw *a, int64_t astride) const {
    const int64_t halfhi = howmany0 / 2 + 1;
    const auto twids = static_cast<const Tw *>(twiddles->data());

    for (int64_t k = 0; k < n; k++) {
      y[k * ostride] = a[k * astride] * w[k];
    }

    for (int64_t h = 1; h < halfhi; h++) {
      const auto yh = y + h * odist0;
      yh[0] = a[h] * w[0];
      for (int64_t k = 1; k < n; k++) {
        const auto twk = twids[(h - 1) * (n - 1) + (k - 1)];
        yh[k * ostride] = a[k * astride + h] * w[k] * twk;
      }
    }
  }

  const int64_t n;
  const int64_t istride, ostride;
  // outer batch dimension parameters
  const int64_t howmany0, idist0, odist0;
  // inner dimension parameters
  const int64_t howmany1;

  const int64_t n_pad;
  pod_vector<Tw> w;
  pod_vector<Tw> b;
  const pod_vector<void> *twiddles;
  fft_plan_ptr fwd_plan;
  fft_plan_ptr bwd_plan;
};

template<typename Tx, typename Ty>
struct bluestein_strategy : public strategy<Tx, Ty> {
  fft_plan_ptr make_plan(const problem &p,
                         plfft::planner &planner) const override {
    using single_plan_t = bluestein_single_plan<Tx, Ty>;
    using batched_plan_t = bluestein_plan<Tx, Ty>;
    using Tw = typename batched_plan_t::Tw;

    if (!is_applicable(p, planner)) {
      return nullptr;
    }

    const auto [_, prec, n, istride, ostride, howmany, idist, odist, dir] = p;
    const auto n_pad = bluestein_convolution_length<Tx, Ty>(n);
    auto subplanner = planner.without(ALLOW_CONVOLUTIONS);

    auto b_plan = subplanner.make_plan(
        make_problem<Tw, Tw>(n_pad, 1, 1, 1, 0, 0, PLFFT_FORWARD));
    auto fwd_plan = subplanner.make_plan(make_problem<Tw, Tw>(
        n_pad, howmany, howmany, howmany, 1, 1, PLFFT_FORWARD));
    auto bwd_plan = subplanner.make_plan(make_problem<Tw, Tw>(
        n_pad, howmany, howmany, howmany, 1, 1, PLFFT_BACKWARD));

    if (!b_plan || !fwd_plan || !bwd_plan) {
      return nullptr;
    }

    if (howmany == 1) {
      return plfft::make_unique<single_plan_t>(
          n, istride, ostride, dir, n_pad, std::move(b_plan),
          std::move(fwd_plan), std::move(bwd_plan));
    }

    return plfft::make_unique<batched_plan_t>(
        n, istride, ostride, howmany, idist, odist, dir, n_pad,
        std::move(b_plan), std::move(fwd_plan), std::move(bwd_plan));
  }

private:
  bool is_applicable(const problem &p, const plfft::planner &planner) const {
    const auto [_, prec, n, istride, ostride, howmany, idist, odist, dir] = p;
    const auto ns = get_kernel_ns<Tx, Ty>();

    return (
        /* correctness constraints */

        /* profitability constraints (heuristics) */

        // no nested Rader/Bluestein
        planner.has(ALLOW_CONVOLUTIONS) &&
        // prefer direct kernels when available
        !is_base_n(n, ns) &&

        /* expensive correctness constraints (check last) */

        // can solve only odd prime-sized problems
        n > 2 && planner.is_prime(n));
  }
};

template<typename Tx, typename Ty>
struct cooley_tukey_bluestein_ab : public strategy<Tx, Ty> {
  fft_plan_ptr make_plan(const problem &p,
                         plfft::planner &planner) const override {
    using plan_t = cooley_tukey_plan<Tx, Ty>;
    using twid_plan_t = bluestein_twid_plan<Tx, Ty>;
    using Tw = typename plan_t::Tw;

    const auto [_, prec, n, istride, ostride, howmany, idist, odist, dir] = p;
    if (!is_applicable(p, planner)) {
      return nullptr;
    }

    const auto r = planner.smallest_prime_factor(n);
    const auto m = n / r;
    const auto ahowmany = is_c2r_v<Tx, Ty> ? (m / 2 + 1) : m;
    const auto n_pad = bluestein_convolution_length<Tx, Ty>(r);
    auto subplanner = planner.without(ALLOW_CONVOLUTIONS);

    auto b_plan = subplanner.make_plan(
        make_problem<Tw, Tw>(n_pad, 1, 1, 1, 0, 0, PLFFT_FORWARD));
    auto fwd_plan = subplanner.make_plan(make_problem<Tw, Tw>(
        n_pad, ahowmany, ahowmany, ahowmany, 1, 1, PLFFT_FORWARD));
    auto bwd_plan = subplanner.make_plan(make_problem<Tw, Tw>(
        n_pad, ahowmany, ahowmany, ahowmany, 1, 1, PLFFT_BACKWARD));
    if (!b_plan || !fwd_plan || !bwd_plan) {
      return nullptr;
    }

    fft_plan_ptr stage1;
    fft_plan_ptr stage2;

    if constexpr (is_dit_v<Tx, Ty>) {
      stage1 = planner.make_plan(
          make_problem<Tx, Ty>(m, istride * r, r, r, istride, 1, dir));
      stage2 = plfft::make_unique<twid_plan_t>(
          r, 1, ostride * m, m, r, ostride, 1, dir, n_pad, std::move(b_plan),
          std::move(fwd_plan), std::move(bwd_plan));
    } else {
      stage1 = plfft::make_unique<twid_plan_t>(
          r, istride * m, 1, m, istride, r, 1, dir, n_pad, std::move(b_plan),
          std::move(fwd_plan), std::move(bwd_plan));
      stage2 = planner.make_plan(
          make_problem<Tx, Ty>(m, r, ostride * r, r, 1, ostride, dir));
    }

    if (!stage1 || !stage2) {
      return nullptr;
    }

    return plfft::make_unique<plan_t>(n, 1, std::move(stage1),
                                      std::move(stage2));
  }

private:
  bool is_applicable(const problem &p, const plfft::planner &planner) const {
    const auto [_, prec, n, istride, ostride, howmany, idist, odist, dir] = p;
    const auto ns = get_kernel_ns<Tx, Ty>();

    return (
        /* correctness constraints */

        // single batch only
        howmany == 1 &&

        /* profitability constraints (heuristics) */

        // no nested Rader/Bluestein
        planner.has(ALLOW_CONVOLUTIONS) &&

        /* expensive correctness constraints (check last) */

        // n is not prime
        n > 2 && !planner.is_prime(n) &&
        // prefer direct kernels when available
        !is_base_n(planner.smallest_prime_factor(n), ns));
  }
};

template<typename Tx, typename Ty>
struct cooley_tukey_bluestein_ac : public strategy<Tx, Ty> {
  fft_plan_ptr make_plan(const problem &p,
                         plfft::planner &planner) const override {
    using plan_t = cooley_tukey_plan<Tx, Ty>;
    using twid_plan_t = bluestein_twid_plan<Tx, Ty>;
    using Tw = typename plan_t::Tw;

    const auto [_, prec, n, istride, ostride, howmany, idist, odist, dir] = p;
    if (!is_applicable(p, planner)) {
      return nullptr;
    }

    const auto r = planner.smallest_prime_factor(n);
    const auto m = n / r;
    const auto ahowmany = is_c2r_v<Tx, Ty> ? (m / 2 + 1) : m;
    const auto n_pad = bluestein_convolution_length<Tx, Ty>(r);
    auto subplanner = planner.without(ALLOW_CONVOLUTIONS);

    auto b_plan = subplanner.make_plan(
        make_problem<Tw, Tw>(n_pad, 1, 1, 1, 0, 0, PLFFT_FORWARD));
    auto fwd_plan = subplanner.make_plan(make_problem<Tw, Tw>(
        n_pad, ahowmany, ahowmany, ahowmany, 1, 1, PLFFT_FORWARD));
    auto bwd_plan = subplanner.make_plan(make_problem<Tw, Tw>(
        n_pad, ahowmany, ahowmany, ahowmany, 1, 1, PLFFT_BACKWARD));
    if (!b_plan || !fwd_plan || !bwd_plan) {
      return nullptr;
    }

    fft_plan_ptr stage1;
    fft_plan_ptr stage2;

    if constexpr (is_dit_v<Tx, Ty>) {
      stage1 = planner.make_plan(make_problem<Tx, Ty>(
          m, istride * r, howmany * r, howmany * r, idist, 1, dir));
      stage2 = plfft::make_unique<twid_plan_t>(
          r, howmany, ostride * m, m, howmany * r, ostride, howmany, dir, n_pad,
          std::move(b_plan), std::move(fwd_plan), std::move(bwd_plan));
    } else {
      stage1 = plfft::make_unique<twid_plan_t>(
          r, istride * m, howmany, m, istride, howmany * r, howmany, dir, n_pad,
          std::move(b_plan), std::move(fwd_plan), std::move(bwd_plan));
      stage2 = planner.make_plan(make_problem<Tx, Ty>(
          m, howmany * r, ostride * r, howmany * r, 1, odist, dir));
    }

    if (!stage1 || !stage2) {
      return nullptr;
    }

    return plfft::make_unique<plan_t>(n, howmany, std::move(stage1),
                                      std::move(stage2));
  }

private:
  bool is_applicable(const problem &p, const plfft::planner &planner) const {
    const auto [_, prec, n, istride, ostride, howmany, idist, odist, dir] = p;
    const auto ns = get_kernel_ns<Tx, Ty>();

    return (
        /* correctness constraints */

        // DIT: input batches are packed, output batches are interleaved
        ((is_dit_v<Tx, Ty> && istride == howmany * idist && odist == 1) ||
         // DIF: input batches are interleaved, output batches are packed
         (!is_dit_v<Tx, Ty> && idist == 1 && ostride == howmany * odist)) &&

        /* profitability constraints (heuristics) */

        // multiple batches only
        howmany > 1 &&
        // no nested Rader/Bluestein
        planner.has(ALLOW_CONVOLUTIONS) &&

        /* expensive correctness constraints (check last) */

        // n is not prime
        n > 2 && !planner.is_prime(n) &&
        // prefer direct kernels when available
        !is_base_n(planner.smallest_prime_factor(n), ns));
  }
};

} // namespace plfft
