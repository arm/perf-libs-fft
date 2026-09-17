/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft_pod_vector.hpp"

namespace plfft {

using buffer_type = pod_vector<std::uint8_t>;

/// Encodes the different places in which we need buffers in our FFTs
enum class buffer_name {
  compositor,
  bluestein,
  rader,
  r2r_in,
  r2r_out,
  batched,
  sme2_direct,
};

/// Manage active compositor buffer, for nested utilization
void release_compositor_buf();

/**
 * Given the id of where we need the buffer and its required size, return a
 * pointer.
 *
 * Implementation note:
 * For the compositor's buffer there is a chance that we have nested plans -
 * e.g. in the case that a Bluestein's plan (which contains other plans) is
 * composed with a Cooley-Tukey plan. If the C-T plan needs to use the
 * compositor buffer then active data in the buffer will be trashed when one of
 * the BS nested plans executes. To avoid this situation, get_memory manages two
 * compositor buffers. Access to the current active compositor buffer is managed
 * by calling the release_compositor_buf function.
 *
 * @param [in] id An identifier for which buffer to return.
 * @return A pointer to THREAD_LOCAL memory for the required buffer.
 */
template<typename T>
T *get_memory(buffer_name id, std::size_t i);

/**
 * Free the identified buffer.
 */
void free_buffer(buffer_name id);

} // end namespace plfft
