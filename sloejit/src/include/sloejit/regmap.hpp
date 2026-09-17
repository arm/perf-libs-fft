/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "reg.hpp"
#include "regset_unsized.hpp"

#include <algorithm>
#include <vector>

namespace sloejit {

/**
 * A class for handling mapping registers into other data. This is useful in
 * e.g. constraint set checking. None of the primary use cases of this
 * structure care about the active_mask of the register being indexed, so it is
 * ignored in this container.
 */
template <typename T>
class regmap {
	/**
	 * Check whether the element of the vector actually contains valid data.
	 * We use this rather than a std::optional around each data element since it
	 * means we can scan through not-present elements much faster.
	 */
	regset_unsized presence;

	/// The actual data being stored, indexed by reg space_id, then by reg id.
	std::array<std::vector<T>, reg_space_count> data;

public:
	/// Reset the container to an empty state.
	inline void clear() {
		presence.clear();
		for (unsigned i = 0; i < data.size(); ++i) {
			data.clear();
		}
	}

	/// Lookup an element in the map, creating it if it doesn't already exist.
	inline T &operator[](reg k) {
		sloejit_assert(k.space_id < data.size());
		auto &data_local = data[k.space_id];
		if (k.id >= data_local.size()) {
			data_local.resize(k.id + 1);
		}
		presence.insert(k);
		return data_local[k.id];
	}

	/// Lookup an element in the map, asserting that it already exists.
	inline const T &operator[](reg k) const {
		return at(k);
	}

	/// Lookup an element in the map, asserting that it already exists.
	inline T &at(reg k) {
		sloejit_assert(presence.count(k));
		return data[k.space_id][k.id];
	}

	/// Lookup an element in the map, asserting that it already exists.
	inline const T &at(reg k) const {
		sloejit_assert(presence.count(k));
		return data[k.space_id][k.id];
	}

	/**
	 * Lookup an element in the map, returning it if it exists, else returning
	 * the specified default.
	 *
	 * @param[in] k The key to lookup with.
	 * @param[in] v The default value if the value for the specified key is not present.
	 */
	inline const T &at_or(reg k, const T &v) const {
		return presence.count(k) ? data[k.space_id][k.id] : v;
	}

	/**
	 * Insert a new entry into the map with replacement.
	 *
	 * @param[in] k The key to lookup with.
	 * @param[in] v The value to insert.
	 * @returns     True if this is a new entry.
	 */
	inline bool insert(reg k, T v) {
		sloejit_assert(k.space_id < data.size());
		auto &data_local = data[k.space_id];
		if (k.id >= data_local.size()) {
			data_local.resize(k.id + 1);
		}
		data_local[k.id] = std::move(v);
		return presence.insert(k);
	}

	/**
	 * Erase the specified entry from the map, or nothing if no value was present.
	 *
	 * @param[in] space_id The space_id index to erase.
	 * @param[in] id       The id index to erase.
	 * @returns            True if a value was previously present.
	 */
	inline bool erase(reg r) {
		bool ret = presence.erase(r);
		if (ret) {
			data[r.space_id][r.id] = T{};
		}
		return ret;
	}

	/**
	 * Count the number of occurrences of the specified register. Since this is
	 * not a multimap, this will always be either 0 or 1.
	 *
	 * @param[in] r The register to lookup.
	 * @returns     The number of occurrences of the specified key, either 0 or 1.
	 */
	inline int count(reg r) const {
		return presence.count(r);
	}

	/// Returns true if this map has no elements.
	inline bool empty() const {
		return presence.empty();
	}
};

} // namespace sloejit
