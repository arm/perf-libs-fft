/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "decimation.hpp"
#include "factorize.hpp"
#include "generate_twiddles.hpp"
#include "kernel_data.hpp"
#include "kernel_function_pointers.hpp"
#include "planner/planner.hpp"
#include "planner/problem.hpp"
#include "planner/strategy.hpp"
#include "plfft/fft_plan.hpp"
#include "plfft_complex.hpp"
#include "plfft_move_vector.hpp"
#include "plfft_pod_vector.hpp"
#include "plfft_util.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>

#ifndef NO_LIBCPP
#include <sstream>
#endif // NO_LIBCPP

namespace plfft {

namespace {

using cooley_tukey_buffer_type = pod_vector<std::uint8_t>;

// Temporary buffer stack for recursive Cooley-Tukey plans.
// Each get_cooley_tukey_buffer call pushes one frame on this stack.
// Remove this when planner plans are linearized.
inline move_vector<cooley_tukey_buffer_type> &cooley_tukey_buffers() {
  THREAD_LOCAL move_vector<cooley_tukey_buffer_type> buffers;
  return buffers;
}

inline int &cooley_tukey_active_buffer() {
  THREAD_LOCAL int active = -1;
  return active;
}

template<typename T>
T *get_cooley_tukey_buffer(int64_t count) {
  ASSERT(count >= 0);
  auto &active = cooley_tukey_active_buffer();
  auto &buffers = cooley_tukey_buffers();

  ++active;
  if (buffers.size() == std::size_t(active)) {
    buffers.emplace_back();
  }
  assert(std::size_t(active) < buffers.size());

  auto &buffer = buffers[active];
  const auto bytes = count * sizeof(T);
  if (buffer.size() < bytes) {
    // allocate new: resize may copy old data we will overwrite
    buffer = cooley_tukey_buffer_type(bytes);
  }
  return reinterpret_cast<T *>(buffer.data());
}

inline void release_cooley_tukey_buffer() {
  cooley_tukey_active_buffer()--;
}

} // namespace

template<typename Tx, typename Ty,
         transform_kind = transform_kind_from_io_types<Tx, Ty>()>
struct ab_twid_direct_plan;

template<typename Tx, typename Ty>
struct ab_twid_direct_plan<Tx, Ty, transform_kind::c2c> : public fft_plan {
  ab_twid_direct_plan(int64_t n_, int64_t istride_, int64_t ostride_,
                      int64_t howmany_, int64_t idist_, int64_t odist_,
                      plfft_direction_t dir_, bool want_sme)
    : n(n_), istride(istride_), ostride(ostride_), howmany(howmany_),
      idist(idist_), odist(odist_), twiddles(nullptr), kernel_n(nullptr),
      kernel(nullptr), flops_{} {
    static_assert(is_c2c_v<Tx, Ty>);

    const auto data_n = get_kernel_data<Ty, Ty>(
        n_, 1, istride, ostride, 1, 1, dir_, 0, order_kind::ORDER_NA, want_sme);
    // provider contract: every n advertised by get_kernel_ns has kernel data
    assert(data_n);
    kernel_n = data_n->ab_n.get_text_ptr();

    const auto data =
        get_kernel_data<Ty, Ty>(n_, howmany_, istride, ostride, idist, odist,
                                dir_, 0, order_kind::ORDER_AB, want_sme);
    // provider contract: every n advertised by get_kernel_ns has kernel data
    assert(data);
    kernel = data->ab_t_dit.get_text_ptr();

    twiddles = generate_twiddles(dir_, howmany_, n_, data->twid_layout);

    flops_ = data_n->ab_n.flops + data->ab_t_dit.flops * (howmany_ - 1);
  }

  void execute(const void *in, void *out) const override {
    const auto *x = static_cast<const Ty *>(in);
    auto *y = static_cast<Ty *>(out);
    kernel_n(x, y, istride, ostride, 1, 0, 0);
    if (howmany > 1) {
      const auto w = twiddles->data();
      kernel(x + idist, y + odist, istride, ostride, w, howmany - 1, idist,
             odist);
    }
  }

#ifndef NO_LIBCPP
  std::string plan_to_string() const override {
    std::ostringstream sstm;
    sstm << "(ab-twid-direct-c2c " << n << ')';
    return std::move(sstm).str();
  }
#endif // NO_LIBCPP

