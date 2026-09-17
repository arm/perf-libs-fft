/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "cooley_tukey.hpp"
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
#include "rader_generator.hpp"

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
// Compute convolution: a <- ifft(fft(a) .* fft(b)).
static void do_rader_convolution(int64_t n, int64_t howmany, int64_t astride,
                                 T *a, const pod_vector<T> &b_fft,
                                 const fft_plan_ptr &fwd_plan,
                                 const fft_plan_ptr &bwd_plan) {
  // a is a packed batch of howmany interleaved transforms:
  // a[k * astride + h] is element k of transform h.
  fwd_plan->execute(a, a);

  pointwise_multiply(a, b_fft.data(), n, howmany, astride);

  bwd_plan->execute(a, a);
}

static inline void
prepare_rader_permutations(int64_t n, int64_t m, pod_vector<int64_t> &perm,
                           pod_vector<int64_t> &perm_rev_idx) {
  int64_t g = find_group_generator(n); // g generates <Z/nZ, *>
  int64_t gkmodn = 1;                  // g^k mod n

  // perm packs x[1:] in generator order, perm_rev_idx undoes it.
  for (int64_t k = 0; k < m; k++) {
    perm[k] = gkmodn;
    perm_rev_idx[gkmodn] = (m - k) % m;
    gkmodn = gkmodn * g % n;
  }
}

template<typename T>
static void prepare_rader_b(int64_t n, plfft_direction_t dir, int64_t m,
                            const pod_vector<int64_t> &perm, pod_vector<T> &b) {
  using scalar_t = typename T::value_type;

  constexpr double PI = consts::pi<double>;
  const double angle = static_cast<int>(dir) * 2.0 * PI / n;
  const scalar_t recip_m = 1.0 / m;

  for (int64_t k = 0; k < m; k++) {
    const double theta = angle * perm[k];
    b[(m - k) % m] = T(std::cos(theta), std::sin(theta)) * recip_m;
  }
}

template<typename T>
static void prepare_rader_tables(int64_t n, plfft_direction_t dir, int64_t m,
                                 pod_vector<int64_t> &perm,
                                 pod_vector<int64_t> &perm_rev_idx,
                                 pod_vector<T> &b, fft_plan_ptr &b_plan) {
  prepare_rader_permutations(n, m, perm, perm_rev_idx);
  prepare_rader_b(n, dir, m, perm, b);

  // precompute fft(b) and then destroy b_plan as it is no longer needed
  b_plan->execute(b.data(), b.data());
  b_plan = fft_plan_ptr{};
}

template<typename Tx, typename Ty>
struct rader_plan : public fft_plan {
  using Tw = add_complex_t<Tx>;

  rader_plan(int64_t n_, int64_t istride_, int64_t ostride_, int64_t howmany_,
             int64_t idist_, int64_t odist_, plfft_direction_t dir_, int64_t m_,
             int64_t astride_, fft_plan_ptr b_plan_, fft_plan_ptr fwd_plan_,
             fft_plan_ptr bwd_plan_)
    : n(n_), istride(istride_), ostride(ostride_), astride(astride_),
      howmany(howmany_), idist(idist_), odist(odist_), m(m_),
      fwd_plan(std::move(fwd_plan_)), bwd_plan(std::move(bwd_plan_)), perm(m),
      perm_rev_idx(n), b(m) {
    prepare_rader_tables(n, dir_, m, perm, perm_rev_idx, b, b_plan_);
  }

