/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "print_value.hpp"
#include "plfft_assert.hpp"

#include "emit.hpp"

namespace plfft::wfta {

template<bool IsSVE, bool IsSME>
void print_value(std::map<std::string, sloejit::function_ptr> &fns,
                 sloejit::stack_frame_info *frame_info,
                 sloejit::aarch64::instr_builder &ib,
                 std::vector<rodata_info> &data_ofs,
                 std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                 ir_value v) {
  if (v->emitted == v->uses.size()) {
    return;
  }
  ++v->emitted;
  if (is_constant_value_true(v) || v->op == IVO_CONST_INT) {
    return;
  }
  if (v->op == IVO_CONST_FLOAT || is_constant_value_here(v)) {
    auto pred = IsSVE ? &p_true : nullptr;
    get_literal_pool_value<IsSVE>(ib, regmap, data_ofs, data_bytes, v, pred);
    return;
  }
  if (v->op == IVO_SHUFFLE) {
    emit_shuffle<IsSVE>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_REINTERPRET) {
    // There is nothing to emit for reinterpret - it is handled when accessing
    // the regmap.
    return;
  } else if (v->op == IVO_CONCAT) {
    emit_concat<IsSVE>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_IADD) {
    emit_iadd(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_ISUB) {
    emit_isub(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_IMUL) {
    emit_imul(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_IDIV) {
    emit_idiv(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_IMOD) {
    emit_imod(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_FADD || v->op == IVO_FSUB) {
    emit_fadd_fsub<IsSVE>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_SQADD || v->op == IVO_SQSUB) {
    emit_sqadd_sqsub<IsSVE>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_FMUL) {
    emit_fmul<IsSVE>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_SQMUL) {
    emit_sqmul<IsSVE>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_FNEG) {
    emit_fneg<IsSVE>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_SQNEG) {
    emit_sqneg<IsSVE>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_FCMUL) {
    emit_fcmla<IsSVE>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_SQCMUL) {
    emit_sqcmla<IsSVE>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_FCONJ) {
    emit_fconj<IsSVE>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_SQCONJ) {
    emit_sqconj<IsSVE>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_PTRUE) {
    // Nothing to emit for ptrue - it is a register alias
    return;
  } else if (v->op == IVO_WHILELT) {
    emit_whilelt(ib, regmap, v);
    return;
  } else if (v->op == IVO_INDEX) {
    emit_index<IsSVE>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_EQ_SEL) {
    emit_eq_sel(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_MIN) {
    emit_min(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_CAST) {
    emit_cast<IsSVE>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_GEP) {
    if constexpr (!IsSVE) {
      if (want_delayed_gep(v)) {
        return;
      }
    }
    emit_gep<IsSVE>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_GATHER) {
    emit_gather<IsSVE, IsSME>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_STRUCTURE_LOAD_GROUP) {
    emit_structure_load_group(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_STRUCTURE_STORE_GROUP) {
    emit_structure_store_group(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_GET_GROUP_OP) {
    // There is nothing to emit for get_group_op - it is handled by the reg map
    return;
  } else if (v->op == IVO_LOAD) {
    emit_load<IsSVE>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_SCATTER) {
    emit_scatter<IsSVE, IsSME>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_LOAD_BCAST) {
    emit_load_bcast<IsSVE>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_STORE) {
    emit_store<IsSVE>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_PARAM) {
    emit_param(frame_info, ib, regmap, v);
    return;
  } else if (v->op == IVO_ZIP1) {
    // TODO: this should eventually be erased once we merge concat/shuffle.
    emit_zip1<IsSVE>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_ZIP2) {
    emit_zip2<IsSVE>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_SVE_TRN1) {
    // TODO: this should eventually be erased once we merge concat/shuffle.
    emit_trn1<true>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_SVE_TRN2) {
    // TODO: this should eventually be erased once we merge concat/shuffle.
    emit_trn2<true>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_UZP1) {
    emit_uzp1<IsSVE>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_UZP2) {
    emit_uzp2<IsSVE>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_SVE_CNTH) {
    emit_cnth(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_SVE_CNTW) {
    emit_cntw(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_SVE_CNTD) {
    emit_cntd(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_SRSHR) {
    emit_srshr<IsSVE>(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_ZA_ZERO) {
    ASSERT(IsSME && "ZA token op requires SME printer");
    return;
  } else if (v->op == IVO_ZA_SLICE_WRITE) {
    ASSERT(IsSME && "ZA slice write op requires SME printer");
    emit_za_slice_write(ib, data_ofs, data_bytes, regmap, v);
    return;
  } else if (v->op == IVO_ZA_SLICE_READ) {
    ASSERT(IsSME && "ZA slice read op requires SME printer");
    emit_za_slice_read(ib, data_ofs, data_bytes, regmap, v);
    return;
  }
  ASSERT(false && "unhandled op kind in irprinter!");
}

template void print_value<false, false>(
    std::map<std::string, sloejit::function_ptr> &fns,
    sloejit::stack_frame_info *frame_info, sloejit::aarch64::instr_builder &ib,
    std::vector<rodata_info> &data_ofs, std::vector<uint8_t> &data_bytes,
    regmap_t &regmap, ir_value v);
template void print_value<true, false>(
    std::map<std::string, sloejit::function_ptr> &fns,
    sloejit::stack_frame_info *frame_info, sloejit::aarch64::instr_builder &ib,
    std::vector<rodata_info> &data_ofs, std::vector<uint8_t> &data_bytes,
    regmap_t &regmap, ir_value v);
template void print_value<true, true>(
    std::map<std::string, sloejit::function_ptr> &fns,
    sloejit::stack_frame_info *frame_info, sloejit::aarch64::instr_builder &ib,
    std::vector<rodata_info> &data_ofs, std::vector<uint8_t> &data_bytes,
    regmap_t &regmap, ir_value v);

} // namespace plfft::wfta
