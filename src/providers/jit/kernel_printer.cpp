/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

// clang-format off
#include "kernel_cache.hpp"
#include "kernel_printer.hpp"
#include "plfft_assert.hpp"

#if defined(_WIN32)
#include <io.h>
#define ftruncate _chsize_s
#include <process.h>
#include <windows.h>
#include <processthreadsapi.h>
#else
#include <unistd.h>
#include <sys/mman.h>
#endif

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sys/types.h>
#include <inttypes.h>

#include "sloejit/util.hpp"
#include "sloejit/plfft_clear_icache.hpp"
#if !defined(NDEBUG) && !defined(__APPLE__) && !defined(_WIN32)
#include "sloejit/gdb.hpp"
#endif
// clang-format on

// These relocation types used in resolve_relocation() are all we need from
// elf.h. define them instead of including elf.h for portability.
#define R_AARCH64_ADD_ABS_LO12_NC 277
#define R_AARCH64_ADR_PREL_LO21 274
#define R_AARCH64_ADR_PREL_PG_HI21 275
#define R_AARCH64_CALL26 283

namespace plfft::wfta {

static size_t get_hash(const uint8_t *p, size_t len) {
  size_t result = 0;
  const size_t prime = 31;
  for (size_t i = 0; i < len; ++i) {
    result = p[i] + (result * prime);
  }
  return result;
}

static void dump_kernel_hash(const std::string &path, size_t val) {
  fprintf(stderr, "emitting new hash file: %s -> 0x%zx\n", path.c_str(), val);
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char *>(&val), sizeof(val));
}

static void dump_kernel(const std::string &fn_name, const uint8_t *text_addr,
                        size_t text_len, const uint8_t *data_addr,
                        size_t data_len,
                        const std::vector<sloejit::reloc_info> &relocs,
                        const std::vector<sloejit::note_info> &notes) {
#if !defined(__APPLE__) && !defined(_WIN32)
  // Write out the kernel object ELF file.
  sloejit::elf_data e;
  std::ofstream out(fn_name, std::ios::binary);
  std::vector<uint8_t> text_bytes{text_addr, text_addr + text_len};
  std::vector<uint8_t> data_bytes{data_addr, data_addr + data_len};
  e.fn_entries.emplace_back(std::move(fn_name), text_addr,
                            std::move(text_bytes), data_addr,
                            std::move(data_bytes), std::move(relocs));
  auto data = emit_elf(e);
  out.write((const char *)data.data(), data.size());

  if (notes.size() > 0) {
    // Write out the kernel notes data file.
    std::ofstream out(fn_name + ".notes.txt");
    for (const auto &note : notes) {
      out << note.ofs << " " << note.note << "\n";
    }
  }
#endif
}

template<typename T>
static void dump_kernel_src(T &out, const std::string &fn_name,
                            const std::string &src, const uint8_t *data_addr,
                            size_t data_len) {
  // Write out the kernel assembly to output stream
  out << "\tFUNC_SECTION(" << fn_name << ")\n";
  out << "\tFUNC_TYPE(" << fn_name << ")\n";
  out << "\t.balign 16\n";
  out << src;
  out << "\tFUNC_SIZE(" << fn_name << ")\n";

  if (data_len > 0) {
    out << "\n";
    out << "\tRODATA_SECTION()\n";
    out << "\t.balign 16\n";
    out << ".rodata." << fn_name << ":\n";
    out << std::hex << std::setfill('0');
    for (size_t i = 0; i < data_len; i += 4) {
      out << "\t.byte ";
      size_t end = std::min(i + 4, data_len);
      for (size_t j = i; j < end; ++j) {
        out << "0x" << std::setw(2) << +data_addr[j];
        if (j + 1 < end) {
          out << ", ";
        }
      }
      out << "\n";
    }
    out << "\n";
  }
}

