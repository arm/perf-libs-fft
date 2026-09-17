/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

namespace sloejit::aarch64 {
<% opcodes = load_module('aarch64_opcodes.py').opcodes %>\

enum class opcode {
%for opcode in opcodes:
	${opcode},
%endfor
};

inline std::string opcode_to_string(int opcode) {
	switch (opcode) {
%for i, opcode in enumerate(opcodes):
	case ${i}: return "${opcode}";
%endfor
	}
	sloejit_assert(false && "unknown opcode");
	return "error";
}

} // namespace sloejit::aarch64
