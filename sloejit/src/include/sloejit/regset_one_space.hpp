/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "reg.hpp"
#include "regset.hpp"

#include <algorithm>
#include <vector>

namespace sloejit {

/**
 * A class for iterating over a particular regset_one_space. Modifications to
 * the underlying structure while iterating are not guaranteed to leave the
 * iterator in a valid state.
 */
class regset_one_space_iterator {
	/// The underlying data of active_mask values (0 = not-present).
	const uint8_t *data;
	/// The size of the underlying data vector.
	uint64_t size;
	/// What space does the underlying regset_one_space refer to.
	uint64_t space_id;
	/// What id are we currently on.
	uint64_t id;

	/// A private method to advance id to the next present element in data.
	void move_to_next_elem() {
		if (id >= size) {
			return;
		}
		auto id_ = id + 1;
		auto data_sz = size;
		auto *data_ = data;
		while (id_ + 7 < data_sz && *reinterpret_cast<const uint64_t *>(&data_[id_]) == 0) {
			id_ += 8;
		}
		while (id_ < data_sz && data_[id_] == 0) {
			++id_;
		}
		id = id_;
	}

public:
	/// Construct a regset_one_space_iterator from the specified data, space_id and id.
	/// The iterator is set to the first present element within the data range.
	regset_one_space_iterator(decltype(data) data, uint64_t size, uint64_t space_id, uint64_t id)
	    : data(data), size(size), space_id(space_id), id(id) {
		// move the iterator to point at a valid element (or end)
		sloejit_assert(data || size == 0);
		if (id < size && data[id] == 0) {
			move_to_next_elem();
		}
	}

	/// Get the current (present) pointed-to element from the iterator.
	inline reg operator*() const {
		sloejit_assert(data[id] != 0);
		return { space_id, id, data[id] };
	}

	/// Advance the iterator to the next present element.
	inline regset_one_space_iterator &operator++() {
		move_to_next_elem();
		return *this;
	}

	inline bool operator==(const regset_one_space_iterator &i) const {
		return space_id == i.space_id && id == i.id;
	}

	inline bool operator!=(const regset_one_space_iterator &i) const {
		return !(*this == i);
	}
};

} // namespace sloejit

namespace std {
template <>
struct iterator_traits<sloejit::regset_one_space_iterator> {
	using difference_type = int;
	using iterator_category = input_iterator_tag;
	using value_type = sloejit::reg;
	using pointer = const sloejit::reg *;
	using reference = sloejit::reg;
};
} // namespace std

namespace sloejit {

/**
 * A container class for registers where we know in advance that all registers
 * belong to a single space. An example of this is when checking for register
 * allocation constraints we don't care about registers from different spaces,
 * since they could never be allocated the same register to begin with).
 */
class regset_one_space {
	/// The underlying active_mask data (0 = not-present).
	std::vector<uint8_t> data;
	/// The space that this regset refers to, set on first insert (else 0).
	uint64_t space_id = 0;

public:
	/// Construct an empty register set.
	regset_one_space() = default;

	/// Construct a register set with the specified register set.
	regset_one_space(const std::initializer_list<reg> &regs) {
		sloejit_assert(regs.size() >= 1);
		for (reg r : regs) {
			insert(r);
		}
	}

	/// Construct a register set from the specified iterators.
	template <typename It, typename = std::void_t<typename std::iterator_traits<It>::value_type>>
	regset_one_space(It begin, It end) {
		sloejit_assert(begin != end);
		for (; begin != end; ++begin) {
			insert(*begin);
		}
	}

	/// Return a regset populated by the contents of this one.
	regset as_regset() const {
		return { begin(), end() };
	}

	/// Reset the set to all-false.
	inline void clear() {
		space_id = 0;
		data.clear();
	}

	/// Get the number of present elements in the set.
	inline size_t size() const {
		return std::distance(begin(), end());
	}

