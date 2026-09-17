/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <memory>

namespace sloejit {

struct interval {
	/// Where does this range begin.
	double begin = 0;

	/// Where does this range end.
	/// This may be either an instruction that consumes this register as
	/// input, or the instruction after the one that produces it if it
	/// is never referenced past this point but must be assigned.
	double end = 0;

	constexpr interval() = default;
	constexpr interval(decltype(begin) begin, decltype(end) end) : begin(begin), end(end) {
	}
};

static inline bool intervals_overlap(interval a, interval b) {
	//      +--------+       +-------+
	//             +------------+
	return b.end > a.begin && b.begin < a.end;
}

} // namespace sloejit
