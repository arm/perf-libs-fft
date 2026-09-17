/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "padded.hpp"
#include "reg.hpp"

#include <algorithm>
#include <array>
#include <stdio.h>
#include <vector>

namespace sloejit {

class regset_iterator {
	const std::array<padded<std::vector<uint8_t>>, reg_space_count> *data;
	uint64_t space_id;
	uint64_t id;

	void move_to_next_elem() {
		if (space_id >= data->size()) {
			return;
		}
		auto space_id_ = space_id;
		auto id_ = id + 1;
	find_nonzero:
		auto data_sz = (*data)[space_id_].size();
		auto *data_ = (*data)[space_id_].data();
		while (id_ + 7 < data_sz && *reinterpret_cast<const uint64_t *>(&data_[id_]) == 0) {
			id_ += 8;
		}
		while (id_ < data_sz && data_[id_] == 0) {
			++id_;
		}
		if (id_ >= data_sz) {
			++space_id_;
			id_ = 0;
			if (space_id_ < data->size()) {
				goto find_nonzero;
			}
		}
		space_id = space_id_;
		id = id_;
	}

public:
	regset_iterator(decltype(data) data, uint64_t space_id, uint64_t id)
	    : data(data), space_id(space_id), id(id) {

		sloejit_assert(data);
		// check that the iterator points at a valid element (or end)
		if (space_id < data->size() && (id >= (*data)[space_id].size() || (*data)[space_id][id] == 0)) {
			move_to_next_elem();
		}
	}

	inline reg operator*() const {
		sloejit_assert((*data)[space_id][id] != 0);
		return { space_id, id, (*data)[space_id][id] };
	}

	inline regset_iterator &operator++() {
		move_to_next_elem();
		return *this;
	}

	inline bool operator==(const regset_iterator &i) const {
		return space_id == i.space_id && id == i.id;
	}

	inline bool operator!=(const regset_iterator &i) const {
		return !(*this == i);
	}
};

} // namespace sloejit

namespace std {
template <>
struct iterator_traits<sloejit::regset_iterator> {
	using difference_type = int;
	using iterator_category = input_iterator_tag;
	using value_type = sloejit::reg;
};
} // namespace std

namespace sloejit {

class regset {
	std::array<padded<std::vector<uint8_t>>, reg_space_count> data;

public:
	/// Construct an empty register set.
	regset() = default;

	/// Construct a register set with the specified register set.
	regset(const std::initializer_list<reg> &regs) {
		for (reg r : regs) {
			insert(r);
		}
	}

	/// Construct a register set from the specified iterators.
	template <typename It, typename = std::void_t<typename std::iterator_traits<It>::value_type>>
	regset(It begin, It end) {
		for (; begin != end; ++begin) {
			insert(*begin);
		}
	}

	inline void clear() {
		for (auto &elem : data) {
			elem.clear();
		}
	}

	inline size_t size() const {
		return std::distance(begin(), end());
	}

	inline bool insert(reg r) {
		sloejit_assert(r.space_id < data.size());
		auto &data_local = data[r.space_id];
		if (r.id >= data_local.size()) {
			data_local.resize(r.id + 1);
		}
		uint8_t new_val = data_local[r.id] | r.active_mask;
		bool ret = new_val != data_local[r.id];
		data_local[r.id] = new_val;
		return ret;
	}

	inline bool erase(uint64_t space_id, uint64_t id) {
		sloejit_assert(space_id < data.size());
		auto &data_local = data[space_id];
		if (id >= data_local.size()) {
			return false;
		}
		bool ret = data_local[id] != 0;
		data_local[id] = 0;
		return ret;
	}

	inline bool erase(reg r) {
		sloejit_assert(r.space_id < data.size());
		auto &data_local = data[r.space_id];
		if (r.id >= data_local.size()) {
			return false;
		}
		uint8_t new_val = data_local[r.id] & ~r.active_mask;
		bool ret = new_val != data_local[r.id];
		data_local[r.id] = new_val;
		return ret;
	}

	/**
	 * Lookup reg id, return reg with filled active_mask which must be
	 * narrower than the input if it exists, else just return the input.
	 * We can almost have an assert here of the form...
	 * `assert((data_local[r.id] | r.active_mask) == r.active_mask);`
	 * however in practice the user may specify a register narrower still
	 * than the previous instruction since they know more than us here.
	 */
	inline reg at_or_narrower(reg r) const {
		sloejit_assert(r.space_id < data.size());
		auto &data_local = data[r.space_id];
		if (r.id >= data_local.size()) {
			return r;
		}
		if (data_local[r.id] == 0) {
			return r;
		}
		uint8_t active_mask = data_local[r.id] & r.active_mask;
		return { r.space_id, r.id, active_mask };
	}

	inline int count(reg r) const {
		sloejit_assert(r.space_id < data.size());
		auto &data_local = data[r.space_id];
		if (r.id >= data_local.size()) {
			return 0;
		}
		return data_local[r.id] != 0;
	}

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

	inline bool insert_many_one_space(const regset &rs, uint64_t space_id) {
		sloejit_assert(space_id < data.size());
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

	inline bool insert_many(const regset &rs) {
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

	template <typename It, typename = std::void_t<typename std::iterator_traits<It>::value_type>>
	inline bool insert_many(It begin, It end) {
		bool any = false;
		for (; begin != end; ++begin) {
			any = insert(*begin) || any;
		}
		return any;
	}

	inline bool intersect(const regset &rs) {
		bool any = false;
		for (uint64_t i = 0; i < data.size(); ++i) {
			auto &data_local = data[i];
			auto &rs_data_local = rs.data[i];
			auto size_local = std::min(data_local.size(), rs_data_local.size());
			for (uint64_t j = 0; j < size_local; ++j) {
				auto new_val = data_local[j] & rs_data_local[j];
				any = any || (new_val != data_local[j]);
				data_local[j] = new_val;
			}
			for (uint64_t j = size_local; j < data_local.size(); ++j) {
				if (data_local[j]) {
					data_local[j] = 0ull;
					any = true;
				}
			}
		}
		return any;
	}

	template <typename It, typename = std::void_t<typename std::iterator_traits<It>::value_type>>
	inline bool intersect(It begin, It end) {
		regset rs(begin, end);
		return intersect(rs);
	}

	inline bool erase_many(const regset &rs) {
		bool any = false;
		for (uint64_t i = 0; i < data.size(); ++i) {
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

	inline regset_iterator begin() const {
		return { &data, 0, 0 };
	}

	inline regset_iterator end() const {
		return { &data, data.size(), 0 };
	}
};

} // namespace sloejit
