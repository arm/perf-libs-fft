/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
/*
 * N = 8 kernels for c2c dft, to be compared with PLFFT kernels.
 *
 * TODO: 1) Benchmark error/accuracy lost
 *       2) Implement backward transform
 */
#include "../test/advanced_test_utils.hpp"
#include "./dot/n8_kernel.h"
#include "./dot/neon_kernels.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <tuple>
#include <vector>

constexpr int64_t n = 8;
constexpr int64_t howmany = 1;
constexpr int64_t istride = 1;
constexpr int64_t idist = n;
constexpr int64_t ostride = 1;
constexpr int64_t odist = n;
constexpr uint32_t reps = 1'000'000;

template<typename Execute, typename... Args>
double measure_time(uint32_t num_reps, Execute &&execute_func, Args &&...args) {
  using clock = std::chrono::steady_clock;

  // First dry run to warm up caches, etc.
  std::invoke(std::forward<Execute>(execute_func), std::forward<Args>(args)...);

  auto start_time = clock::now();
  for (uint32_t i = 0; i < num_reps; i++) {
    std::invoke(std::forward<Execute>(execute_func),
                std::forward<Args>(args)...);
  }
  auto end_time = clock::now();

  // Calculate the time elapsed in milliseconds
  std::chrono::duration<double, std::milli> diff_ms = (end_time - start_time);

  return diff_ms.count();
}

/* Helpers */
static void vprint_cmplx(auto vec, std::string msg) {
  for (int64_t hm = 0; hm < howmany; ++hm) {
    std::cout << msg << "[" << hm << "] = [";
    for (int64_t i = 0; i < n; ++i) {
      const auto &value = vec[hm * odist + i * ostride];
      if (i != 0)
        std::cout << ", ";
      std::cout << '(' << static_cast<double>(+value.real()) * n / 128.0 << ", "
                << static_cast<double>(+value.imag()) * n / 128.0 << ')';
    }
    std::cout << "]\n";
  }
}

template<auto Execute, auto ExecuteBatch>
struct neon_kernel {
  const char *name;

  __attribute__((always_inline)) inline void execute(const cmplx_int8_t *in,
                                                     cmplx_int8_t *out) const {
    Execute(in, out);
  }

  __attribute__((always_inline)) inline void
  execute_batch(const cmplx_int8_t *in, cmplx_int8_t *out,
                const std::size_t batch) const {
    ExecuteBatch(in, out, batch);
  }
};

static constexpr auto neon_kernels = std::tuple{
    neon_kernel<c2c_n8_vdft_s32_v2, c2c_n8_vdft_s32_v2_batch>{"v2"},
    neon_kernel<c2c_n8_vdft_s32_v4, c2c_n8_vdft_s32_v4_batch>{"v4"},
    neon_kernel<c2c_n8_vdft_s32_sym, c2c_n8_vdft_s32_sym_batch>{"sym"},
    neon_kernel<c2c_n8_vdft_s32_sym2, c2c_n8_vdft_s32_sym2_batch>{"sym2"},
};

using neon_kernel_types = decltype(neon_kernels);

template<typename Function>
static inline void for_each_neon_kernel(Function &&function) {
  std::apply(
      [&function](const auto &...kernel) {
        (std::invoke(function, kernel), ...);
      },
      neon_kernels);
}

static bool validate_neon_kernels() {
  using block_t = std::array<cmplx_int8_t, 8>;
  block_t input{};
  block_t ref{};
  std::array<int, std::tuple_size_v<neon_kernel_types>> max_errs{};
  int case_no = 0;

  const auto run_case = [&](const int current_case) {
    c2c_n8_dft_s32(input.data(), ref.data());

    bool valid = true;
    std::size_t kernel_idx = 0;
    for_each_neon_kernel([&](const auto kernel) {
      block_t output{};
      kernel.execute(input.data(), output.data());

      block_t in_place = input;
      kernel.execute(in_place.data(), in_place.data());
      if (in_place != output) {
        std::fprintf(stderr, "In-place validation failed: case=%d, kernel=%s\n",
                     current_case, kernel.name);
        valid = false;
      }

      for (int k = 0; k < 8; ++k) {
        const int real_err = std::abs(+output[k].real() - +ref[k].real());
        const int imag_err = std::abs(+output[k].imag() - +ref[k].imag());
        max_errs[kernel_idx] =
            std::max({max_errs[kernel_idx], real_err, imag_err});

        if (real_err > 1 || imag_err > 1) {
          std::fprintf(stderr,
                       "Validation failed: case=%d, kernel=%s, bin=%d; "
                       "ref=(%d,%d), output=(%d,%d)\n",
                       current_case, kernel.name, k, +ref[k].real(),
                       +ref[k].imag(), +output[k].real(), +output[k].imag());
          valid = false;
        }
      }
      ++kernel_idx;
    });
    return valid;
  };

  if (!run_case(case_no++)) {
    return false;
  }
  for (int sample = 0; sample < 8; ++sample) {
    input[sample] = {
        static_cast<int8_t>((sample & 1) ? -128 : 127),
        static_cast<int8_t>((sample & 2) ? -128 : 127),
    };
  }
  if (!run_case(case_no++)) {
    return false;
  }

  for (int elem = 0; elem < 16; ++elem) {
    for (const int value : {-128, 127}) {
      input.fill({0, 0});
      const int sample = elem >> 1;
      if ((elem & 1) == 0) {
        input[sample] = {static_cast<int8_t>(value), 0};
      } else {
        input[sample] = {0, static_cast<int8_t>(value)};
      }
      if (!run_case(case_no++)) {
        return false;
      }
    }
  }

  uint32_t random_state = 0x8f3a2c1du;
  for (int iter = 0; iter < 4096; ++iter) {
    for (cmplx_int8_t &value : input) {
      random_state = random_state * 1664525u + 1013904223u;
      const int8_t real = static_cast<int8_t>(random_state >> 24);
      random_state = random_state * 1664525u + 1013904223u;
      const int8_t imag = static_cast<int8_t>(random_state >> 24);
      value = {real, imag};
    }
    if (!run_case(case_no++)) {
      return false;
    }
  }

  printf("Validated %d input blocks; max errors:", case_no);
  std::size_t kernel_idx = 0;
  for_each_neon_kernel([&](const auto kernel) {
    printf(" %s=%d", kernel.name, max_errs[kernel_idx++]);
  });
  printf("\n");
  return true;
}

