/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "exec_compositor.hpp"
#include "fft_buffers.hpp"
#include "plfft_parallel.hpp"
#include "plfft_util.hpp"

namespace plfft {

namespace parallel {
parallel_state_t parallel_state;
}

template<typename Tx, typename Ty, typename Tw = add_complex_t<Tx>>
static void execute(const composition<Tx, Ty> &comp, int64_t howmany,
                    const Tx *X, Ty *Y, int64_t istride, int64_t ostride,
                    int64_t idist, int64_t odist, Tw *x_ptr, Tw *y_ptr,
                    const parallel::parallel_split &split, int thread_num) {
  auto howmany_start = 0;
  auto howmany_work = howmany;

  if (split.threads > 1) {
    auto ofs = comp.n * thread_num;
    x_ptr += ofs;
    y_ptr += ofs;

    const auto [hs, hw] = parallel::work_distribution(thread_num, split);
    howmany_start = hs;
    howmany_work = hw;
  }

  for (auto i = howmany_start; i != (howmany_start + howmany_work); ++i) {
    auto *first_lev = comp.levels.front().get();
    first_lev->execute((const void *)&X[i * idist], (void *)y_ptr);
    std::swap(x_ptr, y_ptr);

    for (size_t j = 1; j < comp.levels.size() - 1; ++j) {
      const auto *lev = comp.levels[j].get();
      lev->execute((const void *)x_ptr, (void *)y_ptr);
      std::swap(x_ptr, y_ptr);
    }

    auto *last_lev = comp.levels.back().get();
    last_lev->execute((const void *)x_ptr, (void *)&Y[i * odist]);
  }
}

template<typename Tx, typename Ty, typename Tw = add_complex_t<Tx>>
static void execute(const composition<Tx, Ty> &comp, int64_t howmany,
                    const Tx *X, Ty *Y, int64_t istride, int64_t ostride,
                    int64_t idist, int64_t odist, Tw *x_ptr, Tw *y_ptr,
                    int nt) {
  const auto split = parallel::make_parallel_split(howmany, 1, nt);

  auto exec = [&comp, howmany, X, Y, istride, ostride, idist, odist, x_ptr,
               y_ptr, split](int thread_num) {
    execute<Tx, Ty>(comp, howmany, X, Y, istride, ostride, idist, odist, x_ptr,
                    y_ptr, split, thread_num);
  };

  parallel::parallel_loop(split.threads, exec);
}

template<typename Tx, typename Ty, bool IsParallel>
inline void execute(const composition<Tx, Ty> &comp, int64_t howmany,
                    const Tx *X, Ty *Y, int64_t istride, int64_t ostride,
                    int64_t idist, int64_t odist, int nt) {
  if (comp.nlevels == 1) {
    const auto *lev = comp.levels.front().get();
    lev->execute(howmany, (const void *)X, istride, idist, (void *)Y, ostride,
                 odist);
    return;
  }

  size_t buf_size = comp.n * 2 * nt;

  // Select the buffer as appropriate
  using Tw = add_complex_t<Tx>;
  Tw *x_ptr = get_memory<Tw>(buffer_name::compositor, buf_size);

  auto *y_ptr = x_ptr + comp.n * nt;

  if constexpr (IsParallel) {
    execute<Tx, Ty>(comp, howmany, X, Y, istride, ostride, idist, odist, x_ptr,
                    y_ptr, nt);
  } else {
    execute<Tx, Ty>(comp, howmany, X, Y, istride, ostride, idist, odist, x_ptr,
                    y_ptr, {}, 0);
  }

  // Communicate that we're done with the compositor buffer for this
  // composition.
  release_compositor_buf();
}

template<typename Tx, typename Ty>
void execute(const composition<Tx, Ty> &comp, int64_t howmany, const Tx *X,
             Ty *Y, int64_t istride, int64_t ostride, int64_t idist,
             int64_t odist) {
#ifdef _OPENMP
  auto nt = howmany == 1 ? 1 : get_max_threads();
  if (nt > 1) {
    execute<Tx, Ty, true>(comp, howmany, X, Y, istride, ostride, idist, odist,
                          nt);
    return;
  }
#endif
  execute<Tx, Ty, false>(comp, howmany, X, Y, istride, ostride, idist, odist,
                         1);
}

#define EXECUTE(Tx, Ty)                                                        \
  template void execute<Tx, Ty>(                                               \
      const composition<Tx, Ty> &comp, int64_t howmany, const Tx *X, Ty *Y,    \
      int64_t istride, int64_t ostride, int64_t idist, int64_t odist);

EXECUTE(half, complex_half)
EXECUTE(complex_half, half)
EXECUTE(complex_half, complex_half)
EXECUTE(float, complex_float)
EXECUTE(complex_float, float)
EXECUTE(complex_float, complex_float)
EXECUTE(double, complex_double)
EXECUTE(complex_double, double)
EXECUTE(complex_double, complex_double)
#if PLFFT_ENABLE_FIXED_POINT
EXECUTE(int8_t, complex_int8_t)
EXECUTE(complex_int8_t, int8_t)
EXECUTE(complex_int8_t, complex_int8_t)
EXECUTE(int16_t, complex_int16_t)
EXECUTE(complex_int16_t, int16_t)
EXECUTE(complex_int16_t, complex_int16_t)
#endif // PLFFT_ENABLE_FIXED_POINT

#undef EXECUTE

} // namespace plfft
