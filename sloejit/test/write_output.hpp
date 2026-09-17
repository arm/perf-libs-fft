/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sloejit/sloejit_assert.hpp"

#include <cstddef>
#include <fcntl.h>
#ifdef _WIN32
#include <cstdlib>
#include <io.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#endif

static inline void write_output(const char *fname, const void *const data, const size_t sz) {
#ifdef _WIN32
	int fh = 0;
	int fmode = 0;
	// get current file I/O mode
	auto err = _get_fmode(&fmode);
	sloejit_assert(err == 0);
	sloejit_assert(fmode == _O_BINARY || fmode == _O_TEXT);
	// set file I/O to binary mode
	err = _set_fmode(_O_BINARY);
	sloejit_assert(err == 0);
	err = _sopen_s(&fh, fname, _O_CREAT | _O_TRUNC | _O_WRONLY, _SH_DENYNO, _S_IREAD | _S_IWRITE);
	sloejit_assert(err != -1);
	sloejit_assert(_write(fh, data, sz) == (int) sz);
	_close(fh);
	// set file I/O mode back to original
	err = _set_fmode(fmode);
	sloejit_assert(err == 0);
#else
	auto fd = open(fname, O_CREAT | O_TRUNC | O_WRONLY, 0664);
	sloejit_assert(write(fd, data, sz) == (ssize_t) sz);
	close(fd);
#endif
}
