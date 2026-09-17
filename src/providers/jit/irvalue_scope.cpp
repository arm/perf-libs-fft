/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "irvalue_scope.hpp"

#include "irvalue_scope_iterator.hpp"
#include "plfft_assert.hpp"

#include <sstream>

namespace plfft::wfta {

ir_value_scope::ir_value_scope(ir_value_function *fn)
  : fn(fn), scope_op(IVSO_NORMAL), parent(nullptr) {}

ir_value_scope::ir_value_scope(ir_value_scope *parent, ir_value_scope_op op,
                               std::vector<ir_value> op_deps)
  : fn(parent->fn), scope_op(op), scope_op_deps(std::move(op_deps)),
    parent(std::move(parent)) {
  for (auto &v : this->scope_op_deps) {
    v->add_use(ir_value_use_info::make_scope(this));
  }
}

ir_value ir_value_scope::create_ir_value(ir_value_op op, ir_value_type_ptr type,
                                         std::vector<ir_value> deps,
                                         std::vector<double> literals,
                                         std::string str) {
  auto id = next_id();
  std::unique_ptr<ir_value_impl> ret_owned{
      new ir_value_impl(this, op, type, std::move(deps), std::move(literals),
                        std::move(str), id)};
  auto *ret_unonwed = ret_owned.get();
  for (auto dep : ret_unonwed->deps) {
    dep->add_use(ir_value_use_info::make_dep(ret_unonwed));
  }
  values.emplace(id, std::move(ret_owned));
  return ret_unonwed;
}

std::unique_ptr<ir_value_impl> ir_value_scope::erase_ir_value(int id) {
  ASSERT(attached_iterators.empty());
  auto it = values.find(id);
  ASSERT(it != values.end());
  auto val = std::move(it->second);
  values.erase(it);
  val->scope = nullptr;
  return val;
}

void ir_value_scope::insert_ir_value(std::unique_ptr<ir_value_impl> val) {
  ASSERT(attached_iterators.empty());
  auto it = values.find(val->id);
  ASSERT(it == values.end());
  val->scope = this;
  values.emplace(val->id, std::move(val));
}

ir_value_scope *
ir_value_scope::make_child_scope(ir_value_scope_op op,
                                 std::vector<ir_value> op_deps) {
  std::unique_ptr<ir_value_scope> ret_owned{
      new ir_value_scope(this, op, std::move(op_deps))};
  auto *ret_unowned = ret_owned.get();
  children.emplace_back(std::move(ret_owned));
  return ret_unowned;
}

ir_value_scope_any_order_iterator ir_value_scope::begin_any_order() {
  std::vector<ir_value> unowned_value_set;
  for (auto &p : values) {
    unowned_value_set.emplace_back(p.second.get());
  }
  return {this, unowned_value_set};
}

ir_value_scope_any_order_iterator ir_value_scope::end_any_order() {
  return {this, {}};
}

ir_value_scope_in_order_iterator ir_value_scope::begin_in_order() {
  std::vector<ir_value> unowned_value_set;
  for (auto &p : values) {
    unowned_value_set.emplace_back(p.second.get());
  }
  return {this, unowned_value_set, false};
}

ir_value_scope_in_order_iterator ir_value_scope::end_in_order() {
  return {this, {}, false};
}

ir_value_scope_in_order_iterator ir_value_scope::begin_in_rev_order() {
  std::vector<ir_value> unowned_value_set;
  for (auto &p : values) {
    unowned_value_set.emplace_back(p.second.get());
  }
  return {this, unowned_value_set, true};
}

ir_value_scope_in_order_iterator ir_value_scope::end_in_rev_order() {
  return {this, {}, true};
}

ir_value_scope_any_order_range ir_value_scope::any_order() {
  return {this};
}

ir_value_scope_in_order_range ir_value_scope::in_order() {
  return {this};
}

ir_value_scope_in_rev_order_range ir_value_scope::in_rev_order() {
  return {this};
}

int ir_value_scope::next_id() {
  return fn->next_id();
}

int ir_value_function::next_id() {
  return id++;
}

ir_value_scope *ir_value_function::make_root_scope() {
  root = ir_value_scope_ptr{new ir_value_scope(this)};
  return root.get();
}

ir_value_function_ptr make_ir_value_function(std::string name) {
  return ir_value_function_ptr{new ir_value_function(std::move(name))};
}

} // end namespace plfft::wfta
