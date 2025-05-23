/*
 * Copyright (C) 2012 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

/* Declarations related to the ELF program header table and segments.
 *
 * The design goal is to provide an API that is as close as possible
 * to the ELF spec, and does not depend on linker-specific data
 * structures (e.g. the exact layout of struct soinfo).
 */

#include "linker.h"
#include "linker_mapped_file_fragment.h"
#include "linker_note_gnu_property.h"

#include <list>

#define MAYBE_MAP_FLAG(x, from, to)  (((x) & (from)) ? (to) : 0)
#define PFLAGS_TO_PROT(x)            (MAYBE_MAP_FLAG((x), PF_X, PROT_EXEC) | \
                                      MAYBE_MAP_FLAG((x), PF_R, PROT_READ) | \
                                      MAYBE_MAP_FLAG((x), PF_W, PROT_WRITE))

static constexpr size_t kCompatPageSize = 0x1000;

class ElfReader {
 public:
  ElfReader();

  [[nodiscard]] bool Read(const char* name, int fd, off64_t file_offset, off64_t file_size);
  [[nodiscard]] bool Load(address_space_params* address_space);

  const char* name() const { return name_.c_str(); }
  size_t phdr_count() const { return phdr_num_; }
  ElfW(Addr) load_start() const { return reinterpret_cast<ElfW(Addr)>(load_start_); }
  size_t load_size() const { return load_size_; }
  ElfW(Addr) gap_start() const { return reinterpret_cast<ElfW(Addr)>(gap_start_); }
  size_t gap_size() const { return gap_size_; }
  ElfW(Addr) load_bias() const { return load_bias_; }
  const ElfW(Phdr)* loaded_phdr() const { return loaded_phdr_; }
  const ElfW(Dyn)* dynamic() const { return dynamic_; }
  const char* get_string(ElfW(Word) index) const;
  bool is_mapped_by_caller() const { return mapped_by_caller_; }
  ElfW(Addr) entry_point() const { return header_.e_entry + load_bias_; }
  bool should_pad_segments() const { return should_pad_segments_; }
  bool should_use_16kib_app_compat() const { return should_use_16kib_app_compat_; }
  ElfW(Addr) compat_relro_start() const { return compat_relro_start_; }
  ElfW(Addr) compat_relro_size() const { return compat_relro_size_; }

 private:
  [[nodiscard]] bool ReadElfHeader();
  [[nodiscard]] bool VerifyElfHeader();
  [[nodiscard]] bool ReadProgramHeaders();
  [[nodiscard]] bool ReadSectionHeaders();
  [[nodiscard]] bool ReadDynamicSection();
  [[nodiscard]] bool ReadPadSegmentNote();
  [[nodiscard]] bool ReserveAddressSpace(address_space_params* address_space);
  [[nodiscard]] bool MapSegment(size_t seg_idx, size_t len);
  [[nodiscard]] bool CompatMapSegment(size_t seg_idx, size_t len);
  void ZeroFillSegment(const ElfW(Phdr)* phdr);
  void DropPaddingPages(const ElfW(Phdr)* phdr, uint64_t seg_file_end);
  [[nodiscard]] bool MapBssSection(const ElfW(Phdr)* phdr, ElfW(Addr) seg_page_end,
                                   ElfW(Addr) seg_file_end);
  [[nodiscard]] bool IsEligibleFor16KiBAppCompat(ElfW(Addr)* vaddr);
  [[nodiscard]] bool HasAtMostOneRelroSegment(const ElfW(Phdr)** relro_phdr);
  [[nodiscard]] bool Setup16KiBAppCompat();
  [[nodiscard]] bool LoadSegments();
  [[nodiscard]] bool FindPhdr();
  [[nodiscard]] bool FindGnuPropertySection();
  [[nodiscard]] bool CheckPhdr(ElfW(Addr));
  [[nodiscard]] bool CheckFileRange(ElfW(Addr) offset, size_t size, size_t alignment);

  bool did_read_;
  bool did_load_;
  std::string name_;
  int fd_;
  off64_t file_offset_;
  off64_t file_size_;

  ElfW(Ehdr) header_;
  size_t phdr_num_;

  MappedFileFragment phdr_fragment_;
  const ElfW(Phdr)* phdr_table_;

