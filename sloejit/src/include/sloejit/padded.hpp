/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <climits>
#include <cstddef>
#include <vector>

namespace sloejit {

/// Get the smallest power of two y such that y >= x.
constexpr size_t next_pow2(size_t x) {
	return x <= 1 ? 1 : 1ull << (sizeof(size_t) * CHAR_BIT - __builtin_clzll(x - 1));
}

/**
 * Some uses of an object benefit from having the size be a power of two as
 * this allows more efficient indexing into it (a simple shift rather than a
 * more costly multiply). This class simply provides a wrapper that pads the
 * template argument to be a power of two size to enable this.
 */
template <typename T>
class padded : public T {
	/// Pad whatever this is to be a power of two size!
	char padding_[next_pow2(sizeof(T)) - sizeof(T)];
};

} // namespace sloejit
