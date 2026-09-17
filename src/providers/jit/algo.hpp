/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "expr.hpp"
#include "winograd.hpp"

#include <list>

namespace plfft::wfta {

/** Work backwards through the algo, replacing local pointers and any
 *  non-final occurrences of out pointers with fresh local variables.
 *
 * @param[in,out] algo  The algorithm to prettify.
 * @param[in] iop       The expr to in/out/local pointer mapping.
 * @param[in,out] fresh A factory class to generate new expr nodes.
 */
void prettify_algo(std::list<expr_t> &algo, const io_ptr_t &iop,
                   fresh_atom_factory &fresh);

/** introduce temporaries to avoid reads following writes to out ptrs,
 *  e.g. transform Y[a] = zb + zc;            zd = Y[a] + ze; ...
 *       into      zx   = zb + zc; Y[a] = zx; zd = zx   + ze; ...
 *
 * @param[in,out] algo  The algorithm to prettify.
 * @param[in] iop       The expr to in/out/local pointer mapping.
 * @param[in,out] fresh A factory class to generate new expr nodes.
 */
void isolate_writes(std::list<expr_t> &algo, const io_ptr_t &iop,
                    fresh_atom_factory &fresh);

/** introduce temporaries to avoid multiple reads of the same in ptr,,
 *  e.g. transform                       zx = X[a] + X[b]; zy = X[a] - X[b]; ...
 *       into      za = X[a]; zb = X[b]; zx = za   + zb  ; zy = za   - zb  ; ...
 *
 * @param[in,out] algo  The algorithm to prettify.
 * @param[in] iop       The expr to in/out/local pointer mapping.
 * @param[in,out] fresh A factory class to generate new expr nodes.
 */
void isolate_reads(std::list<expr_t> &algo, const io_ptr_t &iop,
                   fresh_atom_factory &fresh);

/** Create a new list of twiddle factor multiplications, and replace
 *  references to the input with references to the result of
 *  the multiplications
 *
 * @param[in] algo        The algorithm to inject twiddles into.
 * @param[in] n           The FFT problem size we are considering.
 * @param[in] iop         The expr to in/out/local pointer mapping.
 * @param[in] in_perm     The permutation array for loaded data.
 * @param[in,out] fresh   A factory class to generate new expr nodes.
 * @param[in] input_twid  Whether to apply the twiddle against the input or
 * output nodes. (This depends whether we are doing Decimation in
 * Time/Frequency).
 */
std::list<expr_t> twiddle_algo(std::list<expr_t> algo, int64_t n,
                               const io_ptr_t &iop,
                               const std::vector<int64_t> &in_perm,
                               fresh_atom_factory &fresh, bool input_twid);
} // namespace plfft::wfta