  void execute(const void *in, void *out) const override {
    const auto *x = static_cast<const Tx *>(in);
    auto *y = static_cast<Ty *>(out);
    // reserve two extra rows for values needed after convolution
    auto *a = get_memory<Tw>(buffer_name::rader, (m + 2) * astride);
    auto *x0 = a + m * astride;
    auto *y0 = x0 + astride;

    pack_and_save_input(x, a, x0, y0, n, howmany, idist, istride, astride, m,
                        perm.data());

    // compute a <- ifft(fft(a) .* fft(b))
    // b <- fft(b) is precomputed
    do_rader_convolution(m, howmany, astride, a, b, fwd_plan, bwd_plan);

    // compute y[1:] <- x[0] + unpermute(a)
    unpack_output(y, a, x0, y0, n, howmany, odist, ostride, astride,
                  perm_rev_idx.data());
  }

#ifndef NO_LIBCPP
  std::string plan_to_string() const override {
    std::ostringstream sstm;
    sstm << "(rader " << n << ' ';
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
  PLFFT_ALWAYS_INLINE static void
  pack_and_save_input(const Tx *__restrict__ x, Tw *__restrict__ a,
                      Tw *__restrict__ x0, Tw *__restrict__ y0, int64_t /* n */,
                      int64_t howmany, int64_t idist, int64_t istride,
                      int64_t astride, int64_t m, const int64_t *perm)
    requires(!is_c2r_v<Tx, Ty>)
  {
    // Pack each transform, x[h], into a. Save x[h][0] for unpack_output, and
    // compute y[h][0] while the input and packed values are still available.
    for (int64_t h = 0; h < howmany; h++) {
      auto xh = x + h * idist;

      // save x[h][0] here, as convolution may clobber the buffer containing x
      x0[h] = xh[0];

      // compute y0[h] <- sum(x[h]) before convolution overwrites a
      auto sum = xh[0];

      // compute a[:, h] <- permute(x[h][1:])
      for (int64_t k = 0; k < m; k++) {
        const auto xk = xh[perm[k] * istride];
        a[k * astride + h] = xk;
        sum += xk;
      }

      y0[h] = sum;
    }
  }

  PLFFT_ALWAYS_INLINE static void
  pack_and_save_input(const Tx *__restrict__ x, Tw *__restrict__ a,
                      Tw *__restrict__ x0, Tw *__restrict__ y0, int64_t n,
                      int64_t howmany, int64_t idist, int64_t istride,
                      int64_t astride, int64_t m, const int64_t *perm)
    requires is_c2r_v<Tx, Ty>
  {
    const int64_t nhalfhi = n / 2 + 1;

    // Pack each transform, x[h], into a. Save x[h][0] for unpack_output, and
    // compute y[h][0] while the input and packed values are still available.
    for (int64_t h = 0; h < howmany; h++) {
      auto xh = x + h * idist;

      // save x[h][0] here, as convolution may clobber the buffer containing x
      x0[h] = xh[0];

      // compute y0[h] <- sum(x[h]) before convolution overwrites a
      auto sum = xh[0];

      // compute a[:, h] <- permute(x[h][1:])
      for (int64_t k = 0; k < m; k++) {
        const auto pk = perm[k];
        const auto xk =
            pk < nhalfhi ? xh[pk * istride] : conj(xh[(n - pk) * istride]);
        a[k * astride + h] = xk;
        sum += xk;
      }

      y0[h] = sum;
    }
  }

  PLFFT_ALWAYS_INLINE static void
  unpack_output(Ty *__restrict__ y, const Tw *__restrict__ a,
                const Tw *__restrict__ x0, const Tw *__restrict__ y0, int64_t n,
                int64_t howmany, int64_t odist, int64_t ostride,
                int64_t astride, const int64_t *perm_rev_idx)
    requires(!is_c2r_v<Tx, Ty>)
  {
    const int64_t ny = is_r2c_v<Tx, Ty> ? (n / 2 + 1) : n;
    for (int64_t h = 0; h < howmany; h++) {
      auto yh = y + h * odist;
      auto ah = a + h;

      yh[0] = y0[h];
      for (int64_t k = 1; k < ny; k++) {
        yh[k * ostride] = x0[h] + ah[perm_rev_idx[k] * astride];
      }
    }
  }

  PLFFT_ALWAYS_INLINE static void
  unpack_output(Ty *__restrict__ y, const Tw *__restrict__ a,
                const Tw *__restrict__ x0, const Tw *__restrict__ y0, int64_t n,
                int64_t howmany, int64_t odist, int64_t ostride,
                int64_t astride, const int64_t *perm_rev_idx)
    requires is_c2r_v<Tx, Ty>
  {
    for (int64_t h = 0; h < howmany; h++) {
      auto yh = y + h * odist;
      auto ah = a + h;

      yh[0] = real(y0[h]);
      for (int64_t k = 1; k < n; k++) {
        yh[k * ostride] = real(x0[h] + ah[perm_rev_idx[k] * astride]);
      }
    }
  }

  const int64_t n;
  const int64_t istride, ostride, astride;
  const int64_t howmany, idist, odist;
  const int64_t m;
  fft_plan_ptr fwd_plan;
  fft_plan_ptr bwd_plan;

  pod_vector<int64_t> perm, perm_rev_idx;
  pod_vector<Tw> b;
};

// rader_plan specialized for howmany == 1.
template<typename Tx, typename Ty>
struct rader_single_plan : public fft_plan {
  using Tw = add_complex_t<Tx>;

