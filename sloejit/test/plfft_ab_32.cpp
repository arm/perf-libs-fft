/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sloejit/aarch64/aarch64.hpp"
#include "sloejit/gtelf.hpp"

#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <ostream>
#include <sys/stat.h>
#include <sys/types.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace aarch64 = sloejit::aarch64;

static std::vector<float> data_bytes = {
	9.8078528040323043e-01f,  9.8078528040323043e-01f,  9.2387953251128674e-01f,  9.2387953251128674e-01f,
	3.8268343236508967e-01f,  -3.8268343236508967e-01f, 8.3146961230254524e-01f,  8.3146961230254524e-01f,
	5.5557023301960229e-01f,  -5.5557023301960218e-01f, -1.9509032201612861e-01f, -1.9509032201612861e-01f,
	7.0710678118654757e-01f,  7.0710678118654757e-01f,  -7.0710678118654746e-01f, -7.0710678118654746e-01f,
	-1.0000000000000000e+00f, 1.0000000000000000e+00f,  5.5557023301960229e-01f,  5.5557023301960229e-01f,
	8.3146961230254524e-01f,  -8.3146961230254524e-01f, -9.8078528040323043e-01f, -9.8078528040323043e-01f,
	1.9509032201612833e-01f,  -1.9509032201612861e-01f, 3.8268343236508967e-01f,  3.8268343236508967e-01f,
	9.2387953251128674e-01f,  -9.2387953251128685e-01f, -9.2387953251128685e-01f, -9.2387953251128685e-01f,
	-3.8268343236508967e-01f, 3.8268343236508967e-01f,  1.9509032201612833e-01f,  1.9509032201612833e-01f,
	9.8078528040323043e-01f,  -9.8078528040323043e-01f, -5.5557023301960218e-01f, -5.5557023301960218e-01f,
	-8.3146961230254524e-01f, 8.3146961230254524e-01f,  1.0000000000000000e+00f,  -1.0000000000000000e+00f
};

