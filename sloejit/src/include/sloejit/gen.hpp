/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <stdint.h>
#include <stdlib.h>

namespace sloejit {

struct generator {
	int64_t *id;
	int64_t *refcount;

	generator() {
		id = (int64_t *) malloc(sizeof(int64_t));
		refcount = (int64_t *) malloc(sizeof(int64_t));
		*id = 0;
		*refcount = 1;
	}

	generator(const generator &g) : id(g.id), refcount(g.refcount) {
		++*refcount;
	}

	~generator() {
		if (--*refcount == 0) {
			free(id);
			free(refcount);
		}
	}

	int64_t operator()() {
		return ++*id;
	}
};

} // namespace sloejit