  rader_single_plan(int64_t n_, int64_t istride_, int64_t ostride_,
                    plfft_direction_t dir_, int64_t m_, fft_plan_ptr b_plan_,
                    fft_plan_ptr fwd_plan_, fft_plan_ptr bwd_plan_)
    : n(n_), istride(istride_), ostride(ostride_), m(m_),
      fwd_plan(std::move(fwd_plan_)), bwd_plan(std::move(bwd_plan_)), perm(m),
      perm_rev_idx(n), b(m) {
    prepare_rader_tables(n, dir_, m, perm, perm_rev_idx, b, b_plan_);
  }

  void execute(const void *in, void *out) const override {
    const auto *x = static_cast<const Tx *>(in);
    auto *y = static_cast<Ty *>(out);
    auto *a = get_memory<Tw>(buffer_name::rader, m);

    Tw x0, y0;
    pack_and_save_input(x, a, x0, y0, n, m, istride, perm.data());

    // compute a <- ifft(fft(a) .* fft(b))
    // b <- fft(b) is precomputed
    fwd_plan->execute(a, a);
    for (int64_t k = 0; k < m; k++) {
      a[k] *= b[k];
    }
    bwd_plan->execute(a, a);

    // compute y[1:] <- x[0] + unpermute(a)
    unpack_output(y, a, x0, y0, n, ostride, perm_rev_idx.data());
  }

#ifndef NO_LIBCPP
  std::string plan_to_string() const override {
    std::ostringstream sstm;
    sstm << "(rader-single " << n << ' ';
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
  PLFFT_ALWAYS_INLINE static void
  pack_and_save_input(const Tx *__restrict__ x, Tw *__restrict__ a,
                      Tw &__restrict__ x0, Tw &__restrict__ y0, int64_t /* n */,
                      int64_t m, int64_t istride, const int64_t *perm)
    requires(!is_c2r_v<Tx, Ty>)
  {
    x0 = x[0];
    y0 = x0;

    // compute a <- permute(x[1:]) and y[0] <- sum(x)
    for (int64_t k = 0; k < m; k++) {
      const auto xk = x[perm[k] * istride];
      a[k] = xk;
      y0 += xk;
    }
  }

  PLFFT_ALWAYS_INLINE static void
  pack_and_save_input(const Tx *__restrict__ x, Tw *__restrict__ a,
                      Tw &__restrict__ x0, Tw &__restrict__ y0, int64_t n,
                      int64_t m, int64_t istride, const int64_t *perm)
    requires is_c2r_v<Tx, Ty>
  {
    const int64_t nhalfhi = n / 2 + 1;

    x0 = x[0];
    y0 = x0;

    // compute a <- permute(x[1:]) and y[0] <- sum(x)
    for (int64_t k = 0; k < m; k++) {
      const auto pk = perm[k];
      const auto xk =
          pk < nhalfhi ? x[pk * istride] : conj(x[(n - pk) * istride]);
      a[k] = xk;
      y0 += xk;
    }
  }

  PLFFT_ALWAYS_INLINE static void
  unpack_output(Ty *__restrict__ y, const Tw *__restrict__ a,
                const Tw &__restrict__ x0, const Tw &__restrict__ y0, int64_t n,
                int64_t ostride, const int64_t *perm_rev_idx)
    requires(!is_c2r_v<Tx, Ty>)
  {
    const int64_t ny = is_r2c_v<Tx, Ty> ? n / 2 + 1 : n;
    y[0] = y0;
    for (int64_t k = 1; k < ny; k++) {
      y[k * ostride] = x0 + a[perm_rev_idx[k]];
    }
  }

  PLFFT_ALWAYS_INLINE static void
  unpack_output(Ty *__restrict__ y, const Tw *__restrict__ a,
                const Tw &__restrict__ x0, const Tw &__restrict__ y0, int64_t n,
                int64_t ostride, const int64_t *perm_rev_idx)
    requires is_c2r_v<Tx, Ty>
  {
    y[0] = real(y0);
    for (int64_t k = 1; k < n; k++) {
      y[k * ostride] = real(x0 + a[perm_rev_idx[k]]);
    }
  }

  const int64_t n;
  const int64_t istride, ostride;
  const int64_t m;
  fft_plan_ptr fwd_plan;
  fft_plan_ptr bwd_plan;

  pod_vector<int64_t> perm, perm_rev_idx;

  pod_vector<Tw> b;
};

template<typename Tx, typename Ty,
         transform_kind = transform_kind_from_io_types<Tx, Ty>()>
struct rader_twid_plan;

template<typename Tx, typename Ty>
struct rader_twid_plan<Tx, Ty, transform_kind::c2c> : public fft_plan {
  using Tw = add_complex_t<Ty>;
  static constexpr twiddle_layout twid_layout{
      runtime_precision_from_real_type<remove_complex_t<Tw>>(),
      false,
      1,
      // Rader applies twiddles to every column itself, including h = 0.
      true,
  };