void plfft_ab_32_cccnf_gs() {
	sloejit::function fn{ "plfft_ab_32_cccnf_gs", {}, aarch64::get_arch_traits() };
	auto init_block = fn.make_block("init");
	auto body_block = fn.make_block("body");
	auto fini_block = fn.make_block("fini");
	aarch64::instr_builder init{ init_block };
	aarch64::instr_builder body{ body_block };
	aarch64::instr_builder fini{ fini_block };

	auto X = aarch64::x0;
	auto Y = aarch64::x1;
	auto istride = aarch64::x2;
	auto ostride = aarch64::x3;
	auto howmany = aarch64::x5;
	auto idist = aarch64::x6;
	auto odist = aarch64::x7;

	auto data_x = init.make_adr_b(&fn.rodata);

	init.make_cbz_ri(howmany, fini_block);
	body.make_x_sub_rri(howmany, howmany, 1u);

	auto v12 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(X, 0), aarch64::q_regs); // 3-15  14
	auto v14 = body.make_x_mul_rr(istride, body.make_x_movz_i(16)); // 2-13  10
	auto v17 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v14, X), aarch64::q_regs); // 4-15  14
	auto v18 = body.make_fadd_qq(v12, v17, aarch64::qv_2s); // 5-16  5
	auto v19 = body.make_fsub_qq(v12, v17, aarch64::qv_2s); // 5-16  6
	auto v21 = body.make_x_mul_rr(istride, body.make_x_movz_i(8)); // 2-11  10
	auto v24 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v21, X), aarch64::q_regs); // 4-13  14
	auto v26 = body.make_x_mul_rr(istride, body.make_x_movz_i(24)); // 2-11  10
	auto v29 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v26, X), aarch64::q_regs); // 4-13  14
	auto v30 = body.make_fadd_qq(v24, v29, aarch64::qv_2s); // 5-16  5
	auto v31 = body.make_fsub_qq(v24, v29, aarch64::qv_2s); // 5-14  6
	auto v42 = body.make_x_mul_rr(istride, body.make_x_movz_i(4)); // 2-7  10
	auto v45 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v42, X), aarch64::q_regs); // 4-9  14
	auto v47 = body.make_x_mul_rr(istride, body.make_x_movz_i(20)); // 2-7  10
	auto v50 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v47, X), aarch64::q_regs); // 4-9  14
	auto v51 = body.make_fadd_qq(v45, v50, aarch64::qv_2s); // 5-14  5
	auto v52 = body.make_fsub_qq(v45, v50, aarch64::qv_2s); // 5-10  6
	auto v54 = body.make_x_mul_rr(istride, body.make_x_movz_i(12)); // 2-7  10
	auto v57 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v54, X), aarch64::q_regs); // 4-9  14
	auto v59 = body.make_x_mul_rr(istride, body.make_x_movz_i(28)); // 2-7  10
	auto v62 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v59, X), aarch64::q_regs); // 4-9  14
	auto v63 = body.make_fadd_qq(v57, v62, aarch64::qv_2s); // 5-14  5
	auto v64 = body.make_fsub_qq(v57, v62, aarch64::qv_2s); // 5-10  6
	auto v108 = body.make_x_mul_rr(istride, body.make_x_movz_i(2)); // 2-7  10
	auto v111 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v108, X), aarch64::q_regs); // 4-9  14
	auto v113 = body.make_x_mul_rr(istride, body.make_x_movz_i(18)); // 2-7  10
	auto v116 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v113, X), aarch64::q_regs); // 4-9  14
	auto v117 = body.make_fadd_qq(v111, v116, aarch64::qv_2s); // 5-10  5
	auto v118 = body.make_fsub_qq(v111, v116, aarch64::qv_2s); // 5-11  6
	auto v120 = body.make_x_mul_rr(istride, body.make_x_movz_i(10)); // 2-6  10
	auto v123 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v120, X), aarch64::q_regs); // 4-8  14
	auto v125 = body.make_x_mul_rr(istride, body.make_x_movz_i(26)); // 2-6  10
	auto v128 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v125, X), aarch64::q_regs); // 4-8  14
	auto v129 = body.make_fadd_qq(v123, v128, aarch64::qv_2s); // 5-10  5
	auto v130 = body.make_fsub_qq(v123, v128, aarch64::qv_2s); // 5-9  6
	auto v141 = body.make_x_mul_rr(istride, body.make_x_movz_i(6)); // 2-7  10
	auto v144 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v141, X), aarch64::q_regs); // 4-9  14
	auto v146 = body.make_x_mul_rr(istride, body.make_x_movz_i(22)); // 2-7  10
	auto v149 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v146, X), aarch64::q_regs); // 4-9  14
	auto v150 = body.make_fadd_qq(v144, v149, aarch64::qv_2s); // 5-10  5
	auto v151 = body.make_fsub_qq(v144, v149, aarch64::qv_2s); // 5-11  6
	auto v153 = body.make_x_mul_rr(istride, body.make_x_movz_i(14)); // 2-6  10
	auto v156 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v153, X), aarch64::q_regs); // 4-8  14
	auto v158 = body.make_x_mul_rr(istride, body.make_x_movz_i(30)); // 2-6  10
	auto v161 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v158, X), aarch64::q_regs); // 4-8  14
	auto v162 = body.make_fadd_qq(v156, v161, aarch64::qv_2s); // 5-10  5
	auto v163 = body.make_fsub_qq(v156, v161, aarch64::qv_2s); // 5-9  6
	auto v281 =
	    aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(istride, X), aarch64::q_regs); // 4-9  14
	auto v283 = body.make_x_mul_rr(istride, body.make_x_movz_i(17)); // 2-7  10
	auto v286 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v283, X), aarch64::q_regs); // 4-9  14
	auto v287 = body.make_fadd_qq(v281, v286, aarch64::qv_2s); // 5-10  5
	auto v288 = body.make_fsub_qq(v281, v286, aarch64::qv_2s); // 5-11  6
	auto v290 = body.make_x_mul_rr(istride, body.make_x_movz_i(9)); // 2-6  10
	auto v293 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v290, X), aarch64::q_regs); // 4-8  14
	auto v295 = body.make_x_mul_rr(istride, body.make_x_movz_i(25)); // 2-6  10
	auto v298 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v295, X), aarch64::q_regs); // 4-8  14
	auto v299 = body.make_fadd_qq(v293, v298, aarch64::qv_2s); // 5-10  5
	auto v300 = body.make_fsub_qq(v293, v298, aarch64::qv_2s); // 5-9  6
	auto v311 = body.make_x_mul_rr(istride, body.make_x_movz_i(5)); // 2-2  10
	auto v314 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v311, X), aarch64::q_regs); // 4-4  14
	auto v316 = body.make_x_mul_rr(istride, body.make_x_movz_i(21)); // 2-2  10
	auto v319 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v316, X), aarch64::q_regs); // 4-4  14
	auto v320 = body.make_fadd_qq(v314, v319, aarch64::qv_2s); // 5-9  5
	auto v321 = body.make_fsub_qq(v314, v319, aarch64::qv_2s); // 5-5  6
	auto v323 = body.make_x_mul_rr(istride, body.make_x_movz_i(13)); // 2-2  10
	auto v326 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v323, X), aarch64::q_regs); // 4-4  14
	auto v328 = body.make_x_mul_rr(istride, body.make_x_movz_i(29)); // 2-2  10
	auto v331 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v328, X), aarch64::q_regs); // 4-4  14
	auto v332 = body.make_fadd_qq(v326, v331, aarch64::qv_2s); // 5-9  5
	auto v333 = body.make_fsub_qq(v326, v331, aarch64::qv_2s); // 5-5  6
	auto v377 = body.make_x_mul_rr(istride, body.make_x_movz_i(3)); // 2-7  10
	auto v380 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v377, X), aarch64::q_regs); // 4-9  14
	auto v382 = body.make_x_mul_rr(istride, body.make_x_movz_i(19)); // 2-7  10
	auto v385 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v382, X), aarch64::q_regs); // 4-9  14
	auto v386 = body.make_fadd_qq(v380, v385, aarch64::qv_2s); // 5-10  5
	auto v387 = body.make_fsub_qq(v380, v385, aarch64::qv_2s); // 5-11  6
	auto v389 = body.make_x_mul_rr(istride, body.make_x_movz_i(11)); // 2-6  10
	auto v392 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v389, X), aarch64::q_regs); // 4-8  14
	auto v394 = body.make_x_mul_rr(istride, body.make_x_movz_i(27)); // 2-6  10
	auto v397 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v394, X), aarch64::q_regs); // 4-8  14
	auto v398 = body.make_fadd_qq(v392, v397, aarch64::qv_2s); // 5-10  5
	auto v399 = body.make_fsub_qq(v392, v397, aarch64::qv_2s); // 5-9  6
	auto v410 = body.make_x_mul_rr(istride, body.make_x_movz_i(7)); // 2-2  10
	auto v413 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v410, X), aarch64::q_regs); // 4-4  14
	auto v415 = body.make_x_mul_rr(istride, body.make_x_movz_i(23)); // 2-2  10
	auto v418 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v415, X), aarch64::q_regs); // 4-4  14
	auto v419 = body.make_fadd_qq(v413, v418, aarch64::qv_2s); // 5-9  5
	auto v420 = body.make_fsub_qq(v413, v418, aarch64::qv_2s); // 5-5  6
	auto v422 = body.make_x_mul_rr(istride, body.make_x_movz_i(15)); // 2-2  10
	auto v425 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v422, X), aarch64::q_regs); // 4-4  14
	auto v427 = body.make_x_mul_rr(istride, body.make_x_movz_i(31)); // 2-2  10
	auto v430 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_rr(v427, X), aarch64::q_regs); // 4-4  14
	auto v431 = body.make_fadd_qq(v425, v430, aarch64::qv_2s); // 5-9  5
	auto v432 = body.make_fsub_qq(v425, v430, aarch64::qv_2s); // 5-5  6
	auto v35 = body.make_rev64_q(v31, aarch64::qv_2s); // 6-15  3
	auto v37 = body.make_fadd_qq(v18, v30, aarch64::qv_2s); // 6-17  5
	auto v38 = body.make_fsub_qq(v18, v30, aarch64::qv_2s); // 6-17  6
	auto v65 = body.make_fadd_qq(v51, v63, aarch64::qv_2s); // 6-17  5
	auto v66 = body.make_fsub_qq(v51, v63, aarch64::qv_2s); // 6-15  6
	auto v661 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(data_x, 48), aarch64::q_regs);
	auto v79 = body.make_fmul_qq(v52, v661, aarch64::qv_2s); // 6-11  7
	auto v670 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(data_x, 56), aarch64::q_regs);
	auto v88 = body.make_fmul_qq(v64, v670, aarch64::qv_2s); // 6-11  7
	auto v134 = body.make_rev64_q(v130, aarch64::qv_2s); // 6-10  3
	auto v136 = body.make_fadd_qq(v117, v129, aarch64::qv_2s); // 6-15  5
	auto v137 = body.make_fsub_qq(v117, v129, aarch64::qv_2s); // 6-11  6
	auto v167 = body.make_rev64_q(v163, aarch64::qv_2s); // 6-10  3
	auto v169 = body.make_fadd_qq(v150, v162, aarch64::qv_2s); // 6-15  5
	auto v170 = body.make_fsub_qq(v150, v162, aarch64::qv_2s); // 6-11  6
	auto v304 = body.make_rev64_q(v300, aarch64::qv_2s); // 6-10  3
	auto v306 = body.make_fadd_qq(v287, v299, aarch64::qv_2s); // 6-11  5
	auto v307 = body.make_fsub_qq(v287, v299, aarch64::qv_2s); // 6-12  6
	auto v334 = body.make_fadd_qq(v320, v332, aarch64::qv_2s); // 6-11  5
	auto v335 = body.make_fsub_qq(v320, v332, aarch64::qv_2s); // 6-10  6
	auto v348 = body.make_fmul_qq(v321, v661, aarch64::qv_2s); // 6-6  7
	auto v357 = body.make_fmul_qq(v333, v670, aarch64::qv_2s); // 6-6  7
	auto v403 = body.make_rev64_q(v399, aarch64::qv_2s); // 6-10  3
	auto v405 = body.make_fadd_qq(v386, v398, aarch64::qv_2s); // 6-11  5
	auto v406 = body.make_fsub_qq(v386, v398, aarch64::qv_2s); // 6-12  6
	auto v433 = body.make_fadd_qq(v419, v431, aarch64::qv_2s); // 6-11  5
	auto v434 = body.make_fsub_qq(v419, v431, aarch64::qv_2s); // 6-10  6
	auto v447 = body.make_fmul_qq(v420, v661, aarch64::qv_2s); // 6-6  7
	auto v456 = body.make_fmul_qq(v432, v670, aarch64::qv_2s); // 6-6  7
	auto v674 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(data_x, 64), aarch64::q_regs);
	auto v36 = body.make_fmul_qq(v35, v674, aarch64::qv_2s); // 7-16  7
	auto v70 = body.make_rev64_q(v66, aarch64::qv_2s); // 7-16  3
	auto v72 = body.make_fadd_qq(v37, v65, aarch64::qv_2s); // 7-18  5
	auto v73 = body.make_fsub_qq(v37, v65, aarch64::qv_2s); // 7-18  6
	auto v83 = body.make_rev64_q(v79, aarch64::qv_2s); // 7-12  3
	auto v92 = body.make_rev64_q(v88, aarch64::qv_2s); // 7-12  3
	auto v135 = body.make_fmul_qq(v134, v674, aarch64::qv_2s); // 7-11  7
	auto v168 = body.make_fmul_qq(v167, v674, aarch64::qv_2s); // 7-11  7
	auto v173 = body.make_fadd_qq(v136, v169, aarch64::qv_2s); // 7-18  5
	auto v174 = body.make_fsub_qq(v136, v169, aarch64::qv_2s); // 7-16  6
	auto v218 = body.make_fmul_qq(v137, v661, aarch64::qv_2s); // 7-12  7
	auto v227 = body.make_fmul_qq(v170, v670, aarch64::qv_2s); // 7-12  7
	auto v305 = body.make_fmul_qq(v304, v674, aarch64::qv_2s); // 7-11  7
	auto v339 = body.make_rev64_q(v335, aarch64::qv_2s); // 7-11  3
	auto v341 = body.make_fadd_qq(v306, v334, aarch64::qv_2s); // 7-16  5
	auto v342 = body.make_fsub_qq(v306, v334, aarch64::qv_2s); // 7-12  6
	auto v352 = body.make_rev64_q(v348, aarch64::qv_2s); // 7-7  3
	auto v361 = body.make_rev64_q(v357, aarch64::qv_2s); // 7-7  3
	auto v404 = body.make_fmul_qq(v403, v674, aarch64::qv_2s); // 7-11  7
	auto v438 = body.make_rev64_q(v434, aarch64::qv_2s); // 7-11  3
	auto v440 = body.make_fadd_qq(v405, v433, aarch64::qv_2s); // 7-16  5
	auto v441 = body.make_fsub_qq(v405, v433, aarch64::qv_2s); // 7-12  6
	auto v451 = body.make_rev64_q(v447, aarch64::qv_2s); // 7-7  3
	auto v460 = body.make_rev64_q(v456, aarch64::qv_2s); // 7-7  3
	auto v39 = body.make_fsub_qq(v19, v36, aarch64::qv_2s); // 8-17  6
	auto v40 = body.make_fadd_qq(v19, v36, aarch64::qv_2s); // 8-17  5
	auto v71 = body.make_fmul_qq(v70, v674, aarch64::qv_2s); // 8-17  7
	auto v836 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(data_x, 168), aarch64::q_regs);
	auto v84 = body.make_fmul_qq(v83, v836, aarch64::qv_2s); // 8-13  7
	auto v93 = body.make_fmul_qq(v92, v674, aarch64::qv_2s); // 8-13  7
	auto v138 = body.make_fsub_qq(v118, v135, aarch64::qv_2s); // 8-12  6
	auto v139 = body.make_fadd_qq(v118, v135, aarch64::qv_2s); // 8-12  5
	auto v171 = body.make_fsub_qq(v151, v168, aarch64::qv_2s); // 8-12  6
	auto v172 = body.make_fadd_qq(v151, v168, aarch64::qv_2s); // 8-12  5
	auto v178 = body.make_rev64_q(v174, aarch64::qv_2s); // 8-17  3
	auto v180 = body.make_fadd_qq(v72, v173, aarch64::qv_2s); // 8-19  5
	auto v181 = body.make_fsub_qq(v72, v173, aarch64::qv_2s); // 8-19  6
	auto v222 = body.make_rev64_q(v218, aarch64::qv_2s); // 8-13  3
	auto v231 = body.make_rev64_q(v227, aarch64::qv_2s); // 8-13  3
	auto v308 = body.make_fsub_qq(v288, v305, aarch64::qv_2s); // 8-12  6
	auto v309 = body.make_fadd_qq(v288, v305, aarch64::qv_2s); // 8-12  5
	auto v340 = body.make_fmul_qq(v339, v674, aarch64::qv_2s); // 8-12  7
	auto v353 = body.make_fmul_qq(v352, v836, aarch64::qv_2s); // 8-8  7
	auto v362 = body.make_fmul_qq(v361, v674, aarch64::qv_2s); // 8-8  7
	auto v407 = body.make_fsub_qq(v387, v404, aarch64::qv_2s); // 8-12  6
	auto v408 = body.make_fadd_qq(v387, v404, aarch64::qv_2s); // 8-12  5
	auto v439 = body.make_fmul_qq(v438, v674, aarch64::qv_2s); // 8-12  7
	auto v452 = body.make_fmul_qq(v451, v836, aarch64::qv_2s); // 8-8  7
	auto v461 = body.make_fmul_qq(v460, v674, aarch64::qv_2s); // 8-8  7
	auto v475 = body.make_fadd_qq(v341, v440, aarch64::qv_2s); // 8-19  5
	auto v476 = body.make_fsub_qq(v341, v440, aarch64::qv_2s); // 8-17  6
	auto v662 = body.make_fmul_qq(v342, v661, aarch64::qv_2s); // 8-13  7
	auto v671 = body.make_fmul_qq(v441, v670, aarch64::qv_2s); // 8-13  7
	auto v74 = body.make_fsub_qq(v38, v71, aarch64::qv_2s); // 9-18  6
	auto v75 = body.make_fadd_qq(v38, v71, aarch64::qv_2s); // 9-18  5
	auto v94 = body.make_fadd_qq(v79, v84, aarch64::qv_2s); // 9-14  5
	auto v95 = body.make_fadd_qq(v88, v93, aarch64::qv_2s); // 9-14  5
	auto v179 = body.make_fmul_qq(v178, v674, aarch64::qv_2s); // 9-18  7
	auto v559 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(data_x, 8), aarch64::q_regs);
	auto v187 = body.make_fmul_qq(v138, v559, aarch64::qv_2s); // 9-14  7
	auto v191 = body.make_rev64_q(v138, aarch64::qv_2s); // 9-13  3
	auto v763 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(data_x, 104), aarch64::q_regs);
	auto v196 = body.make_fmul_qq(v171, v763, aarch64::qv_2s); // 9-14  7
	auto v200 = body.make_rev64_q(v171, aarch64::qv_2s); // 9-13  3
	auto v223 = body.make_fmul_qq(v222, v836, aarch64::qv_2s); // 9-14  7
	auto v232 = body.make_fmul_qq(v231, v674, aarch64::qv_2s); // 9-14  7
	auto v249 = body.make_fmul_qq(v139, v763, aarch64::qv_2s); // 9-14  7
	auto v253 = body.make_rev64_q(v139, aarch64::qv_2s); // 9-13  3
	auto v772 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(data_x, 120), aarch64::q_regs);
	auto v258 = body.make_fmul_qq(v172, v772, aarch64::qv_2s); // 9-14  7
	auto v262 = body.make_rev64_q(v172, aarch64::qv_2s); // 9-13  3
	auto v343 = body.make_fsub_qq(v307, v340, aarch64::qv_2s); // 9-13  6
	auto v344 = body.make_fadd_qq(v307, v340, aarch64::qv_2s); // 9-13  5
	auto v363 = body.make_fadd_qq(v348, v353, aarch64::qv_2s); // 9-9  5
	auto v364 = body.make_fadd_qq(v357, v362, aarch64::qv_2s); // 9-9  5
	auto v442 = body.make_fsub_qq(v406, v439, aarch64::qv_2s); // 9-13  6
	auto v443 = body.make_fadd_qq(v406, v439, aarch64::qv_2s); // 9-13  5
	auto v462 = body.make_fadd_qq(v447, v452, aarch64::qv_2s); // 9-9  5
	auto v463 = body.make_fadd_qq(v456, v461, aarch64::qv_2s); // 9-9  5
	auto v480 = body.make_rev64_q(v476, aarch64::qv_2s); // 9-18  3
	auto v482 = body.make_fadd_qq(v180, v475, aarch64::qv_2s); // 9-20  5
	body.make_d_str_rri(aarch64::reg_reinterpret_with_class(v482, aarch64::d_regs), Y, 0); // 10-21  15
	auto v483 = body.make_fsub_qq(v180, v475, aarch64::qv_2s); // 9-20  6
	auto v498 = body.make_x_mul_rr(ostride, body.make_x_movz_i(16)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v483, aarch64::d_regs), v498, Y); // 10-21  15
	auto v666 = body.make_rev64_q(v662, aarch64::qv_2s); // 9-14  3
	auto v675 = body.make_rev64_q(v671, aarch64::qv_2s); // 9-14  3
	auto v96 = body.make_fadd_qq(v94, v95, aarch64::qv_2s); // 10-17  5
	auto v97 = body.make_fsub_qq(v95, v94, aarch64::qv_2s); // 10-15  6
	auto v182 = body.make_fsub_qq(v73, v179, aarch64::qv_2s); // 10-19  6
	auto v183 = body.make_fadd_qq(v73, v179, aarch64::qv_2s); // 10-19  5
	auto v563 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(data_x, 16), aarch64::q_regs);
	auto v192 = body.make_fmul_qq(v191, v563, aarch64::qv_2s); // 10-14  7
	auto v767 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(data_x, 112), aarch64::q_regs);
	auto v201 = body.make_fmul_qq(v200, v767, aarch64::qv_2s); // 10-14  7
	auto v233 = body.make_fadd_qq(v218, v223, aarch64::qv_2s); // 10-15  5
	auto v234 = body.make_fadd_qq(v227, v232, aarch64::qv_2s); // 10-15  5
	auto v254 = body.make_fmul_qq(v253, v767, aarch64::qv_2s); // 10-14  7
	auto v776 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(data_x, 128), aarch64::q_regs);
	auto v263 = body.make_fmul_qq(v262, v776, aarch64::qv_2s); // 10-14  7
	auto v365 = body.make_fadd_qq(v363, v364, aarch64::qv_2s); // 10-12  5
	auto v366 = body.make_fsub_qq(v364, v363, aarch64::qv_2s); // 10-10  6
	auto v464 = body.make_fadd_qq(v462, v463, aarch64::qv_2s); // 10-12  5
	auto v465 = body.make_fsub_qq(v463, v462, aarch64::qv_2s); // 10-10  6
	auto v481 = body.make_fmul_qq(v480, v674, aarch64::qv_2s); // 10-19  7
	auto v560 = body.make_fmul_qq(v343, v559, aarch64::qv_2s); // 10-15  7
	auto v564 = body.make_rev64_q(v343, aarch64::qv_2s); // 10-14  3
	auto v569 = body.make_fmul_qq(v442, v763, aarch64::qv_2s); // 10-15  7
	auto v573 = body.make_rev64_q(v442, aarch64::qv_2s); // 10-14  3
	auto v667 = body.make_fmul_qq(v666, v836, aarch64::qv_2s); // 10-15  7
	auto v676 = body.make_fmul_qq(v675, v674, aarch64::qv_2s); // 10-15  7
	auto v764 = body.make_fmul_qq(v344, v763, aarch64::qv_2s); // 10-15  7
	auto v768 = body.make_rev64_q(v344, aarch64::qv_2s); // 10-14  3
	auto v773 = body.make_fmul_qq(v443, v772, aarch64::qv_2s); // 10-15  7
	auto v777 = body.make_rev64_q(v443, aarch64::qv_2s); // 10-14  3
	auto v101 = body.make_rev64_q(v97, aarch64::qv_2s); // 11-16  3
	auto v103 = body.make_fadd_qq(v39, v96, aarch64::qv_2s); // 11-18  5
	auto v104 = body.make_fsub_qq(v39, v96, aarch64::qv_2s); // 11-18  6
	auto v202 = body.make_fadd_qq(v187, v192, aarch64::qv_2s); // 11-15  5
	auto v203 = body.make_fadd_qq(v196, v201, aarch64::qv_2s); // 11-15  5
	auto v235 = body.make_fadd_qq(v233, v234, aarch64::qv_2s); // 11-18  5
	auto v236 = body.make_fsub_qq(v234, v233, aarch64::qv_2s); // 11-16  6
	auto v264 = body.make_fadd_qq(v249, v254, aarch64::qv_2s); // 11-15  5
	auto v265 = body.make_fadd_qq(v258, v263, aarch64::qv_2s); // 11-15  5
	auto v370 = body.make_rev64_q(v366, aarch64::qv_2s); // 11-11  3
	auto v372 = body.make_fadd_qq(v308, v365, aarch64::qv_2s); // 11-13  5
	auto v373 = body.make_fsub_qq(v308, v365, aarch64::qv_2s); // 11-13  6
	auto v469 = body.make_rev64_q(v465, aarch64::qv_2s); // 11-11  3
	auto v471 = body.make_fadd_qq(v407, v464, aarch64::qv_2s); // 11-13  5
	auto v472 = body.make_fsub_qq(v407, v464, aarch64::qv_2s); // 11-13  6
	auto v484 = body.make_fsub_qq(v181, v481, aarch64::qv_2s); // 11-20  6
	auto v493 = body.make_x_mul_rr(ostride, body.make_x_movz_i(8)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v484, aarch64::d_regs), v493, Y); // 12-21  15
	auto v485 = body.make_fadd_qq(v181, v481, aarch64::qv_2s); // 11-20  5
	auto v503 = body.make_x_mul_rr(ostride, body.make_x_movz_i(24)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v485, aarch64::d_regs), v503, Y); // 12-21  15
	auto v565 = body.make_fmul_qq(v564, v563, aarch64::qv_2s); // 11-15  7
	auto v574 = body.make_fmul_qq(v573, v767, aarch64::qv_2s); // 11-15  7
	auto v677 = body.make_fadd_qq(v662, v667, aarch64::qv_2s); // 11-16  5
	auto v678 = body.make_fadd_qq(v671, v676, aarch64::qv_2s); // 11-16  5
	auto v769 = body.make_fmul_qq(v768, v767, aarch64::qv_2s); // 11-15  7
	auto v778 = body.make_fmul_qq(v777, v776, aarch64::qv_2s); // 11-15  7
	auto v102 = body.make_fmul_qq(v101, v836, aarch64::qv_2s); // 12-17  7
	auto v204 = body.make_fadd_qq(v202, v203, aarch64::qv_2s); // 12-18  5
	auto v205 = body.make_fsub_qq(v203, v202, aarch64::qv_2s); // 12-16  6
	auto v240 = body.make_rev64_q(v236, aarch64::qv_2s); // 12-17  3
	auto v242 = body.make_fadd_qq(v74, v235, aarch64::qv_2s); // 12-19  5
	auto v243 = body.make_fsub_qq(v74, v235, aarch64::qv_2s); // 12-19  6
	auto v266 = body.make_fadd_qq(v264, v265, aarch64::qv_2s); // 12-18  5
	auto v267 = body.make_fsub_qq(v265, v264, aarch64::qv_2s); // 12-16  6
	auto v371 = body.make_fmul_qq(v370, v836, aarch64::qv_2s); // 12-12  7
	auto v470 = body.make_fmul_qq(v469, v836, aarch64::qv_2s); // 12-12  7
	auto v508 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(data_x, 0), aarch64::q_regs);
	auto v509 = body.make_fmul_qq(v372, v508, aarch64::qv_2s); // 12-15  7
	auto v513 = body.make_rev64_q(v372, aarch64::qv_2s); // 12-14  3
	auto v610 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(data_x, 24), aarch64::q_regs);
	auto v518 = body.make_fmul_qq(v471, v610, aarch64::qv_2s); // 12-15  7
	auto v522 = body.make_rev64_q(v471, aarch64::qv_2s); // 12-14  3
	auto v575 = body.make_fadd_qq(v560, v565, aarch64::qv_2s); // 12-16  5
	auto v576 = body.make_fadd_qq(v569, v574, aarch64::qv_2s); // 12-16  5
	auto v679 = body.make_fadd_qq(v677, v678, aarch64::qv_2s); // 12-19  5
	auto v680 = body.make_fsub_qq(v678, v677, aarch64::qv_2s); // 12-17  6
	auto v712 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(data_x, 72), aarch64::q_regs);
	auto v713 = body.make_fmul_qq(v373, v712, aarch64::qv_2s); // 12-15  7
	auto v717 = body.make_rev64_q(v373, aarch64::qv_2s); // 12-14  3
	auto v721 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(data_x, 88), aarch64::q_regs);
	auto v722 = body.make_fmul_qq(v472, v721, aarch64::qv_2s); // 12-15  7
	auto v726 = body.make_rev64_q(v472, aarch64::qv_2s); // 12-14  3
	auto v779 = body.make_fadd_qq(v764, v769, aarch64::qv_2s); // 12-16  5
	auto v780 = body.make_fadd_qq(v773, v778, aarch64::qv_2s); // 12-16  5
	auto v105 = body.make_fsub_qq(v40, v102, aarch64::qv_2s); // 13-18  6
	auto v106 = body.make_fadd_qq(v40, v102, aarch64::qv_2s); // 13-18  5
	auto v209 = body.make_rev64_q(v205, aarch64::qv_2s); // 13-17  3
	auto v211 = body.make_fadd_qq(v103, v204, aarch64::qv_2s); // 13-19  5
	auto v212 = body.make_fsub_qq(v103, v204, aarch64::qv_2s); // 13-19  6
	auto v241 = body.make_fmul_qq(v240, v836, aarch64::qv_2s); // 13-18  7
	auto v271 = body.make_rev64_q(v267, aarch64::qv_2s); // 13-17  3
	auto v374 = body.make_fsub_qq(v309, v371, aarch64::qv_2s); // 13-13  6
	auto v375 = body.make_fadd_qq(v309, v371, aarch64::qv_2s); // 13-13  5
	auto v473 = body.make_fsub_qq(v408, v470, aarch64::qv_2s); // 13-13  6
	auto v474 = body.make_fadd_qq(v408, v470, aarch64::qv_2s); // 13-13  5
	auto v725 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(data_x, 96), aarch64::q_regs);
	auto v514 = body.make_fmul_qq(v513, v725, aarch64::qv_2s); // 13-15  7
	auto v614 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(data_x, 32), aarch64::q_regs);
	auto v523 = body.make_fmul_qq(v522, v614, aarch64::qv_2s); // 13-15  7
	auto v577 = body.make_fadd_qq(v575, v576, aarch64::qv_2s); // 13-19  5
	auto v578 = body.make_fsub_qq(v576, v575, aarch64::qv_2s); // 13-17  6
	auto v684 = body.make_rev64_q(v680, aarch64::qv_2s); // 13-18  3
	auto v686 = body.make_fadd_qq(v182, v679, aarch64::qv_2s); // 13-20  5
	auto v692 = body.make_x_mul_rr(ostride, body.make_x_movz_i(4)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v686, aarch64::d_regs), v692, Y); // 14-21  15
	auto v687 = body.make_fsub_qq(v182, v679, aarch64::qv_2s); // 13-20  6
	auto v702 = body.make_x_mul_rr(ostride, body.make_x_movz_i(20)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v687, aarch64::d_regs), v702, Y); // 14-21  15
	auto v716 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(data_x, 80), aarch64::q_regs);
	auto v718 = body.make_fmul_qq(v717, v716, aarch64::qv_2s); // 13-15  7
	auto v727 = body.make_fmul_qq(v726, v725, aarch64::qv_2s); // 13-15  7
	auto v781 = body.make_fadd_qq(v779, v780, aarch64::qv_2s); // 13-19  5
	auto v782 = body.make_fsub_qq(v780, v779, aarch64::qv_2s); // 13-17  6
	auto v210 = body.make_fmul_qq(v209, v836, aarch64::qv_2s); // 14-18  7
	auto v244 = body.make_fsub_qq(v75, v241, aarch64::qv_2s); // 14-19  6
	auto v245 = body.make_fadd_qq(v75, v241, aarch64::qv_2s); // 14-19  5
	auto v272 = body.make_fmul_qq(v271, v836, aarch64::qv_2s); // 14-18  7
	auto v273 = body.make_fadd_qq(v105, v266, aarch64::qv_2s); // 14-19  5
	auto v274 = body.make_fsub_qq(v105, v266, aarch64::qv_2s); // 14-19  6
	auto v524 = body.make_fadd_qq(v509, v514, aarch64::qv_2s); // 14-16  5
	auto v525 = body.make_fadd_qq(v518, v523, aarch64::qv_2s); // 14-16  5
	auto v582 = body.make_rev64_q(v578, aarch64::qv_2s); // 14-18  3
	auto v584 = body.make_fadd_qq(v242, v577, aarch64::qv_2s); // 14-20  5
	auto v590 = body.make_x_mul_rr(ostride, body.make_x_movz_i(2)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v584, aarch64::d_regs), v590, Y); // 15-21  15
	auto v585 = body.make_fsub_qq(v242, v577, aarch64::qv_2s); // 14-20  6
	auto v600 = body.make_x_mul_rr(ostride, body.make_x_movz_i(18)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v585, aarch64::d_regs), v600, Y); // 15-21  15
	auto v611 = body.make_fmul_qq(v374, v610, aarch64::qv_2s); // 14-15  7
	auto v615 = body.make_rev64_q(v374, aarch64::qv_2s); // 14-14  3
	auto v619 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(data_x, 40), aarch64::q_regs);
	auto v620 = body.make_fmul_qq(v473, v619, aarch64::qv_2s); // 14-15  7
	auto v624 = body.make_rev64_q(v473, aarch64::qv_2s); // 14-14  3
	auto v685 = body.make_fmul_qq(v684, v836, aarch64::qv_2s); // 14-19  7
	auto v728 = body.make_fadd_qq(v713, v718, aarch64::qv_2s); // 14-16  5
	auto v729 = body.make_fadd_qq(v722, v727, aarch64::qv_2s); // 14-16  5
	auto v786 = body.make_rev64_q(v782, aarch64::qv_2s); // 14-18  3
	auto v814 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(data_x, 136), aarch64::q_regs);
	auto v815 = body.make_fmul_qq(v375, v814, aarch64::qv_2s); // 14-15  7
	auto v819 = body.make_rev64_q(v375, aarch64::qv_2s); // 14-14  3
	auto v823 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(data_x, 152), aarch64::q_regs);
	auto v824 = body.make_fmul_qq(v474, v823, aarch64::qv_2s); // 14-15  7
	auto v828 = body.make_rev64_q(v474, aarch64::qv_2s); // 14-14  3
	auto v213 = body.make_fsub_qq(v104, v210, aarch64::qv_2s); // 15-19  6
	auto v214 = body.make_fadd_qq(v104, v210, aarch64::qv_2s); // 15-19  5
	auto v275 = body.make_fsub_qq(v106, v272, aarch64::qv_2s); // 15-19  6
	auto v276 = body.make_fadd_qq(v106, v272, aarch64::qv_2s); // 15-19  5
	auto v526 = body.make_fadd_qq(v524, v525, aarch64::qv_2s); // 15-19  5
	auto v527 = body.make_fsub_qq(v525, v524, aarch64::qv_2s); // 15-17  6
	auto v583 = body.make_fmul_qq(v582, v836, aarch64::qv_2s); // 15-19  7
	auto v616 = body.make_fmul_qq(v615, v614, aarch64::qv_2s); // 15-15  7
	auto v818 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(data_x, 144), aarch64::q_regs);
	auto v625 = body.make_fmul_qq(v624, v818, aarch64::qv_2s); // 15-15  7
	auto v688 = body.make_fsub_qq(v183, v685, aarch64::qv_2s); // 15-20  6
	auto v697 = body.make_x_mul_rr(ostride, body.make_x_movz_i(12)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v688, aarch64::d_regs), v697, Y); // 16-21  15
	auto v689 = body.make_fadd_qq(v183, v685, aarch64::qv_2s); // 15-20  5
	auto v707 = body.make_x_mul_rr(ostride, body.make_x_movz_i(28)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v689, aarch64::d_regs), v707, Y); // 16-21  15
	auto v730 = body.make_fadd_qq(v728, v729, aarch64::qv_2s); // 15-19  5
	auto v731 = body.make_fsub_qq(v729, v728, aarch64::qv_2s); // 15-17  6
	auto v787 = body.make_fmul_qq(v786, v836, aarch64::qv_2s); // 15-19  7
	auto v788 = body.make_fadd_qq(v244, v781, aarch64::qv_2s); // 15-20  5
	auto v794 = body.make_x_mul_rr(ostride, body.make_x_movz_i(6)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v788, aarch64::d_regs), v794, Y); // 16-21  15
	auto v789 = body.make_fsub_qq(v244, v781, aarch64::qv_2s); // 15-20  6
	auto v804 = body.make_x_mul_rr(ostride, body.make_x_movz_i(22)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v789, aarch64::d_regs), v804, Y); // 16-21  15
	auto v820 = body.make_fmul_qq(v819, v818, aarch64::qv_2s); // 15-15  7
	auto v827 = aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(data_x, 160), aarch64::q_regs);
	auto v829 = body.make_fmul_qq(v828, v827, aarch64::qv_2s); // 15-15  7
	auto v531 = body.make_rev64_q(v527, aarch64::qv_2s); // 16-18  3
	auto v533 = body.make_fadd_qq(v211, v526, aarch64::qv_2s); // 16-20  5
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v533, aarch64::d_regs), ostride, Y); // 17-21  15
	auto v534 = body.make_fsub_qq(v211, v526, aarch64::qv_2s); // 16-20  6
	auto v549 = body.make_x_mul_rr(ostride, body.make_x_movz_i(17)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v534, aarch64::d_regs), v549, Y); // 17-21  15
	auto v586 = body.make_fsub_qq(v243, v583, aarch64::qv_2s); // 16-20  6
	auto v595 = body.make_x_mul_rr(ostride, body.make_x_movz_i(10)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v586, aarch64::d_regs), v595, Y); // 17-21  15
	auto v587 = body.make_fadd_qq(v243, v583, aarch64::qv_2s); // 16-20  5
	auto v605 = body.make_x_mul_rr(ostride, body.make_x_movz_i(26)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v587, aarch64::d_regs), v605, Y); // 17-21  15
	auto v626 = body.make_fadd_qq(v611, v616, aarch64::qv_2s); // 16-16  5
	auto v627 = body.make_fadd_qq(v620, v625, aarch64::qv_2s); // 16-16  5
	auto v735 = body.make_rev64_q(v731, aarch64::qv_2s); // 16-18  3
	auto v737 = body.make_fadd_qq(v213, v730, aarch64::qv_2s); // 16-20  5
	auto v743 = body.make_x_mul_rr(ostride, body.make_x_movz_i(5)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v737, aarch64::d_regs), v743, Y); // 17-21  15
	auto v738 = body.make_fsub_qq(v213, v730, aarch64::qv_2s); // 16-20  6
	auto v753 = body.make_x_mul_rr(ostride, body.make_x_movz_i(21)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v738, aarch64::d_regs), v753, Y); // 17-21  15
	auto v790 = body.make_fsub_qq(v245, v787, aarch64::qv_2s); // 16-20  6
	auto v799 = body.make_x_mul_rr(ostride, body.make_x_movz_i(14)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v790, aarch64::d_regs), v799, Y); // 17-21  15
	auto v791 = body.make_fadd_qq(v245, v787, aarch64::qv_2s); // 16-20  5
	auto v809 = body.make_x_mul_rr(ostride, body.make_x_movz_i(30)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v791, aarch64::d_regs), v809, Y); // 17-21  15
	auto v830 = body.make_fadd_qq(v815, v820, aarch64::qv_2s); // 16-16  5
	auto v831 = body.make_fadd_qq(v824, v829, aarch64::qv_2s); // 16-16  5
	auto v532 = body.make_fmul_qq(v531, v836, aarch64::qv_2s); // 17-19  7
	auto v628 = body.make_fadd_qq(v626, v627, aarch64::qv_2s); // 17-19  5
	auto v629 = body.make_fsub_qq(v627, v626, aarch64::qv_2s); // 17-17  6
	auto v736 = body.make_fmul_qq(v735, v836, aarch64::qv_2s); // 17-19  7
	auto v832 = body.make_fadd_qq(v830, v831, aarch64::qv_2s); // 17-19  5
	auto v833 = body.make_fsub_qq(v831, v830, aarch64::qv_2s); // 17-17  6
	auto v535 = body.make_fsub_qq(v212, v532, aarch64::qv_2s); // 18-20  6
	auto v544 = body.make_x_mul_rr(ostride, body.make_x_movz_i(9)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v535, aarch64::d_regs), v544, Y); // 19-21  15
	auto v536 = body.make_fadd_qq(v212, v532, aarch64::qv_2s); // 18-20  5
	auto v554 = body.make_x_mul_rr(ostride, body.make_x_movz_i(25)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v536, aarch64::d_regs), v554, Y); // 19-21  15
	auto v633 = body.make_rev64_q(v629, aarch64::qv_2s); // 18-18  3
	auto v635 = body.make_fadd_qq(v273, v628, aarch64::qv_2s); // 18-20  5
	auto v641 = body.make_x_mul_rr(ostride, body.make_x_movz_i(3)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v635, aarch64::d_regs), v641, Y); // 19-21  15
	auto v636 = body.make_fsub_qq(v273, v628, aarch64::qv_2s); // 18-20  6
	auto v651 = body.make_x_mul_rr(ostride, body.make_x_movz_i(19)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v636, aarch64::d_regs), v651, Y); // 19-21  15
	auto v739 = body.make_fsub_qq(v214, v736, aarch64::qv_2s); // 18-20  6
	auto v748 = body.make_x_mul_rr(ostride, body.make_x_movz_i(13)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v739, aarch64::d_regs), v748, Y); // 19-21  15
	auto v740 = body.make_fadd_qq(v214, v736, aarch64::qv_2s); // 18-20  5
	auto v758 = body.make_x_mul_rr(ostride, body.make_x_movz_i(29)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v740, aarch64::d_regs), v758, Y); // 19-21  15
	auto v837 = body.make_rev64_q(v833, aarch64::qv_2s); // 18-18  3
	auto v839 = body.make_fadd_qq(v275, v832, aarch64::qv_2s); // 18-20  5
	auto v845 = body.make_x_mul_rr(ostride, body.make_x_movz_i(7)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v839, aarch64::d_regs), v845, Y); // 19-21  15
	auto v840 = body.make_fsub_qq(v275, v832, aarch64::qv_2s); // 18-20  6
	auto v855 = body.make_x_mul_rr(ostride, body.make_x_movz_i(23)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v840, aarch64::d_regs), v855, Y); // 19-21  15
	auto v634 = body.make_fmul_qq(v633, v836, aarch64::qv_2s); // 19-19  7
	auto v838 = body.make_fmul_qq(v837, v836, aarch64::qv_2s); // 19-19  7
	auto v637 = body.make_fsub_qq(v274, v634, aarch64::qv_2s); // 20-20  6
	auto v646 = body.make_x_mul_rr(ostride, body.make_x_movz_i(11)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v637, aarch64::d_regs), v646, Y); // 21-21  15
	auto v638 = body.make_fadd_qq(v274, v634, aarch64::qv_2s); // 20-20  5
	auto v656 = body.make_x_mul_rr(ostride, body.make_x_movz_i(27)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v638, aarch64::d_regs), v656, Y); // 21-21  15
	auto v841 = body.make_fsub_qq(v276, v838, aarch64::qv_2s); // 20-20  6
	auto v850 = body.make_x_mul_rr(ostride, body.make_x_movz_i(15)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v841, aarch64::d_regs), v850, Y); // 21-21  15
	auto v842 = body.make_fadd_qq(v276, v838, aarch64::qv_2s); // 20-20  5
	auto v860 = body.make_x_mul_rr(ostride, body.make_x_movz_i(31)); // 2-19  10
	body.make_d_str_rrr(aarch64::reg_reinterpret_with_class(v842, aarch64::d_regs), v860, Y); // 21-21  15
	body.make_x_add_rrr(X, X, idist, aarch64::lsl_0);
	body.make_x_add_rrr(Y, Y, odist, aarch64::lsl_0);
	body.make_cbnz_ri(howmany, body_block);

	fini.make_ret();
	std::vector<sloejit::reloc_info> relocs;
	auto text = fn.emit_bin(&relocs);

	sloejit::bytevector data_vec;
	for (float f : data_bytes) {
		uint32_t fi;
		memcpy((void *) &fi, &f, sizeof(float));
		data_vec.push_u32(fi);
	}
#if !defined(__APPLE__) && !defined(_WIN32)
	sloejit::elf_data e;
	e.fn_entries.emplace_back(fn.name, std::move(text).get(), std::move(data_vec).get(), std::move(relocs));

	std::vector<uint8_t> potatoes{ 11, 12, 13, 14 };

	e.fn_entries.emplace_back("potato", std::move(potatoes), std::vector<uint8_t>{});
	auto data2 = sloejit::emit_elf(e);

	auto fd = open("aarch64_fft32.o", O_CREAT | O_TRUNC | O_WRONLY, 0664);
	sloejit_assert(write(fd, &data2[0], data2.size()) == (ssize_t) data2.size());
	close(fd);
#endif
}

int main() {
	plfft_ab_32_cccnf_gs();
}
