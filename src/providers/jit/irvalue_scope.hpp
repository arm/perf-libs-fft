/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "irvalue.hpp"
#include "target.hpp"

#include <map>
#include <set>

namespace plfft::wfta {

class ir_value_scope;
using ir_value_scope_ptr = std::unique_ptr<ir_value_scope>;
class ir_value_function;
using ir_value_function_ptr = std::unique_ptr<ir_value_function>;

class ir_value_scope_iterator;
class ir_value_scope_in_order_range;
class ir_value_scope_in_rev_order_range;
class ir_value_scope_any_order_range;
class ir_value_scope_in_order_iterator;
class ir_value_scope_any_order_iterator;

/// Represents the kind of scope that we are representing.
enum ir_value_scope_op {
  IVSO_NORMAL = 1, ///< This is just a unguarded block.
  IVSO_FOR,        ///< This is a "for (iv=init; iv<limit; iv+=inc)" block.
  IVSO_IF_EQ,      ///< This is an "if (a == b)" block.
  IVSO_IF_LT,      ///< This is an "if (a < b)" block.
};

/** Represents a C-like scope with a specified prefix and suffix.
 *  Scopes own the IR values within them, as well as any nested scopes.
 */
class ir_value_scope {
public:
  /// The function this scope was created in.
  ir_value_function *fn;

  /// What kind of scope are we.
  ir_value_scope_op scope_op;

  /// Values used in the block-op control (e.g. for loop init/limit/inc).
  std::vector<ir_value> scope_op_deps;

  /** The values contained within this scope.
   *  Note that this is unsorted to allow for values to be added and
   *  removed without needing to adjust the value ordering to preserve
   *  dependency ordering.
   */
  std::map<int, std::unique_ptr<ir_value_impl>> values;

  /// The scope above this one, or nullptr if we are the root scope.
  ir_value_scope *parent;

  /// Any nested/child scopes.
  std::vector<ir_value_scope_ptr> children;

  /** Keep track of any iterators currently going through the value
   *  so that we can assert we aren't invalidating anything when we
   *  erase a value.
   */
  std::set<ir_value_scope_iterator *> attached_iterators;

  ir_value_scope(ir_value_function *fn);
  ir_value_scope(ir_value_scope *parent, ir_value_scope_op op,
                 std::vector<ir_value> op_deps);

  ir_value_scope(const ir_value_scope &) = delete;
  ir_value_scope(ir_value_scope &&) = delete;

  //@{
  /** Iterators and range adaptors for going through the values in a
   *  scope in/out-of a topographically-sorted order.
   */
  ir_value_scope_any_order_iterator begin_any_order();
  ir_value_scope_any_order_iterator end_any_order();
  ir_value_scope_in_order_iterator begin_in_order();
  ir_value_scope_in_order_iterator end_in_order();
  ir_value_scope_in_order_iterator begin_in_rev_order();
  ir_value_scope_in_order_iterator end_in_rev_order();
  ir_value_scope_any_order_range any_order();
  ir_value_scope_in_order_range in_order();
  ir_value_scope_in_rev_order_range in_rev_order();
  //@}

  int next_id();

  /// Create a new IR value in the current scope.
  ir_value create_ir_value(ir_value_op op, ir_value_type_ptr type,
                           std::vector<ir_value> deps,
                           std::vector<double> literals, std::string str);

  /// Erase a value with a particular ID, returning the orphaned node.
  std::unique_ptr<ir_value_impl> erase_ir_value(int id);

  /// Insert an orphaned value into the current scope.
  void insert_ir_value(std::unique_ptr<ir_value_impl> x);

  /// Create a child scope of this scope with the specified prefix/suffix.
  ir_value_scope *make_child_scope(ir_value_scope_op op,
                                   std::vector<ir_value> op_deps);
};

/// Represents an IR function, owning the root IR scope.
class ir_value_function {
  int id = 0;

public:
  std::string name;
  ir_value_scope_ptr root;

  ir_value_function(std::string name) : name(std::move(name)) {}

  ir_value_function(const ir_value_scope &) = delete;
  ir_value_function(ir_value_scope &&) = delete;

  int next_id();

  /// Populate the root scope of this function.
  ir_value_scope *make_root_scope();
};

/** Create an ir_value_function with the specified name.
 *  The user will need to call make_root_scope afterwards to actually
 *  populate it with a definition
 */
ir_value_function_ptr make_ir_value_function(std::string name);

} // namespace plfft::wfta
