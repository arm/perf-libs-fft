/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "irvalue.hpp"

#include "irvalue_scope.hpp"
#include "plfft_assert.hpp"

#include <deque>
#include <sstream>

namespace plfft::wfta {

bool is_float_or_fixed_kind(ir_value_basic_kind kind) {
  return kind == IVK_FLOAT || kind == IVK_FIXED;
}

ir_value_type::ir_value_type(ir_value_basic_kind kind, int width,
                             ir_value_num_elems nelems,
                             ir_value_type_ptr inner_type)
  : kind(kind), elem_width(width), nelems(nelems),
    inner_type(std::move(inner_type)) {}

ir_value_type_ptr make_ir_value_type_integer(int width) {
  return std::make_shared<ir_value_type>(IVK_INTEGER, width,
                                         ir_value_num_elems::one(), nullptr);
}

ir_value_type_ptr make_ir_value_type_predicate(int elem_width) {
  // we pretend that predicates are 8 bytes each, but it doesn't really matter.
  return std::make_shared<ir_value_type>(
      IVK_PREDICATE, elem_width,
      ir_value_num_elems::with_nlanes(8,
                                      ir_value_num_elems::scale_t::scalable()),
      nullptr);
}

ir_value_type_ptr make_ir_value_type_real(int width, bool is_float) {
  ir_value_basic_kind kind = is_float ? IVK_FLOAT : IVK_FIXED;
  return std::make_shared<ir_value_type>(kind, width, ir_value_num_elems::one(),
                                         nullptr);
}

ir_value_type_ptr make_ir_value_type_complex(int width, bool is_float) {
  ir_value_basic_kind kind = is_float ? IVK_FLOAT : IVK_FIXED;
  return std::make_shared<ir_value_type>(kind, width, ir_value_num_elems::two(),
                                         nullptr);
}

ir_value_type_ptr make_ir_value_type_vector(ir_value_num_elems nelems,
                                            int nelems_elem_width,
                                            ir_value_type_ptr inner_type) {
  if (nelems.is_one()) {
    return inner_type;
  }
  ASSERT(inner_type->nelems.is_contig());
  ASSERT(!inner_type->nelems.is_sve());

  // Support non-contiguous outer masks (e.g. SVE predicates selecting
  // only real or imag lanes). In such cases we currently only expect to
  // vectorize a scalar inner type (novector), so we can pass the mask
  // through directly.
  if (!nelems.is_contig()) {
    ASSERT(inner_type->nelems.is_one() || inner_type->nelems.is_two());
    if (inner_type->nelems.is_one()) {
      auto nelems_new =
          ir_value_num_elems::with_mask(nelems.segment_mask, nelems.scale);
      return std::make_shared<ir_value_type>(inner_type->kind,
                                             inner_type->elem_width, nelems_new,
                                             inner_type->inner_type);
    }
    ASSERT(is_even_mask(nelems.segment_mask));
    auto mask = nelems.segment_mask | (nelems.segment_mask << 1);
    auto nelems_new = ir_value_num_elems::with_mask(mask, nelems.scale);
    return std::make_shared<ir_value_type>(inner_type->kind,
                                           inner_type->elem_width, nelems_new,
                                           inner_type->inner_type);
  }

  // Both inner and outer are contiguous: compute the combined contiguous mask.
  uint64_t new_mask;
  if (nelems.is_sve() &&
      inner_type->nelems.count_segment_contig() * inner_type->elem_width <
          nelems_elem_width) {
    ASSERT(nelems.count_segment_contig() == 2);
    // for sve loads we want to meld a mask based on the elem_width limiting
    // something?
    int new_width = inner_type->nelems.count_segment_contig();
    ASSERT(new_width > 0 && new_width <= 16);
    new_mask = (1 << new_width) - 1;
    new_mask |= new_mask << (new_width * 2);
  } else {
    int new_width = inner_type->nelems.count_segment_contig() *
                    nelems.count_segment_contig();
    ASSERT(new_width > 0 && new_width <= 16);
    new_mask = (1 << new_width) - 1;
  }

  auto nelems_new = ir_value_num_elems::with_mask(new_mask, nelems.scale);
  return std::make_shared<ir_value_type>(inner_type->kind,
                                         inner_type->elem_width, nelems_new,
                                         inner_type->inner_type);
}

ir_value_type_ptr make_ir_value_type_vector_for_load(
    const target_t &t, register_layout layout, ir_value_num_elems nelems,
    int nelems_elem_width, ir_value_type_ptr inner_type) {
  // loading [inner_type][nelems...].
  assert(!inner_type->nelems.is_sve());
  assert(inner_type->nelems.segment_mask == 0b1 ||
         inner_type->nelems.segment_mask == 0b11);
  assert(nelems.is_contig());

  if (!nelems.is_sve() || !t.has_sve) {
    return make_ir_value_type_vector(nelems, nelems_elem_width, inner_type);
  }

  int ret_mask = inner_type->nelems.segment_mask;
  int ret_mask_bits = inner_type->elem_width * __builtin_popcount(ret_mask);

  // 1) Adjust based on needing to widen to fill a complex value.
  int inner_width_present_bits =
      inner_type->elem_width *
      __builtin_popcount(inner_type->nelems.segment_mask);
  int inner_width_complex_bits = inner_type->elem_width * 2;

  if (inner_width_present_bits != inner_width_complex_bits) {
    // Pad with zeros.
    ret_mask_bits *= 2;
  }

  // 2) Adjust based on needing to widen to align with gather index element
  //    width.
  if (inner_width_complex_bits < nelems_elem_width) {
    ret_mask_bits *= 2;
  }

  // 3) Replicate mask by applying nelems.
  for (int i = 1; i != nelems.count_contig(); i *= 2) {
    ret_mask |= ret_mask << (ret_mask_bits / inner_type->elem_width);
    ret_mask_bits *= 2;
  }

  return std::make_shared<ir_value_type>(
      inner_type->kind, inner_type->elem_width,
      ir_value_num_elems::with_mask(ret_mask, nelems.scale), nullptr);
}

ir_value_type_ptr make_ir_value_type_novector(ir_value_type_ptr inner_type) {
  return std::make_shared<ir_value_type>(
      inner_type->kind, inner_type->elem_width, ir_value_num_elems::one(),
      inner_type->inner_type);
}

ir_value_type_ptr make_ir_value_type_pointer(ir_value_type_ptr inner_type) {
  return std::make_shared<ir_value_type>(
      IVK_POINTER, 64, ir_value_num_elems::one(), std::move(inner_type));
}

ir_value_type_ptr select_widest_ir_value_type_integer(ir_value_type_ptr s,
                                                      ir_value_type_ptr t) {
  assert(s->kind == IVK_INTEGER);
  assert(t->kind == IVK_INTEGER);
  return s->elem_width < t->elem_width ? t : s;
}

bool is_neg_op(ir_value_op v) {
  return v == IVO_FNEG || v == IVO_SQNEG;
}

bool is_add_op(ir_value_op v) {
  return v == IVO_FADD || v == IVO_SQADD;
}

bool is_sub_op(ir_value_op v) {
  return v == IVO_FSUB || v == IVO_SQSUB;
}

bool is_mul_op(ir_value_op v) {
  return v == IVO_FMUL || v == IVO_SQMUL;
}

bool is_cmul_op(ir_value_op v) {
  return v == IVO_FCMUL || v == IVO_SQCMUL;
}

bool is_conj_op(ir_value_op v) {
  return v == IVO_FCONJ || v == IVO_SQCONJ;
}

bool is_generic_load(ir_value_op v) {
  return v == IVO_GATHER || v == IVO_LOAD || v == IVO_LOAD_BCAST ||
         v == IVO_STRUCTURE_LOAD_GROUP;
}

bool ir_value_type::operator==(const ir_value_type &other) const {
  return kind == other.kind && elem_width == other.elem_width &&
         nelems == other.nelems &&
         (inner_type ? other.inner_type && *inner_type == *other.inner_type
                     : !other.inner_type);
}

bool ir_value_type::operator!=(const ir_value_type &other) const {
  return !(*this == other);
}

ir_value_impl::ir_value_impl(ir_value_scope *scope, ir_value_op op,
                             ir_value_type_ptr type, std::vector<ir_value> deps,
                             std::vector<double> literals, std::string str,
                             int id)
  : scope(scope), op(op), type(std::move(type)), deps(std::move(deps)),
    literals(std::move(literals)), str(std::move(str)), id(std::move(id)) {
  if (op == IVO_STORE || op == IVO_SCATTER || op == IVO_STRUCTURE_STORE_GROUP) {
    add_use(ir_value_use_info::make_mem());
  }
}

static std::string op_to_str(ir_value_op op) {
  switch (op) {
  case IVO_PARAM:
    return "IVO_PARAM";
  case IVO_CONST_INT:
    return "IVO_CONST_INT";
  case IVO_CONST_FLOAT:
    return "IVO_CONST_FLOAT";
  case IVO_CONST_FIXED:
    return "IVO_CONST_FIXED";
  case IVO_SHUFFLE:
    return "IVO_SHUFFLE";
  case IVO_CONCAT:
    return "IVO_CONCAT";
  case IVO_FADD:
    return "IVO_FADD";
  case IVO_FSUB:
    return "IVO_FSUB";
  case IVO_FMUL:
    return "IVO_FMUL";
  case IVO_IADD:
    return "IVO_IADD";
  case IVO_ISUB:
    return "IVO_ISUB";
  case IVO_IMUL:
    return "IVO_IMUL";
  case IVO_IDIV:
    return "IVO_IDIV";
  case IVO_IMOD:
    return "IVO_IMOD";
  case IVO_FNEG:
    return "IVO_FNEG";
  case IVO_GEP:
    return "IVO_GEP";
  case IVO_CAST:
    return "IVO_CAST";
  case IVO_GATHER:
    return "IVO_GATHER";
  case IVO_LOAD:
    return "IVO_LOAD";
  case IVO_STRUCTURE_LOAD_GROUP:
    return "IVO_STRUCTURE_LOAD_GROUP";
  case IVO_GET_GROUP_OP:
    return "IVO_GET_GROUP_OP";
  case IVO_STRUCTURE_STORE_GROUP:
    return "IVO_STRUCTURE_STORE_GROUP";
  case IVO_LOAD_BCAST:
    return "IVO_LOAD_BCAST";
  case IVO_SCATTER:
    return "IVO_SCATTER";
  case IVO_STORE:
    return "IVO_STORE";
  case IVO_FCMUL:
    return "IVO_FCMUL";
  case IVO_FCONJ:
    return "IVO_FCONJ";
  case IVO_INDEX:
    return "IVO_INDEX";
  case IVO_EQ_SEL:
    return "IVO_EQ_SEL";
  case IVO_MIN:
    return "IVO_MIN";
  case IVO_SVE_TRN1:
    return "IVO_SVE_TRN1";
  case IVO_SVE_TRN2:
    return "IVO_SVE_TRN2";
  case IVO_UZP1:
    return "IVO_UZP1";
  case IVO_UZP2:
    return "IVO_UZP2";
  case IVO_ZIP1:
    return "IVO_ZIP1";
  case IVO_ZIP2:
    return "IVO_ZIP2";
  case IVO_SVE_CNTH:
    return "IVO_SVE_CNTH";
  case IVO_SVE_CNTW:
    return "IVO_SVE_CNTW";
  case IVO_SVE_CNTD:
    return "IVO_SVE_CNTD";
  case IVO_REINTERPRET:
    return "IVO_REINTERPRET";
  case IVO_SQNEG:
    return "IVO_SQNEG";
  case IVO_SQADD:
    return "IVO_SQADD";
  case IVO_SQSUB:
    return "IVO_SQSUB";
  case IVO_SQMUL:
    return "IVO_SQMUL";
  case IVO_SQCMUL:
    return "IVO_SQCMUL";
  case IVO_SQCONJ:
    return "IVO_SQCONJ";
  case IVO_SRSHR:
    return "IVO_SRSHR";
  case IVO_PTRUE:
    return "IVO_PTRUE";
  case IVO_WHILELT:
    return "IVO_WHILELT";
  case IVO_ZA_ZERO:
    return "IVO_ZA_ZERO";
  case IVO_ZA_SLICE_WRITE:
    return "IVO_ZA_SLICE_WRITE";
  case IVO_ZA_SLICE_READ:
    return "IVO_ZA_SLICE_READ";
  }
  printf("unknown op? %d\n", (int)op);
  ASSERT(false);
}

std::string type_to_str(const ir_value_type &t) {
  std::ostringstream sstm;
  sstm << "<";
  if (t.nelems.is_sve()) {
    sstm << "n x ";
  }
  if (!t.nelems.is_one()) {
    sstm << "[";
    for (int i = 0; (t.nelems.segment_mask >> i) != 0; ++i) {
      char ch = ((t.nelems.segment_mask >> i) & 1) != 0 ? '1' : '0';
      sstm << ch;
    }
    sstm << "] x ";
  }
  switch (t.kind) {
  case IVK_INTEGER:
    sstm << "i" << t.elem_width;
    break;
  case IVK_PREDICATE:
    sstm << "p" << t.elem_width;
    break;
  case IVK_FLOAT:
    sstm << "f" << t.elem_width;
    break;
  case IVK_FIXED:
    sstm << "s" << t.elem_width;
    break;
  case IVK_POINTER:
    sstm << type_to_str(*t.inner_type) << "*";
    break;
  case IVK_ZA_TILE:
    sstm << "za";
    break;
  }
  sstm << ">";
  return std::move(sstm).str();
}

std::string dump_one(ir_value v) {
  std::ostringstream sstm;
  if (v->op != IVO_SCATTER && v->op != IVO_STRUCTURE_STORE_GROUP) {
    sstm << "v" << v->id << ": " << type_to_str(*v->type) << " = ";
  }
  if (v->op == IVO_PARAM) {
    sstm << v->str;
  } else {
    sstm << op_to_str(v->op);
    for (auto d : v->deps) {
      sstm << ", (v" << d->id << ": " << type_to_str(*d->type) << ")";
    }
    for (auto l : v->literals) {
      sstm << ", (" << l << " // " << (int)l << ")";
    }
  }
  return std::move(sstm).str();
}

std::string ir_value_impl::dump(int max_depth) {
  std::ostringstream sstm;
  std::set<ir_value> seen;
  std::deque<std::pair<ir_value, int>> todo;
  todo.emplace_back(this, 0);
  while (!todo.empty()) {
    auto [elem, depth] = todo.front();
    todo.pop_front();
    for (int i = 0; i < depth; ++i) {
      sstm << ' ';
    }
    sstm << dump_one(elem) << std::endl;
    seen.insert(elem);
    if (depth / 4 < max_depth) {
      for (auto i = elem->deps.size(); i--;) {
        auto d = elem->deps[i];
        if (seen.find(d) == seen.end()) {
          todo.emplace_front(d, depth + 4);
        }
      }
    }
  }
  return std::move(sstm).str();
}

void ir_value_impl::add_use(ir_value_use_info new_info) {
  auto it = std::find_if(uses.begin(), uses.end(), [new_info](const auto &i) {
    if (new_info.kind != i.kind) {
      return false;
    }
    switch (new_info.kind) {
    case IVU_DEP:
      return i.v && new_info.v->id == i.v->id;
    case IVU_SCOPE:
      return new_info.scope == i.scope;
    case IVU_KEEP:
      return true;
    case IVU_MEM:
      return true;
    }
    ASSERT(false);
  });
  if (it != uses.end()) {
    // already present, nothing to do
    return;
  }
  uses.push_back(std::move(new_info));
}

bool ir_value_impl::has_use(ir_value v) const {
  auto it = std::find_if(uses.begin(), uses.end(), [=](const auto &i) {
    return i.kind == IVU_DEP && v->id == i.v->id;
  });
  return it != uses.end();
}

bool ir_value_impl::has_scope_use(const ir_value_scope *other_scope) const {
  auto it = std::find_if(uses.begin(), uses.end(), [=](const auto &i) {
    return i.kind == IVU_SCOPE && other_scope == i.scope;
  });
  return it != uses.end();
}

bool ir_value_impl::has_mem_use() const {
  auto it = std::find_if(uses.begin(), uses.end(),
                         [](const auto &i) { return i.kind == IVU_MEM; });
  return it != uses.end();
}

bool ir_value_impl::has_keep_use() const {
  auto it = std::find_if(uses.begin(), uses.end(),
                         [](const auto &i) { return i.kind == IVU_KEEP; });
  return it != uses.end();
}

void ir_value_impl::erase_use(ir_value v, bool allow_not_present) {
  auto it = std::find_if(uses.begin(), uses.end(), [needle = v](const auto &i) {
    return i.v && needle->id == i.v->id;
  });
  if (it == uses.end()) {
    if (!allow_not_present) {
#if assert_ON
      fprintf(stdout, "trying to erase use v, but v is not used?\n");
      fprintf(stdout, "this:\n%s\nv:\n%s\n", this->dump(999).c_str(),
              v->dump(999).c_str());
#endif
      ASSERT(false);
    }
    return;
  }
  uses.erase(it);
  ASSERT(!has_use(v));
}

std::string ir_value_impl::dump_uses() {
  std::ostringstream sstm;
  sstm << "Dependencies of " << dump_one(this) << std::endl;
  for (const auto &use_info : uses) {
    switch (use_info.kind) {
    case IVU_DEP:
      sstm << "    " << dump_one(use_info.v) << std::endl;
      break;
    case IVU_SCOPE:
      sstm << "    (value is used by scope control flow)" << std::endl;
      break;
    case IVU_MEM:
      sstm << "    (value stores to memory)" << std::endl;
      break;
    case IVU_KEEP:
      sstm << "    (unknown / used by magic)" << std::endl;
      break;
    }
  }
  return std::move(sstm).str();
}

} // end namespace plfft::wfta
