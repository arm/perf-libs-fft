/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <algorithm>
#include <utility>

inline unsigned factorial(unsigned n) {
  return n <= 1 ? 1 : n * factorial(n - 1);
}

namespace detail {
/// A class to iterate over all permutations of a range
template<typename T>
class permutation_range_impl {
  T val;

public:
  permutation_range_impl(T val) : val(std::move(val)) {}

  class iterator {
    unsigned remaining;
    std::pair<unsigned, T> val;

  public:
    using difference_type = ptrdiff_t;
    using value_type = std::pair<unsigned, T>;
    using pointer = value_type *;
    using reference = value_type &;
    using iterator_category = std::input_iterator_tag;

    explicit iterator() : remaining(0) {}

    explicit iterator(T val)
      : remaining(factorial(val.size())),
        val(std::make_pair(0u, std::move(val))) {}

    inline iterator &operator++() {
      std::next_permutation(val.second.begin(), val.second.end());
      ++val.first;
      --remaining;
      return *this;
    }

    inline bool operator==(const iterator &other) const {
      return remaining == other.remaining;
    }

    inline bool operator!=(const iterator &other) const {
      return !(*this == other);
    }

    inline const std::pair<unsigned, T> &operator*() const {
      return val;
    }
  };

  inline iterator begin() {
    return iterator{val};
  }

  inline iterator end() {
    return iterator{};
  }
};
} // namespace detail

template<typename T>
inline detail::permutation_range_impl<T> permutation_range(T val) {
  return {std::move(val)};
}