  algo_flops flops() const override {
    return flops_;
  }

private:
  int64_t n;
  int64_t istride, ostride;
  int64_t howmany, idist, odist;

  const pod_vector<void> *twiddles;
  fft_func_n_t<Ty, Ty> *kernel_n;
  fft_func_t_t<Ty> *kernel;

  algo_flops flops_{};
};

template<typename Tx, typename Ty>
struct ab_twid_direct_plan<Tx, Ty, transform_kind::r2c> : public fft_plan {
  ab_twid_direct_plan(int64_t n_, int64_t istride_, int64_t ostride_,
                      int64_t howmany_, int64_t idist_, int64_t odist_,
                      plfft_direction_t dir_, bool want_sme)
    : n(n_), istride(istride_), ostride(ostride_), howmany(howmany_),
      idist(idist_), odist(odist_), twiddle_premul_factor_tfh(1),
      twiddle_interleave_factor_tfh(1), twiddles_tfj(nullptr),
      twiddles_tfh(nullptr), kernel_n_oh(nullptr), kernel_t_j(nullptr),
      kernel_t_ol(nullptr), flops_{} {
    static_assert(is_r2c_v<Tx, Ty>);

    const auto data_n = get_kernel_data<Tx, Ty>(
        n_, 1, istride, ostride, 1, 1, dir_, 0, order_kind::ORDER_NA, want_sme);
    const auto data_tfj =
        get_kernel_data<Tx, Ty>(n_, howmany, istride, ostride, idist, odist,
                                dir_, 0, order_kind::ORDER_AB, want_sme);
    const auto data_tfh = get_kernel_data<Tx, Ty>(
        n_, 1, istride, ostride, 1, 1, dir_, 0, order_kind::ORDER_AB, want_sme);
    // provider contract: every n advertised by get_kernel_ns has kernel data
    assert(data_n && data_tfj && data_tfh);
    assert(data_tfj->twid_layout.interleave_factor > 0);
    assert(data_tfh->twid_layout.interleave_factor > 0);
    assert(data_tfj->twid_layout.precision == data_tfh->twid_layout.precision);
    kernel_n_oh = data_n->ab_nfoh.get_text_ptr();
    if (howmany > 2) {
      kernel_t_j = data_tfj->ab_tfj_dit.get_text_ptr();
    }
    if (howmany % 2 == 0) {
      kernel_t_ol = data_tfh->ab_tfol_dit.get_text_ptr();
    }
    assert(howmany <= 2 || kernel_t_j);
    assert(howmany % 2 != 0 || kernel_t_ol);

    twiddles_tfj = generate_twiddles(dir_, howmany, n_, data_tfj->twid_layout);
    twiddles_tfh = generate_twiddles(dir_, howmany, n_, data_tfh->twid_layout);
    twiddle_premul_factor_tfh = data_tfh->twid_layout.want_premul ? 2 : 1;
    twiddle_interleave_factor_tfh = data_tfh->twid_layout.interleave_factor;

    const auto halflo = (howmany + 1) / 2;
    flops_ = data_n->ab_nfoh.flops;
    flops_ += data_tfj->ab_tfj_dit.flops * (halflo - 1);
    if (howmany % 2 == 0) {
      flops_ += data_tfh->ab_tfol_dit.flops;
    }
  }

  void execute(const void *in, void *out) const override {
    // The twid plan consumes complex data, while Tx and Ty refer to the types
    // of the full transform; in the r2c case, Tx is real.
    const auto *x = static_cast<const Ty *>(in);
    auto *y = static_cast<Ty *>(out);
    kernel_n_oh(x, y, istride, ostride, 1, 0, 0);

    const auto halflo = (howmany + 1) / 2;
    if (halflo > 1) {
      const auto yyoffset = (n / 2 - 1) * ostride + (howmany - 1) * odist;
      kernel_t_j(x + idist, y + odist, y + yyoffset, istride, ostride,
                 twiddles_tfj->data(), halflo - 1, idist, odist);
    }

    if (howmany % 2 == 0) {
      const auto xoffset = halflo * idist;
      const auto yoffset = halflo * odist;
      const auto twiddle_row = halflo - 1;
      const auto twiddle_block = twiddle_row / twiddle_interleave_factor_tfh;
      const auto twiddle_block_row =
          twiddle_row % twiddle_interleave_factor_tfh;
      const auto offsetw =
          twiddle_premul_factor_tfh *
          (twiddle_block * twiddle_interleave_factor_tfh * (n - 1) +
           twiddle_block_row);
      kernel_t_ol(x + xoffset, y + yoffset, istride, ostride,
                  &(*twiddles_tfh)[offsetw], 1, 0, 0);
    }
  }

#ifndef NO_LIBCPP
  std::string plan_to_string() const override {
    std::ostringstream sstm;
    sstm << "(ab-twid-direct-r2c " << n << ')';
    return std::move(sstm).str();
  }
#endif // NO_LIBCPP

