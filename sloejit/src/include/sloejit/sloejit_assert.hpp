/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <cassert>

#ifdef NDEBUG
template <typename T>
static inline void sloejit_assert(T pred) {
	sloejit_assert((bool) pred);
}

template <>
inline void sloejit_assert(bool pred) {
	// For release builds, assert discards the condition, however
	// there may be side-effects which we do not want to be
	// discarded. Check it manually, but do nothing (invoke UB) if
	// the condition fails. This is also useful for working around
	// Wunused-variable or Wunused-parameter when vars or params
	// are only used for assertions.
	if (!pred) {
		__builtin_unreachable();
	}
}
#else
// For debug build, use stdlib assert. Define this to a macro so that
// the condition is reported correctly
#define sloejit_assert assert
#endif
