/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "irprinter.hpp"

#include "emit.hpp"
#include "plfft_assert.hpp"
#include "print_value.hpp"
#include "registers.hpp"

#include <sstream>

using block = sloejit::block;
using function = sloejit::function;
using function_ptr = sloejit::function_ptr;
using reg = sloejit::reg;
using reloc_info = sloejit::reloc_info;
using stack_frame_info = sloejit::stack_frame_info;

namespace aarch64 = sloejit::aarch64;

namespace plfft::wfta {

ir_printer_ptr make_ir_printer(const options_t &opts) {
  if (opts.want_sme) {
    return ir_printer_ptr{new ir_printer_impl<true, true>()};
  }
  if (opts.target.has_sve) {
    return ir_printer_ptr{new ir_printer_impl<true, false>()};
  }
  return ir_printer_ptr{new ir_printer_impl<false, false>()};
}

static std::string id_to_str(const char *prefix, int v) {
  std::ostringstream sstm;
  sstm << prefix << v;
  return std::move(sstm).str();
}

template<bool IsSVE, bool IsSME>
void ir_printer_impl<IsSVE, IsSME>::operator()(
    std::map<std::string, function_ptr> &fns, stack_frame_info *frame_info,
    function &fn, std::vector<rodata_info> &data_ofs,
    std::vector<uint8_t> &data_bytes, regmap_t regmap, ir_value_scope &s) {
  bool is_init = fn.blocks.empty();
  if (is_init) {
    fn.make_block("init");
  }
  block *target_block = &*fn.blocks.back();
  block *end_block = target_block;
  aarch64::instr_builder init_ib{target_block};
  if (is_init) {
    if constexpr (IsSME) {
      if (fn.opts.sme != sloejit::function_options_t::sme_usage::none) {
        auto smopt =
            fn.opts.sme == sloejit::function_options_t::sme_usage::sm_both
                ? aarch64::smopt_both
                : aarch64::smopt_sm;
        init_ib.make_smstart_i(smopt);
      }
    }
    if constexpr (IsSVE) {
      init_ib.make_ptrue(p_true, aarch64::zv_b);
    }
    // Parameters are numbered from -1 downwards
    regmap[-1].reg = aarch64::x0;
    regmap[-2].reg = aarch64::x1;
    regmap[-3].reg = aarch64::x2;
    regmap[-4].reg = aarch64::x3;
    regmap[-5].reg = aarch64::x4;
    regmap[-6].reg = aarch64::x5;
    regmap[-7].reg = aarch64::x6;
    regmap[-8].reg = aarch64::x7;
  }
  switch (s.scope_op) {
  case IVSO_NORMAL:
    break;
  case IVSO_FOR: {
    ASSERT(s.scope_op_deps.size() >= 3);
    ASSERT(s.scope_op_deps.size() % 2 == 1);
    ir_value a = s.scope_op_deps[0], b = s.scope_op_deps[1];
    target_block = fn.make_block(id_to_str("for_body_", next_id()));
    end_block = fn.make_block(id_to_str("for_end_", next_id()));
    regmap[j_var].reg = x_(init_ib, regmap, a);
    regmap[j_lim_var].reg = x_(init_ib, regmap, b);
    init_ib.make_x_cmp_rr(regmap.at(j_var).reg, regmap.at(j_lim_var).reg);
    init_ib.make_b_ge_b(end_block);
    break;
  }
  case IVSO_IF_EQ: {
    ASSERT(s.scope_op_deps.size() == 2);
    ir_value a = s.scope_op_deps[0], b = s.scope_op_deps[1];
    target_block = fn.make_block(id_to_str("ifeq_body_", next_id()));
    end_block = fn.make_block(id_to_str("ifeq_end_", next_id()));
    emit_ctrl_cmp(init_ib, regmap, x_(init_ib, regmap, a), b);
    init_ib.make_b_ne_b(end_block);
    break;
  }
  case IVSO_IF_LT: {
    ASSERT(s.scope_op_deps.size() == 2);
    ir_value a = s.scope_op_deps[0], b = s.scope_op_deps[1];
    target_block = fn.make_block(id_to_str("iflt_body_", next_id()));
    end_block = fn.make_block(id_to_str("iflt_end_", next_id()));
    emit_ctrl_cmp(init_ib, regmap, x_(init_ib, regmap, a), b);
    init_ib.make_b_ge_b(end_block);
    break;
  }
  }

  auto last_prefix_instr = target_block->instr_last;

  // build instruction sequence for this block, keeping track of register
  // mapping.
  for (auto v : s.in_rev_order()) {
    // insert in reverse order, after the last prefix instruction
    // (i.e. before the one after it)
    auto instr_pos = last_prefix_instr ? last_prefix_instr->instr_next
                                       : target_block->instr_first;
    aarch64::instr_builder ib{target_block, instr_pos};
    print_value<IsSVE, IsSME>(fns, frame_info, ib, data_ofs, data_bytes, regmap,
                              v);
  }

  if (s.scope_op == IVSO_NORMAL) {
    // emit children in the same way (copy the regmap to avoid changes being
    // persisted)
    for (const auto &c : s.children) {
      (*this)(fns, frame_info, fn, data_ofs, data_bytes, regmap, *c);
    }
    end_block = &*fn.blocks.back();
  } else {
    ASSERT(s.children.empty());
  }

  aarch64::instr_builder ib{target_block};
  switch (s.scope_op) {
  case IVSO_NORMAL:
    break;
  case IVSO_FOR: {
    ASSERT(s.scope_op_deps.size() >= 3);
    ASSERT(s.scope_op_deps.size() % 2 == 1);
    for (unsigned i = 3; i < s.scope_op_deps.size(); i += 2) {
      ir_value b = s.scope_op_deps[i], c = s.scope_op_deps[i + 1];
      auto b_r = x_(ib, regmap, b);
      emit_iadd(ib, regmap, nullptr, b_r, b_r, c);
    }
    emit_iadd(ib, regmap, nullptr, regmap[j_var].reg, regmap[j_var].reg,
              s.scope_op_deps[2]);
    emit_ctrl_cmp(ib, regmap, regmap[j_var].reg, s.scope_op_deps[1]);
    ib.make_b_lt_b(target_block);
    break;
  }
  case IVSO_IF_EQ:
  case IVSO_IF_LT:
    ib.make_b_i(end_block);
    break;
  }
  if (is_init) {
    aarch64::instr_builder end_ib{end_block};
    if constexpr (IsSME) {
      if (fn.opts.sme != sloejit::function_options_t::sme_usage::none) {
        auto smopt =
            fn.opts.sme == sloejit::function_options_t::sme_usage::sm_both
                ? aarch64::smopt_both
                : aarch64::smopt_sm;
        end_ib.make_smstop_i(smopt);
      }
    }
    end_ib.make_ret();
  }
}

template<bool IsSVE, bool IsSME>
int ir_printer_impl<IsSVE, IsSME>::next_id() {
  return id++;
}

} // end namespace plfft::wfta