  algo_flops flops() const override {
    return flops_;
  }

private:
  int64_t n;
  int64_t istride, ostride;
  int64_t howmany, idist, odist;

  int twiddle_premul_factor_tfh;
  int twiddle_interleave_factor_tfh;
  const pod_vector<void> *twiddles_tfj;
  const pod_vector<void> *twiddles_tfh;
  fft_func_n_t<Ty, Ty> *kernel_n_oh;
  fft_func_j_t<Ty> *kernel_t_j;
  fft_func_t_t<Ty> *kernel_t_ol;

  algo_flops flops_{};
};

template<typename Tx, typename Ty>
struct ab_twid_direct_plan<Tx, Ty, transform_kind::c2r> : public fft_plan {
  ab_twid_direct_plan(int64_t n_, int64_t istride_, int64_t ostride_,
                      int64_t howmany_, int64_t idist_, int64_t odist_,
                      plfft_direction_t dir_, bool want_sme)
    : n(n_), istride(istride_), ostride(ostride_), howmany(howmany_ / 2 + 1),
      howmany_full(howmany_), idist(idist_), odist(odist_), twiddles(nullptr),
      kernel_n(nullptr), kernel_t(nullptr), flops_{} {
    static_assert(is_c2r_v<Tx, Ty>);

    const auto data_n =
        get_kernel_data<Tx, Ty>(n_, 1, istride, ostride, idist, odist, dir_, 0,
                                order_kind::ORDER_NA, want_sme);
    const auto data_t =
        get_kernel_data<Tx, Ty>(n_, std::nullopt, istride, ostride, idist,
                                odist, dir_, 0, order_kind::ORDER_AB, want_sme);
    // provider contract: every n advertised by get_kernel_ns has kernel data
    assert(data_n && data_t);
    assert(data_t->twid_layout.interleave_factor > 0);
    kernel_n = data_n->ab_nbih.get_text_ptr();
    if (howmany > 1) {
      kernel_t = (n % 2 == 0 ? data_t->ab_tbil_dif : data_t->ab_tbih_dif)
                     .get_text_ptr();
    }
    assert(howmany <= 1 || kernel_t);

    twiddles = generate_twiddles(dir_, howmany_full, n_, data_t->twid_layout);

    flops_ = data_n->ab_nbih.flops;
    flops_ +=
        (n % 2 == 0 ? data_t->ab_tbil_dif.flops : data_t->ab_tbih_dif.flops) *
        (howmany - 1);
  }

  void execute(const void *in, void *out) const override {
    // The twid plan consumes complex data, while Tx and Ty refer to the types
    // of the full transform; in the c2r case, Ty is real.
    const auto *x = static_cast<const Tx *>(in);
    auto *y = static_cast<Tx *>(out);
    const auto xxoffset = (n - 1) / 2 * istride;
    kernel_n(x, x + xxoffset, y, istride, ostride, 1, 0, 0);

    if (howmany > 1) {
      const auto xoffset = idist;
      const auto yoffset = odist;
      const auto xxoffset_t = n / 2 * istride - idist;
      kernel_t(x + xoffset, x + xxoffset_t, y + yoffset, istride, ostride,
               twiddles->data(), howmany - 1, idist, odist);
    }
  }

#ifndef NO_LIBCPP
  std::string plan_to_string() const override {
    std::ostringstream sstm;
    sstm << "(ab-twid-direct-c2r " << n << ')';
    return std::move(sstm).str();
  }
#endif // NO_LIBCPP

