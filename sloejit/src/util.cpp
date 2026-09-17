/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sloejit/util.hpp"
#include "sloejit_assert.hpp"
#include <cstddef>

#if defined(_WIN32)

#include <windows.h>

uint8_t *sloejit::alloc_executable_memory(size_t size) {
	LPVOID ptr = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	sloejit_assert(ptr);
	return (uint8_t *) ptr;
}

void sloejit::dealloc_executable_memory(void *addr, size_t size) {
	(void) (size); // unused
	bool err = VirtualFree((LPVOID) addr, 0, MEM_RELEASE);
	sloejit_assert(err);
}

#elif defined(BAREMETAL)

extern uint8_t jit_host;
uint8_t *start = &jit_host;
size_t len = 4 * BM_HOST_INS;

uint8_t *sloejit::alloc_executable_memory(size_t size) {
	constexpr int align = 8;
	size_t new_len = len - size; // bump downwards
	new_len &= ~(align - 1); // round down the alignment
	sloejit_assert(new_len < len); // if new_len is >= old len then we've underflowed
	len = new_len;
	return &start[len];
}

void sloejit::dealloc_executable_memory(void *addr, size_t size) {
	// simply reset len to original value
	len = 4 * BM_HOST_INS;
}

#else

#include <sys/mman.h>

// MAP_JIT not defined in linux, so define it as empty in that case
#ifndef MAP_JIT
#define MAP_JIT 0x0
#endif

uint8_t *sloejit::alloc_executable_memory(size_t size) {
	void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_JIT | MAP_PRIVATE | MAP_ANON, -1, 0);
	sloejit_assert(ptr != MAP_FAILED);
	return (uint8_t *) ptr;
}

void sloejit::dealloc_executable_memory(void *addr, size_t size) {
	int err = munmap(addr, size);
	sloejit_assert(err == 0);
}

#endif
