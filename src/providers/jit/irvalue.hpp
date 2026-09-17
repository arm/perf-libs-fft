/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft_assert.hpp"
#include "target.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace plfft::wfta {

enum register_layout {
  LAYOUT_LOW,
  LAYOUT_EVEN,
};

class ir_value_scope;

static inline bool is_pow2(uint64_t x) {
  return x != 0 && (x & (x - 1)) == 0;
}

static inline unsigned log2_pow2(unsigned x) {
  ASSERT(is_pow2(x));
  return __builtin_ctz(x);
}

static inline bool is_contiguous_mask(uint64_t x) {
  if (x == 0) {
    return false;
  }
  return ((x + 1) & x) == 0;
}

static inline bool is_even_mask(uint64_t x) {
  return x != 0 && ((x >> 1) & x) == 0;
}

/** Represents the different kinds of values.
 *  Note that complex numbers are represented as a pair of floating-point or
 *  a pair of fixed-point elements and are therefore not represented here.
 */
enum ir_value_basic_kind {
  IVK_INTEGER,
  IVK_PREDICATE,
  IVK_FLOAT,
  IVK_FIXED,
  IVK_POINTER,
  IVK_ZA_TILE
};

bool is_float_or_fixed_kind(ir_value_basic_kind kind);

/// Represents the number of elements/bits in a (possibly SVE) vector.
struct ir_value_num_elems {
  struct scale_t {
    /// Number of 128-bit segments in this vector. For Neon this must be 1.
    int fixed;
    /// True if the number of 128-bit segments is unknown until runtime.
    bool vla;

    static scale_t fixed_segments(int segments) {
      ASSERT(segments >= 1);
      return {segments, false};
    }

    static scale_t scalable() {
      return {1, true};
    }

    bool operator==(const scale_t &other) const {
      return fixed == other.fixed && vla == other.vla;
    }
  };

  /// A mask of elements set within a 128-bit segment. The mask may be
  /// replicated across multiple segments based on `scale`.
  uint64_t segment_mask = 0;

  /// Indicates how many 128-bit segments the mask is replicated across.
  scale_t scale = scale_t::fixed_segments(1);

private:
  ir_value_num_elems(uint64_t segment_mask, scale_t scale)
    : segment_mask(segment_mask), scale(scale) {}

public:
  ir_value_num_elems() = default;
  ir_value_num_elems(const ir_value_num_elems &) = default;
  ir_value_num_elems(ir_value_num_elems &&) = default;
  ir_value_num_elems &operator=(const ir_value_num_elems &) = default;
  ir_value_num_elems &operator=(ir_value_num_elems &&) = default;

  static ir_value_num_elems one() {
    return {0b1, scale_t::fixed_segments(1)};
  }

  static ir_value_num_elems two() {
    return {0b11, scale_t::fixed_segments(1)};
  }

  /// Create a contiguous lane mask with an explicit lane count.
  static ir_value_num_elems
  with_nlanes(int lanes, scale_t scale = scale_t::fixed_segments(1)) {
    ASSERT(lanes > 0 && lanes <= 16);
    ASSERT(scale.fixed >= 1);
    uint64_t mask = (1 << lanes) - 1;
    return {mask, scale};
  }

  static ir_value_num_elems
  with_mask(uint64_t mask, scale_t scale = scale_t::fixed_segments(1)) {
    ASSERT(scale.fixed >= 1);
    return {mask, scale};
  }

  bool is_one() const {
    return !is_sve() && segment_mask == 0b1;
  }

  bool is_two() const {
    return !is_sve() && segment_mask == 0b11;
  }

  int count_segment_contig() const {
    ASSERT(is_contig());
    return __builtin_popcountll(segment_mask);
  }

  int count_contig() const {
    ASSERT(is_contig());
    if (is_vla()) {
      return count_segment_contig();
    }
    return count_segment_contig() * scale.fixed;
  }

  bool is_contig() const {
    return is_contiguous_mask(segment_mask);
  }

  bool is_even() const {
    for (uint64_t i = segment_mask; i != 0; i >>= 2) {
      uint64_t live = i & 0b11;
      if (live != 0 && live != 0b11) {
        return false;
      }
    }
    return true;
  }

