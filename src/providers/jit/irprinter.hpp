/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "irvalue.hpp"
#include "irvalue_scope.hpp"
#include "polyval.hpp"
#include "print_algo.hpp"
#include "sloejit/aarch64/aarch64.hpp"
#include "sloejit/ir.hpp"
#include "target.hpp"

#include <memory>

namespace plfft::wfta {

/**
 * Keep track of the contents of the rodata sections so that we can avoid
 * storing duplicate data.
 */
struct rodata_info {
  polyval val;               ///< What value are we storing.
  unsigned ofs;              ///< The offset into .rodata.
  sloejit::reg r;            ///< What register has it been loaded into.
  ir_value_num_elems nelems; ///< How wide is the data (this is just
                             ///< r->type->nelems).

  sloejit::instruction *origin =
      nullptr; ///< What instruction loaded this data into registers.
  unsigned origin_len =
      0; ///< How many instructions were needed to load this data into registers
         ///< beyond setting up the rodata addr.
};

class ir_printer;
using ir_printer_ptr = std::unique_ptr<ir_printer>;

struct regmap_elem {
  sloejit::reg reg;
  sloejit::instruction *origin;

  regmap_elem() = default;

  regmap_elem(sloejit::reg reg) : reg(reg), origin(nullptr) {}
};

typedef std::map<int, regmap_elem> regmap_t;

/// The base class for printing IR Values and Scopes.
class ir_printer {
public:
  virtual ~ir_printer() = default;

  /** Print a particular scope of the passed function (and all child scopes),
   * updating the stack frame info and rodata as we go.
   *
   * @param[in,out] fns        The set of functions current registered. This may
   * be added to if the function being printed itself references currently
   * undeclared functions.
   * @param[in,out] frame_info The expected stack frame layout for this
   * function. This is needed to ensure we can correctly fixup parameter loads
   * from the stack.
   * @param[in] fn             The function to be printed.
   * @param[in,out] data_ofs   The layout of rodata (to avoid emitting duplicate
   * loads)
   * @param[in,out] data_bytes The contents of rodata (to be copied into the
   * final binary or memory).
   * @param[in,out] regmap     A mapping of ir_value id's to virtual/physical
   * registers.
   * @param[in] s              The scope (of the specified function) to be
   * printed.
   */
  virtual void operator()(std::map<std::string, sloejit::function_ptr> &fns,
                          sloejit::stack_frame_info *frame_info,
                          sloejit::function &fn,
                          std::vector<rodata_info> &data_ofs,
                          std::vector<uint8_t> &data_bytes, regmap_t regmap,
                          ir_value_scope &s) = 0;
};

/// A templated implementation for printing SVE/NEON code
template<bool IsSVE, bool IsSME>
class ir_printer_impl : public ir_printer {
  int id = 0;

public:
  /// Print a particular scope of the passed function, updating the stack frame
  /// info and rodata as we go (see ir_printer::operator()).
  void operator()(std::map<std::string, sloejit::function_ptr> &fns,
                  sloejit::stack_frame_info *frame_info, sloejit::function &fn,
                  std::vector<rodata_info> &data_ofs,
                  std::vector<uint8_t> &data_bytes, regmap_t regmap,
                  ir_value_scope &s) override;

  int next_id();
};

/// Create an appropriate IR printer for the specified target.
ir_printer_ptr make_ir_printer(const options_t &t);

} // namespace plfft::wfta