  rader_twid_plan(int64_t n_, int64_t istride_, int64_t ostride_,
                  int64_t howmany0_, int64_t idist0_, int64_t odist0_,
                  int64_t howmany1_, plfft_direction_t dir_, int64_t m_,
                  fft_plan_ptr b_plan_, fft_plan_ptr fwd_plan_,
                  fft_plan_ptr bwd_plan_)
    : n(n_), istride(istride_), ostride(ostride_), astride(howmany0_),
      howmany0(howmany0_), idist0(idist0_), odist0(odist0_),
      howmany1(howmany1_), m(m_),
      twiddles(generate_twiddles(dir_, howmany0_, n, twid_layout)),
      fwd_plan(std::move(fwd_plan_)), bwd_plan(std::move(bwd_plan_)), perm(m),
      perm_rev_idx(n), b(m) {
    static_assert(is_c2c_v<Tx, Ty>);
    prepare_rader_tables(n, dir_, m, perm, perm_rev_idx, b, b_plan_);
  }

  void execute(const void *in, void *out) const override {
    const auto *x = static_cast<const Ty *>(in);
    auto *y = static_cast<Ty *>(out);
    // reserve two extra rows for values needed after convolution
    auto *a = get_memory<Tw>(buffer_name::rader, (m + 2) * astride);
    auto *x0 = a + m * astride;
    auto *y0 = x0 + astride;

    for (int64_t h1 = 0; h1 < howmany1; h1++) {
      const auto *xh = x + h1;
      auto *yh = y + h1;

      pack_and_save_input(xh, a, x0, y0,
                          static_cast<const Tw *>(twiddles->data()), n,
                          howmany0, idist0, istride, astride, m, perm.data());

      // compute a <- ifft(fft(a) .* fft(b))
      // b <- fft(b) is precomputed
      do_rader_convolution(m, howmany0, astride, a, b, fwd_plan, bwd_plan);

      // compute y[1:] <- x[0] + unpermute(a)
      unpack_output(yh, a, x0, y0, n, howmany0, odist0, ostride, astride,
                    perm_rev_idx.data());
    }
  }

#ifndef NO_LIBCPP
  std::string plan_to_string() const override {
    std::ostringstream sstm;
    sstm << "(rader-twid-c2c " << n << ' ';
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
  PLFFT_ALWAYS_INLINE static void
  pack_and_save_input(const Ty *__restrict__ x, Tw *__restrict__ a,
                      Tw *__restrict__ x0, Tw *__restrict__ y0,
                      const Tw *__restrict__ twids, int64_t n, int64_t howmany,
                      int64_t idist, int64_t istride, int64_t astride,
                      int64_t m, const int64_t *perm) {
    // Pack each twiddled transform, x[h], into a. Save x[h][0] for
    // unpack_output, and compute y[h][0] while packed terms are produced.
    for (int64_t h = 0; h < howmany; h++) {
      auto xh = x + h * idist;
      auto *ah = a + h;

      // save x[h][0] here, as convolution may clobber the buffer containing x
      x0[h] = xh[0];

      // initialize y[h][0] with x[h][0]; the packing loop accumulates x[h][1:]
      auto y0i = xh[0];

      // compute a[:, h] <- permute(x[h][1:]) .* twiddles and y[h][0]
      for (int64_t k = 0; k < m; k++) {
        const auto pk = perm[k];
        auto xk = xh[pk * istride] * twids[h * (n - 1) + (pk - 1)];
        ah[k * astride] = xk;
        y0i += xk;
      }
      y0[h] = y0i;
    }
  }

  PLFFT_ALWAYS_INLINE static void
  unpack_output(Ty *__restrict__ y, const Tw *__restrict__ a,
                const Tw *__restrict__ x0, const Tw *__restrict__ y0, int64_t n,
                int64_t howmany, int64_t odist, int64_t ostride,
                int64_t astride, const int64_t *perm_rev_idx) {
    for (int64_t h = 0; h < howmany; h++) {
      auto yh = y + h * odist;
      const auto ah = a + h;

      yh[0] = y0[h];
      for (int64_t k = 1; k < n; k++) {
        yh[k * ostride] = x0[h] + ah[perm_rev_idx[k] * astride];
      }
    }
  }

  const int64_t n;
  const int64_t istride, ostride, astride;
  // outer batch dimension parameters
  const int64_t howmany0, idist0, odist0;
  // inner dimension parameters
  const int64_t howmany1;
  const int64_t m;
  const pod_vector<void> *twiddles;
  fft_plan_ptr fwd_plan;
  fft_plan_ptr bwd_plan;

  pod_vector<int64_t> perm, perm_rev_idx;
  pod_vector<Tw> b;
};

template<typename Tx, typename Ty>
struct rader_twid_plan<Tx, Ty, transform_kind::r2c> : public fft_plan {
  using Tw = add_complex_t<Ty>;
  static constexpr twiddle_layout twid_layout{
      runtime_precision_from_real_type<remove_complex_t<Tw>>(),
      false,
      1,
  };