  static ir_value_num_elems concat(ir_value_num_elems a, ir_value_num_elems b) {
    ASSERT(is_contiguous_mask(a.segment_mask));
    ASSERT(is_contiguous_mask(b.segment_mask));
    ASSERT(!a.is_sve());
    ASSERT(!b.is_sve());
    int nelems = __builtin_popcountll(a.segment_mask) +
                 __builtin_popcountll(b.segment_mask);
    ASSERT(nelems > 0 && nelems <= 16);
    uint64_t mask = (1 << nelems) - 1;
    return {mask, scale_t::fixed_segments(1)};
  }

  ir_value_num_elems max(ir_value_num_elems b) const {
    // One segment mask must be entirely contained by the other, a partial
    // overlap doesn't really make sense.
    uint64_t mask_and = segment_mask & b.segment_mask;
    uint64_t mask_orr = segment_mask | b.segment_mask;
    ASSERT(mask_and == segment_mask || mask_and == b.segment_mask);
    scale_t new_scale;
    if (scale == b.scale) {
      new_scale = scale;
    } else if (scale.vla || b.scale.vla) {
      new_scale = scale_t::scalable();
    } else {
      new_scale = scale_t::fixed_segments(std::max(scale.fixed, b.scale.fixed));
    }
    return {mask_orr, new_scale};
  }

  bool is_vla() const {
    return scale.vla;
  }

  bool is_sve() const {
    // Treat any multi-segment or VLA vector as SVE-capable.
    return scale.vla || scale.fixed > 1;
  }

  int scaled_segments() const {
    return scale.fixed;
  }

  bool operator==(ir_value_num_elems b) const {
    return std::tie(segment_mask, scale) == std::tie(b.segment_mask, b.scale);
  }

  bool operator!=(const ir_value_num_elems &other) const {
    return !(*this == other);
  }
};

struct ir_value_type;
typedef std::shared_ptr<ir_value_type> ir_value_type_ptr;

/** Represents the type of an IR value. This is simply the value's kind,
 *  plus vector / pointee (if IVK_POINTER) details.
 */
struct ir_value_type {
  /// The kind of this type.
  ir_value_basic_kind kind;

  /// The element width, s.t. the mask is of size (128 / elem_width) bits.
  int elem_width;

  /// The number of elements in this vector type.
  /// If scalar, this is {segment_mask=1, scale=fixed_segments(1)}.
  ir_value_num_elems nelems;

  /// The pointee type, if this is a pointer.
  ir_value_type_ptr inner_type;

  ir_value_type(ir_value_basic_kind kind, int width, ir_value_num_elems nelems,
                ir_value_type_ptr inner_type);

  bool operator==(const ir_value_type &right) const;
  bool operator!=(const ir_value_type &right) const;

  int count_contig_bits() const {
    return nelems.count_contig() * elem_width;
  }

  int count_contig_bytes() const {
    return count_contig_bits() / 8;
  }
};

/// Returns the effective element width for predicates/lanes.
inline int effective_elem_bits(const ir_value_type &t) {
  int bits = t.elem_width;
  uint64_t mask = t.nelems.segment_mask;
  if (bits == 32 && mask == 0b1111)
    return 64; // complex<float>
  if (bits == 16 && mask == 0b11111111)
    return 32; // complex<half>
  return bits;
}

std::string type_to_str(const ir_value_type &t);

/// Create a scalar integer type of the specified width.
ir_value_type_ptr make_ir_value_type_integer(int width);

/// Create an SVE vector predicate type.
ir_value_type_ptr make_ir_value_type_predicate(int elem_width);

/// Create a scalar floating-point or fixed-point type of the specified width.
ir_value_type_ptr make_ir_value_type_real(int width, bool is_float);

/** Create a complex floating-point or fixed-point of the specified width.
 *  This is represented as a vector of 2*width floating-point or fixed-point
 *  elements.
 */
ir_value_type_ptr make_ir_value_type_complex(int width, bool is_float);

/** Create a vectorized version of the specified type, with nelems elements.
 *  The input type may itself be a vector, in which case the resulting
 *  width is simply { base.abs*nelems.abs, base.scale || nelems.scale }.
 */
ir_value_type_ptr make_ir_value_type_vector(ir_value_num_elems nelems,
                                            int nelems_elem_width,
                                            ir_value_type_ptr base);

ir_value_type_ptr make_ir_value_type_vector_for_load(const target_t &t,
                                                     register_layout layout,
                                                     ir_value_num_elems nelems,
                                                     int nelems_elem_width,
                                                     ir_value_type_ptr base);