static void check_kernel_hash(const std::string &fn_name,
                              const uint8_t *text_addr, size_t text_len) {
  auto hval_out = get_hash(text_addr, text_len);
  std::string path = fn_name + ".hash";
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    dump_kernel_hash(path, hval_out);
    return;
  }
  decltype(hval_out) hval_in;
  auto nbytes =
      in.readsome(reinterpret_cast<char *>(&hval_in), sizeof(hval_in));
  ASSERT(nbytes >= 0);
  if (static_cast<decltype(hval_out)>(nbytes) != sizeof(hval_in)) {
    fprintf(stderr, "corrupt hash file? %s should be %zu bytes but read %zd\n",
            path.c_str(), sizeof(hval_in), nbytes);
    dump_kernel_hash(path, hval_out);
    return;
  }
  if (hval_in != hval_out) {
    fprintf(stderr, "hash files did not match! (old) 0x%zx != (new) 0x%zx\n",
            hval_in, hval_out);
    dump_kernel_hash(path, hval_out);
    return;
  }
}

static void append_map_file(const void *addr, size_t len,
                            const std::string &fn_name) {
  // --start-address=0x0000ffff9fd90000 --stop-address=0x0000ffff9fd90e5c -l -d
  // --no-show-raw -S -C /tmp/perf-48146.map
  auto pid = getpid();
  char filename[500];
  snprintf(filename, sizeof(filename), "/tmp/perf-%d.map", pid);
  auto f = fopen(filename, "a");
  fprintf(f, "%p %zx %s\n", addr, len, fn_name.c_str());
  fclose(f);
}

static void
debug_dump_kernels_and_fail(const kernel_registry_map &kernel_registry) {
  for (auto &[fn_name, e] : kernel_registry) {
    fprintf(stderr, "  %s \t %p 0x%zx %p 0x%zx\n", fn_name.c_str(),
            e.get_text_ptr(), e.text_len, e.data_ptr, e.data_len);
  }
  ASSERT(false);
}

