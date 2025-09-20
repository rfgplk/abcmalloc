<img align="left" style="width:300px" src="https://github.com/user-attachments/assets/0a2fc934-93dc-448e-860f-56f9334a0946" alt="abc_logo" width="300"/> 

<div align="left">

### *abcmalloc*  <img src="https://img.shields.io/badge/version-indev-green">

#### a modern memory allocator
abcmalloc is a c++23 memory allocator designed to deliver best-in-class performance while maintaining strong security guarantees. it is extensively tested, engineered for strong cache locality, and provides a buddy list–based allocation scheme for efficient space management and rapid block splitting & merging.

</div>



------

<br/>

<br/>



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

Performance Metrics
-------------------
- achieves **~66% average** effective memory utilization, approaching ~100% for page-multiple allocations (4096 bytes)
- provides consistent memory allocation latency across workloads.
- on average, **15–20% higher throughput** compared to contemporary high-performance allocators.
- primarily optimized for workstation environments (16-256gb of working physical memory), configurable for embedded machines

Use Cases
----------

abcmalloc is best suited for:

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
> abcmalloc has a strong requirement that all memory requested from the kernel is *immediately accessible* and *fully addressable*. to ensure correct behavior, it strongly recommends configuring the kernel with:
> ``` vm.overcommit_memory = 2 ```

Drawbacks and Limitations
--------------------------
- underperforms on numerous small (<256–512 bytes) and scattered allocations
- optimized for page-sized allocations (multiples of 4096 bytes)
- may waste memory or incur higher fragmentation for workloads dominated by tiny objects
- *abcmalloc* depends on the *micron standard library* as it's only dependency
  
 

## License
Licensed under the MIT License.
