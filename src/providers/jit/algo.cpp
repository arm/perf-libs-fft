/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "algo.hpp"
#include "plfft_assert.hpp"

#include <algorithm>
#include <map>
#include <set>

namespace plfft::wfta {

void prettify_algo(std::list<expr_t> &algo, const io_ptr_t &iop,
                   fresh_atom_factory &fresh) {
  std::map<atom, atom> vars;
  std::set<atom> is_out_written;

  for (auto it = algo.rbegin(); it != algo.rend(); ++it) {
    // replace lhs if it is a local ptr or a non-final out ptr
    if (is_local_ptr(iop, it->lhs) ||
        (is_out_ptr(iop, it->lhs) && is_out_written.count(it->lhs))) {
      auto rename_it = vars.find(it->lhs);
      ASSERT(rename_it != vars.end());
      it->lhs = rename_it->second;
      vars.erase(rename_it);
    }
    // maintain set of which out-ptrs are already written to
    if (is_out_ptr(iop, it->lhs)) {
      is_out_written.insert(it->lhs);
    }
    // replace left/right if they are local ptrs or non-final out ptrs.
    // if a mapping does not exist, create one with a fresh local variable.
    for (auto *elem : {&it->left, &it->right}) {
      if (is_local_ptr(iop, *elem) ||
          (is_out_ptr(iop, *elem) && is_out_written.count(*elem))) {
        auto rename_it = vars.find(*elem);
        if (rename_it == vars.end()) {
          auto new_atom = fresh();
          vars[*elem] = new_atom;
          *elem = new_atom;
        } else {
          *elem = rename_it->second;
        }
        ASSERT(!is_out_ptr(iop, *elem));
      }
    }
  }
}

void isolate_writes(std::list<expr_t> &algo, const io_ptr_t &iop,
                    fresh_atom_factory &fresh) {
  std::map<atom, atom> vars;

  for (auto it = algo.rbegin(); it != algo.rend(); ++it) {
    if (is_out_ptr(iop, it->lhs) && it->op) {
      auto it2 = vars.find(it->lhs);
      if (it2 != vars.end()) {
        // replace current lhs with temporary, insert new node for Y[lhs] = tmp.
        algo.insert(it.base(), expr_t(it->lhs, it2->second));
        ++it;
        it->lhs = it2->second;
        ASSERT(!is_out_ptr(iop, it->lhs));
        vars.erase(it->lhs);
      }
    }
    for (auto *elem : {&it->left, &it->right}) {
      if (is_out_ptr(iop, *elem)) {
        auto it2 = vars.find(*elem);
        if (it2 != vars.end()) {
          *elem = it2->second;
        } else {
          auto new_atom = fresh();
          vars[*elem] = new_atom;
          *elem = new_atom;
        }
        ASSERT(!is_out_ptr(iop, *elem));
      }
    }
  }
  if (!vars.empty()) {
    fprintf(
        stderr,
        "error: isolated write to output element that was never written to.\n");
    fprintf(stderr,
            "       this is probably a mistake in the algo somewhere\n");
    exit(EXIT_FAILURE);
  }
}

void isolate_reads(std::list<expr_t> &algo, const io_ptr_t &iop,
                   fresh_atom_factory &fresh) {
  std::map<atom, atom> vars;

  for (auto it = algo.begin(); it != algo.end(); ++it) {
    ASSERT(!is_in_ptr(iop, it->lhs));
    if (!it->op)
      continue;
    for (auto *elem : {&it->left, &it->right}) {
      if (is_in_ptr(iop, *elem)) {
        auto it2 = vars.find(*elem);
        if (it2 != vars.end()) {
          *elem = it2->second;
        } else {
          // replace elem with fresh atom, prepend new node for za = X[a].
          auto new_atom = fresh();
          vars[*elem] = new_atom;
          algo.insert(it, expr_t(new_atom, *elem));
          *elem = new_atom;
        }
      }
      ASSERT(!is_in_ptr(iop, *elem));
    }
  }
}

std::list<expr_t> twiddle_algo(std::list<expr_t> algo, int64_t n,
                               const io_ptr_t &iop,
                               const std::vector<int64_t> &io_perm,
                               fresh_atom_factory &fresh, bool input_twid) {

  std::list<expr_t> twiddle_muls;

  for (auto it = algo.begin(); it != algo.end(); ++it) {

    // Use strictly greater than in the first comparisons below because
    // the first input/output element is not multiplied by a twiddle factor.

    if (!input_twid && is_out_ptr(iop, it->lhs)) {
      // Output twiddles apply in backwards transform for complex-to-real
      // usage, so subtract index from n below
      auto ofs = get_out_ofs(iop, it->lhs);
      if (ofs > 0) {
        ASSERT(io_perm[ofs] < n);
        ASSERT(n - io_perm[ofs] > 0);
        size_t w_id_ptr = n - io_perm[ofs] - 1;
        twiddle_muls.push_back(
            expr_t(it->lhs, it->lhs, '*', atom(RT_WPTR, w_id_ptr)));
      }
    }

    for (auto *elem : {&it->left, &it->right}) {
      if (input_twid && is_in_ptr(iop, *elem)) {
        auto ofs = get_in_ofs(iop, *elem);
        if (ofs > 0) {
          auto id = fresh();
          ASSERT(io_perm[ofs] > 0);
          size_t w_id_ptr = io_perm[ofs] - 1;
          twiddle_muls.push_back(
              expr_t(id, *elem, '*', atom(RT_WPTR, w_id_ptr)));
          *elem = id;
        }
      }
    }
  }

  // Remove duplicate multiplications (will happen for each
  // input/output value that appears more than once in the algorithm)
  twiddle_muls.sort();
  twiddle_muls.unique();

  if (input_twid) {
    // Prepend twiddle_muls to algo
    algo.insert(algo.begin(), twiddle_muls.begin(), twiddle_muls.end());
  } else {
    // Append twiddle_muls to algo
    algo.insert(algo.end(), twiddle_muls.begin(), twiddle_muls.end());
  }
  return algo;
}

} // namespace plfft::wfta
