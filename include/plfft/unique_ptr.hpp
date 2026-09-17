/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <cstdlib>
#include <new>
#include <type_traits>
#include <utility>

namespace plfft {

template<class T>
class unique_ptr {
  /* Custom minimal implementation of unique_ptr. std::unique_ptr may invoke
     operator new, which introduces dependency on libstdc++. This version uses
     malloc and placement new, which has no such dependency.  */
public:
  explicit unique_ptr(T *ptr) noexcept : m_ptr(ptr) {}

  unique_ptr() noexcept : m_ptr(nullptr) {}

  unique_ptr(std::nullptr_t) noexcept : m_ptr(nullptr) {}

  /* Delete copy-assignment and copy-constructor - if you can copy it then it's
     not unique!  */
  unique_ptr(const unique_ptr &) = delete;
  unique_ptr &operator=(const unique_ptr &) = delete;

  /* Converting constructor - this is required for implicit conversion from
     unique_ptr<Derived> to unique_ptr<Base>.  */
  template<class U, std::enable_if_t<std::is_base_of_v<T, U> &&
                                         std::is_convertible_v<U *, T *>,
                                     bool> = true>
  unique_ptr(unique_ptr<U> &&other) noexcept : m_ptr{other.release()} {}

  unique_ptr(unique_ptr<T> &&other) noexcept : m_ptr{other.release()} {}

  template<class U>
  std::enable_if_t<std::is_base_of_v<T, U> && std::is_convertible_v<U *, T *>,
                   unique_ptr &>
  operator=(unique_ptr<U> &&other) noexcept {
    if (m_ptr) {
      m_ptr->~T();
      std::free(m_ptr);
    }
    m_ptr = other.release();
    return *this;
  }

  unique_ptr &operator=(unique_ptr &&other) noexcept {
    if (this != &other) {
      if (m_ptr) {
        m_ptr->~T();
        std::free(m_ptr);
      }
      m_ptr = other.m_ptr;
      other.m_ptr = nullptr;
    }
    return *this;
  }

  ~unique_ptr() {
    if (m_ptr) {
      m_ptr->~T();
      std::free(m_ptr);
    }
  }

  T *release() noexcept {
    T *ret = m_ptr;
    m_ptr = nullptr;
    return ret;
  }

  T *get() const noexcept {
    return m_ptr;
  }

  explicit operator bool() const noexcept {
    return m_ptr;
  }

  T *operator->() const noexcept {
    return m_ptr;
  }

  T &operator*() const noexcept {
    return *get();
  }

private:
  T *m_ptr;
};

template<class T, class... Args>
unique_ptr<T> make_unique(Args &&...args) {
  T *buf = (T *)malloc(sizeof(T));
  T *obj = new (buf) T(std::forward<Args>(args)...);
  return unique_ptr(obj);
}

} // namespace plfft