  rader_twid_plan(int64_t n_, int64_t istride_, int64_t ostride_,
                  int64_t howmany0_, int64_t idist0_, int64_t odist0_,
                  int64_t howmany1_, plfft_direction_t dir_, int64_t m_,
                  fft_plan_ptr b_plan_, fft_plan_ptr fwd_plan_,
                  fft_plan_ptr bwd_plan_)
    : n(n_), istride(istride_), ostride(ostride_), howmany0(howmany0_),
      idist0(idist0_), odist0(odist0_), howmany1(howmany1_), m(m_),
      twiddles(generate_twiddles(dir_, howmany0, n, twid_layout)),
      fwd_plan(std::move(fwd_plan_)), bwd_plan(std::move(bwd_plan_)), perm(m),
      perm_rev_idx(n), b(m) {
    static_assert(is_r2c_v<Tx, Ty>);
    prepare_rader_tables(n, dir_, m, perm, perm_rev_idx, b, b_plan_);
  }

  void execute(const void *in, void *out) const override {
    // The twid plan consumes complex data, while Tx and Ty refer to the types
    // of the full transform; in the r2c case, Tx is real.
    const auto *x = static_cast<const Ty *>(in);
    auto *y = static_cast<Ty *>(out);
    const int64_t astride = howmany0;
    auto *a = get_memory<Tw>(buffer_name::rader, (m + 2) * astride);
    // reserve extra rows for values needed after convolution
    auto *x0 = a + m * astride;
    auto *y0 = x0 + astride;

    for (int64_t h1 = 0; h1 < howmany1; h1++) {
      const auto *xh = x + h1;
      auto *yh = y + h1;

      pack_and_save_input(xh, a, x0, y0,
                          static_cast<const Tw *>(twiddles->data()), n,
                          howmany0, idist0, istride, astride, m, perm.data());

      // compute a <- ifft(fft(a) .* fft(b))
      // b <- fft(b) is precomputed
      do_rader_convolution(m, howmany0, astride, a, b, fwd_plan, bwd_plan);

      // compute y[1:] <- x[0] + unpermute(a)
      unpack_output(yh, a, x0, y0, n, howmany0, odist0, ostride, astride,
                    perm_rev_idx.data());
    }
  }

#ifndef NO_LIBCPP
  std::string plan_to_string() const override {
    std::ostringstream sstm;
    sstm << "(rader-twid-r2c " << n << ' ';
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
  PLFFT_ALWAYS_INLINE static void
  pack_and_save_input(const Ty *__restrict__ x, Tw *__restrict__ a,
                      Tw *__restrict__ x0, Tw *__restrict__ y0,
                      const Tw *__restrict__ twids, int64_t n, int64_t howmany,
                      int64_t idist, int64_t istride, int64_t astride,
                      int64_t m, const int64_t *perm) {
    const int64_t halfhi = howmany / 2 + 1;

    // save x[h][0] and initialize y0[h], reconstructing conjugate columns
    for (int64_t h = 0; h < halfhi; h++) {
      const auto xh = x + h * idist;
      auto *ah = a + h;
      const auto x0h = xh[0];
      x0[h] = x0h;
      auto y0i = x0h;

      for (int64_t k = 0; k < m; k++) {
        const auto pk = perm[k];
        auto xk = xh[pk * istride];
        if (h > 0) {
          xk *= twids[(h - 1) * (n - 1) + (pk - 1)];
        }
        ah[k * astride] = xk;
        y0i += xk;
      }
      y0[h] = y0i;
    }
    for (int64_t h = halfhi; h < howmany; h++) {
      const auto xh = x + (howmany - h) * idist;
      auto *ah = a + h;
      const auto x0h = conj(xh[0]);
      x0[h] = x0h;
      auto y0i = x0h;

      for (int64_t k = 0; k < m; k++) {
        const auto pk = perm[k];
        const auto xk =
            conj(xh[pk * istride]) * twids[(h - 1) * (n - 1) + (pk - 1)];
        ah[k * astride] = xk;
        y0i += xk;
      }
      y0[h] = y0i;
    }
  }

  PLFFT_ALWAYS_INLINE static void
  unpack_output(Ty *__restrict__ y, const Tw *__restrict__ a,
                const Tw *__restrict__ x0, const Tw *__restrict__ y0, int64_t n,
                int64_t howmany, int64_t odist, int64_t ostride,
                int64_t astride, const int64_t *perm_rev_idx) {
    const int64_t halfhi = howmany / 2 + 1;

    y[0] = y0[0];
    for (int64_t k = 1; k < n / 2 + 1; k++) {
      y[k * ostride] = x0[0] + a[perm_rev_idx[k] * astride];
    }

    for (int64_t h = 1; h < halfhi; h++) {
      auto yh = y + h * odist;
      const auto ah = a + h;
      yh[0] = y0[h];
      for (int64_t k = 1; k < n / 2 + 1; k++) {
        yh[k * ostride] = x0[h] + ah[perm_rev_idx[k] * astride];
      }
    }

    for (int64_t h = halfhi; h < howmany; h++) {
      auto yh = y + h * odist;
      const auto ah = a + h;
      yh[0] = y0[h];
      for (int64_t k = 1; k < n / 2; k++) {
        yh[k * ostride] = x0[h] + ah[perm_rev_idx[k] * astride];
      }
    }
  }

  const int64_t n;
  const int64_t istride, ostride;
  // outer batch dimension parameters
  const int64_t howmany0, idist0, odist0;
  // inner dimension parameters
  const int64_t howmany1;
  const int64_t m;
  const pod_vector<void> *twiddles;
  fft_plan_ptr fwd_plan;
  fft_plan_ptr bwd_plan;

  pod_vector<int64_t> perm, perm_rev_idx;
  pod_vector<Tw> b;
};

template<typename Tx, typename Ty>
struct rader_twid_plan<Tx, Ty, transform_kind::c2r> : public fft_plan {
  using Tw = add_complex_t<Tx>;
  static constexpr twiddle_layout twid_layout{
      runtime_precision_from_real_type<remove_complex_t<Tw>>(),
      false,
      1,
  };

