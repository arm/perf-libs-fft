/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "sloejit_assert.hpp"

#include <array>
#include <vector>

namespace sloejit {

template <typename T, size_t N>
class small_vector {
	std::array<T, N> small_data;
	std::vector<T> large_data;
	size_t m_size = 0;

public:
	template <typename F>
	void map(F f) const {
		if (m_size <= N) {
			for (size_t i = 0; i < m_size; ++i) {
				f(small_data[i]);
			}
		}
		else {
			for (size_t i = 0; i < m_size; ++i) {
				f(large_data[i]);
			}
		}
	}

	T &operator[](size_t i) {
		return m_size <= N ? small_data[i] : large_data[i];
	}
	const T &operator[](size_t i) const {
		return m_size <= N ? small_data[i] : large_data[i];
	}

	size_t size() const {
		return m_size;
	}

	bool empty() const {
		return m_size == 0;
	}

	void push_back(T elem) {
		if (m_size < N) {
			small_data[m_size++] = std::move(elem);
		}
		else if (m_size == N) {
			large_data.resize(m_size + 1);
			for (size_t i = 0; i < N; ++i) {
				large_data[i] = std::move(small_data[i]);
			}
			large_data[N] = std::move(elem);
			m_size = N + 1;
		}
		else {
			large_data.emplace_back(std::move(elem));
			++m_size;
		}
	}

	void resize(size_t new_size) {
		sloejit_assert(new_size > m_size);
		if (new_size <= N) {
			for (size_t i = m_size; i < new_size; ++i) {
				new (&small_data[i]) T();
			}
			m_size = new_size;
			return;
		}
		sloejit_assert(false && "unimplemented");
	}

	template <typename... Args>
	void emplace_back(Args &&...args) {
		push_back(T{ std::forward<Args>(args)... });
	}
};

} // namespace sloejit
