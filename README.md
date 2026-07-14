<img align="left" width="300" src="https://github.com/user-attachments/assets/2a9138b7-c521-4a32-b0c5-e5715627e88f" alt="abc_logo"/>

### *abcmalloc* 🐊

#### a deterministic, low-latency memory allocator

<div align="left">

**abcmalloc** is a header-only C++23 general-purpose allocator built for **realtime and high-throughput systems**, while maintaining strong security guarantees. It pairs a **TLSF (Two-Level Segregated Fit)** front end for small objects alongside a **buddy block allocator** for larger regions; enabling bounded, constant-time small-object allocation alongside efficient splitting/coalescing of large blocks

</div>

<br clear="left"/>

[![Linux](https://img.shields.io/badge/Linux-FCC624?logo=linux&logoColor=black)](#)
![Version](https://img.shields.io/badge/version-2.0.1-blue)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++23](https://img.shields.io/badge/C++-23-blue.svg)](https://en.cppreference.com/w/cpp/23)

------

> [!WARNING]
> abcmalloc is part of the actively-developed *micron* core library; the ABI may change without notice. It also requires that memory requested from the kernel is *immediately accessible and fully addressable*; configure the kernel with `vm.overcommit_memory = 2`

#### Features
  - hybrid **TLSF + buddy + mmap** architecture: constant-time small allocs, coalescing large blocks, direct mapping for huge regions
  - **flat latency distribution**: p10…p99.9 cluster within a few nanoseconds, with a near-zero (≈0.00%) branch-misprediction rate and ~3.8 IPC on the hot path
  - **near-linear multithreaded scaling**: per-thread arenas, no lock on the owning-thread fast path, lock-free MPSC cross-thread frees
  - a **per-class free cache** (LIFO) and eagerly-warmed hot tiers for fast repeated allocation
  - **guard pages**, per-tier **tombstoning**, double-free detection, with opt-in provenance enforcement, redzone sanitization and zero-on-alloc/free
  - temporal-allocation (`launder`) and tombstone-free (`retire`) primitives for pointer-stable / hardened data structures
  - header-only, freestanding-capable, depends only on the *micron* core library
  - thread-local or global allocator modes; libc drop-in (`malloc`/`free`/...) and an STL-style allocator wrapper

------

##### Design

abcmalloc routes every request to a size tier (thresholds from `config_amd64.hpp`):

| tier      | size range        | strategy                |
|-----------|-------------------|-------------------------|
| precise   | 1 – 256 B         | TLSF                    |
| small     | 257 – 512 B       | TLSF                    |
| medium    | 513 B – 4 KiB     | TLSF                    |
| large     | 4 K – 32 KiB      | buddy                   |
| huge      | 32 K – 256 KiB    | buddy                   |
| 1mb       | 256 K – 1 MiB     | buddy                   |
| gb        | 1 MiB – 512+ GiB  | buddy                   |


##### Latency & realtime suitability

abcmalloc is built so the *distribution*, not just the mean, is predictable.

  - **Flat percentiles.** On the hot path the per-op latency is tightly bounded: e.g. for 1–32 B round-trips, p10 ≈ 6 ns, p50 ≈ 7 ns, p90 ≈ 8 ns, p99 ≈ 8–12 ns, p99.9 ≈ 9–18 ns. The only outliers are unavoidable first-touch page faults (shared by every allocator).
  - **Near-zero branch misprediction.** Measured branch-miss rate is ≈ **0.00%** across pathways (vs ~1–2% for glibc/mimalloc/jemalloc) at ~3.8 instructions/cycle
  - **Bounded by construction.** TLSF gives O(1) small-object placement; the buddy allocator bounds large-block work; tier routing is a handful of comparisons.

##### Benchmarks

(fill this out later properly)
(i will fill this out later i promise, benches live at benches/ if you're curious)

##### Safety guarantees

Default posture (no flags required):

  - **Guard pages** (`PROT_NONE`) between allocation regions catch overflows and out-of-bounds traversal.
  - **Per-tier tombstoning** on large/huge tiers — freed blocks are not handed back until their page is unmapped, trapping use-after-free where it matters most.
  - **Double-free detection** — repeated/foreign frees are rejected rather than corrupting the heap.
  - `salloc` / `calloc` return **zero-initialised** memory; `calloc` / `aligned_alloc` are **overflow-checked**.
  - Cross-thread frees are routed safely via the lock-free MPSC queue (no shared-arena races).

Opt-in hardening (compile-time flags):

  - **Provenance enforcement** (`__default_enforce_provenance`) — verify every freed pointer was allocated by this allocator.
  - **Redzone sanitization** (`__default_sanitize`), **zero-on-alloc / zero-on-free**, fill-on-free patterns.
  - **Tombstoning on every tier**, **read-only freeze** of live regions (`freeze`), temporal-only allocation (`launder`).

##### Doctor mode (forensic debugging)

Compile any translation unit with **`-DABCMALLOC_DOCTOR_HELP`** to activate *doctor mode*; an opt-in, stateful forensic layer that is compiled out entirely (zero overhead) when the flag is absent. It records allocations, installs fault handlers, and turns latent heap bugs into precise, gdb-style diagnostics:

  - **crash-safe fault handling**: guarded faults (overflow into guard pages, wild/foreign pointers) are caught and reported instead of taking down the process.
  - **double-free / bad-free forensics**: offending frees are rejected and reported with their allocation context.
  - **off-heap ledger**: the recording state lives outside the allocator's own VA range, so it cannot be corrupted by the very bug it is diagnosing.

##### Testing & validation

  - **`tests/core/`**: focused unit tests: alloc/free round-trips, arena internals, immediate reuse, size introspection (`abcmalloc_info`), leak accounting, and a broad vetting pass (`abcmalloc_vet`).
  - **`tests/rigor/`**: single-threaded correctness & soak batteries: `abcmalloc.cpp` (tier routing, alignment, provenance, redzones, tombstones, freezes), `abcmalloc_sizes.cpp` (exhaustive size-class coverage), `abc_overlap_probe.cpp` + `abcmalloc_realloc.cpp` (realloc semantics / in-place overlap regression), `abcmalloc_stress.cpp` (exotic/nested patterns), `abcmalloc_persistent.cpp` (pointer-stable / temporal primitives), and `abcmalloc_soak.cpp` / `abcmalloc_soak_serial_bulk.cpp` (long-running deterministic soaks). The soak/realloc tests share the `abc_rigor` harness, built with `-DABC_RIGOR_ST_ONLY` to gate out its multi-threaded worker machinery.
  - **`tests/doctor/`**: forensic-layer self-tests built with `-DABCMALLOC_DOCTOR_HELP` (see *Doctor mode*): crash-safe recovery (`selftests`), fault and overflow trapping (`faults`, `overflow`), structure dumps (`structdump`), wild-pointer handling (`wild`).

Build and run them with `ninja abcmalloc_tests` (or `abcmalloc_core` / `abcmalloc_rigor` / `abcmalloc_doctor`); exit `1` == pass. The *multi-threaded* rigor batteries (`concurrent`, `mt`, `soak_mt`, `arena_recycle`) live only in the parent *micron* tree.

------

##### API

All entry points live in `namespace abc`. Types are *micron* core types (`byte`, `usize`, `micron::__chunk<byte>`).

```cpp
namespace abc {

// core (malloc-style)
byte *alloc(usize size);                    // malloc; nullptr on size 0
byte *salloc(usize size);                   // hardened alloc, zero-initialised
void  dealloc(byte *ptr);                   // free (size looked up)
void  dealloc(byte *ptr, usize len);        // free with explicit, hard-checked size
void *realloc(void *ptr, usize size);       // grow/shrink, may move
template <typename T> void dealloc(T *ptr);
template <typename T> void dealloc(T *ptr, usize len);

// chunk API (returns {ptr, actual_capacity})
micron::__chunk<byte> balloc(usize size);
micron::__chunk<byte> fetch(usize size);
template <typename T> T *fetch();           // one trivially-constructible T

// temporal & safety extensions
byte *launder(usize size);                  // temporal alloc
void  retire(byte *ptr);                    // tombstone free (use-after-free trap)
void  freeze(byte *ptr);                    // make a live region read-only
void  relinquish(byte *ptr);               // unmap the whole sheet ptr lives on
template <typename T> void retire(T *ptr);
template <typename T> void freeze(T *ptr);
template <typename T> void relinquish(T *ptr);

// aligned
void *aligned_alloc(usize alignment, usize size);   // alignment must be a power of two
void  aligned_free(void *ptr);                       // REQUIRED when alignment > 32 B

// introspection
template <typename T> usize query_size(T *ptr);      // actual allocated size
bool  is_present(byte *ptr);                          // allocated & live?
bool  within(byte *ptr);                              // owned by this allocator?
usize musage();                                       // total bytes in use
template <u64 Sz> usize musage();                     // bytes in one size class
void  which();                                        // per-tier usage report (debug)

// external-memory provenance
byte *mark_at(byte *ptr, usize size);                // track externally-mapped memory
byte *unmark_at(byte *ptr, usize size);

} // namespace abc

// libc drop-in (active unless ABCMALLOC_DISABLE is defined)
void *malloc(usize size);
void *calloc(usize num, usize size);
void *realloc(void *ptr, usize size);
void free(void *ptr);
void *aligned_alloc(usize alignment, usize size);
```

##### Configuration

Behavior is set through compile-time constants in `config_amd64.hpp` (workstation) / `config_embed.hpp` (constrained). Both presets are usable out of the box. The most important defaults:

```cpp
__default_multithread_safe   = true;   // per-arena concurrency safety (off in freestanding)
__default_per_class_free_cache = true; // LIFO free cache on hot tiers for fast reuse
__default_eager_hot_tiers    = true;   // pre-warm precise/small/medium
__default_insert_guard_pages = true;   // PROT_NONE guard pages between regions
__default_tombstone (large/huge only)  // cold-tier use-after-free trapping
__default_saturated_mode     = true;   // adapt page provisioning to request bursts
__default_launder            = false;  // global address laundering (immutable structures)
__default_enforce_provenance = false;  // verify every free belongs to this allocator
__default_zero_on_alloc      = false;  // clear memory on allocation
__default_zero_on_free       = false;  // clear memory on free
__default_sanitize           = false;  // redzone/uninit-read detection patterns
__default_oom_enable         = false;  // OOM pressure monitoring (costs performance)
```

See `config_amd64.hpp` for the complete, documented flag set (tier sheet caps, cache depths, OOM thresholds, fail policy, etc.).

##### Building & integration

Header-only. With the *micron* core headers reachable as `<micron/...>` (an installed *micron*, e.g. `/usr/include/micron`, or `-I` a checkout), add `-Isrc` and include the umbrella **`cmalloc.hpp`**, then use `abc::alloc` / `abc::dealloc`. `cmalloc.hpp` is the canonical entry point: it defines `MICRON_ABCMALLOC_DISABLE_STD` (so *micron* core uses this allocator rather than pulling its own copy) and installs the libc drop-ins (`malloc`/`free`/...) unless `ABCMALLOC_DISABLE` is set. The bundled tests/benches build with `ninja` (see `build.ninja`).

  - **LD_PRELOAD** is not wired into the build; it can be added by compiling the allocator as a shared object that exports the libc allocation symbols.
  - **Language bindings** (C / Rust / Zig) do not yet exist; but they will.

##### Limitations

  - first allocation on a thread pays a one-time arena-initialization cost
  - slightly underperforms on workloads dominated by tiny round-trip allocations
  - more than `__max_arenas` (64) genuinely-concurrent allocator threads fall back to a shared arena; keep concurrent threads ≤ 64
  - depends on the *micron* core library as its sole dependency

------

#### License

Licensed under the MIT License.