	/**
	 * Insert a new register into the set.
	 *
	 * @param[in] r The register to insert.
	 * @returns     True if anything changed.
	 */
	inline bool insert(reg r) {
		if (!space_id) {
			space_id = r.space_id;
		}
		sloejit_assert(r.space_id == space_id);
		if (r.id >= data.size()) {
			data.resize(r.id + 1);
		}
		uint8_t new_val = data[r.id] | r.active_mask;
		bool ret = new_val != data[r.id];
		data[r.id] = new_val;
		return ret;
	}

	/**
	 * Erase a particular register from the set (note this ignores the
	 * active_mask of the specified register).
	 *
	 * @param[in] r The register to erase.
	 * @returns     True if the register was previously present.
	 */
	inline bool erase(reg r) {
		if (!space_id || r.space_id != space_id || r.id >= data.size()) {
			return false;
		}
		bool ret = data[r.id] != 0;
		data[r.id] = 0;
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
		sloejit_assert(!space_id || r.space_id == space_id);
		if (r.id >= data.size()) {
			return 0;
		}
		return data[r.id] != 0;
	}

	/// Returns true if this set has no elements.
	inline bool empty() const {
		for (uint64_t j = 0; j < data.size(); ++j) {
			if (data[j] > 0) {
				return false;
			}
		}
		return true;
	}

	/**
	 * Insert all elements from another one in the same register space.
	 *
	 * @param[in] rs The register set to read from.
	 * @returns      True if anything changed.
	 */
	inline bool insert_many(const regset_one_space &rs) {
		if (!rs.space_id) {
			return false;
		}
		if (!space_id) {
			space_id = rs.space_id;
		}
		sloejit_assert(space_id == rs.space_id);
		bool any = false;
		auto &rs_data = rs.data;
		if (data.size() < rs_data.size()) {
			data.resize(rs_data.size());
		}
		for (uint64_t j = 0; j < rs_data.size(); ++j) {
			auto new_val = data[j] | rs_data[j];
			any = any || (new_val != data[j]);
			data[j] = new_val;
		}
		return any;
	}

	/**
	 * Erase elements from this set that are not present in the specified one.
	 *
	 * @param[in] rs The register set to read from.
	 * @returns      True if anything changed.
	 */
	inline bool intersect(const regset_one_space &rs) {
		if (!space_id) {
			return false;
		}
		sloejit_assert(!rs.space_id || space_id == rs.space_id);
		bool any = false;
		auto &rs_data = rs.data;
		auto size = std::min(data.size(), rs_data.size());
		for (uint64_t j = 0; j < size; ++j) {
			auto new_val = data[j] & rs_data[j];
			any = any || (new_val != data[j]);
			data[j] = new_val;
		}
		for (uint64_t j = size; j < data.size(); ++j) {
			if (data[j]) {
				data[j] = 0ull;
				any = true;
			}
		}
		return any;
	}

	/**
	 * Erase elements from this set that are present in the specified one.
	 *
	 * @param[in] rs The register set to read from.
	 * @returns      True if anything changed.
	 */
	inline bool erase_many(const regset_one_space &rs) {
		if (!space_id || !rs.space_id) {
			return false;
		}
		sloejit_assert(space_id == rs.space_id);
		bool any = false;
		auto &rs_data = rs.data;
		auto size = std::min(data.size(), rs_data.size());
		for (uint64_t j = 0; j < size; ++j) {
			auto new_val = data[j] & ~rs_data[j];
			any = any || (new_val != data[j]);
			data[j] = new_val;
		}
		return any;
	}

	/// Make an iterator pointing at the first present element in the set.
	inline regset_one_space_iterator begin() const {
		return { data.data(), data.size(), space_id, 0 };
	}

	/// Make an iterator pointing past the last present element in the set.
	inline regset_one_space_iterator end() const {
		return { data.data(), data.size(), space_id, data.size() };
	}
};

} // namespace sloejit
