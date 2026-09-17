/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "wisdom.hpp"

#include <functional>
#include <utility>

namespace {

bool dominates(plfft::policy a, plfft::policy b) {
  // a dominates b if b's policy set is a subset of a's
  return b.is_subset_of(a);
}

template<typename T>
void hash_combine(std::size_t &seed, const T &value) {
  // based on the "short and effective" hash function from K&R
  seed = std::hash<T>{}(value) + 31 * seed;
}

} // namespace

namespace plfft {

std::size_t problem_hash::operator()(const problem &p) const {
  std::size_t seed = 0;

  hash_combine(seed, p.kind);
  hash_combine(seed, p.precision);
  hash_combine(seed, p.n);
  hash_combine(seed, p.istride);
  hash_combine(seed, p.ostride);
  hash_combine(seed, p.howmany);
  hash_combine(seed, p.idist);
  hash_combine(seed, p.odist);
  hash_combine(seed, p.dir);

  return seed;
}

std::optional<wisdom::entry> wisdom::lookup(const problem &p,
                                            policy requested_policy) {
  const auto *cand_entries = table.find(p);
  if (!cand_entries) {
    return std::nullopt;
  }

  const entry *best = nullptr;

  // keep the usable entry that dominates the best one seen so far
  for (const auto &candidate : *cand_entries) {
    if (!dominates(requested_policy, candidate.policy)) {
      continue;
    }

    if (!best || dominates(candidate.policy, best->policy)) {
      best = &candidate;
    }
  }

  if (!best) {
    return std::nullopt;
  }

  return *best;
}

void wisdom::update(const problem &p, entry entry_) {
  auto *cand_entries = table.find(p);
  if (!cand_entries) {
    cand_entries = table.insert(p, wisdom::entries{});
  }

  for (auto &existing : *cand_entries) {
    // overwrite wisdom if it has the same policy set
    if (existing.policy == entry_.policy) {
      existing = entry_;
      return;
    }
  }

  cand_entries->push_back(std::move(entry_));
}

} // namespace plfft
