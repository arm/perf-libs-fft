/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "print_algo.hpp"
#include "algo.hpp"
#include "irprinter.hpp"
#include "irprinter_utils.hpp"
#include "irvalue_scope.hpp"
#include "irvalue_scope_iterator.hpp"
#include "plfft_assert.hpp"
#include "plfft_complex.hpp"
#include "plfft_util.hpp"
#include "rtype.hpp"
#include "sloejit/ir.hpp"
#include "type_check.hpp"
#include "vector_size.hpp"

#include <algorithm>
#include <map>
#include <numeric>
#include <optional>

using function = sloejit::function;
using function_ptr = sloejit::function_ptr;
using note_info = sloejit::note_info;
using reloc_info = sloejit::reloc_info;
using stack_frame_info = sloejit::stack_frame_info;
namespace aarch64 = sloejit::aarch64;

namespace plfft::wfta {

static kernel_data emit_kernel_data(kernel_registry_entry<void> *out,
                                    const std::string &fn_name, function *fn,
                                    stack_frame_info &frame_info,
                                    std::vector<uint8_t> data_bytes,
                                    algo_flops flops, const options_t &opts) {
  ASSERT(!fn->blocks.empty());
  ASSERT(!fn->blocks[0]->instrs.empty());

  std::vector<reloc_info> relocs;
  std::vector<note_info> notes;
  std::vector<uint8_t> text_bytes =
      fn->emit_bin(&relocs, &notes, &frame_info).get();
  std::optional<std::string> src;
  if (opts.want_kernel_src) {
    src = fn->emit_asm(&notes, &frame_info);
  }

  return {out,
          fn_name,
          std::move(text_bytes),
          std::move(data_bytes),
          std::move(relocs),
          std::move(notes),
          std::move(src),
          std::move(flops)};
}

/** Apply any modifications to output that are required in this case
 *  e.g. Hermitian redundancy
 */
static void apply_output_modifiers(std::list<expr_t> &algo, int64_t n,
                                   const io_ptr_t &iop,
                                   const std::vector<int64_t> &out_perm,
                                   plfft_direction_t dir,
                                   enum out_mods out_mod) {

