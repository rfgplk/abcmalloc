<img align="left" style="width:300px" src="https://github.com/user-attachments/assets/2a9138b7-c521-4a32-b0c5-e5715627e88f" alt="abc_logo" width="300"/> 

<div align="left">

### *abcmalloc*  <img src="https://img.shields.io/badge/version-indev-green">

#### a modern memory allocator
abcmalloc is a c++23 memory allocator designed to deliver best-in-class performance while maintaining strong security guarantees. it is extensively tested, engineered for strong cache locality, and provides a buddy list–based allocation scheme for efficient space management and rapid block splitting & merging.

</div>



------

<br/>

<br/>

API
----
```cpp
// all main public facing functions are found within src/malloc.hpp
bool is_present(addr_t* ptr); // checks if the given pointer has been allocated
bool within(addr_t* ptr); // checks if pointer if technically addressable at any known page owned by abcmalloc
void relinquish(btre* ptr); // unmaps entire sheet on which the pointer lives (unsafe, use with caution)
chunk<byte> balloc(size_t size); // allocates memory, return chunk { ptr, len }
chunk<byte> fetch(size_t size); // equivalent as balloc
void retire(byte* ptr); // frees (tombstones) memory

byte* alloc(size_t size); // allocates memory, identical to ::malloc
void dealloc(byte* ptr); // frees memory, identical to ::free
void dealloc(byte *ptr, size_t len); // frees memory with a length argument
void freze(byte* ptr); // freezes memory (invokes mlock)
byte* launder(size_t size); // launders memory (allocates any address, even if already occupied - use for immutable structures only)
size_t query_size(byte* ptr); // queries the underlying allocated size of pointer

// libc legacy compatibility
void* malloc(size_t size); // ::malloc
void* calloc(size_t num, size_t size); // ::calloc
void* realloc(void* ptr, size_t size); // ::realloc
void free(void* ptr); // ::free
```



Features
--------
 - **small code size** for rapid compilation (roughly 98kb of source)
 - extremely fast memory allocations and deallocations
 - innovative **stack-like allocator design** (newest allocations are always located at the end of the vmap segment)
 - extensive hardening and security features
 - novel mechanisms such as **memory laundering** and active metadata storage
 - modular design for flexible integration
 - always returns **aligned addresses**
 - strong cache locality considerations
 - supports both per-thread and global lock allocation modes
 - immutable mode enabled via laundering

Is This Usable Out of the Box?
------------------------------

Yes, fully. The standard `config.hpp` file provided is pretuned for general amd64 workstations with sufficient RAM size (>16 GB), while the `config_embed.hpp` file is pretuned for embedded platforms (or platforms with memory constrains (<256 MB)). You might need to fine tune certain settings if you desire optimal performance.


Where is LD_PRELOAD Support?
-----------------------------

Haven't gotten around to adding compiler settings for it yet. Should be fairly simple to build a simple `*.so` file without much hassle. Otherwise, it's generally preferable to call `abc::` namespaced functions.


Where is C/zig/Rust Support?
-----------------------------

No plans to make bindings and/or ports for/to those languages any time soon, sorry.


Performance Metrics
-------------------
- achieves **~66% average** effective memory utilization, approaching ~100% for page-multiple allocations (4096 bytes)
- provides consistent memory allocation latency across workloads.
- on average, **15–20% higher throughput** compared to contemporary high-performance allocators.
- primarily optimized for workstation environments (16-256gb of working physical memory), configurable for embedded machines

Use Cases
----------

abcmalloc is best suited for:

 - designs where intricate control of the allocator is desired
 - workloads requiring high throughput
 - workloads requiring consistent aligned addresses
 - workloads spanning a wide random range of allocated sizes
 - systems needing modular management with strong safety guarantees


Motivation
----------

<p align="justify"> 

*abcmalloc* was developed as part of the *micron standard library* as a modern memory allocator that prioritizes a small and modular codebase, making it easy to integrate, audit, and maintain across diverse systems. it leverages c++23 features and modern development practices to provide type safety, strict memory guarantees, and efficient resource management. the allocator is designed to deliver high performance through optimizations for throughput, cache locality, and large allocations, while also incorporating strong security measures, including memory laundering, immutable allocation modes, and hardened metadata handling. its modular and extensible design allows adaptation for workloads ranging from embedded systems to high-performance workstations, and it is rigorously tested to ensure reliability under diverse allocation patterns and stress conditions. by combining performance, safety, and modern engineering, abcmalloc aims to address the limitations of legacy allocators while providing a versatile and trustworthy memory management solution.

<p align="justify"> 

> [!WARNING]
> abcmalloc has a **strong requirement** that all memory requested from the kernel is *immediately accessible* and *fully addressable*. to ensure correct behavior, it strongly recommends configuring the kernel with:
> ``` vm.overcommit_memory = 2 ```

Drawbacks and Limitations
--------------------------
- underperforms on numerous small (<256–512 bytes) and scattered allocations
- optimized for page-sized allocations (multiples of 4096 bytes)
- may waste memory or incur higher fragmentation for workloads dominated by tiny objects
- *abcmalloc* depends on the *micron standard library* as it's only dependency
  
 

## License
Licensed under the MIT License.