int main() {

  if (!validate_neon_kernels()) {
    return EXIT_FAILURE;
  }

  using real_t = remove_complex_t<int8_t>;
  const real_t nf = n;

  size_t in_size = (howmany - 1) * idist + (n - 1) * istride + 1;
  size_t out_size = (howmany - 1) * odist + (n - 1) * ostride + 1;

  std::vector<cmplx_int8_t> in(in_size);
  std::vector<cmplx_int8_t> out(out_size);
  // std::vector<int8_t> out2(in_size);

  for (int64_t hm = 0; hm < howmany; ++hm) {
    for (int64_t i = 0; i < n; ++i) {
      in[hm * idist + i * istride] =
          convert_to<real_t>{}(static_cast<double>(i + hm + 1) / nf);
    }
  }

  // Print outputs to check correctness, quantify accuracy/error in the future
  c2c_n8_dft_s32(in.data(), out.data());
  vprint_cmplx(out, "scalar");

  // Benchmark performance; timings at the moment
  double total_ms = measure_time(reps, [&] {
    c2c_n8_dft_s32(in.data(), out.data());
    // Prevent optimizing repeated identical calls away (GCC/Clang)
    asm volatile("" : : "r"(out.data()) : "memory");
  });

  printf(" Total: %.3f ms \n", total_ms);
  printf(" Average: %.2f ns per iter \n", total_ms * 1'000'000.0 / reps);

  for_each_neon_kernel([&](const auto kernel) {
    kernel.execute(in.data(), out.data());
    vprint_cmplx(out, kernel.name);

    const double kernel_total_ms = measure_time(reps, [&] {
      kernel.execute(in.data(), out.data());
      asm volatile("" : : "r"(out.data()) : "memory");
    });

    printf(" %s total: %.3f ms \n", kernel.name, kernel_total_ms);
    printf(" %s average: %.2f ns per iter \n", kernel.name,
           kernel_total_ms * 1'000'000.0 / reps);
  });

  constexpr std::size_t batch = 256;
  constexpr uint32_t batch_reps = 20'000;
  std::vector<cmplx_int8_t> batch_in(batch * n);
  std::vector<cmplx_int8_t> batch_out(batch * n);
  for (std::size_t hm = 0; hm < batch; ++hm) {
    std::copy_n(in.data(), n, batch_in.data() + hm * n);
  }

  for_each_neon_kernel([&](const auto kernel) {
    const double kernel_total_ms = measure_time(batch_reps, [&] {
      kernel.execute_batch(batch_in.data(), batch_out.data(), batch);
      asm volatile("" : : "r"(batch_out.data()) : "memory");
    });
    printf(" %s batch average: %.2f ns per transform \n", kernel.name,
           kernel_total_ms * 1'000'000.0 /
               (static_cast<double>(batch_reps) * batch));
  });

  /* Reference PLFFT outputs */
  auto forward_plan = plfft::make_batched_1d_plan<cmplx_int8_t, cmplx_int8_t>(
      n, howmany, istride, idist, ostride, odist, -1, PLFFT_IO_NO_ALIAS,
      static_cast<plfft_r2r_kind_t>(0), 10, 0.05);

  if (!forward_plan) {
    std::fprintf(stderr, "Failed to create c2c FFT plan for n = %" PRId64 "\n",
                 n);
    return EXIT_FAILURE;
  }

  std::cout << forward_plan->plan_to_string() << std::endl;
  forward_plan->execute(in.data(), out.data());
  // backward_plan->execute(out, out2);

  total_ms = measure_time(reps, [&] {
    forward_plan->execute(in.data(), out.data());
    // Prevent optimizing repeated identical calls away (GCC/Clang)
    asm volatile("" : : "r"(out.data()) : "memory");
  });

  printf(" PLFFT total: %.3f ms \n", total_ms);
  printf(" PLFFT average: %.2f ns per iter \n", total_ms * 1'000'000.0 / reps);

  printf(" Forward c2c PLFFT: \n");
  vprint_cmplx(out, "PLFFT");

  plfft::clean();
  return EXIT_SUCCESS;
}