  algo_flops flops() const override {
    return flops_;
  }

private:
  int64_t n;
  int64_t istride, ostride;
  int64_t howmany, howmany_full, idist, odist;

  const pod_vector<void> *twiddles;
  fft_func_in_t<Tx> *kernel_n;
  fft_func_it_t<Tx> *kernel_t;

  algo_flops flops_{};
};

template<typename Tx, typename Ty,
         transform_kind = transform_kind_from_io_types<Tx, Ty>()>
struct ac_twid_direct_plan;

template<typename Tx, typename Ty>
struct ac_twid_direct_plan<Tx, Ty, transform_kind::c2c> : public fft_plan {
  ac_twid_direct_plan(int64_t n_, int64_t istride_, int64_t ostride_,
                      int64_t howmany0_, int64_t idist0_, int64_t odist0_,
                      int64_t howmany1_, plfft_direction_t dir_, bool want_sme)
    : n(n_), istride(istride_), ostride(ostride_), howmany0(howmany0_),
      idist0(idist0_), odist0(odist0_), howmany1(howmany1_),
      twiddle_premul_factor(0), twiddles(nullptr), kernel_n(nullptr),
      kernel_t(nullptr) {
    static_assert(is_c2c_v<Tx, Ty>);

    const auto data_n =
        get_kernel_data<Ty, Ty>(n_, howmany1, istride, ostride, 1, 1, dir_, 0,
                                order_kind::ORDER_NA, want_sme);
    // provider contract: every n advertised by get_kernel_ns has kernel data
    assert(data_n);
    kernel_n = data_n->ab_n.get_text_ptr();

    const auto data_t =
        get_kernel_data<Ty, Ty>(n_, howmany1, istride, ostride, 1, 1, dir_, 0,
                                order_kind::ORDER_AC, want_sme);
    // provider contract: every n advertised by get_kernel_ns has kernel data
    assert(data_t);
    kernel_t = data_t->ac_t_dit.get_text_ptr();

    assert(data_t->twid_layout.interleave_factor == 1);
    twiddles = generate_twiddles(dir_, howmany0, n_, data_t->twid_layout);
    twiddle_premul_factor = data_t->twid_layout.want_premul ? 2 : 1;

    flops_ = data_n->ab_n.flops * howmany1;
    flops_ += data_t->ac_t_dit.flops * howmany1 * (howmany0 - 1);
  }

  void execute(const void *in, void *out) const override {
    const auto *x = static_cast<const Ty *>(in);
    auto *y = static_cast<Ty *>(out);
    kernel_n(x, y, istride, ostride, howmany1, 1, 1);
    for (int64_t i = 1; i < howmany0; i++) {
      const auto offsetw = (i - 1) * twiddle_premul_factor * (n - 1);
      kernel_t(x + i * idist0, y + i * odist0, istride, ostride,
               &(*twiddles)[offsetw], howmany1, 1, 1);
    }
  }

#ifndef NO_LIBCPP
  std::string plan_to_string() const override {
    std::ostringstream sstm;
    sstm << "(ac-twid-direct-c2c " << n << ')';
    return std::move(sstm).str();
  }
#endif // NO_LIBCPP

  algo_flops flops() const override {
    return flops_;
  }

private:
  int64_t n;
  int64_t istride, ostride;
  // outer batch dimension parameters
  int64_t howmany0, idist0, odist0;
  // inner dimension parameters
  int64_t howmany1;

  int twiddle_premul_factor;
  const pod_vector<void> *twiddles;

  fft_func_n_t<Ty, Ty> *kernel_n;
  fft_func_t_t<Ty> *kernel_t;

