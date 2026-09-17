/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft_assert.hpp"
#include "plfft_complex.hpp"
#include "plfft_lazy.hpp"
#include "plfft_util.hpp"
#include "print_algo.hpp"

#include <sstream>
#include <string>
#include <string_view>

namespace plfft::wfta {

using kernel_registry_map = std::map<std::string, kernel_registry_entry<void>>;

static inline char direction_to_char(plfft_direction_t d) {
  switch (d) {
  case PLFFT_FORWARD:
    return 'f';
  case PLFFT_BACKWARD:
    return 'b';
  }
  ASSERT(false);
}

static inline std::string_view order_to_string(order_kind k) {
  switch (k) {
  case ORDER_NA:
    return "na";
  case ORDER_AB:
    return "ab";
  case ORDER_AC:
    return "ac";
  }
  ASSERT(false);
}

static inline std::string_view rtype_to_string(rtype rt, bool mod_real) {
  switch (rt.bits()) {
  case 8:
    return mod_real ? "r" : "p";
  case 16:
    return mod_real ? "h" : "j";
  case 32:
    return mod_real ? "s" : "c";
  case 64:
    return mod_real ? "d" : "z";
  }
  ASSERT(false);
}

/**
 * Allocate memory for kernels and resolve relocations. This is done at
 * once to avoid mmap giving us memory addresses that are too far away
 * from each other (which would be troublesome for resolving relocations).
 *
 * @param[in,out] kernel_registry The kernel registry to register the
 * in-progress kernels against.
 * @param[in] in_progress_kernels The kernels to resolve and register.
 */
void finalize_kernels(kernel_registry_map &kernel_registry,
                      const std::vector<kernel_data> &in_progress_kernels,
                      const options_t &opts);

template<typename Tx, typename Ty, typename Tw>
static std::string get_kernel_name(const std::string &mid, twiddleness twiddle,
                                   plfft_direction_t dir, order_kind order,
                                   const known_layout_t &known_layout,
                                   const io_mods_t &mods,
                                   const options_t &opts) {
  static constexpr auto rtx = rtype_from_real_type_v<remove_complex_t<Tx>>;
  static constexpr auto rty = rtype_from_real_type_v<remove_complex_t<Ty>>;
  static constexpr auto rtw = rtype_from_real_type_v<remove_complex_t<Tw>>;

  std::string id = "";
  if (!rtw.is_float()) {
    id += "q";
    id += std::to_string(rtw.bits() - 1);
    id += "_";
  }
  id += rtype_to_string(rtx, mods.in == in_mods::im_real);
  id += rtype_to_string(rtw, /*mod_real=*/false);
  id += rtype_to_string(rty, mods.out == out_mods::om_real);
  id += twiddle == twiddleness::none ? 'n' : 't';
  id += direction_to_char(dir);
  if (mods.out == out_mods::halfhi) {
    id += "oh";
  } else if (mods.out == out_mods::halflo) {
    id += "ol";
  } else if (mods.out == out_mods::conj_reverse) {
    id += 'j';
  } else if (mods.in == in_mods::im_halfhi) {
    id += "ih";
  } else if (mods.in == in_mods::im_halflo) {
    id += "il";
  }
  if (twiddle == twiddleness::dit) {
    id += "_dit";
  } else if (twiddle == twiddleness::dif) {
    id += "_dif";
  }

  id += "_";

  if (known_layout.idist == 1) {
    id += "u"; // unit stride
  } else if (known_layout.istride == 1) {
    id += "t"; // transposed
  } else {
    id += "g"; // gathered
  }
  if (known_layout.odist == 1) {
    id += "u"; // unit stride
  } else if (known_layout.ostride == 1) {
    id += "t"; // transposed
  } else {
    id += "s"; // scattered
  }

  if (known_layout.howmany == 1) {
    id += "n";
  }

  std::ostringstream sstm;
  sstm << "plfft_";
  if (order != ORDER_NA) {
    sstm << order_to_string(order) << '_';
  }
  sstm << mid << '_' << id << '_' << opts.target.name;
  return sstm.str();
}

struct kernel_algo_data {
  std::list<expr_t> algo;        ///< The algo to generate a kernel for.
  std::vector<int64_t> in_perm;  ///< The permutation array for input data.
  std::vector<int64_t> out_perm; ///< The permutation array for output data.
  io_ptr_t iop;                  ///< The expr to in/out/local pointer mapping.

