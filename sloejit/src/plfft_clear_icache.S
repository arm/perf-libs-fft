/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

.text
.global plfft_clear_icache
plfft_clear_icache:
	stp x29, x30, [sp, #-16]!
	mov x29, sp
	// https://developer.arm.com/documentation/den0024/a/Caches/Cache-maintenance

	// X0 = base address
	// X1 = length (we assume the length is not 0)

	// Calculate end of the region
	add x1, x1, x0                 // Base Address + Length
	mrs x2, ctr_el0                // Read Cache Type Register

	// Get the minimum instruction cache line (x2 contains ctr_el0)
	and x2, x2, #0xf               // Extract IminLine (log2 of the cache line)
	mov x3, #4                     // IminLine is the number of words (4 bytes)
	lsl x3, x3, x2                 // x3 should contain the cache line
	sub x4, x3, #1                 // Get the mask for the cache line
	bic x4, x0, x4                 // Aligned the base address of the region

.Lclean_instruction_cache:
	ic ivau, x4                    // Clean instruction cache line by VA to PoU
	add x4, x4, x3                 // Next cache line
	cmp x4, x1                     // Is x4 (current cache line) smaller than the end of the region
	b.lt .Lclean_instruction_cache // while (address < end_address)

	dsb ish                        // Ensure completion of the invalidations
	isb                            // Synchronize the fetched instruction stream

	ldp x29, x30, [sp], #16
	ret
#ifdef __ELF__
.size  plfft_clear_icache, .-plfft_clear_icache
#endif
