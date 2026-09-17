/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "io_pointers.hpp"

#include <complex>
#include <list>
#include <vector>

namespace plfft::wfta {

struct atom;
struct expr;
class fresh_atom_factory;

using addfunc = void(std::list<expr> &algo, atom *y, const atom *x, int num,
                     fresh_atom_factory &faf);
using multfunc = std::vector<std::complex<double>>(void);

addfunc z2_in;
addfunc z2_out;
multfunc z2_mult;

addfunc z3_in;
addfunc z3_out;
multfunc z3_mult;

addfunc z4_in;
addfunc z4_out;
multfunc z4_mult;

addfunc z5_in;
addfunc z5_out;
multfunc z5_mult;

addfunc z7_in;
addfunc z7_out;
multfunc z7_mult;

addfunc z8_in;
addfunc z8_out;
multfunc z8_mult;

addfunc z9_in;
addfunc z9_out;
multfunc z9_mult;

addfunc z11_in;
addfunc z11_out;
multfunc z11_mult;

addfunc z13_in;
addfunc z13_out;
multfunc z13_mult;

addfunc z16_in;
addfunc z16_out;
multfunc z16_mult;

addfunc z17_in;
addfunc z17_out;
multfunc z17_mult;

addfunc z19_in;
addfunc z19_out;
multfunc z19_mult;

// Special case non-Winograd kernels which may not be combined to form larger
// kernels:
void split_radix_z16(std::list<expr> &algo, atom *y, const atom *x,
                     fresh_atom_factory &faf);
void radix5_z25(std::list<expr> &algo, atom *y, const atom *x,
                fresh_atom_factory &faf);
void split_radix_z32(std::list<expr> &algo, atom *y, const atom *x,
                     fresh_atom_factory &faf);

} // namespace plfft::wfta
