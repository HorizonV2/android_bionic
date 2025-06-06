/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef _PRIVATE_BIONIC_GLOBALS_H
#define _PRIVATE_BIONIC_GLOBALS_H

#include <inttypes.h>
#include <link.h>
#include <platform/bionic/malloc.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/cdefs.h>

#include "private/WriteProtected.h"
#include "private/bionic_allocator.h"
#include "private/bionic_elf_tls.h"
#include "private/bionic_fdsan.h"
#include "private/bionic_malloc_dispatch.h"
#include "private/bionic_vdso.h"

struct libc_globals {
  vdso_entry vdso[VDSO_END];
  long setjmp_cookie;
  uintptr_t heap_pointer_tag;
  _Atomic(bool) decay_time_enabled;
  _Atomic(bool) memtag;

  // In order to allow a complete switch between dispatch tables without
  // the need for copying each function by function in the structure,
  // use a single atomic pointer to switch.
  // The current_dispatch_table pointer can only ever be set to a complete
  // table. Any dispatch table that is pointed to by current_dispatch_table
  // cannot be modified after that. If the pointer changes in the future,
  // the old pointer must always stay valid.
  // The malloc_dispatch_table is modified by malloc debug, malloc hooks,
  // and heaprofd. Only one of these modes can be active at any given time.
  _Atomic(const MallocDispatch*) current_dispatch_table;
  // This pointer is only used by the allocation limit code when both a
  // limit is enabled and some other hook is enabled at the same time.
  _Atomic(const MallocDispatch*) default_dispatch_table;
  MallocDispatch malloc_dispatch_table;
};

struct memtag_dynamic_entries_t {
  void* memtag_globals;
  size_t memtag_globalssz;
  bool has_memtag_mode;
  unsigned memtag_mode;
  bool memtag_heap;
  bool memtag_stack;
};

__LIBC_HIDDEN__ extern WriteProtected<libc_globals> __libc_globals;
// These cannot be in __libc_globals, because we cannot access the
// WriteProtected in a thread-safe way.
// See b/328256432.
//
// __libc_memtag_stack says whether stack MTE is enabled on the process, i.e.
// whether the stack pages are mapped with PROT_MTE. This is always false if
// MTE is disabled for the process (i.e. libc_globals.memtag is false).
__LIBC_HIDDEN__ extern _Atomic(bool) __libc_memtag_stack;
// __libc_memtag_stack_abi says whether the process contains any code that was
// compiled with memtag-stack. This is true even if the process does not have
// MTE enabled (e.g. because it was overridden using MEMTAG_OPTIONS, or because
// MTE is disabled for the device).
// Code compiled with memtag-stack needs a stack history buffer in
// TLS_SLOT_STACK_MTE, because the codegen will emit an unconditional
// (to keep the code branchless) write to it.
// Protected by g_heap_creation_lock.
__LIBC_HIDDEN__ extern bool __libc_memtag_stack_abi;

struct abort_msg_t;
struct crash_detail_page_t;
namespace gwp_asan {
struct AllocatorState;
struct AllocationMetadata;
};  // namespace gwp_asan

// Globals shared between the dynamic linker and libc.so.
struct libc_shared_globals {
  // Construct the shared globals using a constexpr constructor to ensure that
  // the object doesn't need dynamic initialization. The object is accessed
  // before the dynamic linker has relocated itself.
  constexpr libc_shared_globals() {}

  FdTable fd_table;

  // When the linker is invoked on a binary (e.g. `linker64 /system/bin/date`),
  // record the number of arguments passed to the linker itself rather than to
  // the program it's loading. Typically 0, sometimes 1.
  int initial_linker_arg_count = 0;

  ElfW(auxv_t)* auxv = nullptr;

  pthread_mutex_t abort_msg_lock = PTHREAD_MUTEX_INITIALIZER;
  abort_msg_t* abort_msg = nullptr;

  StaticTlsLayout static_tls_layout;
  TlsModules tls_modules;
  BionicAllocator tls_allocator;

  // Values passed from libc.so to the loader.
  void (*load_hook)(ElfW(Addr) base, const ElfW(Phdr)* phdr, ElfW(Half) phnum) = nullptr;
  void (*unload_hook)(ElfW(Addr) base, const ElfW(Phdr)* phdr, ElfW(Half) phnum) = nullptr;
  void (*set_target_sdk_version_hook)(int target) = nullptr;

  // Values passed from the linker to libc.so.
  const char* init_progname = nullptr;
  char** init_environ = nullptr;

  const gwp_asan::AllocatorState* gwp_asan_state = nullptr;
  const gwp_asan::AllocationMetadata* gwp_asan_metadata = nullptr;
  bool (*debuggerd_needs_gwp_asan_recovery)(void* fault_addr) = nullptr;
  void (*debuggerd_gwp_asan_pre_crash_report)(void* fault_addr) = nullptr;
  void (*debuggerd_gwp_asan_post_crash_report)(void* fault_addr) = nullptr;

  const char* scudo_stack_depot = nullptr;
  const char* scudo_region_info = nullptr;
  const char* scudo_ring_buffer = nullptr;
  size_t scudo_ring_buffer_size = 0;
  size_t scudo_stack_depot_size = 0;

  HeapTaggingLevel initial_heap_tagging_level = M_HEAP_TAGGING_LEVEL_NONE;
  // See comments for __libc_memtag_stack / __libc_memtag_stack_abi above.
  bool initial_memtag_stack = false;
  bool initial_memtag_stack_abi = false;
  int64_t heap_tagging_upgrade_timer_sec = 0;

  void (*memtag_stack_dlopen_callback)() = nullptr;
  pthread_mutex_t crash_detail_page_lock = PTHREAD_MUTEX_INITIALIZER;
  crash_detail_page_t* crash_detail_page = nullptr;
};

__LIBC_HIDDEN__ libc_shared_globals* __libc_shared_globals();
__LIBC_HIDDEN__ bool __libc_mte_enabled();
__LIBC_HIDDEN__ void __libc_init_mte(const memtag_dynamic_entries_t*, const void*, size_t,
                                     uintptr_t);
__LIBC_HIDDEN__ void __libc_init_mte_stack(void*);
__LIBC_HIDDEN__ void __libc_init_fdsan();
__LIBC_HIDDEN__ void __libc_init_fdtrack();
__LIBC_HIDDEN__ void __libc_init_profiling_handlers();

__LIBC_HIDDEN__ void __libc_init_malloc(libc_globals* globals);
__LIBC_HIDDEN__ void __libc_init_setjmp_cookie(libc_globals* globals);
__LIBC_HIDDEN__ void __libc_init_vdso(libc_globals* globals);

#if defined(__i386__)
__LIBC_HIDDEN__ extern void* __libc_sysinfo;
extern "C" __LIBC_HIDDEN__ void __libc_int0x80();
__LIBC_HIDDEN__ void __libc_init_sysinfo();
#endif

#endif