  rader_twid_plan(int64_t n_, int64_t istride_, int64_t ostride_,
                  int64_t howmany0_, int64_t idist0_, int64_t odist0_,
                  int64_t howmany1_, plfft_direction_t dir_, int64_t m_,
                  fft_plan_ptr b_plan_, fft_plan_ptr fwd_plan_,
                  fft_plan_ptr bwd_plan_)
    : n(n_), istride(istride_), ostride(ostride_), howmany0(howmany0_),
      idist0(idist0_), odist0(odist0_), howmany1(howmany1_), m(m_),
      twiddles(generate_twiddles(dir_, howmany0, n, twid_layout)),
      fwd_plan(std::move(fwd_plan_)), bwd_plan(std::move(bwd_plan_)), perm(m),
      perm_rev_idx(n), b(m) {
    static_assert(is_c2r_v<Tx, Ty>);
    prepare_rader_tables(n, dir_, m, perm, perm_rev_idx, b, b_plan_);
  }

  void execute(const void *in, void *out) const override {
    // The twid plan consumes complex data, while Tx and Ty refer to the types
    // of the full transform; in the c2r case, Ty is real.
    const auto *x = static_cast<const Tx *>(in);
    auto *y = static_cast<Tx *>(out);
    const int64_t halfhi = howmany0 / 2 + 1;
    const int64_t astride = halfhi;
    auto *a = get_memory<Tw>(buffer_name::rader, (m + 2) * astride);
    // reserve extra rows for values needed after convolution
    auto *x0 = a + m * astride;
    auto *y0 = x0 + astride;

    for (int64_t h1 = 0; h1 < howmany1; h1++) {
      const auto *xh = x + h1;
      auto *yh = y + h1;

      pack_and_save_input(xh, a, x0, y0, n, howmany0, idist0, istride, astride,
                          m, perm.data());

      // compute a <- ifft(fft(a) .* fft(b))
      // b <- fft(b) is precomputed
      do_rader_convolution(m, halfhi, astride, a, b, fwd_plan, bwd_plan);

      // compute y[1:] <- x[0] + unpermute(a)
      unpack_output(yh, a, x0, y0, static_cast<const Tw *>(twiddles->data()), n,
                    howmany0, odist0, ostride, astride, perm_rev_idx.data());
    }
  }

#ifndef NO_LIBCPP
  std::string plan_to_string() const override {
    std::ostringstream sstm;
    sstm << "(rader-twid-c2r " << n << ' ';
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
  PLFFT_ALWAYS_INLINE static void
  pack_and_save_input(const Tx *__restrict__ x, Tw *__restrict__ a,
                      Tw *__restrict__ x0, Tw *__restrict__ y0, int64_t n,
                      int64_t howmany, int64_t idist, int64_t istride,
                      int64_t astride, int64_t m, const int64_t *perm) {
    const int64_t nhalfhi = n / 2 + 1;
    const int64_t halfhi = howmany / 2 + 1;

    // Save the independent x[h][0] values and initialize y[h][0].
    for (int64_t h = 0; h < halfhi; h++) {
      x0[h] = x[h * idist];
      y0[h] = x0[h];
    }

    // compute a[:, h] <- permute(x[h][1:]) and y0[h] <- sum(x[h])
    for (int64_t k = 0; k < m; k++) {
      auto ak = a + k * astride;
      const auto pk = perm[k];
      if (pk < nhalfhi) {
        for (int64_t h = 0; h < halfhi; h++) {
          const auto xk = x[h * idist + pk * istride];
          ak[h] = xk;
          y0[h] += xk;
        }
      } else {
        const auto xk0 = conj(x[(n - pk) * istride]);
        ak[0] = xk0;
        y0[0] += xk0;
        for (int64_t h = 1; h < halfhi; h++) {
          const auto strides = m - pk; // rader needs m as transform length here
          const auto xk = conj(x[(howmany - h) * idist + strides * istride]);
          ak[h] = xk;
          y0[h] += xk;
        }
      }
    }
  }

  PLFFT_ALWAYS_INLINE static void
  unpack_output(Tx *__restrict__ y, const Tw *__restrict__ a,
                const Tw *__restrict__ x0, const Tw *__restrict__ y0,
                const Tw *__restrict__ twids, int64_t n, int64_t howmany,
                int64_t odist, int64_t ostride, int64_t astride,
                const int64_t *perm_rev_idx) {
    const int64_t halfhi = howmany / 2 + 1;

    y[0] = y0[0];
    for (int64_t k = 1; k < n; k++) {
      y[k * ostride] = x0[0] + a[perm_rev_idx[k] * astride];
    }

    for (int64_t h = 1; h < halfhi; h++) {
      auto yh = y + h * odist;
      const auto ah = a + h;
      yh[0] = y0[h];
      const auto *twidsh = twids + (h - 1) * (n - 1);
      for (int64_t k = 1; k < n; k++) {
        auto yelem = x0[h] + ah[perm_rev_idx[k] * astride];
        yelem *= twidsh[k - 1];
        yh[k * ostride] = yelem;
      }
    }
  }

  const int64_t n;
  const int64_t istride, ostride;
  // outer batch dimension parameters
  const int64_t howmany0, idist0, odist0;
  // inner dimension parameters
  const int64_t howmany1;
  const int64_t m;
  const pod_vector<void> *twiddles;
  fft_plan_ptr fwd_plan;
  fft_plan_ptr bwd_plan;

  pod_vector<int64_t> perm, perm_rev_idx;
  pod_vector<Tw> b;
};

template<typename Tx, typename Ty>
struct rader_strategy : public strategy<Tx, Ty> {
  fft_plan_ptr make_plan(const problem &p,
                         plfft::planner &planner) const override {
    using single_plan_t = rader_single_plan<Tx, Ty>;
    using batched_plan_t = rader_plan<Tx, Ty>;
    using Tw = typename batched_plan_t::Tw;

    if (!is_applicable(p, planner)) {
      return nullptr;
    }

    const auto [_, prec, n, istride, ostride, howmany, idist, odist, dir] = p;
    const auto m = n - 1;
    const auto astride = howmany;
    auto subplanner = planner.without(ALLOW_CONVOLUTIONS);

    auto b_plan = subplanner.make_plan(
        make_problem<Tw, Tw>(m, 1, 1, 1, 0, 0, PLFFT_FORWARD));
    auto fwd_plan = subplanner.make_plan(make_problem<Tw, Tw>(
        m, astride, astride, howmany, 1, 1, PLFFT_FORWARD));
    auto bwd_plan = subplanner.make_plan(make_problem<Tw, Tw>(
        m, astride, astride, howmany, 1, 1, PLFFT_BACKWARD));

    if (!b_plan || !fwd_plan || !bwd_plan) {
      return nullptr;
    }

    if (howmany == 1) {
      return plfft::make_unique<single_plan_t>(
          n, istride, ostride, dir, m, std::move(b_plan), std::move(fwd_plan),
          std::move(bwd_plan));
    }

    return plfft::make_unique<batched_plan_t>(
        n, istride, ostride, howmany, idist, odist, dir, m, astride,
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
struct cooley_tukey_rader_ab : public strategy<Tx, Ty> {
  fft_plan_ptr make_plan(const problem &p,
                         plfft::planner &planner) const override {
    using plan_t = cooley_tukey_plan<Tx, Ty>;
    using twid_plan_t = rader_twid_plan<Tx, Ty>;
    using Tw = typename plan_t::Tw;

    const auto [_, prec, n, istride, ostride, howmany, idist, odist, dir] = p;
    if (!is_applicable(p, planner)) {
      return nullptr;
    }

    const auto r = planner.smallest_prime_factor(n);
    const auto m = n / r;
    const auto rm1 = r - 1;
    const auto ahowmany = is_c2r_v<Tx, Ty> ? (m / 2 + 1) : m;
    const auto astride = ahowmany;
    auto subplanner = planner.without(ALLOW_CONVOLUTIONS);

    auto b_plan = subplanner.make_plan(
        make_problem<Tw, Tw>(rm1, 1, 1, 1, 0, 0, PLFFT_FORWARD));
    auto fwd_plan = subplanner.make_plan(make_problem<Tw, Tw>(
        rm1, astride, astride, ahowmany, 1, 1, PLFFT_FORWARD));
    auto bwd_plan = subplanner.make_plan(make_problem<Tw, Tw>(
        rm1, astride, astride, ahowmany, 1, 1, PLFFT_BACKWARD));
    if (!b_plan || !fwd_plan || !bwd_plan) {
      return nullptr;
    }

    fft_plan_ptr stage1;
    fft_plan_ptr stage2;

    if constexpr (is_dit_v<Tx, Ty>) {
      stage1 = planner.make_plan(
          make_problem<Tx, Ty>(m, istride * r, r, r, istride, 1, dir));
      stage2 = plfft::make_unique<twid_plan_t>(
          r, 1, ostride * m, m, r, ostride, 1, dir, rm1, std::move(b_plan),
          std::move(fwd_plan), std::move(bwd_plan));
    } else {
      stage1 = plfft::make_unique<twid_plan_t>(
          r, istride * m, 1, m, istride, r, 1, dir, rm1, std::move(b_plan),
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
struct cooley_tukey_rader_ac : public strategy<Tx, Ty> {
  fft_plan_ptr make_plan(const problem &p,
                         plfft::planner &planner) const override {
    using plan_t = cooley_tukey_plan<Tx, Ty>;
    using twid_plan_t = rader_twid_plan<Tx, Ty>;
    using Tw = typename plan_t::Tw;

    const auto [_, prec, n, istride, ostride, howmany, idist, odist, dir] = p;
    if (!is_applicable(p, planner)) {
      return nullptr;
    }

    const auto r = planner.smallest_prime_factor(n);
    const auto m = n / r;
    const auto rm1 = r - 1;
    const auto ahowmany = is_c2r_v<Tx, Ty> ? (m / 2 + 1) : m;
    auto subplanner = planner.without(ALLOW_CONVOLUTIONS);

    auto b_plan = subplanner.make_plan(
        make_problem<Tw, Tw>(rm1, 1, 1, 1, 0, 0, PLFFT_FORWARD));
    auto fwd_plan = subplanner.make_plan(make_problem<Tw, Tw>(
        rm1, ahowmany, ahowmany, ahowmany, 1, 1, PLFFT_FORWARD));
    auto bwd_plan = subplanner.make_plan(make_problem<Tw, Tw>(
        rm1, ahowmany, ahowmany, ahowmany, 1, 1, PLFFT_BACKWARD));
    if (!b_plan || !fwd_plan || !bwd_plan) {
      return nullptr;
    }

    fft_plan_ptr stage1;
    fft_plan_ptr stage2;

    if constexpr (is_dit_v<Tx, Ty>) {
      stage1 = planner.make_plan(make_problem<Tx, Ty>(
          m, istride * r, howmany * r, howmany * r, idist, 1, dir));
      stage2 = plfft::make_unique<twid_plan_t>(
          r, howmany, ostride * m, m, howmany * r, ostride, howmany, dir, rm1,
          std::move(b_plan), std::move(fwd_plan), std::move(bwd_plan));
    } else {
      stage1 = plfft::make_unique<twid_plan_t>(
          r, istride * m, howmany, m, istride, howmany * r, howmany, dir, rm1,
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
