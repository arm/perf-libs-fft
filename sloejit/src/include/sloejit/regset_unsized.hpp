/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "reg.hpp"

#include <stdio.h>
#include <vector>

namespace sloejit {

/**
 * A class for handling a set of registers where we do not care about the
 * active_mask element, only whether it is present or not. The main use
 * case for this is where we are dealing purely with virtual registers,
 * since the active_mask is usually insigificant (a virtual registers id
 * uniquely determines the active mask, this is not true for physical
 * registers as they may alias each other).
 */
class regset_unsized {
	/**
	 * We use a vector of uint64_t data under the hood to allow us to quickly
	 * seek through unset elements using the clz instruction. The vectors
	 * are indexed first by reg space_id, then by reg id divided by 64.
	 */
	std::vector<std::vector<uint64_t>> data;

public:
	/// Create an empty regset_unsized.
	regset_unsized() = default;

	/// Create a regset_unsized with the specified elements.
	regset_unsized(const std::initializer_list<reg> &regs) {
		for (reg r : regs) {
			insert(r);
		}
	}

	/// Reset the container to an empty state.
	inline void clear() {
		data.clear();
	}

	/**
	 * Insert a new entry into the set.
	 *
	 * @param[in] r The reg to add to the set (the active_mask is ignored).
	 * @returns     True if this is a new entry.
	 */
	inline bool insert(reg r) {
		if (r.space_id >= data.size()) {
			data.resize(r.space_id + 1);
		}
		auto &data_local = data[r.space_id];
		uint64_t ofs = r.id / 64ull;
		if (ofs >= data_local.size()) {
			data_local.resize(ofs + 1);
		}
		auto new_val = data_local[ofs] | (1ull << (r.id % 64));
		bool ret = new_val != data_local[ofs];
		data_local[ofs] = new_val;
		return ret;
	}

	/**
	 * Erase the specified entry from the map, or nothing if no value was
	 * present.
	 *
	 * @param[in] space_id The space_id index to erase.
	 * @param[in] id       The id index to erase.
	 * @returns            True if a value was previously present.
	 */
	inline bool erase(reg r) {
		if (r.space_id >= data.size()) {
			return false;
		}
		auto &data_local = data[r.space_id];
		uint64_t ofs = r.id / 64;
		if (ofs >= data_local.size()) {
			return false;
		}
		auto new_val = data_local[ofs] & ~(1ull << (r.id % 64));
		bool ret = new_val != data_local[ofs];
		data_local[ofs] = new_val;
		return ret;
	}

	/**
	 * Count the number of occurrences of the specified register. Since this is
	 * not a multiset, this will always be either 0 or 1.
	 *
	 * @param[in] r The register to lookup.
	 * @returns     The number of occurrences of the specified key, either 0 or 1.
	 */
	inline int count(reg r) const {
		if (r.space_id >= data.size()) {
			return 0;
		}
		auto &data_local = data[r.space_id];
		uint64_t ofs = r.id / 64;
		if (ofs >= data_local.size()) {
			return 0;
		}
		return (data_local[ofs] >> (r.id % 64)) & 0x1;
	}

	/// Returns true if this set has no elements.
	inline bool empty() const {
		for (uint64_t i = 0; i < data.size(); ++i) {
			auto &data_local = data[i];
			for (uint64_t j = 0; j < data_local.size(); ++j) {
				if (data_local[j] > 0) {
					return false;
				}
			}
		}
		return true;
	}

	/**
	 * Inserts registers present in another regset_unsized to this, but only
	 * for the specified space_id (other spaces are ignored).
	 *
	 * @param[in] rs       The regset_unsized to insert from.
	 * @param[in] space_id The space_id of interest.
	 * @returns            True if any new values were added.
	 */
	inline bool insert_many_one_space(const regset_unsized &rs, uint64_t space_id) {
		if (space_id >= rs.data.size()) {
			return false;
		}
		if (data.size() < space_id) {
			data.resize(space_id + 1);
		}
		bool any = false;
		auto &data_local = data[space_id];
		auto &rs_data_local = rs.data[space_id];
		if (data_local.size() < rs_data_local.size()) {
			data_local.resize(rs_data_local.size());
		}
		for (uint64_t j = 0; j < rs_data_local.size(); ++j) {
			auto new_val = data_local[j] | rs_data_local[j];
			any = any || (new_val != data_local[j]);
			data_local[j] = new_val;
		}
		return any;
	}

	/**
	 * Inserts registers present in another regset_unsized to this.
	 *
	 * @param[in] rs       The regset_unsized to insert from.
	 * @returns            True if any new values were added.
	 */
	inline bool insert_many(const regset_unsized &rs) {
		if (data.size() < rs.data.size()) {
			data.resize(rs.data.size());
		}
		bool any = false;
		for (uint64_t i = 0; i < rs.data.size(); ++i) {
			auto &data_local = data[i];
			auto &rs_data_local = rs.data[i];
			if (data_local.size() < rs_data_local.size()) {
				data_local.resize(rs_data_local.size());
			}
			for (uint64_t j = 0; j < rs_data_local.size(); ++j) {
				auto new_val = data_local[j] | rs_data_local[j];
				any = any || (new_val != data_local[j]);
				data_local[j] = new_val;
			}
		}
		return any;
	}

	/**
	 * Inserts registers between two iterators.
	 *
	 * @param[in] begin The start point (inclusive) for the iteration.
	 * @param[in] end   The end point (exclusive) for the iteration.
	 * @returns         True if any new values were added.
	 */
	template <typename It>
	inline bool insert_many(It begin, It end) {
		bool any = false;
		for (; begin != end; ++begin) {
			any = insert(*begin) || any;
		}
		return any;
	}

	/**
	 * Erases values from this regset_unsized that are not also present in the
	 * specified regset_unsized.
	 *
	 * @param[in] rs The regset_unsized to check.
	 */
	inline void intersect(const regset_unsized &rs) {
		auto size = std::min(data.size(), rs.data.size());
		for (uint64_t i = 0; i < size; ++i) {
			auto &data_local = data[i];
			auto &rs_data_local = rs.data[i];
			auto size_local = std::min(data_local.size(), rs_data_local.size());
			for (uint64_t j = 0; j < size_local; ++j) {
				data_local[j] &= rs_data_local[j];
			}
			for (uint64_t j = size_local; j < data_local.size(); ++j) {
				data_local[j] = 0ull;
			}
		}
		for (uint64_t i = size; i < data.size(); ++i) {
			auto &data_local = data[i];
			for (uint64_t j = 0; j < data_local.size(); ++j) {
				data_local[j] = 0ull;
			}
		}
	}

	/**
	 * Erases registers present in another regset_unsized from this.
	 *
	 * @param[in] rs The regset_unsized to check.
	 * @returns      True if any values were erased.
	 */
	inline bool erase_many(const regset_unsized &rs) {
		auto size = std::min(data.size(), rs.data.size());
		bool any = false;
		for (uint64_t i = 0; i < size; ++i) {
			auto &data_local = data[i];
			auto &rs_data_local = rs.data[i];
			auto size_local = std::min(data_local.size(), rs_data_local.size());
			for (uint64_t j = 0; j < size_local; ++j) {
				auto new_val = data_local[j] & ~rs_data_local[j];
				any = any || (new_val != data_local[j]);
				data_local[j] = new_val;
			}
		}
		return any;
	}
};

} // namespace sloejit