  algo_flops flops_{};
};

template<typename Tx, typename Ty>
struct ac_twid_direct_plan<Tx, Ty, transform_kind::r2c> : public fft_plan {
  ac_twid_direct_plan(int64_t n_, int64_t istride_, int64_t ostride_,
                      int64_t howmany0_, int64_t idist0_, int64_t odist0_,
                      int64_t howmany1_, plfft_direction_t dir_, bool want_sme)
    : n(n_), istride(istride_), ostride(ostride_), howmany0(howmany0_),
      idist0(idist0_), odist0(odist0_), howmany1(howmany1_),
      twiddle_premul_factor(0), twiddles(nullptr), kernel_n_oh(nullptr),
      kernel_t_j(nullptr), kernel_t_ol(nullptr), flops_{} {
    static_assert(is_r2c_v<Tx, Ty>);

    const auto data_n =
        get_kernel_data<Tx, Ty>(n_, howmany1, istride, ostride, 1, 1, dir_, 0,
                                order_kind::ORDER_NA, want_sme);
    const auto data_t =
        get_kernel_data<Tx, Ty>(n_, howmany1, istride, ostride, 1, 1, dir_, 0,
                                order_kind::ORDER_AC, want_sme);
    // provider contract: every n advertised by get_kernel_ns has kernel data
    assert(data_n && data_t);
    assert(data_t->twid_layout.interleave_factor == 1);
    kernel_n_oh = data_n->ab_nfoh.get_text_ptr();
    if (howmany0 > 2) {
      kernel_t_j = data_t->ac_tfj_dit.get_text_ptr();
    }
    if (howmany0 % 2 == 0) {
      kernel_t_ol = data_t->ac_tfol_dit.get_text_ptr();
    }
    assert(howmany0 <= 2 || kernel_t_j);
    assert(howmany0 % 2 != 0 || kernel_t_ol);

    twiddles = generate_twiddles(dir_, howmany0, n_, data_t->twid_layout);
    twiddle_premul_factor = data_t->twid_layout.want_premul ? 2 : 1;

    const auto halflo = (howmany0 + 1) / 2;
    flops_ = data_n->ab_nfoh.flops * howmany1;
    flops_ += data_t->ac_tfj_dit.flops * howmany1 * (halflo - 1);
    if (howmany0 % 2 == 0) {
      flops_ += data_t->ac_tfol_dit.flops * howmany1;
    }
  }

  void execute(const void *in, void *out) const override {
    // The twid plan consumes complex data, while Tx and Ty refer to the types
    // of the full transform; in the r2c case, Tx is real.
    const auto *x = static_cast<const Ty *>(in);
    auto *y = static_cast<Ty *>(out);
    kernel_n_oh(x, y, istride, ostride, howmany1, 1, 1);

    const auto halflo = (howmany0 + 1) / 2;
    if (halflo > 1) {
      const auto yyoffset = (n / 2 - 1) * ostride + howmany0 * odist0;
      for (int64_t i = 1; i < halflo; i++) {
        const auto offsetw = (i - 1) * twiddle_premul_factor * (n - 1);
        kernel_t_j(x + i * idist0, y + i * odist0, y + yyoffset - i * odist0,
                   istride, ostride, &(*twiddles)[offsetw], howmany1, 1, 1);
      }
    }

    if (howmany0 % 2 == 0) {
      const auto xoffset = halflo * idist0;
      const auto yoffset = halflo * odist0;
      const auto offsetw = (halflo - 1) * twiddle_premul_factor * (n - 1);
      kernel_t_ol(x + xoffset, y + yoffset, istride, ostride,
                  &(*twiddles)[offsetw], howmany1, 1, 1);
    }
  }

#ifndef NO_LIBCPP
  std::string plan_to_string() const override {
    std::ostringstream sstm;
    sstm << "(ac-twid-direct-r2c " << n << ')';
    return std::move(sstm).str();
  }
#endif // NO_LIBCPP

  algo_flops flops() const override {
    return flops_;
  }

private:
  int64_t n;
  int64_t istride, ostride;
  // outer batch dimension parameters
  int64_t howmany0, idist0, odist0;
  // inner dimension parameters
  int64_t howmany1;

  int twiddle_premul_factor;
  const pod_vector<void> *twiddles;
  fft_func_n_t<Ty, Ty> *kernel_n_oh;
  fft_func_j_t<Ty> *kernel_t_j;
  fft_func_t_t<Ty> *kernel_t_ol;