static void resolve_relocation(const std::string &fn_name, uint8_t *text,
                               const uint8_t *data,
                               const sloejit::reloc_info &reloc,
                               kernel_registry_map &kernel_registry) {
  // An ELF relocation looks something like this:
  // struct reloc_info {
  //     std::string name; ///< What symbol is the relocation resolving to?
  //     int offset; ///< Offset to patch, in bytes
  //     int type;   ///< How to patch, see elf.h (R_AARCH64_...)
  //     int addend; ///< Value to patch is (&name + addend).
  // };
  // Luckily sloejit currently only emits relocations of types
  // (see descriptions inline in switch):
  //     R_AARCH64_ADD_ABS_LO12_NC
  //     R_AARCH64_ADR_PREL_LO21
  //     R_AARCH64_ADR_PREL_PG_HI21
  //     R_AARCH64_CALL26

  const uint8_t *target_addr = nullptr;
  if (reloc.name.rfind(".rodata.", 0, 8) == 0) {
    ASSERT(reloc.name.substr(8) == fn_name);
    target_addr = data;
  } else {
    // target is another function, look for its text ptr
    auto it = kernel_registry.find(reloc.name);
    ASSERT(it != kernel_registry.end());
    target_addr = (const uint8_t *)it->second.get_text_ptr();
  }
  ASSERT(target_addr);
  target_addr += reloc.addend;
  uint32_t *dst = (uint32_t *)(&text[reloc.offset]);

  // at this point we have both the instruction to modify (dst) and the target
  // to point it at (target), we just need to know how to patch in the address!

  switch (reloc.type) {
  case R_AARCH64_ADD_ABS_LO12_NC: {
    // patch low 12 bits of absolute address into bits [21:10] of instr.
    // NC = no-check (because the actual abs address is >12 bits)
    if ((*dst & (0xfffu << 10)) != 0u) {
      fprintf(stderr,
              "resolving R_AARCH64_ADD_ABS_LO12_NC reloc but dst address %p "
              "was already filled, got 0x%x\n",
              dst, *dst);
      debug_dump_kernels_and_fail(kernel_registry);
    }
    uint64_t umod = ((uint64_t)target_addr) & 0xfffu;
    *dst |= (umod << 10);
    break;
  }
  case R_AARCH64_ADR_PREL_LO21: {
    // patch 21-bit signed pc-relative address into bits [23:5]:[30:29] of
    // instr. fail on signed overflow. (this gives a +/- 1M range).
    if ((*dst & (0x7ffffu << 5)) != 0u || (*dst & (0x3u << 29)) != 0u) {
      fprintf(stderr,
              "resolving R_AARCH64_ADR_PREL_LO21 reloc but dst address %p was "
              "already filled, got 0x%x\n",
              dst, *dst);
      debug_dump_kernels_and_fail(kernel_registry);
    }
    int64_t s_target_addr = (int64_t)target_addr;
    int64_t s_dst_addr = (int64_t)dst;
    int64_t smod = s_target_addr - s_dst_addr;
    if ((smod << (64 - 21)) >> (64 - 21) != smod) {
      fprintf(stderr,
              "resolving R_AARCH64_ADR_PREL_LO21 reloc but smod does not fit "
              "in +/-1M, got 0x%" PRIx64 " - 0x%" PRIx64 "  = 0x%" PRIx64 "\n",
              s_target_addr, s_dst_addr, smod);
      debug_dump_kernels_and_fail(kernel_registry);
    }
    uint32_t umod_hi = (((uint64_t)smod) & 0x1ffffcu) >> 2;
    uint32_t umod_lo = ((uint64_t)smod) & 0x3u;
    *dst |= (umod_hi << 5) | (umod_lo << 29);
    break;
  }
  case R_AARCH64_ADR_PREL_PG_HI21: {
    // patch bits [33:12] of signed pc-relative address into bits [23:5]:[30:29]
    // of instr. fail on signed overflow. (this gives a +/- 4G range).
    if ((*dst & (0x7ffffu << 5)) != 0u || (*dst & (0x3u << 29)) != 0u) {
      fprintf(stderr,
              "resolving R_AARCH64_ADR_PREL_PG_HI21 reloc but dst address %p "
              "was already filled, got "
              "0x%x\n",
              dst, *dst);
      debug_dump_kernels_and_fail(kernel_registry);
    }
    int64_t s_target_addr = (int64_t)((uint64_t)target_addr & ~0xfffllu);
    int64_t s_dst_addr = (int64_t)((uint64_t)dst & ~0xfffllu);
    int64_t smod = s_target_addr - s_dst_addr;
    if ((smod << (64 - 33)) >> (64 - 33) != smod) {
      fprintf(stderr,
              "resolving R_AARCH64_ADR_PREL_PG_HI21 reloc but smod does not "
              "fit in +/-4G, got 0x%" PRIx64 " - 0x%" PRIx64 "  = 0x%" PRIx64
              "\n",
              s_target_addr, s_dst_addr, smod);
      debug_dump_kernels_and_fail(kernel_registry);
    }
    uint32_t umod_hi = (((uint64_t)smod) & 0x1ffffc000u) >> 14;
    uint32_t umod_lo = (((uint64_t)smod) & 0x3000u) >> 12;
    *dst |= (umod_hi << 5) | (umod_lo << 29);
    break;
  }
  case R_AARCH64_CALL26: {
    // patch bits [27:2] of signed pc-relative address into bits [25:0] of
    // instr. fail on signed overflow. (this gives a +/- 128M range).
    if ((*dst & 0x1ffffffu) != 0u) {
      fprintf(stderr,
              "resolving R_AARCH64_CALL26 reloc but dst address %p was already "
              "filled, got 0x%x\n",
              dst, *dst);
      debug_dump_kernels_and_fail(kernel_registry);
    }
    int64_t s_target_addr = (int64_t)target_addr;
    int64_t s_dst_addr = (int64_t)dst;
    int64_t smod = s_target_addr - s_dst_addr;
    if ((smod << (64 - 28)) >> (64 - 28) != smod || (smod & 0x3) != 0) {
      fprintf(stderr,
              "resolving R_AARCH64_CALL26 reloc but smod does not fit in "
              "+/-128M, got 0x%" PRIx64 " - 0x%" PRIx64 "  = 0x%" PRIx64 "\n",
              s_target_addr, s_dst_addr, smod);
      debug_dump_kernels_and_fail(kernel_registry);
    }
    uint32_t umod = (((uint64_t)smod) & 0xffffffcu) >> 2;
    *dst |= umod;
    break;
  }
  default:
    ASSERT(false && "unhandled reloc type");
  }
}

static void resolve_relocations(const std::string &fn_name, uint8_t *text,
                                const uint8_t *data,
                                const std::vector<sloejit::reloc_info> &relocs,
                                kernel_registry_map &kernel_registry) {
  for (const auto &reloc : relocs) {
    resolve_relocation(fn_name, text, data, reloc, kernel_registry);
  }
}

