/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sloejit/regset.hpp"

#include <iostream>

static void run_regset_tests() {
	sloejit::regset rs, rs2, rs3;
	sloejit_assert(rs.empty());
	sloejit_assert(rs.count({ 1, 5, 1 }) == 0);
	sloejit_assert(rs.insert({ 1, 5, 1 }));
	sloejit_assert(!rs.insert({ 1, 5, 1 }));
	sloejit_assert(rs.count({ 1, 5, 1 }) == 1);
	sloejit_assert(!rs.empty());
	sloejit_assert(rs.erase(1, 5));
	sloejit_assert(!rs.erase(1, 5));
	sloejit_assert(rs.count({ 1, 5, 1 }) == 0);
	sloejit_assert(rs.empty());
	sloejit_assert(rs.count({ 1, 987, 1 }) == 0);
	sloejit_assert(rs.insert({ 1, 987, 1 }));
	sloejit_assert(!rs.insert({ 1, 987, 1 }));
	sloejit_assert(rs.count({ 1, 987, 1 }) == 1);
	sloejit_assert(!rs.empty());
	sloejit_assert(rs.erase(1, 987));
	sloejit_assert(!rs.erase(1, 987));
	sloejit_assert(rs.count({ 1, 987, 1 }) == 0);
	sloejit_assert(rs.empty());
	sloejit_assert(rs.insert({ 1, 8, 1 }));
	sloejit_assert(rs2.insert({ 1, 9, 1 }));
	sloejit_assert(rs.insert_many(rs2));
	sloejit_assert(rs.count({ 1, 8, 1 }) == 1);
	sloejit_assert(rs.count({ 1, 9, 1 }) == 1);
	sloejit_assert(!rs.insert_many(rs2));
	sloejit_assert(rs.erase_many(rs2));
	sloejit_assert(rs.count({ 1, 8, 1 }) == 1);
	sloejit_assert(rs.count({ 1, 9, 1 }) == 0);
	sloejit_assert(!rs.erase_many(rs2));
	std::cout << "regset tests passed!" << std::endl;
}

int main() {
	run_regset_tests();
}