  algo_flops flops_{};
};

template<typename Tx, typename Ty>
struct ac_twid_direct_plan<Tx, Ty, transform_kind::c2r> : public fft_plan {
  ac_twid_direct_plan(int64_t n_, int64_t istride_, int64_t ostride_,
                      int64_t howmany0_, int64_t idist0_, int64_t odist0_,
                      int64_t howmany1_, plfft_direction_t dir_, bool want_sme)
    : n(n_), istride(istride_), ostride(ostride_), howmany0(howmany0_),
      idist0(idist0_), odist0(odist0_), howmany1(howmany1_),
      twiddle_premul_factor(0), twiddles(nullptr), kernel_n(nullptr),
      kernel_t(nullptr), flops_{} {
    static_assert(is_c2r_v<Tx, Ty>);

    const auto data_n =
        get_kernel_data<Tx, Ty>(n_, howmany1, istride, ostride, 1, 1, dir_, 0,
                                order_kind::ORDER_NA, want_sme);
    const auto data_t =
        get_kernel_data<Tx, Ty>(n_, howmany1, istride, ostride, 1, 1, dir_, 0,
                                order_kind::ORDER_AC, want_sme);
    // provider contract: every n advertised by get_kernel_ns has kernel data
    assert(data_n && data_t);
    assert(data_t->twid_layout.interleave_factor == 1);
    kernel_n = data_n->ab_nbih.get_text_ptr();
    if (howmany0 > 1) {
      kernel_t = (n % 2 == 0 ? data_t->ac_tbil_dif : data_t->ac_tbih_dif)
                     .get_text_ptr();
    }
    assert(howmany0 <= 1 || kernel_t);

    twiddles = generate_twiddles(dir_, howmany0, n_, data_t->twid_layout);
    twiddle_premul_factor = data_t->twid_layout.want_premul ? 2 : 1;

    const auto halfhi = howmany0 / 2 + 1;
    flops_ =
        data_n->ab_nbih.flops * howmany1 +
        (n % 2 == 0 ? data_t->ac_tbil_dif.flops : data_t->ac_tbih_dif.flops) *
            howmany1 * (halfhi - 1);
  }

  void execute(const void *in, void *out) const override {
    // The twid plan consumes complex data, while Tx and Ty refer to the types
    // of the full transform; in the c2r case, Ty is real.
    const auto *x = static_cast<const Tx *>(in);
    auto *y = static_cast<Tx *>(out);
    const auto xxoffset = (n - 1) / 2 * istride;
    kernel_n(x, x + xxoffset, y, istride, ostride, howmany1, 1, 1);

    const auto halfhi = howmany0 / 2 + 1;
    if (halfhi > 1) {
      const auto xxoffset_t = n / 2 * istride;
      for (int64_t i = 1; i < halfhi; i++) {
        const auto offsetw = (i - 1) * twiddle_premul_factor * (n - 1);
        kernel_t(x + i * idist0, x + xxoffset_t - i * idist0, y + i * odist0,
                 istride, ostride, &(*twiddles)[offsetw], howmany1, 1, 1);
      }
    }
  }

#ifndef NO_LIBCPP
  std::string plan_to_string() const override {
    std::ostringstream sstm;
    sstm << "(ac-twid-direct-c2r " << n << ')';
    return std::move(sstm).str();
  }
#endif // NO_LIBCPP

  algo_flops flops() const override {
    return flops_;
  }

private:
  int64_t n;
  int64_t istride, ostride;
  // outer batch dimension parameters
  int64_t howmany0, idist0, odist0;
  // inner dimension parameters
  int64_t howmany1;

  int twiddle_premul_factor;
  const pod_vector<void> *twiddles;
  fft_func_in_t<Tx> *kernel_n;
  fft_func_it_t<Tx> *kernel_t;

  algo_flops flops_{};
};

template<typename Tx, typename Ty>
struct cooley_tukey_plan : public fft_plan {
  using Tw = add_complex_t<Tx>;

  cooley_tukey_plan(int64_t n_, int64_t howmany_, fft_plan_ptr stage1_,
                    fft_plan_ptr stage2_)
    : n(n_), howmany(howmany_), stage1(std::move(stage1_)),
      stage2(std::move(stage2_)) {}

  void execute(const void *in, void *out) const override {
    const auto *x = static_cast<const Tx *>(in);
    auto *y = static_cast<Ty *>(out);
    const auto buffer_size = mul_assert_no_overflow(n, howmany);
    auto *buffer = get_cooley_tukey_buffer<Tw>(buffer_size);
    stage1->execute(x, buffer);
    stage2->execute(buffer, y);
    release_cooley_tukey_buffer();
  }

#ifndef NO_LIBCPP
  std::string plan_to_string() const override {
    std::ostringstream sstm;
    sstm << "(cooley-tukey " << n << ' ';
    sstm << stage1->plan_to_string() << ' ';
    sstm << stage2->plan_to_string() << ')';
    return std::move(sstm).str();
  }
#endif // NO_LIBCPP

