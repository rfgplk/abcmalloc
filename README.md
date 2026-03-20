<img align="left" width="300" src="https://github.com/user-attachments/assets/2a9138b7-c521-4a32-b0c5-e5715627e88f" alt="abc_logo"/>

### *abcmalloc* 🐊
<img src="https://img.shields.io/badge/version-indev-green">

#### a modern memory allocator
abcmalloc is a C++23 memory allocator designed to deliver best-in-class performance while maintaining strong security guarantees. It combines **TLSF (Two-Level Segregated Fit)** for small allocations with a **buddy block allocator** for larger allocations, enabling constant-time small-object allocation while preserving efficient block splitting and merging for large memory regions.

<br clear="left"/>

![Version](https://img.shields.io/badge/version-1.0.2-blue)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++23](https://img.shields.io/badge/C++-23-blue.svg)](https://en.cppreference.com/w/cpp/23)

---

# API

```cpp
// presence / provenance checks
bool is_present(addr_t* ptr);
bool is_present(byte* ptr);

template <typename T>
bool is_present(T* ptr);

bool within(const addr_t* ptr);
bool within(addr_t* ptr);
bool within(byte* ptr);

// page operations
void relinquish(byte* ptr);

template <typename T>
void relinquish(T* ptr);

// allocation primitives
chunk<byte> balloc(size_t size);
chunk<byte> fetch(size_t size);

template <typename T>
T* fetch(void);

// freeing
void retire(byte* ptr);

template <typename T>
void retire(T* ptr);

// malloc-style API
byte* alloc(size_t size);
byte* salloc(size_t size);

void dealloc(byte* ptr);
void dealloc(byte* ptr, size_t len);

template <typename T>
void dealloc(T* ptr);

template <typename T>
void dealloc(T* ptr, size_t len);

// memory policies
void freeze(byte* ptr);

template <typename T>
void freeze(T* ptr);

// laundering
byte* launder(size_t size);

// allocator inspection
size_t query_size(byte* ptr);

template <typename T>
size_t query_size(T* ptr);

// usage statistics
size_t musage(void);

template <u64 Sz>
size_t musage(void);

// libc compatibility
void* malloc(size_t size);
void* calloc(size_t num, size_t size);
void* realloc(void* ptr, size_t size);
void free(void* ptr);
void* aligned_alloc(size_t alignment, size_t size);
```

---


# Features

 - small code size (~188 KB source)
 - hybrid TLSF + buddy allocator architecture
 - extremely fast allocations and deallocations
 - stack-like arena locality model
 - strong cache locality
 - modular design for integration
 - metadata stored outside allocation memory
 - memory laundering support
 - tombstoning support
 - guard pages
 - optional sanitization
 - configurable security hardening
 - aligned addresses always returned
 - support for thread-local or global allocator modes


Security and Memory Features
------------------------------

## Tombstoning

Tombstoning prevents previously freed memory from being reused.

When tombstoning is enabled, freed allocations remain permanently invalid. The allocator will not reuse freed memory until the page containing that memory has been unmapped and returned to the system.

Advantages:

 - mitigates use-after-free vulnerabilities
 - improves debugging determinism
 - prevents temporal memory reuse attacks

Tradeoffs:

 - increased memory consumption
 - potential fragmentation under heavy allocation churn

---

## Memory Laundering

Memory laundering allows allocations to reuse identical virtual addresses intentionally.

The laundering API allows requesting memory in a way that bypasses typical reuse restrictions, permitting the allocator to return addresses that were previously occupied.

Typical use cases:

 - immutable graph structures
 - persistent data layouts
 - pointer-stable data structures
 - structural sharing systems

This feature should only be used for immutable memory regions because it weakens temporal safety guarantees.


Performance Metrics
-----------------------

 - approximately **66 percent average effective memory utilization**
 - near **100 percent utilization for page-sized allocations**
 - **15 to 20 percent higher throughput** than many common allocators (extensive benchmarks will soon be available)
 - constant-time latency for small allocations
 - strong cache locality across allocation patterns

abcmalloc is primarily optimized for workstation-class machines with **16 GB to 256 GB of RAM**, but configuration options allow adaptation to embedded systems.


Most Important Configuration Flags
------------------------------------

Allocator behavior is primarily controlled through compile-time constants defined in the configuration headers.

Important flags include:

```cpp
constexpr static const bool __is_constrained = false;
// Enables a constrained-memory operating mode. When true the allocator enforces
// __alloc_limit and becomes more conservative about page growth. Intended for
// embedded systems or environments where total RAM must be strictly bounded.

constexpr static const bool __default_launder = false;
// Enables address laundering globally. When true the allocator may intentionally
// return the same virtual address for multiple allocations of identical size.
// Useful for immutable structures that benefit from address stability. Unsafe
// for mutable objects because temporal safety guarantees are weakened.

constexpr static const bool __default_lazy_construct = true;
// Buckets and allocation classes are initialized only when first used.
// Reduces startup cost and memory footprint. Recommended for most workloads
// unless predictable initialization latency is required.

constexpr static const bool __default_single_instance = true;
// Enables a single allocator instance per thread. Improves locality and reduces
// lock contention. Appropriate for thread-heavy workloads. Deprecated in the
// current design but retained for compatibility.

constexpr static const bool __default_global_instance = false;
// Enables a single global allocator shared across threads. Useful when memory
// must be centrally managed or when deterministic allocation order across
// threads is required. Cannot be enabled simultaneously with __default_single_instance.

constexpr static const bool __default_multithread_safe = true;
// Enables locking around allocator API calls. Required when allocations may
// occur concurrently across threads. Disabling improves throughput but makes
// the allocator unsafe in multi-threaded contexts.

constexpr static const bool __default_saturated_mode = true;
// Enables adaptive saturation buffering. The allocator monitors allocation
// request rates and adjusts page provisioning to reduce repeated kernel
// mappings during bursts of allocations. Useful for high-throughput workloads.

constexpr static const bool __default_init_large_pages = false;
// Enables large-page initialization when supported by the system. Can improve
// TLB efficiency and reduce page-table overhead for large allocations. May
// increase startup latency and requires OS support.

constexpr static const bool __default_oom_enable = true;
// Enables out-of-memory monitoring and mitigation logic. Allows the allocator
// to detect approaching memory pressure and react before hard failure. Adds
// minor runtime overhead but improves robustness.

constexpr static const bool __default_borrow_auto = true;
// Enables automatic borrowing of memory pages between allocation classes when
// one class becomes exhausted. Improves allocator flexibility under uneven
// allocation distributions.

constexpr static const bool __default_enforce_provenance = false;
// Forces the allocator to verify that every pointer passed to free/dealloc was
// originally allocated by this allocator instance. Detects invalid frees and
// cross-allocator pointer misuse. Costs performance and therefore disabled by
// default.

constexpr static const bool __default_tombstone = true;
// Prevents freed memory from being reused for new allocations. The memory
// remains permanently invalid until its page is unmapped. Improves temporal
// safety and debugging determinism at the cost of higher memory consumption.

constexpr static const bool __default_insert_guard_pages = true;
// Inserts protected guard pages between allocation regions. Detects buffer
// overflows and out-of-bounds pointer traversal. Increases virtual memory
// usage but greatly improves memory safety diagnostics.

constexpr static const bool __default_self_cleanup = true;
// Ensures allocator pages are explicitly released during program shutdown
// instead of relying on the kernel to reclaim memory. Useful in environments
// where deterministic teardown behavior is required.

constexpr static const bool __default_debug_notices = false;
// Enables verbose debugging diagnostics emitted by the allocator. Intended
// for development builds and allocator debugging. Should remain disabled in
// production because of significant performance impact.

constexpr static const bool __default_zero_on_alloc = false;
// Clears newly allocated memory with zeros before returning it to the caller.
// Improves safety for programs relying on zero-initialized memory but adds
// measurable allocation overhead.

constexpr static const bool __default_zero_on_free = false;
// Clears memory when it is freed. Helps detect use-after-free reads and
// prevents stale data leakage. Costs additional runtime overhead.

constexpr static const bool __default_full_on_free = false;
// Fills freed memory with a fixed byte pattern instead of zeroing it. Useful
// for debugging memory corruption or dangling pointer access.

constexpr static const bool __default_sanitize = false;
// Enables allocator-level sanitization passes. When enabled the allocator
// fills allocations with a defined pattern to help detect uninitialized reads
// and stale pointer accesses. Significantly impacts performance.

constexpr static const bool __default_collect_stats = false;
// Enables runtime statistics collection such as allocation counts and memory
// utilization metrics. Useful for profiling allocator behavior but introduces
// additional bookkeeping overhead.
```


Is This Usable Out of the Box?
---------------------------------

Yes.

Two configuration presets are provided.

**config.hpp**

Optimized for workstation-class machines with large RAM capacities.

**config_embed.hpp**

Optimized for memory-constrained environments such as embedded platforms.

Some tuning may improve performance depending on workload characteristics.


Where is LD_PRELOAD Support?
-------------------------------

LD_PRELOAD support is not currently included in the build configuration. However, it can be added by compiling the allocator as a shared library and exporting the libc allocation symbols.


Where is C / Zig / Rust Support?
------------------------------------

No language bindings currently exist. The allocator is intended primarily for use within the **micron standard library** ecosystem.


Drawbacks and Limitations
-------------------------

 - costlier startup penalties due to arena initialization
 - optimized for page-sized allocations (multiples of 4096 bytes)
 - may waste memory or incur higher fragmentation for workloads dominated by tiny objects
 - *abcmalloc* depends on the *micron standard library* as it's only dependency


Use Cases
-------------

abcmalloc is well suited for:

 - custom runtime environments
 - deterministic memory management systems
 - high-throughput allocation workloads
 - game engines
 - embedded runtimes with controlled memory policies


Motivation
-----------------

<p align="justify"> 

abcmalloc was developed as part of the **micron standard library** to provide a modern memory allocator that emphasizes minimal code size, strong safety guarantees, deterministic allocation behavior, and modern C++23 integration.

The design prioritizes auditability, modularity, and predictable performance while remaining adaptable to workloads ranging from embedded systems to large workstation environments.

</p align="justify"> 


> [!WARNING]
> abcmalloc has a **strong requirement** that all memory requested from the kernel is *immediately accessible* and *fully addressable*. to ensure correct behavior, it strongly recommends configuring the kernel with:
> ``` vm.overcommit_memory = 2 ```

---

# License

Licensed under the MIT License.
