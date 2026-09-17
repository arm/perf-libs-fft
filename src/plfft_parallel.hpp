/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft_util.hpp"
#include <cassert>

#define PLFFT_UNUSED(x) (void)(x)

namespace plfft::parallel {

/**
 * We keep track of the global maximum number of threads that we've
 * needed since library startup. For any parallel region, we request
 * this number of threads so that the OpenMP runtime never shrinks
 * the thread pool, avoiding the overhead of unnecessarily spawning
 * threads in the future.
 */
class parallel_state_t {
  int max_threads;

public:
  parallel_state_t() : max_threads{1} {}

  /**
   * Return the number of threads to spawn for a parallel region given
   * the number of threads that we want to use, updating the global
   * state (i.e. max threads we've used so far).
   *
   * Note: this is not an atomic operation and is therefore not thread
   * safe. If multiple threads attempt to perform an update then we
   * may end up with a garbage value in max_threads, but this will not
   * propagate: our bounds checks ensures that the returned value lies
   * in the range [ nt, omp_get_max_threads() ].
   *
   * @param[in] nt The number of threads that we want to use.
   * @returns      The number of threads to spawn.
   */
  PLFFT_ALWAYS_INLINE int request(const int nt) {
    // Operate on a thread-local copy of global max
    const int current_max = max_threads;

    if (nt > current_max) {
      const int new_max = bounded_get_max_threads(nt);
      max_threads = new_max; // update global state
      return new_max;
    }
    return bounded_get_max_threads(current_max);
  }
}; // class parallel_state_t

/**
 * OpenMP parallel loop abstraction
 *
 * @param nt number of threads
 * @param f the callable object to execute in parallel, takes form  f(int) ->
 * (ignored)
 */
template<typename Func>
PLFFT_ALWAYS_INLINE void parallel_loop(int nt, Func f) {
  assert(nt > 0);

#ifdef _OPENMP
  if (nt > 1) {
    // Decide how many threads to spawn
    extern parallel_state_t parallel_state;
    const int num_spawn = parallel_state.request(nt);
    PLFFT_UNUSED(num_spawn); // suppress erroneous warning from clang

#pragma omp parallel for num_threads(num_spawn) firstprivate(f)
    for (int i = 0; i < nt; ++i) {
      f(i);
    }
    return;
  }
#endif

  /*
   * we need to use a loop here in case the workload has been
   * split up despite not having multiple threads, this also
   * handles the nt < 1 case correctly
   */
  for (int i = 0; i < nt; ++i) {
    f(i);
  }
}

struct parallel_split {
  /// how many threads are we going to use
  int64_t threads;

  int64_t work_per_thread;

  int64_t total_actual_work;
  // the amount to add to each thread that needs additional work
  int64_t modifier;
  // the number of threads we need to add additional work to
  int64_t modified_threads;

  int64_t interleaved_rows;
}; // struct parallel_split

/**
 * Produces an even split of a work size across a specified number of threads
 * @param dimension the worksize
 * @param interleave specifies where there is a degree of indivisibility to the
 * worksize
 * @param max_threads the number of thread to split the work across
 */
PLFFT_ALWAYS_INLINE parallel_split make_parallel_split(int64_t dimension,
                                                       int64_t interleave,
                                                       int64_t max_threads) {

  if (max_threads == 1 || dimension == 0)
    return {1, dimension, dimension, 0, 1};

  const int64_t actual_work = iround_div(dimension, interleave);
  const int64_t threads = max_threads < actual_work ? max_threads : actual_work;
  const int64_t work_per_thread = actual_work / threads;
  const int64_t modified_threads =
      actual_work - (threads * work_per_thread); // avoid using mod

  return {threads, work_per_thread, dimension, 1, modified_threads, interleave};
}

PLFFT_ALWAYS_INLINE static std::pair<int64_t, int64_t>
work_distribution(int64_t thread_num, const parallel_split &split) {

  int64_t start = split.work_per_thread * thread_num;
  int64_t thread_work = split.work_per_thread;

  if (thread_num < split.modified_threads) {
    thread_work += split.modifier;
    start += thread_num * split.modifier;
  } else {
    start += split.modified_threads;
  }

  const int64_t actual_start = start * split.interleaved_rows;

  if (thread_num == (split.threads - 1)) {
    const int64_t actual_work = split.total_actual_work - actual_start;

    return {actual_start, actual_work};
  } else {
    const int64_t actual_work = thread_work * split.interleaved_rows;

    return {actual_start, actual_work};
  }
}

} // end namespace plfft::parallel
