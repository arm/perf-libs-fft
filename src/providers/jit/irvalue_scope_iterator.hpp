/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "irvalue.hpp"

#include <cstddef>
#include <iterator>
#include <set>
#include <vector>

namespace plfft::wfta {

/** A base class for IR value iterators. We keep track of the scope
 *  we are currently iterating over so we can inform the scope when
 *  we are beginning/ending iteration. This allows the scope to check
 *  they are not invalidating any active iterators by mistake!
 */
class ir_value_scope_iterator {
  ir_value_scope *scope;

public:
  using difference_type = ptrdiff_t;
  using value_type = ir_value;
  using pointer = value_type *;
  using reference = value_type &;
  using iterator_category = std::input_iterator_tag;

  ir_value_scope_iterator(ir_value_scope *);
  ir_value_scope_iterator(const ir_value_scope_iterator &);
  ir_value_scope_iterator(ir_value_scope_iterator &&);
  ~ir_value_scope_iterator();
};

/** Iterator for traversing a scope's IR values in a
 *  topographically-sorted way. Note that this is quite expensive
 *  as it involves performing the sort on construction, but we
 *  shouldn't need to use this too often outside of printing.
 */
class ir_value_scope_in_order_iterator : public ir_value_scope_iterator {
  /// The sorted values.
  std::vector<ir_value> values;
  /// Where we are through the vector.
  unsigned i = 0;

public:
  using difference_type = ptrdiff_t;
  using value_type = ir_value_impl;
  using pointer = value_type *;
  using reference = value_type &;
  using iterator_category = std::input_iterator_tag;

  ir_value_scope_in_order_iterator(ir_value_scope *scope,
                                   std::vector<ir_value> values, bool reverse);

  bool operator==(const ir_value_scope_in_order_iterator &rhs);
  bool operator!=(const ir_value_scope_in_order_iterator &rhs);

  ir_value operator*();
  ir_value_scope_in_order_iterator &operator++();
};

/// Iterator for traversing a scope's IR values in any order.
class ir_value_scope_any_order_iterator : public ir_value_scope_iterator {
  /// The unsorted values.
  std::vector<ir_value> values;
  /// Where we are through the vector.
  unsigned i = 0;

public:
  using difference_type = ptrdiff_t;
  using value_type = ir_value_impl;
  using pointer = value_type *;
  using reference = value_type &;
  using iterator_category = std::input_iterator_tag;

  ir_value_scope_any_order_iterator(ir_value_scope *scope,
                                    std::vector<ir_value> values);

  bool operator==(const ir_value_scope_any_order_iterator &rhs);
  bool operator!=(const ir_value_scope_any_order_iterator &rhs);

  ir_value operator*();
  ir_value_scope_any_order_iterator &operator++();
};

/** A range adaptor that just forwards {begin,end} to
 *  scope->{begin,end}_in_order()
 */
class ir_value_scope_in_order_range {
  ir_value_scope *scope;

public:
  ir_value_scope_in_order_range(ir_value_scope *s);
  ir_value_scope_in_order_iterator begin();
  ir_value_scope_in_order_iterator end();
};

/** A range adaptor that just forwards {begin,end} to
 *  scope->{begin,end}_in_rev_order()
 */
class ir_value_scope_in_rev_order_range {
  ir_value_scope *scope;

public:
  ir_value_scope_in_rev_order_range(ir_value_scope *s);
  ir_value_scope_in_order_iterator begin();
  ir_value_scope_in_order_iterator end();
};

/** A range adaptor that just forwards {begin,end} to
 *  scope->{begin,end}_any_order()
 */
class ir_value_scope_any_order_range {
  ir_value_scope *scope;

public:
  ir_value_scope_any_order_range(ir_value_scope *s);
  ir_value_scope_any_order_iterator begin();
  ir_value_scope_any_order_iterator end();
};

} // namespace plfft::wfta