  kernel_algo_data() = default;

  kernel_algo_data(decltype(algo) algo, decltype(in_perm) in_perm,
                   decltype(out_perm) out_perm, decltype(iop) iop)
    : algo(std::move(algo)), in_perm(std::move(in_perm)),
      out_perm(std::move(out_perm)), iop(std::move(iop)) {}
};

/**
 * A helper class to aid in building multiple similar kernels that share
 * the majority of their parameters (such as problem size and base
 * algorithm).
 */
template<typename Tx, typename Ty, typename Tw>
class kernel_printer {
  kernel_registry_map &kernel_registry;
  lazy<kernel_algo_data> data;
  int64_t n;
  const std::string &mid;
  plfft_direction_t dir;
  const options_t &opts;
  known_layout_t known_layout;

  /// The kernels waiting to be finalized (see emit and finalize_kernels).
  std::vector<kernel_data> in_progress_kernels;

public:
  /**
   * @param[in,out] kernel_registry The mapping from FFT kernel name to text
   *                                section pointer, to allow us to avoid
   *                                duplicated kernel generations.
   * @param[in] data                Data needed for generating new kernels.
   * @param[in] n                   The problem size to generate a kernel for.
   * @param[in] mid                 A string representation of the kernel
   *                                factorisation.
   * @param[in] dir                 The plfft_direction_t (forwards/backwards)
   * of the algorithm.
   * @param[in] opts                The options structure, including the target
   *                                to generate for.
   * @param[in] known_layout        Layout config vals, if known.
   */
  kernel_printer(kernel_registry_map &kernel_registry,
                 lazy<kernel_algo_data> data, int64_t n, const std::string &mid,
                 plfft_direction_t dir, const options_t &opts,
                 const known_layout_t &known_layout)
    : kernel_registry(kernel_registry), data(std::move(data)), n(n), mid(mid),
      dir(dir), opts(opts), known_layout(known_layout) {}

  /**
   * Loop over all kernels in progress, allocate memory for all of them
   * and resolve relocations.
   */
  void emit() {
    if (in_progress_kernels.size() > 0) {
      finalize_kernels(kernel_registry, in_progress_kernels, opts);
    }
  }

  /**
   * Build the appropriate FFT kernel for a problem with the specified
   * properties (just calls print_algo if the kernel doesn't
   * already exist).
   *
   * @param[in,out] text_ptr        A pointer to the destination text_ptr to
   * set.
   * @param[in] twiddle             The twiddle application mode.
   * @param[in] order               The iteration order of the algorithm.
   * @param[in] mods                The input/output modifiers to use when
   *                                emitting loads from X and stores to Y.
   */
  template<typename F>
  inline void print_algo(kernel_registry_entry<F> *out, twiddleness twiddle,
                         order_kind order, const io_mods_t &mods) {
    auto fnname = get_kernel_name<Tx, Ty, Tw>(mid, twiddle, dir, order,
                                              known_layout, mods, opts);
    auto it = kernel_registry.find(fnname);
    if (it != kernel_registry.end()) {
      if (out) {
        *(kernel_registry_entry<void> *)out = it->second;
      }
      return;
    }

    in_progress_kernels.push_back(wfta::print_algo<Tx, Ty, Tw>(
        (kernel_registry_entry<void> *)out, data->algo, n, mid, data->iop,
        data->in_perm, data->out_perm, twiddle, dir, order, std::move(fnname),
        opts, known_layout, mods));
  }
};

} // namespace plfft::wfta