/** Remove the vectorization from a type. This is equivalent to just
 *  setting nelems={1, fixed_segments(1)}, total_width={elem_width, false}.
 */
ir_value_type_ptr make_ir_value_type_novector(ir_value_type_ptr);

/// Create a type that is a pointer to the specified type.
ir_value_type_ptr make_ir_value_type_pointer(ir_value_type_ptr);

/// Select the wider integer type between types s and t.
ir_value_type_ptr select_widest_ir_value_type_integer(ir_value_type_ptr s,
                                                      ir_value_type_ptr t);

struct ir_value_impl;
using ir_value = ir_value_impl *;

/** Represents the operation that is being performed by an expression.
 *  TODO: shuffles should probably be made stronger into what is currently
 *        concat+shuffle, since these are frequently used together and
 *        would allow us to get rid of the SVE-specific TRNn/ZIP1 operations.
 */
enum ir_value_op {
  IVO_PARAM = 1,   ///< For injecting parameters into the final program.
  IVO_CONST_INT,   ///< Value is a constant integer.
  IVO_CONST_FLOAT, ///< Value is a constant floating-point value.
  IVO_CONST_FIXED, ///< Value is a constant fixed-point value.
  IVO_SHUFFLE, ///< Value is a vector shuffle/permute, the literals (of length
               ///< nelems) denote the result position of vector elements,
               ///< replicated across an SVE vector if needed.
  IVO_CONCAT,  ///< Value is a concatenation of two non-SVE vectors.
  IVO_FADD,    ///< Value is a floating-point addition.
  IVO_FSUB,    ///< Value is a floating-point subtraction.
  IVO_FMUL,    ///< Value is a floating-point multiplication.
  IVO_IADD,    ///< Value is an integer addition.
  IVO_ISUB,    ///< Value is an integer subtraction.
  IVO_IMUL,    ///< Value is an integer multiplication.
  IVO_IDIV,    ///< Value is an integer division.
  IVO_IMOD,    ///< Value is an integer modulus.
  IVO_FNEG,    ///< Value is a floating-point negation.
  IVO_GEP,     ///< Value is a GetElementPtr operation (pointer offset: &a[b])
  IVO_CAST,    ///< Upcast/Downcast elements to a new type.
  IVO_GATHER,  ///< Value is a load/gather.
  IVO_LOAD,    ///< Value is a contiguous load where both base and offset are
               ///< scalar.
  IVO_STRUCTURE_LOAD_GROUP,  ///< Structured load group
  IVO_GET_GROUP_OP,          ///< Get an output from a grouped op.
  IVO_STRUCTURE_STORE_GROUP, ///< Structured store group
  IVO_SCATTER,               ///< Value is a store/scatter.
  IVO_STORE, ///< Value is a contiguous store where both base and offset are
             ///< scalar.
  IVO_LOAD_BCAST, ///< Value is a contiguous broadcast load.
  IVO_FCMUL, ///< Value is a floating-point complex multiplication instruction.
             ///< Note this isn't actually a real instruction,
             ///< we emulate this via an FCMLA with a zero accumulator.
  IVO_FCONJ, ///< Value is a floating-point complex conjugation instruction.
             ///< This can be emulated by either a predicated FNEG or a
             ///< double-width unpredicated FNEG.
  IVO_INDEX, ///< Value is an SVE index instruction.
  IVO_PTRUE, ///< Value is an SVE predicate set to all-true.
  IVO_WHILELT,  ///< Value is an SVE predicate from whilelt (start < end).
  IVO_EQ_SEL,   ///< Value is a conditional select on equality (a == b ? c : d).
  IVO_MIN,      ///< Value is the minimum of its inputs.
  IVO_SVE_TRN1, ///< Value is an SVE TRN1 instruction.
  IVO_SVE_TRN2, ///< Value is an SVE TRN2 instruction.
  IVO_UZP1,     ///< Value is a UZP1 instruction.
  IVO_UZP2,     ///< Value is a UZP2 instruction.
  IVO_ZIP1,     ///< Value is a ZIP1 instruction.
  IVO_ZIP2,     ///< Value is a ZIP2 instruction.
  IVO_SVE_CNTH, ///< Value is the number of halfwords in an SVE vector.
  IVO_SVE_CNTW, ///< Value is the number of words in an SVE vector.
  IVO_SVE_CNTD, ///< Value is the number of doubles in an SVE vector.
  IVO_REINTERPRET, ///< Value is a type reinterpretation (bitcast).
  IVO_SQNEG,       ///< Value is a signed saturating negate.
  IVO_SQADD,       ///< Value is a signed saturating add.
  IVO_SQSUB,       ///< Value is a signed saturating subtract.
  IVO_SQMUL,       ///< Value is a signed saturating multiply.
  IVO_SQCMUL,      ///< Value is a signed saturating complex multiply.
  IVO_SQCONJ, ///< Value is a signed saturating complex conjugation. This can be
              ///< emulated by a predicated SQNEG.
  IVO_SRSHR,  ///< Value is a signed, rounding right shift.
  IVO_ZA_ZERO,        ///< Value zeros ZA tile state.
  IVO_ZA_SLICE_WRITE, ///< Value writes a vector slice into ZA.
  IVO_ZA_SLICE_READ,  ///< Value reads a vector slice from ZA using pred_full.
};

