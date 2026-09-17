/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <functional>

namespace plfft {

template<typename T>
class lazy {
  mutable T value;
  std::function<T()> fn;
  mutable bool has_value;

  T *get() {
    if (!has_value) {
      value = fn();
      has_value = true;
    }
    return &value;
  }

  const T *get() const {
    if (!has_value) {
      value = fn();
      has_value = true;
    }
    return &value;
  }

public:
  lazy(T value) : value(std::move(value)), has_value(true) {}

  lazy(std::function<T()> fn) : fn(std::move(fn)), has_value(false) {}

  T &operator*() & {
    return *get();
  }

  T operator*() && {
    return std::move(*get());
  }

  const T &operator*() const & {
    return *get();
  }

  T *operator->() {
    return get();
  }

  const T *operator->() const {
    return get();
  }
};

template<typename F>
auto make_lazy(F fn) -> lazy<decltype(fn())> {
  return lazy<decltype(fn())>(std::move(fn));
}

} // end namespace plfft