  MappedFileFragment shdr_fragment_;
  const ElfW(Shdr)* shdr_table_;
  size_t shdr_num_;

  MappedFileFragment dynamic_fragment_;
  const ElfW(Dyn)* dynamic_;

  MappedFileFragment strtab_fragment_;
  const char* strtab_;
  size_t strtab_size_;

  // First page of reserved address space.
  void* load_start_;
  // Size in bytes of reserved address space.
  size_t load_size_;
  // First page of inaccessible gap mapping reserved for this DSO.
  void* gap_start_;
  // Size in bytes of the gap mapping.
  size_t gap_size_;
  // Load bias.
  ElfW(Addr) load_bias_;

  // Loaded phdr.
  const ElfW(Phdr)* loaded_phdr_;

  // Is map owned by the caller
  bool mapped_by_caller_;

  // Pad gaps between segments when memory mapping?
  bool should_pad_segments_ = false;

  // Use app compat mode when loading 4KiB max-page-size ELFs on 16KiB page-size devices?
  bool should_use_16kib_app_compat_ = false;

  // RELRO region for 16KiB compat loading
  ElfW(Addr) compat_relro_start_ = 0;
  ElfW(Addr) compat_relro_size_ = 0;

  // Only used by AArch64 at the moment.
  GnuPropertySection note_gnu_property_ __unused;
};

size_t phdr_table_get_load_size(const ElfW(Phdr)* phdr_table, size_t phdr_count,
                                ElfW(Addr)* min_vaddr = nullptr, ElfW(Addr)* max_vaddr = nullptr);

size_t phdr_table_get_maximum_alignment(const ElfW(Phdr)* phdr_table, size_t phdr_count);
size_t phdr_table_get_minimum_alignment(const ElfW(Phdr)* phdr_table, size_t phdr_count);

int phdr_table_protect_segments(const ElfW(Phdr)* phdr_table, size_t phdr_count,
                                ElfW(Addr) load_bias, bool should_pad_segments,
                                bool should_use_16kib_app_compat,
                                const GnuPropertySection* prop = nullptr);

int phdr_table_unprotect_segments(const ElfW(Phdr)* phdr_table, size_t phdr_count,
                                  ElfW(Addr) load_bias, bool should_pad_segments,
                                  bool should_use_16kib_app_compat);

int phdr_table_protect_gnu_relro(const ElfW(Phdr)* phdr_table, size_t phdr_count,
                                 ElfW(Addr) load_bias, bool should_pad_segments,
                                 bool should_use_16kib_app_compat);

int phdr_table_protect_gnu_relro_16kib_compat(ElfW(Addr) start, ElfW(Addr) size);

int phdr_table_serialize_gnu_relro(const ElfW(Phdr)* phdr_table, size_t phdr_count,
                                   ElfW(Addr) load_bias, int fd, size_t* file_offset);

int phdr_table_map_gnu_relro(const ElfW(Phdr)* phdr_table, size_t phdr_count,
                             ElfW(Addr) load_bias, int fd, size_t* file_offset);

#if defined(__arm__)
int phdr_table_get_arm_exidx(const ElfW(Phdr)* phdr_table, size_t phdr_count, ElfW(Addr) load_bias,
                             ElfW(Addr)** arm_exidx, size_t* arm_exidix_count);
#endif

void phdr_table_get_dynamic_section(const ElfW(Phdr)* phdr_table, size_t phdr_count,
                                    ElfW(Addr) load_bias, ElfW(Dyn)** dynamic,
                                    ElfW(Word)* dynamic_flags);

const char* phdr_table_get_interpreter_name(const ElfW(Phdr)* phdr_table, size_t phdr_count,
                                            ElfW(Addr) load_bias);

bool page_size_migration_supported();

int remap_memtag_globals_segments(const ElfW(Phdr) * phdr_table, size_t phdr_count,
                                  ElfW(Addr) load_bias);

void protect_memtag_globals_ro_segments(const ElfW(Phdr) * phdr_table, size_t phdr_count,
                                        ElfW(Addr) load_bias);

void name_memtag_globals_segments(const ElfW(Phdr) * phdr_table, size_t phdr_count,
                                  ElfW(Addr) load_bias, const char* soname,
                                  std::list<std::string>* vma_names);
