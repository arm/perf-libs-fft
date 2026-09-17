/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "gen_asm.hpp"
#include "plfft_assert.hpp"
#include "plfft_complex.hpp"
#include "plfft_kernels.hpp"
#include "providers/jit/kernel_printer.hpp"
#include "providers/jit/print_algo.hpp"
#include "providers/jit/target.hpp"
#include "providers/jit/wfta_nfact.hpp"
#include "providers/jit/winograd_reorder.hpp"
#include <cinttypes>
#include <string>
#include <type_traits>

static std::vector<int> get_factorisation(int prod) {
  ASSERT(1 < prod && prod < 23 && "unsupported length");

  // supported factors: 2, 3, 4, 5, 7, 8, 9, 11, 13, 16, 17, 19
  switch (prod) {
  case 6:
  case 10:
  case 14:
  case 18:
  case 22:
    return {prod / 2, 2};
  case 12:
  case 15:
  case 21:
    return {prod / 3, 3};
  case 20:
    return {5, 4};
  default:
    return {prod};
  }
}

template<typename Tx, typename Ty>
void plfft::generate_kernel(const wfta::options_t &opts, int64_t n,
                            plfft_direction_t dir, wfta::twiddleness twiddle,
                            order_kind order,
                            const wfta::known_layout_t &known_layout,
                            const io_mods_t &mods) {
  static_assert(std::is_same_v<remove_complex_t<Tx>, remove_complex_t<Ty>>,
                "Tx and Ty must have the same non-complex type");

  // TODO: move Tw to template parameter list to allow promoting to higher
  //       precision for intermediate arithmetic (e.g. for jcj kernels)
  using Tw = add_complex_t<Tx>;

  constexpr bool is_fixed_point = std::is_integral_v<remove_complex_t<Tw>>;

  auto factors = get_factorisation((int)n);
  auto nfact = wfta::wfta_nfact(factors, is_fixed_point);
  auto in_perm = wfta::get_in_perm(factors);
  auto out_perm = wfta::get_out_perm(factors);

  std::string n_str = std::to_string(n);

  const std::string fnname = wfta::get_kernel_name<Tx, Ty, Tw>(
      n_str, twiddle, dir, order, known_layout, mods, opts);

  kernel_registry_entry<void> out;
  auto kernel_data = wfta::print_algo<Tx, Ty, Tw>(
      &out, nfact.algo, n, n_str, nfact.iop, in_perm, out_perm, twiddle, dir,
      order, std::move(fnname), opts, known_layout, mods);

  wfta::kernel_registry_map kernel_registry;
  wfta::finalize_kernels(kernel_registry, {kernel_data}, opts);
}

#define GENERATE_KERNEL(Tx, Ty)                                                \
  template void plfft::generate_kernel<Tx, Ty>(                                \
      const wfta::options_t &options, int64_t n, plfft_direction_t dir,        \
      wfta::twiddleness twiddle, order_kind order,                             \
      const wfta::known_layout_t &known_layout, const io_mods_t &mods);

GENERATE_KERNEL(half, std::complex<half>)
GENERATE_KERNEL(std::complex<half>, half)
GENERATE_KERNEL(std::complex<half>, std::complex<half>)
GENERATE_KERNEL(float, std::complex<float>)
GENERATE_KERNEL(std::complex<float>, float)
GENERATE_KERNEL(std::complex<float>, std::complex<float>)
GENERATE_KERNEL(double, std::complex<double>)
GENERATE_KERNEL(std::complex<double>, double)
GENERATE_KERNEL(std::complex<double>, std::complex<double>)
GENERATE_KERNEL(int8_t, std::complex<int8_t>)
GENERATE_KERNEL(std::complex<int8_t>, int8_t)
GENERATE_KERNEL(std::complex<int8_t>, std::complex<int8_t>)
GENERATE_KERNEL(int16_t, std::complex<int16_t>)
GENERATE_KERNEL(std::complex<int16_t>, int16_t)
GENERATE_KERNEL(std::complex<int16_t>, std::complex<int16_t>)

#undef GENERATE_KERNEL
