/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "irvalue_scope_iterator.hpp"

#include "irvalue_scope.hpp"
#include "plfft_assert.hpp"
#include "plfft_util.hpp"

#include <algorithm>
#include <cstring>

namespace plfft::wfta {

ir_value_scope_iterator::ir_value_scope_iterator(ir_value_scope *scope)
  : scope(scope) {
  ASSERT(scope->attached_iterators.insert(this).second);
}

ir_value_scope_iterator::ir_value_scope_iterator(
    const ir_value_scope_iterator &other)
  : scope(other.scope) {
  ASSERT(scope->attached_iterators.insert(this).second);
}

ir_value_scope_iterator::ir_value_scope_iterator(
    ir_value_scope_iterator &&other)
  : scope(other.scope) {
  ASSERT(scope->attached_iterators.insert(this).second);
}

ir_value_scope_iterator::~ir_value_scope_iterator() {
  ASSERT(scope->attached_iterators.erase(this) == 1);
}

/// Transitively visit all uses of a value, setting strata_min as we go.
static int set_strata_min(ir_value x, int i) {
  if (i <= x->strata_min) {
    return x->strata_min;
  }
  int i_max = x->strata_min = i;
  for (const auto &use_info : x->uses) {
    switch (use_info.kind) {
    case IVU_DEP: {
      auto *u = use_info.v;
      ASSERT(u);
      if (u->scope == x->scope) {
        i_max = std::max(i_max, set_strata_min(u, i + 1));
      }
      break;
    }
    case IVU_SCOPE:
      break; // nothing to do
    case IVU_KEEP:
      break; // nothing to do
    case IVU_MEM:
      break; // nothing to do
    }
  }
  return i_max;
}

/// Transitively visit all deps of a value, setting strata_max as we go.
static void set_strata_max(ir_value x, int i) {
  if (x->strata_max != 0 && i > x->strata_max) {
    return;
  }
  x->strata_max = i;
  for (auto d : x->deps) {
    if (d->scope == x->scope) {
      set_strata_max(d, i - 1);
    }
  }
}

/// Set strata_min for all values in a scope, and return the largest strata_min
/// value.
static int build_strata_forwards(std::vector<ir_value> &values) {
  int strata_max = 0;
  for (unsigned i = 0; i < values.size(); ++i) {
    auto x = values[i];
    bool should_set = true;
    // we only want to set the strata starting from nodes that have no in-scope
    // dependencies. (this includes nodes with no dependencies at all, like
    // literals!)
    for (auto d : x->deps) {
      should_set = should_set && d->scope != x->scope;
    }
    if (should_set) {
      strata_max = std::max(strata_max, set_strata_min(x, 1));
    }
  }
  return strata_max;
}

static int op_priority(ir_value a) {
  if (is_generic_load(a->op) || a->op == IVO_SCATTER || a->op == IVO_STORE ||
      a->op == IVO_STRUCTURE_STORE_GROUP) {
    return 0;
  }
  return 1;
}

static std::vector<ir_value>
choose_next_forwards(const std::vector<ir_value> &ready) {
  std::vector<ir_value> ret;
  int num_gathers = 0;
  for (auto it = ready.rbegin(); it != ready.rend(); ++it) {
    if (is_generic_load((*it)->op) && ++num_gathers > 1)
      continue;
    if (ret.size() > 100)
      continue;
    ret.push_back(*it);
  }
  std::stable_sort(ret.begin(), ret.end(), [](auto a, auto b) {
    return op_priority(a) < op_priority(b);
  });
  return ret;
}

static std::vector<ir_value>
choose_next_backwards(const std::vector<ir_value> &ready) {
  std::vector<ir_value> ret;
  for (auto it = ready.rbegin(); it != ready.rend(); ++it) {
    if (ret.size() > 100)
      continue;
    ret.push_back(*it);
  }
  std::stable_sort(ret.begin(), ret.end(), [](auto a, auto b) {
    return op_priority(a) < op_priority(b);
  });
  return ret;
}

/// Set strata_max for all values in a scope, and sort the values in strata
/// order.
static void build_strata_backwards(std::vector<ir_value> &values,
                                   int strata_max, bool reverse) {
  for (auto i = values.size(); i--;) {
    auto x = values[i];
    if (x->op == IVO_SCATTER || x->op == IVO_STRUCTURE_STORE_GROUP) {
      set_strata_max(x, strata_max);
    } else {
      // if we aren't a scatter but all uses of this value occur in other
      // scopes, we still need to traverse through to set the strata
      // (since otherwise it won't be set by anything!)
      bool all_uses_in_other_scopes = true;
      for (const auto &use_info : x->uses) {
        auto *u = use_info.v;
        switch (use_info.kind) {
        case IVU_DEP:
          all_uses_in_other_scopes =
              all_uses_in_other_scopes && x->scope != u->scope;
          break;
        case IVU_SCOPE:
          break; // nothing to do
        case IVU_KEEP:
          break; // nothing to do
        case IVU_MEM:
          break; // nothing to do
        }
      }
      if (all_uses_in_other_scopes) {
        set_strata_max(x, strata_max);
      }
    }
  }
  std::vector<ir_value> ready_fwd, ready_back;
  for (auto *v : values) {
    if (v->strata_min == 1) {
      ready_fwd.push_back(v);
    }
    // else if (v->strata_max == strata_max) {
    //	ready_back.push_back(v);
    // }
  }
  std::set<ir_value> done;
  std::vector<ir_value> new_values(values.size());
  for (size_t i = 0, j = values.size(); i < j;) {
    ASSERT(!ready_fwd.empty() || !ready_back.empty());
    auto vs = choose_next_forwards(ready_fwd);
    ASSERT(vs.size() >= 0 && vs.size() <= j - i);
    for (auto *v : vs) {
      new_values[i++] = v;
      ASSERT(done.insert(v).second);
      ready_fwd.erase(std::find(ready_fwd.begin(), ready_fwd.end(), v));
      for (const auto &use_info : v->uses) {
        auto *u = use_info.v;
        ASSERT(use_info.kind != IVU_DEP || use_info.v);
        if (!u || u->scope != v->scope || done.count(u)) {
          continue;
        }
        ASSERT(!done.count(u));
        bool all_deps_done = true;
        for (auto *dep : u->deps) {
          if (dep->scope == u->scope && !done.count(dep)) {
            all_deps_done = false;
            break;
          }
        }
        if (all_deps_done) {
          ASSERT(!std::count(ready_fwd.begin(), ready_fwd.end(), u));
          ready_fwd.push_back(u);
        }
      }
    }
    vs = choose_next_backwards(ready_back);
    ASSERT(vs.size() >= 0 && vs.size() <= j - i);
    for (auto *v : vs) {
      new_values[--j] = v;
      ASSERT(done.insert(v).second);
      ready_back.erase(std::find(ready_back.begin(), ready_back.end(), v));
      for (auto d : v->deps) {
        if (d->scope != v->scope || done.count(d)) {
          continue;
        }
        ASSERT(!done.count(d));
        bool all_uses_done = true;
        for (const auto &use_info : d->uses) {
          if (use_info.kind != IVU_DEP) {
            continue;
          }
          auto *u = use_info.v;
          ASSERT(u);
          if (u->scope == d->scope && !done.count(u)) {
            all_uses_done = false;
            break;
          }
        }
        if (all_uses_done) {
          ASSERT(!std::count(ready_back.begin(), ready_back.end(), d));
          ready_back.push_back(d);
        }
      }
    }
  }
  values = std::move(new_values);
  if (reverse) {
    std::reverse(values.begin(), values.end());
  }
  for (auto v : values) {
    for (auto d : v->deps) {
      ASSERT(d->has_use(v));
      ASSERT(d->scope != v->scope || d->strata_min < v->strata_min);
      ASSERT(d->scope != v->scope || d->strata_max < v->strata_max);
    }
    for (const auto &use_info : v->uses) {
      if (use_info.kind != IVU_DEP) {
        continue;
      }
      auto *u = use_info.v;
      ASSERT(u);
      ASSERT(v->scope != u->scope || v->strata_min < u->strata_min);
      ASSERT(v->scope != u->scope || v->strata_max < u->strata_max);
    }
  }
}

static void build_strata(std::vector<ir_value> &values, bool reverse) {
  for (unsigned i = 0; i < values.size(); ++i) {
    values[i]->strata_min = values[i]->strata_max = 0;
  }
  int strata_max = build_strata_forwards(values);
  build_strata_backwards(values, strata_max, reverse);
}

ir_value_scope_in_order_iterator::ir_value_scope_in_order_iterator(
    ir_value_scope *scope, std::vector<ir_value> values, bool reverse)
  : ir_value_scope_iterator(scope), values(std::move(values)) {
  build_strata(this->values, reverse);
}

bool ir_value_scope_in_order_iterator::operator==(
    const ir_value_scope_in_order_iterator &rhs) {
  if (values.empty()) {
    return rhs.values.empty() || rhs.i == rhs.values.size();
  }
  return rhs.values.empty() ? i == values.size() : i == rhs.i;
}

bool ir_value_scope_in_order_iterator::operator!=(
    const ir_value_scope_in_order_iterator &rhs) {
  return !(*this == rhs);
}

ir_value ir_value_scope_in_order_iterator::operator*() {
  return values[i];
}

ir_value_scope_in_order_iterator &
ir_value_scope_in_order_iterator::operator++() {
  ++i;
  return *this;
}

ir_value_scope_any_order_iterator::ir_value_scope_any_order_iterator(
    ir_value_scope *scope, std::vector<ir_value> values)
  : ir_value_scope_iterator(scope), values(std::move(values)) {}

bool ir_value_scope_any_order_iterator::operator==(
    const ir_value_scope_any_order_iterator &rhs) {
  if (values.empty()) {
    return rhs.values.empty() || rhs.i == rhs.values.size();
  }
  return rhs.values.empty() ? i == values.size() : i == rhs.i;
}

bool ir_value_scope_any_order_iterator::operator!=(
    const ir_value_scope_any_order_iterator &rhs) {
  return !(*this == rhs);
}

ir_value ir_value_scope_any_order_iterator::operator*() {
  return values[i];
}

ir_value_scope_any_order_iterator &
ir_value_scope_any_order_iterator::operator++() {
  ++i;
  return *this;
}

ir_value_scope_in_order_range::ir_value_scope_in_order_range(ir_value_scope *s)
  : scope(s) {}

ir_value_scope_in_order_iterator ir_value_scope_in_order_range::begin() {
  return scope->begin_in_order();
}

ir_value_scope_in_order_iterator ir_value_scope_in_order_range::end() {
  return scope->end_in_order();
}

ir_value_scope_in_rev_order_range::ir_value_scope_in_rev_order_range(
    ir_value_scope *s)
  : scope(s) {}

ir_value_scope_in_order_iterator ir_value_scope_in_rev_order_range::begin() {
  return scope->begin_in_rev_order();
}

ir_value_scope_in_order_iterator ir_value_scope_in_rev_order_range::end() {
  return scope->end_in_rev_order();
}

ir_value_scope_any_order_range::ir_value_scope_any_order_range(
    ir_value_scope *s)
  : scope(s) {}

ir_value_scope_any_order_iterator ir_value_scope_any_order_range::begin() {
  return scope->begin_any_order();
}

ir_value_scope_any_order_iterator ir_value_scope_any_order_range::end() {
  return scope->end_any_order();
}

} // end namespace plfft::wfta