// Helpers to match either floating-point or fixed-point ops.
bool is_neg_op(ir_value_op v);
bool is_add_op(ir_value_op v);
bool is_sub_op(ir_value_op v);
bool is_mul_op(ir_value_op v);
bool is_cmul_op(ir_value_op v);
bool is_conj_op(ir_value_op v);
bool is_generic_load(ir_value_op v);

enum ir_value_use_kind {
  IVU_DEP,   ///< Use forms a normal dependency chain.
  IVU_SCOPE, ///< The value is used by a scope control flow.
  IVU_KEEP,  ///< Force the value to be preserved via an unspecified dependency
             ///< chain.
  IVU_MEM,   ///< The value is "used" by virtue of storing to memory.
};

struct ir_value_use_info {
  ir_value_use_kind kind; ///< The kind of use this is.
  ir_value v; ///< The ir_value that consumes this value, if kind is IVU_DEP.
  ir_value_scope *scope; ///< The ir_value_scope that consumes this value, if
                         ///< kind is IVU_SCOPE.

private:
  ir_value_use_info(decltype(kind) kind, decltype(v) v, decltype(scope) scope)
    : kind(kind), v(v), scope(scope) {}

public:
  static inline ir_value_use_info make_dep(ir_value v) {
    return {IVU_DEP, v, nullptr};
  }

  static inline ir_value_use_info make_scope(ir_value_scope *scope) {
    return {IVU_SCOPE, nullptr, scope};
  }

  static inline ir_value_use_info make_keep() {
    return {IVU_KEEP, nullptr, nullptr};
  }

  static inline ir_value_use_info make_mem() {
    return {IVU_MEM, nullptr, nullptr};
  }
};

/// Represents a single IR SSA value and the operation that produces it.
struct ir_value_impl {
  /// The scope in which this value was declared.
  ir_value_scope *scope;

  /// The operation done to yield this value.
  ir_value_op op;

  /// The type of this value.
  ir_value_type_ptr type;

  /// The dependencies of this value (order is important here).
  std::vector<ir_value> deps;

  /// The set of other ir_values that make use of this ir_value.
  std::vector<ir_value_use_info> uses;

  /// Any literals used by the operation (these are possibly actually integers
  /// when appropriate).
  std::vector<double> literals;

  /// for IVO_UNDEF values, the string to be printed.
  std::string str;

  /// The unique ID for this value.
  int id;

  //@{
  /** These are intrusive values used for sorting operations
   *  topographically prior to sorting.
   */
  int strata_min = 0;
  int strata_max = 0;
  //@}

  /** Keep track of whether we have already emitted something for this
   *  value. This allows us to emit instructions that cover multiple
   *  ir_values simultaneously (e.g. st1 for shuffle+store).
   *  If emitted == uses.size(), and we aren't a shuffle, nothing to do.
   */
  unsigned emitted = 0;

  ir_value_impl(ir_value_scope *scope, ir_value_op op, ir_value_type_ptr type,
                std::vector<ir_value> deps, std::vector<double> literals,
                std::string str, int id);

  std::string dump(int depth);
  std::string dump_uses();

  void add_use(ir_value_use_info new_info);
  bool has_use(ir_value v) const;
  bool has_scope_use(const ir_value_scope *scope) const;
  bool has_mem_use() const;
  bool has_keep_use() const;
  void erase_use(ir_value v, bool allow_not_present = false);
};

} // namespace plfft::wfta
