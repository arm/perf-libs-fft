/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

// Wrappers / stubs to avoid libstdc++ dependence on Linux.

#ifndef __APPLE__
// Forward on calls to __cxa_thread_atexits, provided by the C++ runtime on
// Linux (-lstdc++ or -lc++) to a call to __cxa_thread_atexit_impl, which is
// provided by glibc (-lc). __cxa_thread_atexit_impl was introduced into glibc
// 2.18: https://gitlab.com/gnutools/glibc/-/tags/glibc-2.18
extern "C" int __cxa_thread_atexit_impl(void (*dtor)(void *), void *obj,
                                        void *dso_handle);

extern "C" __attribute__((weak)) int
__cxa_thread_atexit(void (*dtor)(void *), void *obj, void *dso_handle) {
  return __cxa_thread_atexit_impl(dtor, obj, dso_handle);
}
#endif

// called if a pure-virtual function is called
extern "C" __attribute__((weak)) void __cxa_pure_virtual() {
  __builtin_trap();
}
