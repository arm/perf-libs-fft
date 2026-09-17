/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "expr.hpp"
#include "kernel_registry_entry.hpp"
#include "plfft.h"
#include "plfft/algo_flops.hpp"
#include "plfft_kernels.hpp"
#include "rtype.hpp"
#include "sloejit/ir.hpp"
#include "target.hpp"
#include "winograd.hpp"

#include <optional>
#include <string>

namespace plfft::wfta {

struct known_layout_t {
  std::optional<int64_t> howmany;
  std::optional<int64_t> istride;
  std::optional<int64_t> idist;
  std::optional<int64_t> ostride;
  std::optional<int64_t> odist;
};

struct options_t {
  target_t target;
  bool want_map_file;
  bool want_check_kernel_hash;
  bool want_kernel_src;
  bool want_sme;

  /// If set, override the output filename for the emitted assembly source.
  std::optional<std::string> src_path;
};

enum class twiddleness { none, dit, dif };

/**
 * A representation of in-progress kernels that still need allocating and
 * relocating. These can be allocated properly by calling finalize_kernels (see
 * kernel_printer.hpp).
 */
struct kernel_data {
  kernel_registry_entry<void> *out; ///< A pointer where a pointer to the kernel
                                    ///< should be stored to when allocated.
  std::string fn_name;              ///< The name of the kernel.
  std::vector<uint8_t> text_bytes;  ///< The .text code bytes of the kernel.
  std::vector<uint8_t> data_bytes;  ///< The .rodata code bytes of the kernel.
  std::vector<sloejit::reloc_info>
      relocs; ///< Relocations to be applied to the function once allocated.
  std::vector<sloejit::note_info>
      notes;                      ///< Notes to be stored alongside the kernel.
  std::optional<std::string> src; ///< The assembly source code representation
                                  ///< of the kernel.
  algo_flops flops; ///< The flops data for this kernel, used in fftw_flops.

  kernel_data(decltype(out) out, decltype(fn_name) fn_name,
              decltype(text_bytes) text_bytes, decltype(data_bytes) data_bytes,
              decltype(relocs) relocs, decltype(notes) notes, decltype(src) src,
              decltype(flops) flops)
    : out(out), fn_name(std::move(fn_name)), text_bytes(std::move(text_bytes)),
      data_bytes(std::move(data_bytes)), relocs(std::move(relocs)),
      notes(std::move(notes)), src(std::move(src)), flops(std::move(flops)) {}
};

/**
 * Build the appropriate FFT kernel for a problem with the specified
 * properties.
 * A pointer to the kernel text section is returned, and also registered to
 * the specified kernel registry (where the name it is registered with is
 * defined as a function of the kernel properties, see docs/kernel_naming.md.
 *
 * @param[in,out] out         A pointer to the destination metadata to set.
 * @param[in] algo            The algo to generate a kernel for.
 * @param[in] n               The problem size to generate a kernel for.
 * @param[in] mid             A string representation of the kernel
 *                            factorisation.
 * @param[in] iop             The expr to in/out/local pointer mapping.
 * @param[in] in_perm         The permutation array for input data.
 * @param[in] out_perm        The permutation array for output data.
 * @param[in] twiddle         The twiddle application mode.
 * @param[in] dir             The plfft_direction_t (forwards/backwards) of the
 *                            algorithm.
 * @param[in] order           The iteration order of the algorithm.
 * @param[in] fnname          What to name the kernel being generated (see
 *                            url in description).
 * @param[in] opts            The options structure, including the target to
 *                            generate for.
 * @param[in] known_layout    Layout config vals, if known.
 * @param[in] mods            The input/output modifiers to use when emitting
 *                            loads from X and stores to Y.
 * @returns A kernel to be finalized with finalize_kernels.
 */
template<typename Tx, typename Ty, typename Tw>
kernel_data
print_algo(kernel_registry_entry<void> *out, std::list<expr_t> algo, int64_t n,
           const std::string &mid, const io_ptr_t &iop,
           const std::vector<int64_t> &in_perm,
           const std::vector<int64_t> &out_perm, twiddleness twiddle,
           plfft_direction_t dir, order_kind order, std::string fnname,
           const options_t &opts, const known_layout_t &known_layout,
           const io_mods_t &mods);

} // namespace plfft::wfta
