/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace sloejit {

class bytevector {
	std::vector<uint8_t> bytes;

public:
	template <typename... Ts>
	inline void push_u8(Ts... xs) {
		(bytes.push_back(xs), ...);
	}

	inline void push_u32(uint32_t x) {
		// assuming little-endian...
		bytes.push_back((x & 0x000000ffu) >> 0);
		bytes.push_back((x & 0x0000ff00u) >> 8);
		bytes.push_back((x & 0x00ff0000u) >> 16);
		bytes.push_back((x & 0xff000000u) >> 24);
	}

	inline size_t size() const {
		return bytes.size();
	}

	inline uint8_t &operator[](size_t i) {
		return bytes[i];
	}

	inline const uint8_t &operator[](size_t i) const {
		return bytes[i];
	}

	std::vector<uint8_t> &&get() && {
		return std::move(bytes);
	}
};

} // namespace sloejit