static kernel_registry_entry<void>
emit_single(kernel_registry_map &kernel_registry, const std::string &fn_name,
            uint8_t *text_ptr, size_t text_len, const uint8_t *data_ptr,
            size_t data_len, const std::vector<sloejit::reloc_info> &relocs,
            const std::vector<sloejit::note_info> &notes,
            const std::optional<std::string> &src, algo_flops flops,
            const options_t &opts) {
  kernel_registry_entry<void> kre{text_ptr, text_len, data_ptr, data_len,
                                  flops};
  ASSERT(kernel_registry.emplace(fn_name, kre).second);

  if (opts.want_map_file) {
    dump_kernel(fn_name, text_ptr, text_len, data_ptr, data_len, relocs, notes);
    append_map_file(text_ptr, text_len, fn_name);
  }

  if (opts.want_check_kernel_hash) {
    check_kernel_hash(fn_name, text_ptr, text_len);
  }

  if (opts.want_kernel_src && src.has_value()) {
    if (opts.src_path) {
      auto out = std::ofstream(*opts.src_path + ".S");
      dump_kernel_src(out, fn_name, *src, data_ptr, data_len);
    } else {
      dump_kernel_src(std::cout, fn_name, *src, data_ptr, data_len);
    }
  }

  resolve_relocations(fn_name, text_ptr, data_ptr, relocs, kernel_registry);

#if !defined(NDEBUG) && !defined(__APPLE__) && !defined(_WIN32)
  // When generating perf map files (PLFFT_WFTA_PERF=1), also register the
  // JIT function with GDB so that backtraces and disassembly have symbols.
  if (opts.want_map_file) {
    sloejit::add_entry((void *)text_ptr, (int)text_len, fn_name.c_str());
  }
#endif
  return kre;
}

void finalize_kernels(kernel_registry_map &kernel_registry,
                      const std::vector<kernel_data> &in_progress_kernels,
                      const options_t &opts) {
  // align function boundaries to 32-bytes.
  // TODO: this generally works well, but some uarches may prefer a different
  // alignment.
  // TODO: we may also want to align loop start points in a similar fashion.
  constexpr size_t text_align = 32;
  size_t text_len = 0;
  size_t data_len = 0;

  for (auto &data : in_progress_kernels) {
    text_len += data.text_bytes.size();
    data_len += data.data_bytes.size();
    text_len = iround(text_len, text_align);
  }

  ASSERT(text_len + data_len != 0);

  uint8_t *text_ptr = sloejit::alloc_executable_memory(text_len + data_len);
  uint8_t *data_ptr = text_ptr + text_len;

  kernel_cache::allocated_mem.emplace_back((void *)text_ptr,
                                           text_len + data_len);

  size_t text_ofs = 0;
  size_t data_ofs = 0;
  for (auto &data : in_progress_kernels) {
    text_ofs = iround(text_ofs, text_align);
    memcpy(&text_ptr[text_ofs], data.text_bytes.data(), data.text_bytes.size());
    memcpy(&data_ptr[data_ofs], data.data_bytes.data(), data.data_bytes.size());
    auto kre = emit_single(kernel_registry, data.fn_name, &text_ptr[text_ofs],
                           data.text_bytes.size(), &data_ptr[data_ofs],
                           data.data_bytes.size(), data.relocs, data.notes,
                           data.src, data.flops, opts);
    if (data.out) {
      *data.out = kre;
    }
    text_ofs += data.text_bytes.size();
    data_ofs += data.data_bytes.size();
  }

#if !defined(BAREMETAL) && !defined(_WIN32)
  int rv = mprotect(text_ptr, text_len + data_len, PROT_READ | PROT_EXEC);
  ASSERT(rv != -1);

  // luckily there is a compiler builtin for this, except it is named
  // differently between gcc and clang for some reason!
#ifdef __clang__
  __clear_cache(text_ptr, text_ptr + text_len);
#else
  __builtin___clear_cache(text_ptr, text_ptr + text_len);
#endif

#elif defined(_WIN32)
  DWORD flOldProtect = 0;
  bool rv = VirtualProtect(text_ptr, text_len + data_len, PAGE_EXECUTE_READ,
                           &flOldProtect);
  ASSERT(rv);
  FlushInstructionCache(GetCurrentProcess(), text_ptr, text_len);
#else

  // finally, issue a lot of dsb/isb instructions to sync icache.
  plfft_clear_icache(text_ptr, text_len);
#endif
}

} // end namespace plfft::wfta
