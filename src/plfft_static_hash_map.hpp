/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft_move_vector.hpp"

#include <cstddef>
#include <utility>

// static_hash_map with chaining using move_vector for each bucket
// Suitable when Value is always a unique_ptr and chaining is needed

namespace plfft {

template<typename Key, typename Value, size_t BucketCount, typename Hasher>
class static_hash_map {
public:
  struct entry {
    Key key;
    Value value;
  };

  static_hash_map() {
    for (size_t i = 0; i < BucketCount; ++i) {
      buckets_[i] = move_vector<entry>();
    }
  }

  Value *find(const Key &key) {
    size_t h = hasher_(key) % BucketCount;
    for (auto &e : buckets_[h]) {
      if (e.key == key)
        return &e.value;
    }
    return nullptr;
  }

  Value *insert(const Key &key, Value &&value) {
    size_t h = hasher_(key) % BucketCount;
    auto &chain = buckets_[h];
    for (auto &e : chain) {
      if (e.key == key)
        return &e.value;
    }
    chain.emplace_back(entry{key, std::move(value)});
    return &chain.back().value;
  }

private:
  Hasher hasher_;
  move_vector<entry> buckets_[BucketCount];
};

} // end namespace plfft
