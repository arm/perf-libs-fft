/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "sloejit_assert.hpp"
#include <stdint.h>
#include <tuple>

namespace sloejit {

struct reg {
	uint64_t id;
	uint32_t space_id;
	uint8_t active_mask;

	constexpr reg() : id(0), space_id(0), active_mask(0) {
	}

	constexpr reg(uint64_t space_id, uint64_t id, uint8_t active_mask)
	    : id(id), space_id(space_id), active_mask(active_mask) {
	}

	inline bool operator==(reg o) const {
		return std::tie(space_id, id) == std::tie(o.space_id, o.id);
	}

	inline bool operator!=(reg o) const {
		return std::tie(space_id, id) != std::tie(o.space_id, o.id);
	}

	inline bool operator<(reg o) const {
		return std::tie(space_id, id) < std::tie(o.space_id, o.id);
	}
};

static_assert(sizeof(reg) == 2 * sizeof(uint64_t), "");

// The 5 reg space slots are [0]=none, 1=x, 2=v, 3=p, 4=za. See aarch64/aarch64.hpp.mako
constexpr size_t reg_space_count = 5;
} // namespace sloejit
