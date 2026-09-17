/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

// enums used in kernel generation / lookup.
// see docs/kernel_naming.md
namespace plfft {

/// Represents the supported iteration orders for kernels.
enum order_kind {
  ORDER_NA = 0, ///< Iteration order not applicable for non-twiddle kernels.
  ORDER_AB,     ///< Perform an FFT over the primary dimension,
                ///< howmany is over the second dimension.
  ORDER_AC      ///< Perform an FFT over the primary dimension,
                ///< howmany is over the third dimension.
};

/// The options for kernels with unit / non-unit [i|o]dist/stride data layout
enum dist_types {
  gs = 0, ///< gather / scatter
  uu,     ///< unit idist/odist
  us,     ///< unit idist/scatter
  gu,     ///< gather / unit odist
  tu,     ///< unit istride/odist
  tt,     ///< unit istride/ostride
  uun,    ///< unit idist/odist, howmany == 1
  ut,     ///< unit idist/ostride
};

// * halfhi indicates that writes are to the first n/2 + 1 elements only.
// * halflo indicates that writes are to the first (n + 1)/2 elements only.
// * conj_reverse indicates that writes are to the first (n + 1)/2 elements as
//   normal in Y followed by writes of the following n/2 elements in reverse
//   order and conjugated to YY.
// * om_real indicates that the output is real only (and therefore also
//   indicates Hermitian input - i.e. conjugate reverse top half of input)
enum out_mods { om_none = 0, halfhi, halflo, conj_reverse, om_real };

// * both values halfhi, halflo here indicate that the second half of the
//   input come from XX and are conjugated, reversed.
// * im_halfhi indicates that the second half starts at n/2 + 1
// * im_halflo indicates that the second half starts at n/2
// * im_real indicates that the input is real (r2c transform)
enum in_mods { im_none = 0, im_halfhi, im_halflo, im_real };

struct io_mods_t {
  out_mods out; ///< Output modifier to use when emitting stores to Y.
  in_mods in;   ///< Input modifier to use when emitting loads from X.
};

} // namespace plfft
