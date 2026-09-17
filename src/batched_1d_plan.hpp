/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "fft_buffers.hpp"
#include "fft_internal_plan.hpp"
#include "plfft_complex.hpp"
#include "plfft_parallel.hpp"
#include "plfft_util.hpp"

namespace plfft {

// TODO: remove this alias once new planner supports multithreaded execution
#ifdef _OPENMP
using fft_1d_plan_ptr = fft_internal_plan_ptr;
#else
using fft_1d_plan_ptr = fft_plan_ptr;
#endif

template<typename T1, typename T2>
class batched_1d_plan : public fft_plan {

  const int64_t n_;
  const int64_t howmany_;
  const int64_t istride_;
  const int64_t ostride_;
  const int64_t idist_;
  const int64_t odist_;
  const bool create_and_reorder_buffer_;

  const fft_1d_plan_ptr plan_1d_;

  void copy_input_into_buffer(const int64_t howmany, const int64_t idist,
                              const int64_t istride, const int64_t odist,
                              const int64_t ostride, const T1 *in,
                              T1 *out) const {
    auto nelems = !is_c2r_v<T1, T2> ? n_ : n_ / 2 + 1;
    for (int64_t i = 0; i < howmany; i++) {
      auto cur_in = in + i * idist;
      auto cur_out = out + i * n_;

      for (int64_t j = 0; j != nelems; ++j) {
        *cur_out = *cur_in;

        std::advance(cur_in, istride);
        std::advance(cur_out, ostride);
      }
    }
  }

public:
  batched_1d_plan() = delete;
  batched_1d_plan(batched_1d_plan &&) = default;
  batched_1d_plan(const batched_1d_plan &) = delete;

  batched_1d_plan(const int64_t n, const int64_t howmany, const int64_t istride,
                  const int64_t ostride, const int64_t idist,
                  const int64_t odist, const bool create_and_reorder_buffer,
                  fft_1d_plan_ptr plan)
    : n_{n}, howmany_{howmany}, istride_{istride}, ostride_{ostride},
      idist_{idist}, odist_{odist},
      create_and_reorder_buffer_{create_and_reorder_buffer},
      plan_1d_{std::move(plan)} {}

#ifndef NO_LIBCPP
  inline std::string plan_to_string() const override {
    std::stringstream sstm;
    sstm << "(batched-1d (" << n_ << ", " << howmany_;
    sstm << ", " << istride_ << ", " << idist_ << ", ";
    sstm << ostride_ << ", " << odist_ << ", " << create_and_reorder_buffer_
         << ") ";
    sstm << plan_1d_->plan_to_string();
    sstm << ")";
    return std::move(sstm).str();
  }
#endif // NO_LIBCPP
  inline algo_flops flops() const override {
    return plan_1d_->flops();
  }

  inline void execute(const void *in, void *out) const override {
    const T1 *in_c = (const T1 *)in;
    T2 *out_c = (T2 *)out;
#ifdef _OPENMP
    /*
      For out-of-place, and c2c-in-place 1D batched transforms, it
      is possible to parallelize the execution of the batch at the higher
      level of the execute method. For non-c2c-in-place transform that
      is not possible mostly because the transform is from C (R) to R (C)
      and therefore the meaning of strides and dists represents different
      number of elements in memory from the passed arguments.

      For instance an idist of 2 for complex is actually an idist or 4
      in memory while an odist of 2 for real is actually an odist of 2
      and so on. For this reason, for now, the execution of the batch is
      parallelized only for c2c (in and out of place) and out of place
      c2r and r2c.
    */
    if (howmany_ > 1 && !create_and_reorder_buffer_) {
      const auto nt = get_max_threads();
      const auto split = parallel::make_parallel_split(howmany_, 1, nt);

      auto exec = [this, split, in_c, out_c](int thread_num) {
        const auto [start, work] =
            parallel::work_distribution(thread_num, split);
        plan_1d_->execute(work, &in_c[start * idist_], &out_c[start * odist_]);
      };
      parallel::parallel_loop(split.threads, exec);
      return;
    }
#endif
    if (howmany_ > 1 && create_and_reorder_buffer_) {

      auto buffer = get_memory<add_complex_t<T1>>(buffer_name::batched,
                                                  n_ * howmany_ * 2);
      auto b1 = reinterpret_cast<T1 *>(buffer);
      this->copy_input_into_buffer(howmany_, idist_, istride_, odist_, 1, in_c,
                                   b1);

      in_c = b1;
    }
    plan_1d_->execute(in_c, out_c);
    return;
  }
};

} // namespace plfft