  if (out_mod == out_mods::halfhi || out_mod == out_mods::halflo) {
    auto getY = [&](const atom &var) {
      auto ofs = get_out_ofs(iop, var);
      if (dir == PLFFT_FORWARD) {
        return out_perm[ofs];
      } else {
        return (n - out_perm[ofs]) % n;
      }
    };

    auto nlim = out_mod == out_mods::halfhi   ? n / 2 + 1
                : out_mod == out_mods::halflo ? (n + 1) / 2
                                              : n;
    for (auto it = algo.begin(); it != algo.end();) {
      if (is_out_ptr(iop, it->lhs) && getY(it->lhs) >= nlim) {
        it = algo.erase(it);
      } else {
        ++it;
      }
    }
  }
}

static std::list<expr_t>
apply_algo_transforms(std::list<expr_t> algo, int64_t n, const io_ptr_t &iop,
                      const std::vector<int64_t> &in_perm,
                      const std::vector<int64_t> &out_perm,
                      twiddleness twiddle) {

  fresh_atom_factory fresh{iop.local_end};

  prettify_algo(algo, iop, fresh); // Ensures we get sensible variable names

  if (twiddle == twiddleness::dif) {
    algo = twiddle_algo(algo, n, iop, out_perm, fresh, false);
  }
  if (twiddle == twiddleness::dit) {
    algo = twiddle_algo(algo, n, iop, in_perm, fresh, true);
  }

  prettify_algo(algo, iop, fresh); // Ensures we get sensible variable names
  isolate_reads(algo, iop, fresh);
  isolate_writes(algo, iop, fresh);
  return algo;
}

/* Main printing functions - create file and fill it with algorithm */

static void value_replace_uses(ir_value from, ir_value to) {
  for (const auto &use_info : from->uses) {
    switch (use_info.kind) {
    case IVU_DEP: {
      auto *u = use_info.v;
      ASSERT(u);
      for (unsigned i = 0; i < u->deps.size(); ++i) {
        if (u->deps[i] == from) {
          u->deps[i] = to;
        }
      }
      break;
    }
    case IVU_SCOPE: {
      auto *scope = use_info.scope;
      ASSERT(scope);
      for (unsigned i = 0; i < scope->scope_op_deps.size(); ++i) {
        if (scope->scope_op_deps[i] == from) {
          scope->scope_op_deps[i] = to;
        }
      }
      break;
    }
    case IVU_MEM:
      break;
    case IVU_KEEP:
      fprintf(stderr, "tried to replace a with b:\na:\n%s\nb:\n%s\n",
              from->dump_uses().c_str(), to->dump_uses().c_str());
      ASSERT(false && "refusing to replace uses of value with IVU_KEEP");
    }
  }
  for (const auto &use_info : from->uses) {
    to->add_use(use_info);
  }
  from->uses = {};
}

static void value_erase(ir_value_scope *scope, ir_value x) {
  for (const auto &use_info : x->uses) {
    if (use_info.kind == IVU_MEM) {
      continue;
    }
    printf("error erasing x, value is still used?\nx:\n%s\n",
           x->dump(999).c_str());
    ASSERT(false);
  }
  // check uses are valid before erasing anything, since if the same dep occurs
  // multiple times then this check would fail after the first was erased!
  for (auto dep : x->deps) {
    ASSERT(dep->has_use(x));
  }
  for (auto dep : x->deps) {
    dep->erase_use(x, /*allow_not_present=*/true);
  }
  scope->erase_ir_value(x->id);
}

static bool is_uniform_real_constant_value(ir_value x, double value) {
  if (x->op == IVO_CONST_FLOAT) {
    return x->literals[0] == value;
  }
  if (x->op == IVO_CONST_FIXED) {
    // fixed-point literals are stored as a double internally
    return x->literals[0] == value;
  }
  return false;
}

static bool is_uniform_constant_value(ir_value x, double value) {
  if (x->op == IVO_CONST_INT) {
    return (int)x->literals[0] == (int)value;
  }
  if (x->op == IVO_CONCAT) {
    for (auto d : x->deps) {
      if (!is_uniform_constant_value(d, value)) {
        return false;
      }
    }
    return true;
  }
  return is_uniform_real_constant_value(x, value);
}

static bool is_complex_one(ir_value x) {
  return x->op == IVO_CONCAT && x->deps.size() == 2 &&
         is_uniform_constant_value(x->deps[0], 1) &&
         is_uniform_constant_value(x->deps[1], 0);
}

static bool is_uniform_value(ir_value x) {
  if (x->op == IVO_CONCAT) {
    ASSERT(!x->deps.empty());
    for (auto d : x->deps) {
      if (d != x->deps[0]) {
        return false;
      }
    }
    return true;
  }
  return x->type->nelems.is_one();
}

static bool is_conj(ir_value x) {
  if (is_conj_op(x->op)) {
    return true;
  }
  if (is_mul_op(x->op)) {
    auto b = x->deps[1];
    if (b->op == IVO_CONCAT && b->deps.size() == 2 &&
        is_uniform_constant_value(b->deps[0], 1) &&
        is_uniform_constant_value(b->deps[1], -1)) {
      return true;
    }
  }
  return false;
}

static bool do_opt(ir_builder &builder) {
  // TODO: there is potentially an optimization missing where
  //       we combine FMUL/FCMUL operations with an accumulator,
  //       although in practice the compiler does a pretty good
  //       job of this for NEON at least.
  ir_value_scope *scope = builder.scope;
  bool changed_ever = false;
  bool changed = true;
  while (changed) {
    changed = false;
    std::set<ir_value> to_erase;
    for (auto d : scope->any_order()) {
      // dead code elimination
      if (d->uses.empty()) {
        to_erase.insert(d);
        changed = changed_ever = true;
        continue;
      }
      // try to fix this pattern:
      // a = { _1, _2 }; b = { _3, _4 }; c = mul(a, b); d = rev(c);
      // this can be expressed instead as:
      // a = { _2, _1 }; b = { _4, _3 }; d = mul(a, b);
      if (d->op == IVO_SHUFFLE &&
          d->literals == std::vector<double>{1.0, 0.0}) {
        ASSERT(d->deps.size() == 1);
        auto c = d->deps[0];
        if (is_mul_op(c->op) && c->uses.size() == 1) {
          ASSERT(c->uses[0].v);
          ASSERT(c->uses[0].v->id == d->id);
          ASSERT(c->deps.size() == 2);
          auto a = c->deps[0], b = c->deps[1];
          if (a->op == IVO_CONCAT && b->op == IVO_CONCAT &&
              a->uses.size() == 1 && b->uses.size() == 1 &&
              a->deps.size() == 2 && b->deps.size() == 2) {
            ASSERT(a->uses[0].v);
            ASSERT(a->uses[0].v->id == c->id);
            ASSERT(b->uses[0].v);
            ASSERT(b->uses[0].v->id == c->id);
            std::swap(a->deps[0], a->deps[1]);
            std::swap(b->deps[0], b->deps[1]);
            value_replace_uses(d, c);
            to_erase.insert(d);
            changed = changed_ever = true;
            continue;
          }
        }
      }
      // try to fix this pattern:
      // a = { _1, 0 }; b = { _3, _4 }; d = op(a, b);
      // this can be expressed instead as:
      // a = op(_1, _3); d = { a, 0 };
      if ((is_mul_op(d->op) || is_add_op(d->op) || is_sub_op(d->op)) &&
          d->type->nelems.segment_mask == 0b11 && !d->type->nelems.is_sve()) {
        ASSERT(d->deps.size() == 2);
        auto a = d->deps[0], b = d->deps[1];
        if (a->op == IVO_CONCAT && b->op == IVO_CONCAT && a->uses.size() == 1 &&
            b->uses.size() == 1 && a->deps.size() == 2 && b->deps.size() == 2) {
          ASSERT(a->uses[0].v);
          ASSERT(a->uses[0].v->id == d->id);
          ASSERT(b->uses[0].v);
          ASSERT(b->uses[0].v->id == d->id);
          auto a_0 = a->deps[0], a_1 = a->deps[1];
          auto b_0 = b->deps[0], b_1 = b->deps[1];
          if (!is_sub_op(d->op)) {
            // The op is either FADD, FMUL, SQADD, or SQMUL
            if (is_uniform_real_constant_value(a_0, 0.0)) {
              // d=(a={a_0=0.0, a_1} + b={b_0, b_1}) => a_new={b_0, d_new=(a_1 +
              // b_1)}. d=(a={a_0=0.0, a_1} * b={b_0, b_1}) => a_new={a_0=0.0,
              // d_new=(a_1 * b_1)}.
              auto a_0_new = is_mul_op(d->op) ? a_0 : b_0;
              auto d_new = builder.build_binop(a_1, d->op, b_1);
              auto a_new = builder.build_vec_from_elems({a_0_new, d_new});
              value_replace_uses(d, a_new);
              changed = changed_ever = true;
              continue;
            }
            if (is_uniform_real_constant_value(a_1, 0.0)) {
              // d=(a={a_0, a_1=0.0} + b={b_0, b_1}) => a_new={d_new=(a_0 +
              // b_0), b_1}. d=(a={a_0, a_1=0.0} * b={b_0, b_1}) =>
              // a_new={d_new=(a_0 * b_0), a_1=0.0}.
              auto a_1_new = is_mul_op(d->op) ? a_1 : b_1;
              auto d_new = builder.build_binop(a_0, d->op, b_0);
              auto a_new = builder.build_vec_from_elems({d_new, a_1_new});
              value_replace_uses(d, a_new);
              changed = changed_ever = true;
              continue;
            }
          }
          if (is_uniform_real_constant_value(b_0, 0.0)) {
            // d=(a={a_0, a_1} +- b={b_0=0.0, b_1}) => a_new={a_0, d_new=(a_1 +-
            // b_1)}. d=(a={a_0, a_1} * b={b_0=0.0, b_1}) => a_new={b_0=0.0,
            // d_new=(a_1 * b_1)}.
            auto a_0_new = is_mul_op(d->op) ? b_0 : a_0;
            auto d_new = builder.build_binop(a_1, d->op, b_1);
            auto a_new = builder.build_vec_from_elems({a_0_new, d_new});
            value_replace_uses(d, a_new);
            changed = changed_ever = true;
            continue;
          }
          if (is_uniform_real_constant_value(b_1, 0.0)) {
            // d=(a={a_0, a_1} +- b={b_0, b_1=0.0}) => a_new={d_new=(a_0 +-
            // b_0), a_1}. d=(a={a_0, a_1} * b={b_0, b_1=0.0}) =>
            // a_new={d_new=(a_0 * b_0), b_1=0.0}.
            auto a_1_new = is_mul_op(d->op) ? b_1 : a_1;
            auto d_new = builder.build_binop(a_0, d->op, b_0);
            auto a_new = builder.build_vec_from_elems({d_new, a_1_new});
            value_replace_uses(d, a_new);
            changed = changed_ever = true;
            continue;
          }
        }
      }
      // try to fix this pattern (where b is scalar):
      // a = index(x, y); d = mul(a, b);
      // a = index(x, y); d = add/sub(a, b);
      // this can be expressed instead as:
      // a = mul(x, b); c = mul(y, b); d = index(a, c)
      // a = add/sub(x, b); d = index(a, b)
      if (d->op == IVO_IMUL || d->op == IVO_IADD || d->op == IVO_ISUB) {
        ASSERT(d->deps.size() == 2);
        auto a = d->deps[0], b = d->deps[1];
        // TODO we're intentionally not checking for uses == 1, so there's no
        // guarantee this actually pays off
        //      if it turns out that indexes are quite expensive and we end up
        //      with a lot of similar ones
        bool is_negated = false;
        if (b->op == IVO_INDEX && is_uniform_value(a)) {
          std::swap(a, b);
          is_negated = d->op == IVO_ISUB;
        }
        if (a->op == IVO_INDEX && is_uniform_value(b)) {
          ASSERT(!b->deps.empty());
          ASSERT(a->deps.size() == 2);
          auto a_0 = a->deps[0], a_1 = a->deps[1];
          auto b_0 = b->deps[0];
          // make some new nodes
          ir_value x_new, y_new;
          if (d->op == IVO_IADD) {
            x_new = builder.build_iadd(a_0, b_0);
            y_new = a_1;
          } else if (d->op == IVO_ISUB) {
            x_new = builder.build_isub(a_0, b_0);
            y_new = is_negated ? builder.build_ineg(a_1) : a_1;
          } else {
            ASSERT(d->op == IVO_IMUL);
            x_new = builder.build_imul(a_0, b_0);
            y_new = builder.build_imul(a_1, b_0);
          }
          auto d_new = builder.build_index(x_new, y_new, d->type->nelems);
          value_replace_uses(d, d_new);
          changed = changed_ever = true;
          continue;
        }
      }
      // try to fix this pattern:
      // a = index(x, y); d = scatter(b, c, a)
      // a = index(x, y); d = gather(c, a)
      // this can be expressed as:
      // a = index(0, y); e = gep(c, x); d = scatter(b, e, a)
      // a = index(0, y); e = gep(c, x); d = gather(e, a)
      // TODO: This is not always beneficial, it only helps it enables
      //       you to hoist the index out of the loop. In other cases this
      //       is actually a misoptimisation, however armclang fails
      //       to generate anything except zero-based indexes correctly!
      if (d->op == IVO_SCATTER || d->op == IVO_GATHER) {
        ASSERT(d->deps.size() == (d->op == IVO_SCATTER ? 3 : 2));
        auto c = d->op == IVO_SCATTER ? d->deps[1] : d->deps[0];
        auto a = d->op == IVO_SCATTER ? d->deps[2] : d->deps[1];
        if (a->op == IVO_INDEX) {
          ASSERT(a->deps.size() == 2);
          auto a_0 = a->deps[0], a_1 = a->deps[1];
          if (a_0->op != IVO_CONST_INT ||
              a_0->literals != std::vector<double>{0.0} ||
              a_0->type->elem_width != a_1->type->elem_width) {
            // make some new nodes, get rid of old scatter/gather
            auto zero = builder.build_int_constant(a_1->type->elem_width, 0);
            auto e_new = builder.build_gep(c, a_0);
            auto a_new = builder.build_index(zero, a_1, a->type->nelems);
            if (d->op == IVO_SCATTER) {
              builder.build_scatter(d->deps[0], e_new, a_new);
              to_erase.insert(d);
            } else {
              auto d_new = builder.build_load_or_gather(e_new, a_new, d->type);
              value_replace_uses(d, d_new);
            }
            changed = changed_ever = true;
            continue;
          }
        }
      }
      // try to fix this pattern (which is quite common in sve generation):
      // c = { a, b }; d = { c }
      // this can be expressed instead as:
      // d = { a, b }
      if (d->op == IVO_CONCAT && d->deps.size() == 1) {
        auto c = d->deps[0];
        if (c->op == IVO_CONCAT) {
          // we're not explicitly checking that c only has one use here,
          // but it shouldn't be a bad idea either way really.
          auto d_new = scope->create_ir_value(d->op, d->type, c->deps,
                                              d->literals, d->str);
          value_replace_uses(d, d_new);
          changed = changed_ever = true;
          continue;
        }
      }
      // try to fix this pattern:
      // a = load(b, c); d = { a } or d = { a, a, ... }
      // this can be expressed instead as:
      // d = load_bcast(b, c)
      if (d->op == IVO_CONCAT &&
          (d->deps.size() == 1 || is_all_irvalue_equal(d->deps))) {
        auto a = d->deps[0];
        if (a->op == IVO_LOAD && a->deps.size() == 2 &&
            a->type->nelems.is_contig() && d->type->nelems.is_contig() &&
            a->deps[0]->type->inner_type) {
          auto base = a->deps[0];
          auto offset = a->deps[1];
          auto d_new = builder.build_load_bcast(base, offset, d->type);
          value_replace_uses(d, d_new);
          changed = changed_ever = true;
          continue;
        }
      }
      // try to fix this pattern:
      // a = x[i], b = x[j]; d = { a, b }  ->  d = shuffle(x, {i,j})
      if (d->op == IVO_CONCAT && d->deps.size() == 2) {
        auto a = d->deps[0];
        auto b = d->deps[1];
        if (a->op == IVO_SHUFFLE && b->op == IVO_SHUFFLE &&
            a->deps.size() == 1 && b->deps.size() == 1 &&
            a->deps[0] == b->deps[0] && a->literals.size() == 1 &&
            b->literals.size() == 1) {
          auto a_idx = a->literals[0];
          auto b_idx = b->literals[0];
          // we're not explicitly checking that c only has one use here,
          // but it shouldn't be a bad idea either way really.
          auto d_new = scope->create_ir_value(IVO_SHUFFLE, d->type, a->deps,
                                              {a_idx, b_idx}, d->str);
          value_replace_uses(d, d_new);
          changed = changed_ever = true;
          continue;
        }
      }
      // try to fix this pattern (which clang does not elide in SVE):
      // a = { 1.0 }; d = mul(a, b)
      // a = { 0.0 }; d = add(a, b)
      // this can be expressed instead as:
      // d = b
      // TODO: We shouldn't have to do this, armclang just forgets sometimes.
      if (is_mul_op(d->op) || is_add_op(d->op)) {
        ASSERT(d->deps.size() == 2);
        auto a = d->deps[0], b = d->deps[1];
        if (b->op == IVO_CONCAT) {
          std::swap(a, b);
        }
        if (a->op == IVO_CONCAT) {
          double identity = is_mul_op(d->op) ? 1.0 : 0.0;
          bool valid = true;
          for (unsigned i = 0; i < a->deps.size(); ++i) {
            auto a_i = a->deps[i];
            valid &= is_uniform_real_constant_value(a_i, identity);
          }
          if (valid) {
            ASSERT(*b->type == *d->type);
            value_replace_uses(d, b);
            changed = changed_ever = true;
            continue;
          }
        }
      }
      // try to fix this pattern
      // a = { _, 0 }; d = conj(a)
      // this can be expressed instead as:
      // a = { _, 0 }; d = a
      // this requires individual lane tracking, which clang/gcc don't do.
      if (is_conj_op(d->op)) {
        ASSERT(d->deps.size() == 1);
        auto a = d->deps[0];
        if ((a->op == IVO_SVE_TRN1 || a->op == IVO_CONCAT ||
             a->op == IVO_ZIP1) &&
            a->deps.size() == 2) {
          if (is_uniform_constant_value(a->deps[1], 0)) {
            value_replace_uses(d, a);
            changed = changed_ever = true;
            continue;
          }
        }
      }
      // do constant propagation
      if ((d->op == IVO_IMUL || d->op == IVO_IADD || d->op == IVO_ISUB ||
           d->op == IVO_GEP) &&
          !d->type->nelems.is_sve()) {
        ASSERT(d->deps.size() == 2);
        auto a = d->deps[0], b = d->deps[1];
        double identity = d->op == IVO_IMUL ? 1.0 : 0.0;
        bool can_lhs_be_identity = d->op == IVO_IADD || d->op == IVO_IMUL;
        if (can_lhs_be_identity && is_uniform_constant_value(a, identity)) {
          // a = identity; d = a op b -> use b instead of d
          value_replace_uses(d, b);
          changed = changed_ever = true;
          continue;
        }
        if (is_uniform_constant_value(b, identity)) {
          // a = const; b = identity; d = a op b -> use a instead of d
          value_replace_uses(d, a);
          changed = changed_ever = true;
          continue;
        }
        if (d->op == IVO_IMUL && (is_uniform_constant_value(a, 0) ||
                                  is_uniform_constant_value(b, 0))) {
          auto d2 = builder.build_int_constant(d->type->elem_width, 0);
          value_replace_uses(d, d2);
          changed = changed_ever = true;
          continue;
        }
        if (a->op == IVO_CONST_INT && b->op == IVO_CONST_INT) {
          ASSERT(d->op != IVO_GEP);
          int new_val =
              d->op == IVO_IMUL   ? (int)a->literals[0] * (int)b->literals[0]
              : d->op == IVO_IADD ? (int)a->literals[0] + (int)b->literals[0]
                                  : (int)a->literals[0] - (int)b->literals[0];
          auto d2 = builder.build_int_constant(d->type->elem_width, new_val);
          value_replace_uses(d, d2);
          changed = changed_ever = true;
          continue;
        }
      }
      // try to fix these patterns:
      // fmul(0,a) => 0
      // fmul(a,0) => 0
      // fmul(1,a) => a
      // fmul(a,1) => a
      // fcmul(1+0i,a) => a
      // fcmul(a,1+0i) => a
      // TO DO: merge this optimization into the above (do constant propagation)
      if (is_mul_op(d->op) || is_cmul_op(d->op)) {
        ASSERT(d->deps.size() == 2);
        auto a = d->deps[0];
        auto b = d->deps[1];
        if (is_uniform_constant_value(a, 0) ||
            is_uniform_constant_value(b, 0)) {
          auto zero = builder.build_real_constant(d->type, 0);
          auto d_new = builder.build_splat_if_needed(zero, d->type->nelems);
          value_replace_uses(d, d_new);
          changed = changed_ever = true;
          continue;
        }
        if (is_mul_op(d->op)) {
          if (is_uniform_constant_value(a, 1)) {
            value_replace_uses(d, b);
            changed = changed_ever = true;
            continue;
          }
          if (is_uniform_constant_value(b, 1)) {
            value_replace_uses(d, a);
            changed = changed_ever = true;
            continue;
          }
        } else if (is_cmul_op(d->op)) {
          if (is_complex_one(a)) {
            value_replace_uses(d, b);
            changed = changed_ever = true;
            continue;
          }
          if (is_complex_one(b)) {
            value_replace_uses(d, a);
            changed = changed_ever = true;
            continue;
          }
        }
      }

      // Look for sve indexes  with a range of 0 to 1
      // Gather dep 1 is an index
      if (d->op == IVO_GATHER) {
        ASSERT(d->deps.size() == 2);
        auto base = d->deps[0];
        auto index = d->deps[1];
        if (index->op == IVO_INDEX) {
          ir_value a = index->deps[0], b = index->deps[1];
          if (is_uniform_constant_value(a, 0) &&
              is_uniform_constant_value(b, 1)) {
            ir_value d_new;
            if (d->type->nelems.is_sve()) {
              // a = &x[y]; v = a[0]   =>   v = x[y]
              auto new_base = base->deps[0];
              auto new_offset = base->deps[1];
              // for double complex, multiply the offset by 2 because there is
              // no LD1D zt, pg, [xn, xm, lsl #4]. This is so that we do not
              // need to emit an additional add instruction later.
              if (d->type->nelems.segment_mask == 0b11 &&
                  d->type->elem_width == 64) {
                new_offset = builder.build_imul(
                    new_offset, builder.build_int_constant(
                                    new_offset->type->elem_width, 2));
              }
              auto d_load = builder.build_load(new_base, new_offset, d->type);
              if (d->type->nelems.is_one() && d->type->elem_width == 64) {
                auto zero = builder.build_real_constant(d->type, 0);
                auto zero_vec =
                    builder.build_splat_if_needed(zero, d->type->nelems);
                d_new = builder.build_zip(d_load, zero_vec);
              } else {
                d_new = d_load;
              }
            } else {
              // Create a constant to replace the index, this change is used
              // later to output load instructions in place of gather
              // instructions
              auto new_offset =
                  builder.build_int_constant(index->type->elem_width, 0);
              d_new = builder.build_load(base, new_offset, d->type);
            }
            value_replace_uses(d, d_new);
            changed = changed_ever = true;
            continue;
          }
          if (is_uniform_constant_value(a, 0) &&
              is_uniform_constant_value(b, -1) &&
              !index->type->nelems.is_sve()) {
            // A gather with step -1 is a load+rev.
            ASSERT(base->type->inner_type);
            ASSERT(!base->type->inner_type->nelems.is_sve());
            ASSERT(base->type->inner_type->nelems.is_contig());
            ASSERT(index->type->nelems.is_contig());
            auto nelems = index->type->nelems.count_contig() /
                          base->type->inner_type->nelems.count_contig();
            auto new_offset = builder.build_int_constant(
                index->type->elem_width, -(nelems - 1));
            auto d_new = builder.build_gather(base, new_offset, d->type);
            auto val_new = builder.build_rev_vec(d_new);
            value_replace_uses(d, val_new);
            changed = changed_ever = true;
            continue;
          }
        }
      }
      // Look for sve indexes  with a range of 0 to 1
      // Scatter dep 2 is an index
      if (d->op == IVO_SCATTER) {
        ASSERT(d->deps.size() == 3);
        auto val = d->deps[0];
        auto base = d->deps[1];
        auto index = d->deps[2];
        if (index->op == IVO_INDEX) {
          ir_value a = index->deps[0], b = index->deps[1];
          if (is_uniform_constant_value(a, 0) &&
              is_uniform_constant_value(b, 1)) {
            ir_value new_offset;
            if (val->type->nelems.is_sve()) {
              // a = &x[y]; b[0] = a   =>   x[y] = a
              auto new_base = base->deps[0];
              auto new_val = val;
              new_offset = base->deps[1];
              // for double complex, multiply the offset by 2 because there is
              // no ST1D zt, pg, [xn, xm, lsl #4]. This is so that we do not
              // need to emit an additional add instruction later.
              if (val->type->nelems.segment_mask == 0b11 &&
                  val->type->elem_width == 64) {
                new_offset = builder.build_imul(
                    new_offset, builder.build_int_constant(
                                    new_offset->type->elem_width, 2));
              } else if (val->type->nelems.is_one() &&
                         val->type->elem_width == 64) {
                new_val = builder.build_sve_take_neon_real(val);
              }
              builder.build_store(new_val, new_base, new_offset);
            } else {
              // Create a constant to replace the index, this change is used
              // later to output store instructions in place of scatter
              // instructions
              new_offset =
                  builder.build_int_constant(index->type->elem_width, 0);
              builder.build_store(val, base, new_offset);
            }
            to_erase.insert(d);
            changed = changed_ever = true;
            continue;
          }
          if (is_uniform_constant_value(a, 0) &&
              is_uniform_constant_value(b, -1) &&
              !index->type->nelems.is_sve()) {
            // A scatter with step -1 is a rev+store.
            ASSERT(base->type->inner_type);
            ASSERT(!base->type->inner_type->nelems.is_sve());
            ASSERT(base->type->inner_type->nelems.is_contig());
            ASSERT(index->type->nelems.is_contig());
            auto nelems = index->type->nelems.count_contig() /
                          base->type->inner_type->nelems.count_contig();
            auto new_offset = builder.build_int_constant(
                index->type->elem_width, -(nelems - 1));
            auto val_new = builder.build_rev_vec(val);
            builder.build_store(val_new, base, new_offset);
            to_erase.insert(d);
            changed = changed_ever = true;
            continue;
          }
        }
      }
      // For double complex scatter, re-calculate the offset as [0, 1, odist*2,
      // (odist*2)+1, ...] For double complex gather, re-calculate the offset as
      // [0, 1, idist*2, (idist*2)+1, ...]
      if (d->op == IVO_SCATTER || d->op == IVO_GATHER) {
        auto val = d->op == IVO_SCATTER ? d->deps[0] : d;
        auto base = d->op == IVO_SCATTER ? d->deps[1] : d->deps[0];
        auto offset = d->op == IVO_SCATTER ? d->deps[2] : d->deps[1];
        if (val->type->nelems.is_sve() &&
            val->type->nelems.segment_mask == 0b11 &&
            val->type->elem_width == 64 && offset->op == IVO_INDEX) {
          ASSERT(offset->deps.size() == 2);
          auto index_start = offset->deps[0];
          auto index_step = offset->deps[1];
          if (is_uniform_constant_value(index_start, 0)) {
            auto zero =
                builder.build_int_constant(index_start->type->elem_width, 0);
            auto index_step_new = builder.build_iadd(index_step, index_step);
            auto a =
                builder.build_index(zero, index_step_new, offset->type->nelems);
            auto b = builder.build_iadd(
                a, builder.build_int_constant(a->type->elem_width, 1));
            auto offset_new = builder.build_zip(a, b);
            if (d->op == IVO_SCATTER) {
              builder.build_scatter(d->deps[0], base, offset_new);
              to_erase.insert(d);
            } else if (d->op == IVO_GATHER) {
              auto d_new =
                  builder.build_load_or_gather(base, offset_new, d->type);
              value_replace_uses(d, d_new);
            }
            changed = changed_ever = true;
            continue;
          }
        }
      }
      // For double complex, when loading twiddle factor with scalar base and
      // offset multiply offset by a factor of 2 because there is no LD1D zt,
      // pg, [xn, xm, lsl #4]. This is so that we do not need to emit an
      // additional add instruction later. d = gather(e, f) => b = mul(f, 2); d
      // = gather(e, b)
      if (d->op == IVO_GATHER && d->type->nelems.is_sve() &&
          d->type->nelems.segment_mask == 0b11 && d->type->elem_width == 64) {
        auto base = d->deps[0];
        auto offset = d->deps[1];
        if (base->type->kind == IVK_POINTER && base->type->nelems.is_one() &&
            offset->type->nelems.is_one() &&
            !is_uniform_constant_value(offset, 0)) {
          auto new_offset = builder.build_imul(
              offset, builder.build_int_constant(offset->type->elem_width, 2));
          auto d_new = builder.build_load(base, new_offset, d->type);
          value_replace_uses(d, d_new);
          changed = changed_ever = true;
          continue;
        }
      }
      // try to fix this pattern
      // op(re(a), re(b)) => re(op(a,b))
      // op(im(a), im(b)) => im(op(a,b))
      // op(cmplx(a), cmplx(b)) => cmplx(op(a,b)) (only SVE)
      if (is_sub_op(d->op) || is_add_op(d->op)) {
        ASSERT(d->deps.size() == 2);
        auto a = d->deps[0];
        auto b = d->deps[1];
        if ((a->op == IVO_SVE_TRN2 &&
             is_uniform_constant_value(a->deps[1], 0) &&
             b->op == IVO_SVE_TRN2 &&
             is_uniform_constant_value(b->deps[1], 0)) ||
            (a->op == IVO_SVE_TRN1 &&
             is_uniform_constant_value(a->deps[1], 0) &&
             b->op == IVO_SVE_TRN1 &&
             is_uniform_constant_value(b->deps[1], 0))) {
          auto a0 = a->deps[0];
          auto b0 = b->deps[0];
          auto op_new =
              scope->create_ir_value(d->op, a0->type, {a0, b0}, {}, "");
          auto d_new = a->op == IVO_SVE_TRN2 ? builder.build_take_imag(op_new)
                       : (a0->type->nelems.segment_mask & 0b11) == 0b11
                           ? builder.build_take_real(op_new)
                           : builder.build_make_complex(op_new);
          value_replace_uses(d, d_new);
          changed = changed_ever = true;
          continue;
        } else if ((is_neon_take_real(a) && is_neon_take_real(b)) ||
                   (is_neon_take_imag(a) && is_neon_take_imag(b)) ||
                   (is_lane_get(a) && is_lane_get(b) &&
                    a->literals == b->literals)) {
          auto a0 = a->deps[0];
          auto b0 = b->deps[0];
          auto op_new =
              scope->create_ir_value(d->op, a0->type, {a0, b0}, {}, "");
          auto d_new =
              scope->create_ir_value(a->op, a->type, {op_new}, a->literals, "");
          value_replace_uses(d, d_new);
          changed = changed_ever = true;
          continue;
        }
      }
      // try to fix this pattern which makes a real vector be a complex vector
      // with only an imaginary part shuffle(trn1(a0,0)) => trn1(0,a0)
      if (d->op == IVO_SHUFFLE &&
          d->literals == std::vector<double>{1.0, 0.0}) {
        auto a = d->deps[0];
        if (a->op == IVO_SVE_TRN1 && is_uniform_constant_value(a->deps[1], 0)) {
          auto a0 = a->deps[0];
          auto d_new = builder.build_sve_promote_to_imag_complex(a0);
          value_replace_uses(d, d_new);
          changed = changed_ever = true;
          continue;
        }
      }
      // try to fix this pattern which put real parts into consecutive elements
      // uzp1(trn1(a0,0),trn1(a0,0)) => uzp1(a0,a0)
      if (is_neon_take_real(d)) {
        auto a = d->deps[0];
        if (a->op == IVO_SVE_TRN1 && is_uniform_constant_value(a->deps[1], 0)) {
          auto a0 = a->deps[0];
          auto d_new = builder.build_sve_take_neon_real(a0);
          value_replace_uses(d, d_new);
          changed = changed_ever = true;
          continue;
        }
      }
      // try to fix this pattern
      // rev_pairs({ a0, a1 }) => { a1, a0 }
      if (d->op == IVO_SHUFFLE &&
          (d->literals == std::vector<double>{1.0, 0.0} ||
           d->literals == std::vector<double>{1.0, 0.0, 3.0, 2.0})) {
        ASSERT(d->deps.size() == 1);
        auto a = d->deps[0];
        if ((a->op == IVO_CONCAT || a->op == IVO_ZIP1) && a->deps.size() == 2) {
          auto a0 = a->deps[0];
          auto a1 = a->deps[1];
          auto d_new =
              scope->create_ir_value(IVO_ZIP1, d->type, {a1, a0}, {}, "");
          value_replace_uses(d, d_new);
          changed = changed_ever = true;
          continue;
        }
      }
      // try to fix this pattern
      // fadd(a, -b) => fsub(a, b)
      // fsub(a, -b) => fadd(a, b)
      if (is_add_op(d->op) || is_sub_op(d->op)) {
        auto a = d->deps[0];
        auto b = d->deps[1];
        if (is_neg_op(b->op)) {
          auto b0 = b->deps[0];
          auto d_new = is_add_op(d->op) ? builder.build_sub(a, b0)
                                        : builder.build_add(a, b0);
          value_replace_uses(d, d_new);
          changed = changed_ever = true;
          continue;
        }
      }
      // try to fix this pattern
      // fadd(-a, b) => fsub(b, a)
      if (is_add_op(d->op)) {
        auto a = d->deps[0];
        auto b = d->deps[1];
        if (is_neg_op(a->op)) {
          auto a0 = a->deps[0];
          auto d_new = builder.build_sub(b, a0);
          value_replace_uses(d, d_new);
          changed = changed_ever = true;
          continue;
        }
      }
      // try to fix this pattern
      // fadd(zip1(a,0),zip1(0,b)) => zip1(a,b)
      // fadd(zip1(0,a),zip1(b,0)) => zip1(b,a)
      // fsub(zip1(a,0),zip1(0,b)) => conj(zip1(a,b))
      // fsub(zip1(0,a),zip1(b,0)) => fmul(zip1(b,a),{-1,1})
      if (is_add_op(d->op) || is_sub_op(d->op)) {
        ASSERT(d->deps.size() == 2);
        auto a = d->deps[0];
        auto b = d->deps[1];
        if ((a->op == IVO_CONCAT && a->deps.size() == 2 &&
             is_uniform_constant_value(a->deps[1], 0) && b->op == IVO_ZIP1 &&
             is_uniform_constant_value(b->deps[0], 0)) ||
            (a->op == IVO_ZIP1 && is_uniform_constant_value(a->deps[0], 0) &&
             b->op == IVO_CONCAT && b->deps.size() == 2 &&
             is_uniform_constant_value(b->deps[1], 0))) {
          ir_value d_new;
          auto re = a->op == IVO_CONCAT ? a : b;
          auto im = a->op == IVO_CONCAT ? b : a;
          auto a_new = scope->create_ir_value(
              IVO_ZIP1, d->type, {re->deps[0], im->deps[1]}, {}, "");
          if (is_add_op(d->op)) {
            d_new = a_new;
          } else {
            if (re == a) {
              d_new = builder.build_conj(a_new);
            } else {
              auto neg_real =
                  builder.build_complex_constant(d->type, -1.0, 1.0);
              d_new = builder.build_mul(a_new, neg_real);
            }
          }
          value_replace_uses(d, d_new);
          changed = changed_ever = true;
          continue;
        }
      }
      // try to fix this pattern
      // re(conj(a)) => re(a)
      if (is_neon_take_real(d) || (is_lane_get(d) && d->literals[0] == 0.0)) {
        auto a = d->deps[0];
        if (is_conj(a)) {
          auto a0 = a->deps[0];
          auto d_new =
              scope->create_ir_value(d->op, d->type, {a0}, d->literals, "");
          value_replace_uses(d, d_new);
          changed = changed_ever = true;
          continue;
        }
      }
    }

    // Loop over ir_value in the erase list and erase them
    for (auto x : to_erase) {
      value_erase(scope, x);
    }
    for (auto &c : scope->children) {
      ir_builder b2 = builder.get_builder_for_scope(c.get());
      if (do_opt(b2)) {
        changed = changed_ever = true;
      }
    }
  }
  return changed_ever;
}

/**
 * Merge duplicate IR values in a given scope.
 *
 * IR values are regarded as duplicates if they have matching
 * dependencies and approximately matching literals.
 */
static void do_dedup(ir_value_scope *scope) {
  for (bool changed = true; changed;) {
    std::set<ir_value> to_erase;
    std::vector<std::vector<ir_value>> seen;
    for (auto it = scope->begin_any_order(); it != scope->end_any_order();
         ++it) {
      auto a = *it;
      if (a->op == IVO_PARAM || to_erase.find(a) != to_erase.end() ||
          a->has_scope_use(scope)) {
        continue;
      }
      unsigned op = static_cast<unsigned>(a->op);
      if (op >= seen.size()) {
        seen.resize(op + 1);
      }
      for (auto b : seen[op]) {
        if (a->literals.size() != b->literals.size() || a->deps != b->deps ||
            a->has_keep_use() || b->has_keep_use()) {
          continue;
        }
        if (*a->type != *b->type) {
          continue;
        }
        bool all_same = true;
        for (unsigned i = 0; i < a->literals.size(); ++i) {
          if (std::abs(a->literals[i] - b->literals[i]) > RVAL_APPROX_MARGIN) {
            all_same = false;
            break;
          }
        }
        if (!all_same) {
          continue;
        }
        value_replace_uses(b, a);
        to_erase.insert(b);
      }
      seen[op].push_back(a);
    }
    changed = !to_erase.empty();
    for (auto x : to_erase) {
      value_erase(scope, x);
    }
  }
  for (auto &c : scope->children) {
    do_dedup(c.get());
  }
}

/**
 * Hoist all IR values up into the outermost scope possible.
 *
 * A value can be hoisted if it does not contain any dependencies in its
 * current scope.
 */
static void do_hoist(ir_value_scope *scope) {
  // do child scopes first to allow hoisting over many levels
  for (auto &c : scope->children) {
    do_hoist(c.get());
  }
  if (!scope->parent)
    return;
  for (bool changed = true; changed;) {
    std::set<ir_value> to_hoist;
    for (auto it = scope->begin_any_order(); it != scope->end_any_order();
         ++it) {
      auto a = *it;
      if (a->op == IVO_PARAM)
        continue;
      if (a->has_scope_use(scope))
        continue;
      ASSERT(a->scope == scope);
      bool all_deps_above_scope = true;
      for (auto b : a->deps) {
        // this should really be "the dep's scope must be a parent of scope",
        // but the only other scope it could realistically be is the current
        // scope so we can get away without traversing the whole scope chain.
        if (b->scope == scope || b->has_scope_use(scope)) {
          all_deps_above_scope = false;
          break;
        }
      }
      if (!all_deps_above_scope) {
        continue;
      }
      // hoist a into parent scope
      to_hoist.insert(a);
    }
    for (auto x : to_hoist) {
      ASSERT(x->scope == scope);
      scope->parent->insert_ir_value(scope->erase_ir_value(x->id));
    }
    changed = !to_hoist.empty();
  }
}

static bool scope_uses_za(ir_value_scope *scope) {
  for (auto v : scope->in_order()) {
    if (v->op == IVO_ZA_ZERO || v->op == IVO_ZA_SLICE_WRITE ||
        v->op == IVO_ZA_SLICE_READ) {
      return true;
    }
  }
  for (const auto &c : scope->children) {
    if (scope_uses_za(c.get())) {
      return true;
    }
  }
  return false;
}

// scalar printing

static void for_loop_add_induction(std::vector<ir_value> &out, ir_value init,
                                   ir_value inc) {
  out.push_back(init);
  out.push_back(inc);
}

static void
for_loop_add_param_inductions(std::vector<ir_value> &out,
                              const std::map<std::string, ir_value> &params,
                              order_kind order, ir_builder b, ir_value x_inc,
                              ir_value y_inc, ir_value w_inc) {
  for_loop_add_induction(out, params.at("X"), x_inc);
  if (params.count("XX")) {
    auto xx_inc = order == ORDER_AB ? b.build_ineg(x_inc) : x_inc;
    for_loop_add_induction(out, params.at("XX"), xx_inc);
  }
  for_loop_add_induction(out, params.at("Y"), y_inc);
  if (params.count("YY")) {
    auto yy_inc = order == ORDER_AB ? b.build_ineg(y_inc) : y_inc;
    for_loop_add_induction(out, params.at("YY"), yy_inc);
  }
  if (params.count("W") && order == ORDER_AB) {
    for_loop_add_induction(out, params.at("W"), w_inc);
  }
}

static auto get_sve_loop_inc(ir_builder &b, rtype rtw,
                             std::optional<int> known_vector_length_bytes,
                             bool want_sme) {
  const int element_width_bytes = 2 * (rtw.bits() / 8);

  if (known_vector_length_bytes) {
    // SME kernels only support VL=512
    ASSERT(!want_sme || *known_vector_length_bytes == 64);
    return b.build_int_constant(64, *known_vector_length_bytes /
                                        element_width_bytes);
  }

  // vector-length agnostic
  ASSERT(!want_sme);
  switch (element_width_bytes) {
  case 4:
    return b.build_sve_cntw();
  case 8:
    return b.build_sve_cntd();
  case 16:
    return b.build_idiv(b.build_sve_cntd(), b.build_int_constant(64, 2));
  }
  ASSERT(false);
}

void setup_params(std::map<std::string, ir_value> &params, const target_t &t,
                  bool want_twid, const kernel_types_t &types,
                  const io_mods_t &mods, const known_layout_t &known_layout,
                  ir_builder &b);

static ir_value build_input_value(ir_builder &b, const target_t &t,
                                  const std::map<std::string, ir_value> &params,
                                  int64_t n,
                                  const std::vector<int64_t> &in_perm,
                                  const kernel_types_t &rt, order_kind order,
                                  const io_mods_t &mods, int64_t idx) {
  auto nhalfhi = n / 2 + 1;
  int Xindex = in_perm[idx];
  bool use_xx = (mods.in == im_halflo && Xindex >= n / 2) ||
                (mods.in == im_halfhi && Xindex >= n / 2 + 1);
  bool needs_conj =
      (mods.out == out_mods::om_real && Xindex >= nhalfhi) || use_xx;

  if (mods.out == out_mods::om_real && Xindex >= nhalfhi) {
    Xindex = (int)n - Xindex;
  } else if (mods.in == im_halflo && Xindex >= n / 2) {
    Xindex = Xindex - (int)(n / 2);
  } else if (mods.in == im_halfhi && Xindex >= n / 2 + 1) {
    Xindex = Xindex - (int)((n / 2) + 1);
  }
  char op = '+';
  if (use_xx) {
    Xindex = -Xindex;
    if (order == ORDER_AB) {
      op = '-';
    }
  }

  // we only want to allow a few of these to be hoisted, so we instead
  // express it as Xindex = Xindex_a + Xindex_b*8 (where the addition
  // is _not_ hoisted). This means that e.g. for n=32 we instead only
  // need (8+4=12 variables rather than 32!)
  auto Xindex_a = Xindex % 8;
  auto Xindex_b = Xindex - Xindex_a;

  auto param_j = params.at("j");
  auto Xbase = use_xx ? params.at("XX") : params.at("X");
  auto istride = params.at("istride");
  auto idx1_a = b.build_imul(
      istride, b.build_int_constant(istride->type->elem_width, Xindex_a));
  auto idx1_b = b.build_imul(
      istride, b.build_int_constant(istride->type->elem_width, Xindex_b));
  Xbase = b.build_gep(Xbase, idx1_b);
  ir_value value;
  if (param_j->op == IVO_INDEX) {
    auto idist = params.at("idist");
    if (t.has_sve && is_uniform_constant_value(idist, 1) &&
        is_uniform_constant_value(param_j->deps[1], 1) && op != '-') {
      auto idx2_base = param_j->deps[0];
      Xbase = b.build_gep(Xbase, idx2_base);
      auto inner_type = Xbase->type->inner_type;
      auto ty = make_ir_value_type_vector_for_load(
          t, b.layout, param_j->type->nelems, param_j->type->elem_width,
          inner_type);
      // If loading fp64 complex we need to double the index to compensate
      // for using ld1d's lsl #3 scaling instead of a hypothetical ld1q with
      // lsl #4 scaling.
      if (inner_type->nelems.segment_mask == 0b11 &&
          inner_type->elem_width == 64) {
        idx1_a = b.build_imul(
            idx1_a, b.build_int_constant(idx1_a->type->elem_width, 2));
      }
      value = b.build_load(std::move(Xbase), std::move(idx1_a), std::move(ty));
    } else {
      auto idx2_base = b.build_imul(idist, param_j->deps[0]);
      auto idx2_step = b.build_imul(idist, param_j->deps[1]);
      if (op == '-') {
        idx2_base = b.build_ineg(std::move(idx2_base));
        idx2_step = b.build_ineg(std::move(idx2_step));
      }
      Xbase = b.build_gep(Xbase, idx2_base);
      Xbase = b.build_gep(Xbase, idx1_a);
      auto zero_idx = b.build_int_constant(idx2_step->type->elem_width, 0);
      auto j_offsets =
          b.build_index(zero_idx, idx2_step, param_j->type->nelems);
      value = b.build_load_or_gather(std::move(Xbase), std::move(j_offsets));
    }
  } else {
    auto idx2 = b.build_imul(params.at("idist"), param_j);
    if (op == '-') {
      idx2 = b.build_ineg(std::move(idx2));
    }
    Xbase = b.build_gep(Xbase, idx2);
    value = b.build_load_or_gather(std::move(Xbase), std::move(idx1_a));
  }
  if (!rt.x.is_float()) {
    // apply per-stage scaling in fixed-point kernels
    auto shift = log2_pow2(n);
    value = b.build_srshr(std::move(value), shift);
  }
  if (rt.x != rt.w) {
    value = b.build_cast(std::move(value), rt.w.bits());
  }
  if (needs_conj) {
    value = b.build_conj(std::move(value));
  }
  return value;
}

static std::vector<ir_value>
build_input_values(ir_builder &b, const target_t &t,
                   const std::map<std::string, ir_value> &params, int64_t n,
                   const io_ptr_t &iop, const std::vector<int64_t> &in_perm,
                   const kernel_types_t &types, order_kind order,
                   const io_mods_t &mods) {
  const int64_t in_size = get_in_size_elems(iop);
  std::vector<ir_value> in_vals(in_size);
  for (int64_t idx = 0; idx < in_size; ++idx) {
    in_vals[idx] =
        build_input_value(b, t, params, n, in_perm, types, order, mods, idx);
  }
  return in_vals;
}

static std::vector<ir_value> build_input_values_transposing_load_paired(
    ir_builder &b, const std::map<std::string, ir_value> &params, int64_t n,
    const std::vector<int64_t> &in_perm, ir_value_type_ptr vec_type,
    int64_t in_size, std::optional<int64_t> known_half_vscale) {
  const auto param_j = params.at("j");
  const auto j_base = params.at("j_base");
  auto howmany = params.at("howmany");

  ASSERT(n == 6 || n == 8);
  ASSERT(param_j->type->nelems.count_contig() % 2 == 0);
  const int structured_width = n / 2;
  auto pair_nelems = ir_value_num_elems::with_nlanes(
      param_j->type->nelems.count_contig() / 2, param_j->type->nelems.scale);
  auto pair_type = make_ir_value_type_vector(pair_nelems, 64,
                                             make_ir_value_type_real(64, true));
  ir_value half_vscale;
  ir_value scalar_j_base = j_base;
  if (known_half_vscale) {
    half_vscale =
        b.build_int_constant(j_base->type->elem_width, *known_half_vscale);
  } else {
    scalar_j_base = b.scope->create_ir_value(
        IVO_PARAM, make_ir_value_type_integer(64), {}, {}, "j");
    auto vscale = params.at("vscale");
    // A little unnecessary work as we could select a better CNT
    // instruction instead of dividing by two
    auto two = b.build_int_constant(vscale->type->elem_width, 2);
    half_vscale = b.build_idiv(vscale, two);
  }
  auto hi_j_base = b.build_iadd(scalar_j_base, half_vscale);
  auto pair_stride =
      b.build_int_constant(scalar_j_base->type->elem_width, n / 2);
  auto lo_pair_offset = b.build_imul(pair_stride, scalar_j_base);
  auto hi_pair_offset = b.build_imul(pair_stride, hi_j_base);
  std::vector<ir_value> lo;
  std::vector<ir_value> hi;
  if (pair_type->nelems.is_sve()) {
    auto pred_lo = b.build_whilelt(scalar_j_base, howmany, 64);
    auto pred_hi = b.build_whilelt(hi_j_base, howmany, 64);
    lo = b.build_sve_structure_load(structured_width, pred_lo, params.at("X"),
                                    lo_pair_offset, pair_type);
    hi = b.build_sve_structure_load(structured_width, pred_hi, params.at("X"),
                                    hi_pair_offset, pair_type);
  } else {
    lo = b.build_structure_load(structured_width, params.at("X"),
                                lo_pair_offset, pair_type);
    hi = b.build_structure_load(structured_width, params.at("X"),
                                hi_pair_offset, pair_type);
  }

  std::vector<ir_value> raw_vals(in_size);
  for (int pair = 0; pair < structured_width; ++pair) {
    raw_vals[2 * pair] = b.build_uzp1(lo[pair], hi[pair], vec_type, 32);
    raw_vals[2 * pair + 1] = b.build_uzp2(lo[pair], hi[pair], vec_type, 32);
  }
  std::vector<ir_value> in_vals(in_size);
  for (int idx = 0; idx < in_size; ++idx) {
    in_vals[idx] = raw_vals[in_perm[idx]];
  }
  return in_vals;
}

static bool can_use_structure_load(int64_t n,
                                   const known_layout_t &known_layout,
                                   const kernel_types_t &rt,
                                   const io_mods_t &mods) {
  return known_layout.istride == 1 && known_layout.idist == n &&
         (n == 2 || n == 3 || n == 4 ||
          (rt.x.bits() == 16 && mods.in != im_real && (n == 6 || n == 8))) &&
         (rt.x.bits() == 32 || rt.x.bits() == 16) &&
         mods.out != out_mods::om_real &&
         !(mods.in == im_halflo || mods.in == im_halfhi ||
           mods.in == im_real) &&
         rt.x.is_float() && rt.x == rt.w;
}

static bool can_use_structure_store(int64_t n,
                                    const known_layout_t &known_layout,
                                    const kernel_types_t &rt,
                                    const io_mods_t &mods) {
  return known_layout.ostride == 1 && known_layout.odist == n &&
         (n == 2 || n == 3 || n == 4 ||
          (rt.y.bits() == 16 && (n == 6 || n == 8))) &&
         (rt.y.bits() == 32 || rt.y.bits() == 16) &&
         mods.out == out_mods::om_none && rt.y.is_float() && rt.y == rt.w;
}

static bool supports_transposing_load(
    const target_t &t, const std::map<std::string, ir_value> &params,
    const io_ptr_t &iop, const kernel_types_t &rt, const io_mods_t &mods) {
  const int64_t tile_dim =
      *t.known_vector_length_bytes /
      params.at("X")->type->inner_type->count_contig_bytes();
  const int max_tile_count = rt.x.bits() / 8;
  return get_in_size_elems(iop) <= max_tile_count * tile_dim &&
         (rt.x.bits() == 32 || rt.x.bits() == 16) &&
         !(mods.in == im_halflo || mods.in == im_halfhi) && rt.x.is_float() &&
         rt.x == rt.w;
}

static std::vector<ir_value>
build_input_values_structure_load(ir_builder &b, const target_t &t,
                                  const std::map<std::string, ir_value> &params,
                                  int64_t n, const io_ptr_t &iop,
                                  const std::vector<int64_t> &in_perm) {
  const auto param_j = params.at("j");
  const auto j_base = params.at("j_base");
  auto inner_type = params.at("X")->type->inner_type;
  auto vec_type =
      make_ir_value_type_vector_for_load(t, b.layout, param_j->type->nelems,
                                         param_j->type->elem_width, inner_type);
  auto howmany = params.at("howmany");
  if (n == 6 || n == 8) {
    std::optional<int64_t> known_half_vscale;
    if (t.known_vector_length_bytes) {
      known_half_vscale =
          *t.known_vector_length_bytes /
          (2 * params.at("X")->type->inner_type->count_contig_bytes());
    }
    return build_input_values_transposing_load_paired(
        b, params, n, in_perm, vec_type, get_in_size_elems(iop),
        known_half_vscale);
  }
  auto row_stride = b.build_int_constant(j_base->type->elem_width, n);
  auto row_offset = b.build_imul(row_stride, j_base);
  if (vec_type->nelems.is_sve()) {
    auto pred_structured =
        b.build_whilelt(j_base, howmany, effective_elem_bits(*vec_type));
    return b.build_sve_structure_load(n, pred_structured, params.at("X"),
                                      row_offset, vec_type);
  }
  return b.build_structure_load(n, params.at("X"), row_offset, vec_type);
}

static std::vector<ir_value> build_input_values_transposing_load(
    ir_builder &b, const target_t &t,
    const std::map<std::string, ir_value> &params, int64_t n,
    const io_ptr_t &iop, const std::vector<int64_t> &in_perm,
    const kernel_types_t &rt, order_kind order, const io_mods_t &mods) {
  const int64_t tile_dim =
      *t.known_vector_length_bytes /
      params.at("X")->type->inner_type->count_contig_bytes();
  const int64_t in_size = get_in_size_elems(iop);
  const int max_tile_count = rt.x.bits() / 8;

  // First, some assertions that the config is supported
  ASSERT(
      in_size <= max_tile_count * tile_dim &&
      "Transposing load not supported for input size greater than za capacity");
  ASSERT((rt.x.bits() == 32 || rt.x.bits() == 16) &&
         "Transposing load not yet supported for FP64");
  ASSERT(!(mods.in == im_halflo || mods.in == im_halfhi) &&
         "Transposing load only implemented for full-spectrum input");
  ASSERT(rt.x.is_float() &&
         "Transposing load not yet supported for fixed-point");
  ASSERT(rt.x == rt.w &&
         "Transposing load not yet supported for mixed-precision kernels");

  const auto param_j = params.at("j");
  const auto j_base = params.at("j_base");

  auto inner_type = params.at("X")->type->inner_type;
  auto vec_type =
      make_ir_value_type_vector_for_load(t, b.layout, param_j->type->nelems,
                                         param_j->type->elem_width, inner_type);

  auto idist = params.at("idist");
  auto howmany = params.at("howmany");

  // Use de-interleaved pairs of structured loads where possible. Without
  // availability of LD[34]Q, we can only do this optimization for FP16.
  if ((n == 6 || n == 8) && rt.x.bits() == 16 && mods.in != im_real) {
    return build_input_values_transposing_load_paired(
        b, params, n, in_perm, vec_type, in_size, tile_dim / 2);
  }

  // Config is supported and no shortcut possible - effect the transpose by
  // writing to horizontal slices and reading from vertical slices
  auto zero = b.build_int_constant(idist->type->elem_width, 0);
  auto one = b.build_int_constant(idist->type->elem_width, 1);
  auto rows_left = b.build_isub(howmany, j_base);
  auto tile_dim_val = b.build_int_constant(idist->type->elem_width, tile_dim);
  auto rows = b.build_min(rows_left, tile_dim_val);
  auto rows_limit = b.build_isub(rows, one);
  auto za = b.build_za_zero();
  const int tile_count = (in_size + tile_dim - 1) / tile_dim;
  for (int tile = 0; tile < tile_count; ++tile) {
    auto tile_offset =
        b.build_int_constant(idist->type->elem_width, tile * tile_dim);
    ir_value pred_elems;
    if (tile < in_size / tile_dim) {
      // We will fill this row, so use ptrue
      pred_elems = b.build_ptrue();
    } else {
      // Vector tail - we will not fill this row
      auto tail_val =
          b.build_int_constant(idist->type->elem_width, in_size % tile_dim);
      pred_elems =
          b.build_whilelt(zero, tail_val, effective_elem_bits(*vec_type));
    }
    for (int row = 0; row < tile_dim; ++row) {
      auto row_idx = b.build_int_constant(j_base->type->elem_width, row);
      auto row_clamped = b.build_min(row_idx, rows_limit);
      auto row_j = b.build_iadd(j_base, row_clamped);
      auto row_offset = b.build_iadd(b.build_imul(idist, row_j), tile_offset);
      auto row_base = params.at("X");
      auto row_vals =
          b.build_predicated_load(pred_elems, row_base, row_offset, vec_type);
      auto slice = b.build_int_constant(64, row);
      za = b.build_za_slice_write(za, pred_elems, row_vals, 0, tile, slice);
    }
  }

  std::vector<ir_value> in_vals(in_size);
  for (int col = 0; col < in_size; ++col) {
    const int idx = in_perm[col];
    auto slice = b.build_int_constant(64, idx % tile_dim);
    in_vals[col] =
        b.build_za_slice_read(za, vec_type, 1, idx / tile_dim, slice);
  }
  return in_vals;
}

static void emit_output_value(ir_builder &b, const target_t &t,
                              const std::map<std::string, ir_value> &params,
                              int64_t n, const std::vector<int64_t> &out_perm,
                              plfft_direction_t dir, const kernel_types_t &rt,
                              order_kind order, const io_mods_t &mods,
                              int64_t out_ofs, ir_value value) {
  auto nhalfhi = n / 2 + 1;
  auto nhalflo = (n + 1) / 2;
  int Yindex =
      dir == PLFFT_FORWARD ? out_perm[out_ofs] : (n - out_perm[out_ofs]) % n;

  // We don't want to write back to the second half of the output
  if (mods.out == out_mods::halfhi && Yindex >= nhalfhi) {
    return;
  } else if (mods.out == out_mods::halflo && Yindex >= nhalflo) {
    return;
  }

  const bool is_conj_reverse =
      mods.out == out_mods::conj_reverse && Yindex >= nhalflo;
  auto Ybase = is_conj_reverse ? params.at("YY") : params.at("Y");
  if (is_conj_reverse) {
    value = b.build_conj(std::move(value));
    Yindex = -Yindex % nhalflo;
  }

  if (rt.y != rt.w) {
    value = b.build_cast(std::move(value), rt.y.bits());
  }

  // we only want to allow a few of these to be hoisted, so we instead
  // express it as Yindex = Yindex_a + Yindex_b*8 (where the addition
  // is _not_ hoisted). This means that e.g. for n=32 we instead only
  // need (8+4=12 variables rather than 32!)
  int Yindex_a = Yindex % 8;
  int Yindex_b = Yindex - Yindex_a;
  auto ostride = params.at("ostride");
  auto idx1_a = b.build_imul(
      ostride, b.build_int_constant(ostride->type->elem_width, Yindex_a));
  auto idx1_b = b.build_imul(
      ostride, b.build_int_constant(ostride->type->elem_width, Yindex_b));
  Ybase = b.build_gep(Ybase, idx1_b);

  auto param_j = params.at("j");
  bool want_neg = is_conj_reverse && order == ORDER_AB;
  if (param_j->op == IVO_INDEX) {
    auto odist = params.at("odist");
    if (t.has_sve && is_uniform_constant_value(odist, 1) &&
        is_uniform_constant_value(param_j->deps[1], 1) && !want_neg) {
      auto idx2_base = param_j->deps[0];
      Ybase = b.build_gep(Ybase, idx2_base);
      // If storing fp64 complex we need to double the index to compensate
      // for using st1d's lsl #3 scaling instead of a hypothetical st1q with
      // lsl #4 scaling.
      if (Ybase->type->inner_type->nelems.count_contig() > 1 &&
          Ybase->type->inner_type->elem_width == 64) {
        idx1_a = b.build_imul(
            idx1_a, b.build_int_constant(idx1_a->type->elem_width, 2));
      }
      b.build_store(std::move(value), Ybase, std::move(idx1_a));
    } else {
      auto idx2_base = b.build_imul(odist, param_j->deps[0]);
      auto idx2_step = b.build_imul(odist, param_j->deps[1]);
      if (want_neg) {
        idx2_base = b.build_ineg(std::move(idx2_base));
        idx2_step = b.build_ineg(std::move(idx2_step));
      }
      Ybase = b.build_gep(Ybase, idx2_base);
      Ybase = b.build_gep(Ybase, idx1_a);
      auto zero = b.build_int_constant(idx2_step->type->elem_width, 0);
      auto j_offsets = b.build_index(zero, idx2_step, param_j->type->nelems);
      b.build_scatter(std::move(value), Ybase, std::move(j_offsets));
    }
  } else {
    auto idx2 = b.build_imul(params.at("odist"), param_j);
    if (is_conj_reverse && order == ORDER_AB) {
      idx2 = b.build_ineg(std::move(idx2));
    }
    Ybase = b.build_gep(Ybase, idx2);
    b.build_store_or_scatter(std::move(value), Ybase, std::move(idx1_a));
  }
}

static void emit_output_values(ir_builder &b, const target_t &t,
                               const std::map<std::string, ir_value> &params,
                               int64_t n, const io_ptr_t &iop,
                               const std::vector<int64_t> &out_perm,
                               plfft_direction_t dir,
                               const kernel_types_t &types, order_kind order,
                               const io_mods_t &mods,
                               std::vector<ir_value> &out_vals) {
  const int64_t out_size = get_out_size_elems(iop);
  for (int64_t idx = 0; idx < out_size; ++idx) {
    emit_output_value(b, t, params, n, out_perm, dir, types, order, mods, idx,
                      std::move(out_vals[idx]));
  }
}

static void emit_output_values_structure_store_paired(
    ir_builder &b, const target_t &t,
    const std::map<std::string, ir_value> &params, int64_t n,
    std::vector<ir_value> &store_vals,
    std::optional<int64_t> known_half_vscale) {
  const auto param_j = params.at("j");
  const auto j_base = params.at("j_base");
  auto howmany = params.at("howmany");

  ASSERT(n == 6 || n == 8);
  ASSERT(param_j->type->nelems.count_contig() % 2 == 0);
  const int structured_width = n / 2;
  auto pair_nelems = ir_value_num_elems::with_nlanes(
      param_j->type->nelems.count_contig() / 2, param_j->type->nelems.scale);
  auto pair_type = make_ir_value_type_vector(pair_nelems, 64,
                                             make_ir_value_type_real(64, true));
  ir_value half_vscale;
  ir_value scalar_j_base = j_base;
  if (known_half_vscale) {
    half_vscale =
        b.build_int_constant(j_base->type->elem_width, *known_half_vscale);
  } else {
    scalar_j_base = b.scope->create_ir_value(
        IVO_PARAM, make_ir_value_type_integer(64), {}, {}, "j");
    auto vscale = params.at("vscale");
    auto two = b.build_int_constant(vscale->type->elem_width, 2);
    half_vscale = b.build_idiv(vscale, two);
  }
  auto hi_j_base = b.build_iadd(scalar_j_base, half_vscale);
  auto pair_stride =
      b.build_int_constant(scalar_j_base->type->elem_width, n / 2);
  auto lo_pair_offset = b.build_imul(pair_stride, scalar_j_base);
  auto hi_pair_offset = b.build_imul(pair_stride, hi_j_base);

  std::vector<ir_value> lo(structured_width);
  std::vector<ir_value> hi(structured_width);
  for (int pair = 0; pair < structured_width; ++pair) {
    lo[pair] = b.build_zip1(store_vals[2 * pair], store_vals[2 * pair + 1],
                            pair_type, 32);
    hi[pair] = b.build_zip2(store_vals[2 * pair], store_vals[2 * pair + 1],
                            pair_type, 32);
  }

  if (pair_type->nelems.is_sve()) {
    auto pred_lo = b.build_whilelt(scalar_j_base, howmany, 64);
    auto pred_hi = b.build_whilelt(hi_j_base, howmany, 64);
    b.build_sve_structure_store(lo, pred_lo, params.at("Y"), lo_pair_offset);
    b.build_sve_structure_store(hi, pred_hi, params.at("Y"), hi_pair_offset);
    return;
  }
  ASSERT(!t.has_sve);
  b.build_structure_store(lo, params.at("Y"), lo_pair_offset);
  b.build_structure_store(hi, params.at("Y"), hi_pair_offset);
}

static void emit_output_values_structure_store(
    ir_builder &b, const target_t &t,
    const std::map<std::string, ir_value> &params, int64_t n,
    const io_ptr_t &iop, const std::vector<int64_t> &out_perm,
    plfft_direction_t dir, std::vector<ir_value> &out_vals) {
  const int64_t out_size = get_out_size_elems(iop);
  ASSERT(out_size == n);
  std::vector<ir_value> store_vals(n);
  for (int64_t idx = 0; idx < out_size; ++idx) {
    const int64_t y_index =
        dir == PLFFT_FORWARD ? out_perm[idx] : (n - out_perm[idx]) % n;
    ASSERT(y_index >= 0 && y_index < n);
    store_vals[y_index] = std::move(out_vals[idx]);
  }
  for (const auto &value : store_vals) {
    ASSERT(value);
  }

  if (n == 6 || n == 8) {
    std::optional<int64_t> known_half_vscale;
    if (t.known_vector_length_bytes) {
      known_half_vscale =
          *t.known_vector_length_bytes /
          (2 * params.at("Y")->type->inner_type->count_contig_bytes());
    }
    emit_output_values_structure_store_paired(b, t, params, n, store_vals,
                                              known_half_vscale);
    return;
  }

  const auto j_base = params.at("j_base");
  auto row_stride = b.build_int_constant(j_base->type->elem_width, n);
  auto row_offset = b.build_imul(row_stride, j_base);
  if (store_vals[0]->type->nelems.is_sve()) {
    auto pred_structured =
        b.build_whilelt(j_base, params.at("howmany"),
                        effective_elem_bits(*store_vals[0]->type));
    b.build_sve_structure_store(store_vals, pred_structured, params.at("Y"),
                                row_offset);
    return;
  }
  ASSERT(!t.has_sve);
  b.build_structure_store(store_vals, params.at("Y"), row_offset);
}

static ir_builder
print_common_neon(std::list<expr_t> algo, const options_t &opts, int64_t n,
                  const io_ptr_t &iop, const std::vector<int64_t> &in_perm,
                  const std::vector<int64_t> &out_perm, twiddleness twiddle,
                  plfft_direction_t dir, const kernel_types_t &rt,
                  order_kind order, const io_mods_t &mods,
                  const known_layout_t &known_layout, ir_value_scope *fn_scope,
                  std::map<std::string, ir_value> &params,
                  std::map<atom, ir_value> &vals) {
  ir_builder fn_b{fn_scope, opts.target, LAYOUT_LOW};

  bool want_twid = twiddle != twiddleness::none;
  setup_params(params, opts.target, want_twid, rt, mods, known_layout, fn_b);

  if (known_layout.howmany == 1) {
    params["j"] = fn_b.build_int_constant(64, 0);
    auto in_vals = build_input_values(fn_b, opts.target, params, n, iop,
                                      in_perm, rt, order, mods);
    std::vector<ir_value> out_vals(get_out_size_elems(iop));
    for (auto it = algo.cbegin(); it != algo.cend(); ++it) {
      it->print(fn_b, opts.target, params, vals, n, iop, in_vals, out_vals, dir,
                rt, order, mods);
    }
    emit_output_values(fn_b, opts.target, params, n, iop, out_perm, dir, rt,
                       order, mods, out_vals);
    return fn_b;
  }

  const int vector_unroll = 64 / rt.w.bits();
  const bool use_structure_store =
      can_use_structure_store(n, known_layout, rt, mods);

  int unroll = 1;
  if ((known_layout.odist == 1 || use_structure_store) && rt.x == rt.w &&
      rt.w == rt.y && (mods.out != conj_reverse || order == ORDER_AC)) {
    unroll = vector_unroll;
  }

  bool want_premul_twiddles = !opts.target.has_fcma;
  int ofs_mul = want_premul_twiddles ? 2 : 1;
  int j_mul = ofs_mul * (n - 1);
  int w_mul = params.count("W") && order == ORDER_AB
                  ? params.at("W")->type->inner_type->count_contig_bytes()
                  : 0;

  auto howmany = params.at("howmany");
  auto loop_start = fn_b.build_int_constant(64, 0);
  if (unroll > 1) {
    auto init = fn_b.build_int_constant(64, 0);
    auto inc = fn_b.build_int_constant(64, unroll);
    auto limit = fn_b.build_imul(fn_b.build_idiv(howmany, inc), inc);
    std::vector<ir_value> for_loop_params{init, limit, inc};
    auto x_inc = fn_b.build_imul(
        params.at("idist"),
        fn_b.build_int_constant(
            64,
            unroll * params.at("X")->type->inner_type->count_contig_bytes()));
    auto y_inc = fn_b.build_imul(
        params.at("odist"),
        fn_b.build_int_constant(
            64,
            unroll * params.at("Y")->type->inner_type->count_contig_bytes()));
    auto w_inc = params.count("W") && order == ORDER_AB
                     ? fn_b.build_int_constant(64, unroll * j_mul * w_mul)
                     : nullptr;
    for_loop_add_param_inductions(for_loop_params, params, order, fn_b, x_inc,
                                  y_inc, w_inc);

    auto loop1_scope =
        fn_scope->make_child_scope(IVSO_FOR, std::move(for_loop_params));
    ir_builder b1 = fn_b.get_builder_for_scope(loop1_scope);
    auto j_base = b1.build_int_constant(64, 0);
    auto j_step = b1.build_int_constant(64, 1);
    // For fp16 this may lead to an index with a total width > 128 bits, but
    // this is fine since the index is never materialised.
    params["j"] =
        b1.build_index(j_base, j_step, ir_value_num_elems::with_nlanes(unroll));
    params["j_base"] = j_base;
    const bool use_structure_load =
        can_use_structure_load(n, known_layout, rt, mods);
    auto in_vals = use_structure_load
                       ? build_input_values_structure_load(
                             b1, opts.target, params, n, iop, in_perm)
                       : build_input_values(b1, opts.target, params, n, iop,
                                            in_perm, rt, order, mods);
    std::vector<ir_value> out_vals(get_out_size_elems(iop));
    for (auto it = algo.cbegin(); it != algo.cend(); ++it) {
      it->print(b1, opts.target, params, vals, n, iop, in_vals, out_vals, dir,
                rt, order, mods);
    }
    if (use_structure_store) {
      emit_output_values_structure_store(b1, opts.target, params, n, iop,
                                         out_perm, dir, out_vals);
    } else {
      emit_output_values(b1, opts.target, params, n, iop, out_perm, dir, rt,
                         order, mods, out_vals);
    }
    loop_start = limit;
  }
  if (!(unroll > 1 && known_layout.howmany &&
        *known_layout.howmany % unroll == 0)) {
    auto init = loop_start;
    auto limit = howmany;
    auto inc = fn_b.build_int_constant(64, 1);
    std::vector<ir_value> for_loop_params{init, limit, inc};
    auto x_inc = fn_b.build_imul(
        params.at("idist"),
        fn_b.build_int_constant(
            64, params.at("X")->type->inner_type->count_contig_bytes()));
    auto y_inc = fn_b.build_imul(
        params.at("odist"),
        fn_b.build_int_constant(
            64, params.at("Y")->type->inner_type->count_contig_bytes()));
    auto w_inc = params.count("W") && order == ORDER_AB
                     ? fn_b.build_int_constant(64, j_mul * w_mul)
                     : nullptr;
    for_loop_add_param_inductions(for_loop_params, params, order, fn_b, x_inc,
                                  y_inc, w_inc);

    auto loop2_scope =
        fn_scope->make_child_scope(IVSO_FOR, std::move(for_loop_params));
    ir_builder b2 = fn_b.get_builder_for_scope(loop2_scope);
    params["j"] = fn_b.build_int_constant(64, 0);
    auto in_vals = build_input_values(b2, opts.target, params, n, iop, in_perm,
                                      rt, order, mods);
    std::vector<ir_value> out_vals(get_out_size_elems(iop));
    for (auto it = algo.cbegin(); it != algo.cend(); ++it) {
      it->print(b2, opts.target, params, vals, n, iop, in_vals, out_vals, dir,
                rt, order, mods);
    }
    emit_output_values(b2, opts.target, params, n, iop, out_perm, dir, rt,
                       order, mods, out_vals);
  }
  return fn_b;
}

static ir_builder
print_common_sve(std::list<expr_t> algo, const options_t &opts, int64_t n,
                 const io_ptr_t &iop, const std::vector<int64_t> &in_perm,
                 const std::vector<int64_t> &out_perm, twiddleness twiddle,
                 plfft_direction_t dir, const kernel_types_t &rt,
                 order_kind order, const io_mods_t &mods,
                 const known_layout_t &known_layout, ir_value_scope *fn_scope,
                 std::map<std::string, ir_value> &params,
                 std::map<atom, ir_value> &vals) {
  // figure out what a suitable unroll looks like:
  //        data: [re, im,  re,  im,  X,   X,   X,   X  ], inc = 4
  //   pred_full: [1,  1,   1,   1,   0,   0,   0,   0  ]
  //   pred_real: [1,  0,   1,   0,   0,   0,   0,   0  ]
  //   pred_imag: [0,  1,   0,   1,   0,   0,   0,   0  ]
  //   pred_half: [1,  1,   0,   0,   0,   0,   0,   0  ]
  //   half:   j: [j,                 j+2,              ]
  //   float:  j: [j,       j+1,      j+2,      j+3,    ]
  //   double: j: [j,  j,   j+1, j+1, j+2, j+2, j+3, j+3]
  //  double: j1: [0,  1,   0,   1,   0,   1,   0,   1  ]
  ir_builder fn_b{fn_scope, opts.target, LAYOUT_EVEN};
  bool want_twid = twiddle != twiddleness::none;
  setup_params(params, opts.target, want_twid, rt, mods, known_layout, fn_b);

  auto howmany = known_layout.howmany
                     ? fn_b.build_int_constant(64, *known_layout.howmany)
                     : params.at("howmany");
  auto init = fn_b.build_int_constant(64, 0);
  auto limit = howmany;
  auto inc = get_sve_loop_inc(fn_b, rt.w, opts.target.known_vector_length_bytes,
                              opts.want_sme);
  params["vscale"] = inc;

  // Half-precision SVE gathers / scatters use 32-bit offsets
  auto inc_width = opts.target.has_sve && rt.w.bits() < 32 ? 32 : 64;
  auto inc_t = make_ir_value_type_integer(inc_width);

  std::vector<ir_value> for_loop_params{init, limit, inc};
  if (order == ORDER_AB) {
    int j_mul = n - 1;
    int w_mul = params.at("W")->type->inner_type->count_contig_bytes();
    for_loop_add_induction(
        for_loop_params, params.at("W"),
        fn_b.build_imul(inc, fn_b.build_int_constant(64, j_mul * w_mul)));
  }

  auto loop_scope =
      fn_scope->make_child_scope(IVSO_FOR, std::move(for_loop_params));
  ir_builder b = fn_b.get_builder_for_scope(loop_scope);
  auto j_base = loop_scope->create_ir_value(IVO_PARAM, inc_t, {}, {}, "j");
  auto one = b.build_int_constant(j_base->type->elem_width, 1);
  params["j_base"] = j_base;
  // Technically the index mask is the lesser of a mask based on the element
  // width and a mask based on inc_bits, but an if condition works fine too...
  if (rt.w.bits() == 64) {
    params["j"] =
        b.build_index(j_base, one,
                      ir_value_num_elems::with_mask(
                          1, ir_value_num_elems::scale_t::scalable()));
  } else {
    int nlanes = 128 / inc_width;
    params["j"] =
        b.build_index(j_base, one,
                      ir_value_num_elems::with_nlanes(
                          nlanes, ir_value_num_elems::scale_t::scalable()));
  }

  const bool use_structure_load =
      can_use_structure_load(n, known_layout, rt, mods) &&
      !(opts.want_sme && (n == 6 || n == 8));
  const bool use_tile_transpose =
      opts.want_sme && known_layout.istride == 1 && known_layout.idist == n &&
      supports_transposing_load(opts.target, params, iop, rt, mods);
  std::vector<ir_value> in_vals;
  if (use_structure_load) {
    in_vals = build_input_values_structure_load(b, opts.target, params, n, iop,
                                                in_perm);
  } else if (use_tile_transpose) {
    in_vals = build_input_values_transposing_load(
        b, opts.target, params, n, iop, in_perm, rt, order, mods);
  } else {
    in_vals = build_input_values(b, opts.target, params, n, iop, in_perm, rt,
                                 order, mods);
  }

  std::vector<ir_value> out_vals(get_out_size_elems(iop));
  for (auto it = algo.cbegin(); it != algo.cend(); ++it) {
    it->print(b, opts.target, params, vals, n, iop, in_vals, out_vals, dir, rt,
              order, mods);
  }
  if (can_use_structure_store(n, known_layout, rt, mods)) {
    emit_output_values_structure_store(b, opts.target, params, n, iop, out_perm,
                                       dir, out_vals);
  } else {
    emit_output_values(b, opts.target, params, n, iop, out_perm, dir, rt, order,
                       mods, out_vals);
  }
  return fn_b;
}

static ir_value_impl *get_param_or_const(ir_builder b, ir_value_scope *fn_scope,
                                         ir_value_type_ptr ty,
                                         std::optional<int64_t> maybe_const,
                                         int param_num) {
  if (maybe_const) {
    return b.build_int_constant(ty->elem_width, *maybe_const);
  }
  return b.scope->create_ir_value(IVO_PARAM, ty, {}, {(double)param_num}, "");
}

void setup_params(std::map<std::string, ir_value> &params, const target_t &t,
                  bool want_twid, const kernel_types_t &rt,
                  const io_mods_t &mods, const known_layout_t &known_layout,
                  ir_builder &b) {
  auto int_t = make_ir_value_type_integer(64);
  auto xr_t = make_ir_value_type_real(rt.x.bits(), rt.x.is_float());
  auto yr_t = make_ir_value_type_real(rt.y.bits(), rt.y.is_float());
  auto xc_t = make_ir_value_type_complex(rt.x.bits(), rt.x.is_float());
  auto yc_t = make_ir_value_type_complex(rt.y.bits(), rt.y.is_float());
  auto wc_t = make_ir_value_type_complex(rt.w.bits(), rt.w.is_float());
  // Half-precision SVE gathers / scatters use 32-bit offsets
  auto inc_width = t.has_sve && rt.w.bits() < 32 ? 32 : 64;
  auto inc_t = make_ir_value_type_integer(inc_width);

  // Argument Order:
  // | X  (input pointer)
  // | XX (input pointer, halfxx in only)
  // | Y  (output pointer)
  // | YY (output pointer, conjrev out only)
  // | istride
  // | ostride
  // | W  (twiddle factors)
  // | howmany
  // | idist
  // | odist
  // hence if either XX/YY are present, odist will be put on the stack
  int param_num = 0;

  auto in_t = mods.in == im_real ? xr_t : xc_t;
  auto in_ptr_t = make_ir_value_type_pointer(in_t);
  params["X"] = b.scope->create_ir_value(IVO_PARAM, in_ptr_t, {},
                                         {(double)param_num++}, "");
  if (mods.in == im_halflo || mods.in == im_halfhi) {
    params["XX"] = b.scope->create_ir_value(IVO_PARAM, in_ptr_t, {},
                                            {(double)param_num++}, "");
  }

  auto out_t = mods.out == om_real ? yr_t : yc_t;
  auto out_ptr_t = make_ir_value_type_pointer(out_t);
  params["Y"] = b.scope->create_ir_value(IVO_PARAM, out_ptr_t, {},
                                         {(double)param_num++}, "");
  if (mods.out == out_mods::conj_reverse) {
    params["YY"] = b.scope->create_ir_value(IVO_PARAM, out_ptr_t, {},
                                            {(double)param_num++}, "");
  }

  params["istride"] =
      get_param_or_const(b, b.scope, int_t, known_layout.istride, param_num++);
  params["ostride"] =
      get_param_or_const(b, b.scope, int_t, known_layout.ostride, param_num++);

  if (want_twid) {
    auto w_ptr_t = make_ir_value_type_pointer(wc_t);
    params["W"] = b.scope->create_ir_value(IVO_PARAM, w_ptr_t, {},
                                           {(double)param_num++}, "");
  }
  params["howmany"] =
      get_param_or_const(b, b.scope, inc_t, known_layout.howmany, param_num++);
  params["idist"] =
      get_param_or_const(b, b.scope, inc_t, known_layout.idist, param_num++);
  params["odist"] =
      get_param_or_const(b, b.scope, inc_t, known_layout.odist, param_num++);

  int expected_num_params = (mods.out == out_mods::conj_reverse ||
                             mods.in == im_halflo || mods.in == im_halfhi)
                                ? 9
                                : 8;
  // We no longer include twiddle factors when they're not required
  if (!want_twid) {
    expected_num_params--;
  }
  ASSERT(param_num == expected_num_params);
}

static kernel_data print_common(kernel_registry_entry<void> *out,
                                std::list<expr_t> algo, const options_t &opts,
                                int64_t n, const io_ptr_t &iop,
                                const std::vector<int64_t> &in_perm,
                                const std::vector<int64_t> &out_perm,
                                twiddleness twiddle, plfft_direction_t dir,
                                const kernel_types_t &types, order_kind order,
                                std::string fnname, const io_mods_t &mods,
                                const known_layout_t &known_layout) {
  algo = apply_algo_transforms(std::move(algo), n, iop, in_perm, out_perm,
                               twiddle);

  auto in_type = mods.in == in_mods::im_real ? TK_REAL : TK_COMPLEX;
  auto out_type = mods.out == out_mods::om_real ? TK_REAL : TK_COMPLEX;
  auto vars = type_check(algo, iop, in_type, out_type);
  apply_output_modifiers(algo, n, iop, out_perm, dir, mods.out);

  ASSERT(get_in_size_elems(iop) == n);
  ASSERT(get_out_size_elems(iop) == n);

  auto ir_fn = make_ir_value_function(fnname);
  auto fn_scope = ir_fn->make_root_scope();
  std::map<std::string, ir_value> params;
  std::map<atom, ir_value> vals;

  algo_flops flops;
  for (const auto &e : algo) {
    flops += e.flops();
  }

  ir_builder fn_b =
      opts.target.has_sve || opts.want_sme
          ? print_common_sve(algo, opts, n, iop, in_perm, out_perm, twiddle,
                             dir, types, order, mods, known_layout, fn_scope,
                             params, vals)
          : print_common_neon(algo, opts, n, iop, in_perm, out_perm, twiddle,
                              dir, types, order, mods, known_layout, fn_scope,
                              params, vals);

  // Repeatedly apply optimizations until they yield
  // no changes, de-duplicating after each pass
  while (do_opt(fn_b)) {
    do_dedup(fn_scope);
  }

  // Lift up any irvalues that can be hoisted
  do_hoist(fn_scope);

  // Remove any new duplicates introduced by hoisting
  do_dedup(fn_scope);

  std::map<std::string, function_ptr> fns;
  auto sme = sloejit::function_options_t::sme_usage::none;
  if (opts.want_sme) {
    sme = scope_uses_za(fn_scope)
              ? sloejit::function_options_t::sme_usage::sm_both
              : sloejit::function_options_t::sme_usage::sm;
  }
  sloejit::function_options_t fn_opts{
      .keep_frame_pointer = opts.want_map_file,
      .validate = true,
      .sme = sme,
  };
  auto *fn =
      &*fns.emplace(fnname, std::make_unique<function>(
                                fnname, fn_opts, aarch64::get_arch_traits()))
            .first->second;
  auto p = make_ir_printer(opts);
  stack_frame_info frame_info;
  std::vector<rodata_info> data_ofs;
  std::vector<uint8_t> data_bytes;
  (*p)(fns, &frame_info, *fn, data_ofs, data_bytes, {}, *fn_scope);
  return emit_kernel_data(out, fnname, fn, frame_info, std::move(data_bytes),
                          std::move(flops), opts);
}

template<typename Tx, typename Ty, typename Tw>
kernel_data
print_algo(kernel_registry_entry<void> *out, std::list<expr_t> algo, int64_t n,
           const std::string &mid, const io_ptr_t &iop,
           const std::vector<int64_t> &in_perm,
           const std::vector<int64_t> &out_perm, twiddleness twiddle,
           plfft_direction_t dir, order_kind order, std::string fnname,
           const options_t &opts, const known_layout_t &known_layout,
           const io_mods_t &mods) {
  constexpr kernel_types_t types{
      rtype_from_real_type_v<remove_complex_t<Tx>>,
      rtype_from_real_type_v<remove_complex_t<Ty>>,
      rtype_from_real_type_v<remove_complex_t<Tw>>,
  };

  return print_common(out, algo, opts, n, iop, in_perm, out_perm, twiddle, dir,
                      types, order, fnname, mods, known_layout);
}

/* expr member functions for printing a single expression */
// print scalar
void expr::print(ir_builder &b, const target_t &target,
                 const std::map<std::string, ir_value> &params,
                 std::map<atom, ir_value> &vals, int64_t n, const io_ptr_t &iop,
                 const std::vector<ir_value> &in_vals,
                 std::vector<ir_value> &out_vals, plfft_direction_t dir,
                 const kernel_types_t &rt, order_kind order,
                 const io_mods_t &mods) const {
  auto xr_t = make_ir_value_type_real(rt.x.bits(), rt.x.is_float());
  auto yr_t = make_ir_value_type_real(rt.y.bits(), rt.y.is_float());
  auto wr_t = make_ir_value_type_real(rt.w.bits(), rt.w.is_float());
  auto xc_t = make_ir_value_type_complex(rt.x.bits(), rt.x.is_float());
  auto yc_t = make_ir_value_type_complex(rt.y.bits(), rt.y.is_float());
  auto wc_t = make_ir_value_type_complex(rt.w.bits(), rt.w.is_float());
  auto c_ptr_t = make_ir_value_type_pointer(wc_t);
  auto in_t = mods.in == im_real ? xr_t : xc_t;
  auto out_t = mods.out == om_real ? yr_t : yc_t;
  auto in_ptr_t = make_ir_value_type_pointer(in_t);

  ir_value value;
  // Handle twiddle factor multiplication separately.
  if (right.kind == RT_WPTR) {
    ASSERT(op == '*');
    ASSERT(is_local_ptr(iop, left));

    // TODO: since we control the twiddle array, it should be possible
    //       to rearrange the layout to make these contiguous loads based
    //       on the selected kernel. This would help both NEON and SVE!

    // If iterating over the third dimension, we only need to load a segment
    // of W rather than the whole twiddle array.
    ir_value wj_addr = params.at("W");
    bool want_premul_twiddles = !target.has_sve && !target.has_fcma;
    int ofs_mul = want_premul_twiddles ? 2 : 1;
    ir_value w1;
    ir_value w1_idx_for_w2;
    ir_value w2_step;
    int w2_step_int = 1;
    if (order == ORDER_AB) {
      if (target.has_sve) {
        ASSERT(ofs_mul == 1);
        // ORDER_AB interleaved-twiddle path (c2c + SVE Hermitian cases)
        auto w1_ofs = b.build_int_constant(64, (int)right.ival);
        w1_ofs = b.build_imul(params.at("vscale"), w1_ofs);
        auto twiddle_vec_t = make_ir_value_type_vector_for_load(
            target, b.layout, params.at("j")->type->nelems,
            params.at("j")->type->elem_width, params.at("W")->type->inner_type);
        auto inner_type = params.at("W")->type->inner_type;
        if (inner_type->nelems.segment_mask == 0b11 &&
            inner_type->elem_width == 64) {
          w1_ofs = b.build_imul(
              w1_ofs, b.build_int_constant(w1_ofs->type->elem_width, 2));
          w2_step_int = 2;
        }
        w1_idx_for_w2 = w1_ofs;
        w2_step = b.build_int_constant(w1_ofs->type->elem_width, w2_step_int);
        w1 = b.build_load(wj_addr, std::move(w1_ofs), twiddle_vec_t);
      } else {
        auto j_mul = b.build_int_constant(params.at("j")->type->elem_width,
                                          ofs_mul * (int)(n - 1));
        // ab, cannot use contiguous loads with interleaved twiddles due
        // to need to allow offsets in wrap kernels.
        auto w1_ofs = b.build_int_constant(params.at("j")->type->elem_width,
                                           ofs_mul * (int)right.ival);
        auto j_ofs = b.build_imul(params.at("j"), j_mul);
        auto w_idx = b.build_iadd(w1_ofs, j_ofs);
        w1_idx_for_w2 = w_idx;
        w1 = b.build_load_or_gather(wj_addr, std::move(w_idx));
        w2_step = b.build_int_constant(w_idx->type->elem_width, w2_step_int);
      }
    } else {
      // ac, load is a constant offset into the twiddle array
      auto w1_ofs = b.build_int_constant(wj_addr->type->elem_width,
                                         ofs_mul * (int)right.ival);
      w1_idx_for_w2 = w1_ofs;
      auto inner_type = wj_addr->type->inner_type;
      auto t = make_ir_value_type_vector_for_load(
          b.target, b.layout, params.at("j")->type->nelems,
          params.at("j")->type->elem_width, inner_type);
      w1 = b.build_load_bcast(wj_addr, std::move(w1_ofs), std::move(t));
      w2_step = b.build_int_constant(w1_ofs->type->elem_width, w2_step_int);
    }

    if (target.has_fcma) {
      auto l = vals.at(left);
      value = b.build_cmul(l, w1);
    } else {
      auto l1 = b.build_splat_real(vals.at(left));
      auto l2 = b.build_splat_imag(vals.at(left));

      auto w2_addr = b.build_iadd(w1_idx_for_w2, w2_step);
      ir_value w2 = b.build_load_or_gather(wj_addr, std::move(w2_addr));

      auto x = b.build_mul(std::move(l1), std::move(w1));
      auto y = b.build_mul(std::move(l2), std::move(w2));
      value = b.build_add(std::move(x), std::move(y));
    }
  } else {
    std::vector<const atom *> elems{&left};
    if (right.kind) {
      elems.push_back(&right);
    }
    std::vector<ir_value> values;
    for (auto *elem : elems) {
      bool take_real_part =
          lhs.self_type.kind == TK_REAL && elem->self_type.kind == TK_COMPLEX;
      bool take_imag_part =
          lhs.self_type.kind == TK_IMAG && elem->self_type.kind == TK_COMPLEX;
      bool needs_promote = lhs.self_type.kind == TK_COMPLEX &&
                           elem->self_type.kind != TK_COMPLEX &&
                           (op == '+' || op == '-');
      bool elem_needs_mul_i =
          lhs.self_type.kind == TK_COMPLEX && elem->self_type.kind == TK_IMAG;
      ASSERT((take_real_part + take_imag_part + needs_promote) <= 1);

      ir_value v;
      if (elem->kind == RT_PTR) {
        if (is_in_ptr(iop, *elem)) {
          v = in_vals.at(get_in_ofs(iop, *elem));
        } else {
          ASSERT(is_local_ptr(iop, *elem));
          v = vals.at(*elem);
        }
        if (take_real_part) {
          v = op_type.kind == TK_COMPLEX ? b.build_splat_real(v)
                                         : b.build_take_real(v);
        }
        if (take_imag_part) {
          v = op_type.kind == TK_COMPLEX ? b.build_splat_imag(v)
                                         : b.build_take_imag(v);
        }
        if (needs_promote) {
          v = b.build_make_complex(std::move(v));
        }
        if ((op == '+' || op == '-') && elem_needs_mul_i) {
          v = b.build_make_imag(v);
        }
      } else {
        ASSERT(elem->kind == RT_REAL_CONST || elem->kind == RT_IMAG_CONST);
        if (op == '*' && op_type.kind == TK_COMPLEX) {
          if (elem->kind == RT_REAL_CONST) {
            v = b.build_complex_constant(wr_t, elem->rval, elem->rval);
          } else if (target.has_fcma) {
            v = b.build_complex_constant(wr_t, 0, elem->rval);
          } else {
            v = b.build_complex_constant(wr_t, -elem->rval, elem->rval);
          }
        } else if ((op == '+' || op == '-') && op_type.kind == TK_COMPLEX) {
          v = b.build_complex_constant(wr_t, elem->rval, 0);
        } else {
          v = b.build_real_constant(wr_t, elem->rval);
        }
      }
      values.push_back(std::move(v));
    }

    ASSERT(values.size() == 1 || values.size() == 2);

    bool needs_mul_i = op == '*' && left.self_type.kind == TK_COMPLEX &&
                       right.self_type.kind == TK_IMAG;
    if (!target.has_fcma && needs_mul_i) {
      values[0] = b.build_rev_pairs(values[0]);
    }

    value = std::move(values[0]);
    if (values.size() == 2) {
      if (target.has_fcma && needs_mul_i) {
        value = b.build_cmul(std::move(value), std::move(values[1]));
      } else {
        value = b.build_binop(std::move(value), op, std::move(values[1]));
      }
    }

    bool need_neg =
        op == '*' && op_type.kind == TK_IMAG && lhs.self_type.kind == TK_REAL;
    if (need_neg) {
      value = b.build_neg(std::move(value));
    }

    bool need_promote =
        op_type.kind == TK_REAL && lhs.self_type.kind == TK_COMPLEX;
    if (need_promote) {
      value = b.build_make_complex(value);
    }

    bool need_take_real =
        op_type.kind == TK_COMPLEX && lhs.self_type.kind == TK_REAL;
    if (need_take_real) {
      value = b.build_take_real(value);
    }

    bool need_take_imag =
        op_type.kind == TK_COMPLEX && lhs.self_type.kind == TK_IMAG;
    if (need_take_imag) {
      value = b.build_take_imag(value);
    }
  }

  if (is_out_ptr(iop, lhs)) {
    out_vals.at(get_out_ofs(iop, lhs)) = std::move(value);
  } else {
    ASSERT(is_local_ptr(iop, lhs));
    vals[lhs] = std::move(value);
  }
}

#define PRINT_ALGO(Tx, Ty, Tw)                                                 \
  template kernel_data print_algo<Tx, Ty, Tw>(                                 \
      kernel_registry_entry<void> * out, std::list<expr_t> algo, int64_t n,    \
      const std::string &mid, const io_ptr_t &iop,                             \
      const std::vector<int64_t> &in_perm,                                     \
      const std::vector<int64_t> &out_perm, twiddleness twiddle,               \
      plfft_direction_t dir, order_kind order, std::string fnname,             \
      const options_t &opts, const known_layout_t &known_layout,               \
      const io_mods_t &mods);

PRINT_ALGO(half, std::complex<half>, std::complex<half>)
PRINT_ALGO(std::complex<half>, half, std::complex<half>)
PRINT_ALGO(std::complex<half>, std::complex<half>, std::complex<half>)
PRINT_ALGO(half, std::complex<half>, std::complex<float>)
PRINT_ALGO(std::complex<half>, half, std::complex<float>)
PRINT_ALGO(std::complex<half>, std::complex<half>, std::complex<float>)
PRINT_ALGO(float, std::complex<float>, std::complex<float>)
PRINT_ALGO(std::complex<float>, float, std::complex<float>)
PRINT_ALGO(std::complex<float>, std::complex<float>, std::complex<float>)
PRINT_ALGO(double, std::complex<double>, std::complex<double>)
PRINT_ALGO(std::complex<double>, double, std::complex<double>)
PRINT_ALGO(std::complex<double>, std::complex<double>, std::complex<double>)
PRINT_ALGO(int8_t, std::complex<int8_t>, std::complex<int8_t>)
PRINT_ALGO(std::complex<int8_t>, int8_t, std::complex<int8_t>)
PRINT_ALGO(std::complex<int8_t>, std::complex<int8_t>, std::complex<int8_t>)
PRINT_ALGO(int16_t, std::complex<int16_t>, std::complex<int16_t>)
PRINT_ALGO(std::complex<int16_t>, int16_t, std::complex<int16_t>)
PRINT_ALGO(std::complex<int16_t>, std::complex<int16_t>, std::complex<int16_t>)

#undef PRINT_ALGO

} // end namespace plfft::wfta
