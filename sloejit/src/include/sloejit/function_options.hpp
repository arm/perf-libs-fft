/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

namespace sloejit {

/**
 * Options affecting code modifications done prior to emission.
 */
struct function_options_t {
	/// Whether to force insertion of frame-pointer maintenance logic even when
	/// no spills/stack args require a frame. Needed for perf frame-pointer
	/// call-graph unwinding (perf --call-graph fp), which depends on functions
	/// preserving a standard frame-pointer chain.
	const bool keep_frame_pointer = true;

	/// Whether to validate code before emitting (e.g. to confirm that basic
	/// blocks end with some kind of control flow instruction).
	const bool validate = true;

	enum class sme_usage {
		none, ///< Not running in SME streaming mode.
		sm, ///< Streaming mode enabled, ZA not used.
		sm_both, ///< Streaming mode enabled with ZA usage.
	};

	/// Whether the main loop runs in SME streaming mode, and if it uses ZA
	/// (affects how spills are emitted and SMSTART/SMSTOP options).
	const sme_usage sme = sme_usage::none;
};

} // namespace sloejit