  algo_flops flops() const override {
    return stage1->flops() + stage2->flops();
  }

private:
  int64_t n;
  int64_t howmany;
  fft_plan_ptr stage1;
  fft_plan_ptr stage2;
};

template<typename Tx, typename Ty>
struct cooley_tukey_ab : public strategy<Tx, Ty> {
  explicit cooley_tukey_ab(int64_t r_) : r(r_) {}

  fft_plan_ptr make_plan(const problem &p,
                         plfft::planner &planner) const override {
    using twid_plan_t = ab_twid_direct_plan<Tx, Ty>;
    using plan_t = cooley_tukey_plan<Tx, Ty>;

    if (!is_applicable(p)) {
      return nullptr;
    }

    const auto [_, prec, n, istride, ostride, howmany, idist, odist, dir] = p;
    const auto m = n / r;
    const bool want_sme = planner.has(ALLOW_SME);

    fft_plan_ptr stage1;
    fft_plan_ptr stage2;

    if constexpr (is_dit_v<Tx, Ty>) {
      stage1 = planner.make_plan(
          make_problem<Tx, Ty>(m, istride * r, r, r, istride, 1, dir));
      stage2 = plfft::make_unique<twid_plan_t>(r, 1, ostride * m, m, r, ostride,
                                               dir, want_sme);
    } else {
      stage1 = plfft::make_unique<twid_plan_t>(r, istride * m, 1, m, istride, r,
                                               dir, want_sme);
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
  const int64_t r; // radix

  bool is_applicable(const problem &p) const {
    const auto [_, prec, n, istride, ostride, howmany, idist, odist, dir] = p;
    const auto ns = get_kernel_ns<Tx, Ty>();

    return (
        /* correctness constraints */

        // r must be a proper divisor of n
        r < n && n % r == 0 &&
        // must have a radix-r base kernel
        is_base_n(r, ns) &&
        // single batch only
        howmany == 1);
  }
};

template<typename Tx, typename Ty>
struct cooley_tukey_ac : public strategy<Tx, Ty> {
  explicit cooley_tukey_ac(int64_t r_) : r(r_) {}

  fft_plan_ptr make_plan(const problem &p,
                         plfft::planner &planner) const override {
    using twid_plan_t = ac_twid_direct_plan<Tx, Ty>;
    using plan_t = cooley_tukey_plan<Tx, Ty>;

    if (!is_applicable(p)) {
      return nullptr;
    }

    const auto [_, prec, n, istride, ostride, howmany, idist, odist, dir] = p;
    const auto m = n / r;
    const bool want_sme = planner.has(ALLOW_SME);

    fft_plan_ptr stage1;
    fft_plan_ptr stage2;

    if constexpr (is_dit_v<Tx, Ty>) {
      stage1 = planner.make_plan(make_problem<Tx, Ty>(
          m, istride * r, howmany * r, howmany * r, idist, 1, dir));
      stage2 = plfft::make_unique<twid_plan_t>(r, howmany, ostride * m, m,
                                               howmany * r, ostride, howmany,
                                               dir, want_sme);
    } else {
      stage1 =
          plfft::make_unique<twid_plan_t>(r, istride * m, howmany, m, istride,
                                          howmany * r, howmany, dir, want_sme);
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
  const int64_t r; // radix

  bool is_applicable(const problem &p) const {
    const auto [_, prec, n, istride, ostride, howmany, idist, odist, dir] = p;
    const auto ns = get_kernel_ns<Tx, Ty>();

    return (
        /* correctness constraints */

        // r must be a proper divisor of n
        r < n && n % r == 0 &&
        // must have a radix-r base kernel
        is_base_n(r, ns) &&
        (
            // DIT: input batches are packed, output batches are interleaved
            (is_dit_v<Tx, Ty> && istride == howmany * idist && odist == 1) ||
            // DIF: input batches are interleaved, output batches are packed
            (!is_dit_v<Tx, Ty> && idist == 1 && ostride == howmany * odist)) &&

        /* profitability constraints (heuristics) */

        // multiple batches only
        howmany > 1);
  }
};

} // namespace plfft
