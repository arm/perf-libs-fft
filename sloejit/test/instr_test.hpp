/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sloejit/aarch64/aarch64.hpp"
#include "sloejit/sloejit_assert.hpp"

#include <iostream>

namespace aarch64 = sloejit::aarch64;

// Helper to make the call to add_case a little neater
#define IB(f) &aarch64::instr_builder::f
// Similar to IB but helps with overloaded members
#define IBO(f, ...) static_cast<void (aarch64::instr_builder::*)(__VA_ARGS__)>(&aarch64::instr_builder::f)

class SloejitInstrTest {
public:
	SloejitInstrTest(const std::string &func)
	    : fn{ "aarch64_tests_" + func, { .validate = false }, aarch64::get_arch_traits() },
	      ib{ fn.make_block("b") } {
	}

	template <typename Callable, typename... ArgsT>
	void add_case(uint32_t expec_bin, const char *expec_asm, Callable &&make_instr, ArgsT &&...instr_args) {
		expected_bin.push_back(expec_bin);
		expected_asm.emplace_back(expec_asm);
		std::invoke(std::forward<Callable>(make_instr), ib, std::forward<ArgsT>(instr_args)...);
	}

	int validate() {
		return check_bin_output() | check_asm_output();
	}

private:
	int check_bin_output();
	int check_asm_output();

	sloejit::function fn;
	aarch64::instr_builder ib;
	std::vector<uint32_t> expected_bin;
	std::vector<std::string> expected_asm;
};

int SloejitInstrTest::check_asm_output() {
	const auto output_asm_s = fn.emit_asm();
	const std::string_view output_asm(output_asm_s);
	size_t line_begin = output_asm.find(fn.blocks[0]->name);
	size_t line_end = output_asm.find("\n", line_begin);
	line_begin = line_end + 1;
	line_end = output_asm.find("\n", line_begin);
	int status = 0;
	size_t line_idx = 0;
	while (line_end != output_asm.npos) {
		const auto line(output_asm.substr(line_begin, line_end - line_begin));
		const auto &line_ref = expected_asm[line_idx];

		if (output_asm[line_begin] != '\t') {
			std::cerr << "Assembly output instruction #" << std::dec << line_idx << ":\n"
			          << line << "\ndid not begin with \\t\n";
			status = 1;
		}

		if (line.substr(1, line.npos) != line_ref) {
			std::cerr << "Assembly output instruction #" << std::dec << line_idx << ":\n"
			          << line << "\ndid not match expected:\n\t" << line_ref << '\n';
			status = 1;
		}

		line_begin = line_end + 1;
		line_end = output_asm.find('\n', line_begin);
		line_idx++;
	}

	sloejit_assert(line_idx + 2 != expected_asm.size());

	return status;
}

int SloejitInstrTest::check_bin_output() {
	const auto output_bin_bytes = fn.emit_bin();
	sloejit_assert(output_bin_bytes.size() % 4 == 0);
	const std::vector<uint32_t> output_bin{ (const uint32_t *) &output_bin_bytes[0],
		                                    (const uint32_t *) (&output_bin_bytes[0] +
		                                                        output_bin_bytes.size()) };
	sloejit_assert(output_bin.size() == expected_bin.size());
	int status = 0;
	for (unsigned i = 0; i < output_bin.size(); i++) {
		if (output_bin[i] != expected_bin[i]) {
			std::cerr << "Binary output instruction #" << std::dec << i << ", 0x" << std::hex << output_bin[i]
			          << " did not match expected, 0x" << expected_bin[i] << '\n';
			status = 1;
		}
	}
	return status;
}
