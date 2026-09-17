/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <cassert>
#include <cstdlib>
#include <new>
#include <type_traits>
#include <utility>

namespace plfft {

// A type for handling a vector of unique_ptrs, in place of std::vector
// i.e. this is restricted to working with non-copyable types
template<typename T>
class move_vector {
  static_assert(std::is_move_constructible_v<T>,
                "T must be move-constructible");

  T *data_ = nullptr;
  size_t size_ = 0;
  size_t capacity_ = 0;

public:
  move_vector() = default;

  ~move_vector() {
    clear();
    std::free(data_);
  }

  move_vector(const move_vector &) = delete;
  move_vector &operator=(const move_vector &) = delete;

  move_vector(move_vector &&other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
  }

  move_vector &operator=(move_vector &&other) noexcept {
    if (this != &other) {
      clear();
      std::free(data_);
      data_ = other.data_;
      size_ = other.size_;
      capacity_ = other.capacity_;
      other.data_ = nullptr;
      other.size_ = 0;
      other.capacity_ = 0;
    }
    return *this;
  }

  template<typename... Args>
  T &emplace_back(Args &&...args) {
    if (size_ == capacity_) {
      grow();
    }
    T *loc = new (&data_[size_]) T(std::forward<Args>(args)...);
    ++size_;
    return *loc;
  }

  T &push_back(T &&val) {
    if (size_ == capacity_) {
      grow();
    }
    new (&data_[size_]) T(std::move(val));
    return data_[size_++];
  }

  T &operator[](size_t i) {
    return data_[i];
  }

  const T &operator[](size_t i) const {
    return data_[i];
  }

  T &front() {
    return data_[0];
  }

  const T &front() const {
    return data_[0];
  }

  T &back() {
    assert(size_ > 0);
    return data_[size_ - 1];
  }

  const T &back() const {
    assert(size_ > 0);
    return data_[size_ - 1];
  }

  size_t size() const {
    return size_;
  }

  size_t capacity() const {
    return capacity_;
  }

  T *begin() {
    return data_;
  }

  const T *begin() const {
    return data_;
  }

  T *end() {
    return data_ + size_;
  }

  const T *end() const {
    return data_ + size_;
  }

  void clear() {
    for (size_t i = 0; i < size_; ++i) {
      data_[i].~T();
    }
    size_ = 0;
  }

private:
  void grow() {
    size_t new_cap = capacity_ ? capacity_ * 2 : 4;
    T *new_data = static_cast<T *>(std::malloc(sizeof(T) * new_cap));

    for (size_t i = 0; i < size_; ++i) {
      new (&new_data[i]) T(std::move(data_[i]));
      data_[i].~T();
    }
    std::free(data_);
    data_ = new_data;
    capacity_ = new_cap;
  }
};

} // namespace plfft
