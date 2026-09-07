// Hosted allocator comparison suite.  Every allocator is called through its
// own exported symbol; the process malloc namespace is never used by a test
// kernel, so linking several allocator libraries cannot silently interpose.
#define ABCMALLOC_DISABLE 1

#include "bench_common.hpp"
#include "../src/cmalloc.hpp"

static_assert(!abc::__default_multithread_safe, "single-thread comparison must compile with allocator locks disabled");

#include <algorithm>
#include <cstddef>
#include <cstdint>

extern "C" void *__libc_malloc(std::size_t) noexcept;
extern "C" void __libc_free(void *) noexcept;
extern "C" void *mi_malloc(std::size_t) noexcept;
extern "C" void mi_free(void *) noexcept;
extern "C" void *mallocx(std::size_t, int) noexcept;
extern "C" void dallocx(void *, int) noexcept;
extern "C" void *tc_malloc(std::size_t) noexcept;
extern "C" void tc_free(void *) noexcept;
extern "C" void *scalable_malloc(std::size_t) noexcept;
extern "C" void scalable_free(void *) noexcept;

namespace
{

using namespace abcmalloc_bench;
using byte = unsigned char;
using allocate_fn = void *(*)(std::size_t) noexcept;
using free_fn = void (*)(void *) noexcept;

constexpr u64 MAX_COUNT = 8192;
constexpr u64 LATENCY_SAMPLES = 256;
constexpr u64 LIVE_BUDGET = 32ULL << 20;
constexpr u64 COUNTS[] = { 256, 2048, 8192 };

struct category {
  const char *name;
  std::size_t lo;
  std::size_t hi;
};

constexpr category CATEGORIES[] = {
  { "precise", 1, 256 }, { "small", 257, 512 }, { "medium", 513, 4096 }, { "large", 4097, 32768 },
  { "huge", 32769, 262144 },
};

struct allocator {
  const char *name;
  allocate_fn allocate;
  free_fn deallocate;
  bool launder;
};

struct outcome {
  sample throughput{};
  percentiles latency{};
  u64 latency_count = 0;
};

alignas(64) std::size_t g_sizes[MAX_COUNT]{};
alignas(64) void *g_ptrs[MAX_COUNT]{};
alignas(64) u64 g_order[MAX_COUNT]{};
alignas(64) u64 g_latencies[LATENCY_SAMPLES]{};
volatile u64 g_sink = 0;
double g_ns_per_tick = 1.0;

void *abc_allocate(std::size_t size) noexcept { return abc::alloc(size); }
void abc_free(void *pointer) noexcept { abc::dealloc(static_cast<byte *>(pointer)); }
void *abc_launder(std::size_t size) noexcept { return abc::launder(size); }
void *glibc_allocate(std::size_t size) noexcept { return __libc_malloc(size); }
void glibc_free(void *pointer) noexcept { __libc_free(pointer); }
void *mimalloc_allocate(std::size_t size) noexcept { return mi_malloc(size); }
void mimalloc_free(void *pointer) noexcept { mi_free(pointer); }
void *jemalloc_allocate(std::size_t size) noexcept { return mallocx(size, 0); }
void jemalloc_free(void *pointer) noexcept { dallocx(pointer, 0); }
void *tcmalloc_allocate(std::size_t size) noexcept { return tc_malloc(size); }
void tcmalloc_free(void *pointer) noexcept { tc_free(pointer); }
void *tbbmalloc_allocate(std::size_t size) noexcept { return scalable_malloc(size); }
void tbbmalloc_free(void *pointer) noexcept { scalable_free(pointer); }

allocator ALLOCATORS[] = {
  { "abc", abc_allocate, abc_free, false },       { "glibc", glibc_allocate, glibc_free, false },
  { "mimalloc", mimalloc_allocate, mimalloc_free, false }, { "jemalloc", jemalloc_allocate, jemalloc_free, false },
  { "tcmalloc", tcmalloc_allocate, tcmalloc_free, false }, { "tbbmalloc", tbbmalloc_allocate, tbbmalloc_free, false },
};

allocator ABC_LAUNDER = { "abc-launder", abc_launder, abc_free, true };

u64
next_random(u64 &state) noexcept
{
  state += 0x9E3779B97F4A7C15ULL;
  u64 value = state;
  value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
  value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
  return value ^ (value >> 31);
}

void
fill_sizes(const category &category, u64 count, u64 seed) noexcept
{
  for ( u64 i = 0; i < count; ++i )
    g_sizes[i] = category.lo + next_random(seed) % (category.hi - category.lo + 1);
}

void
fill_order(u64 count, u64 seed) noexcept
{
  for ( u64 i = 0; i < count; ++i ) g_order[i] = i;
  for ( u64 i = count; i > 1; --i ) {
    const u64 j = next_random(seed) % i;
    std::swap(g_order[i - 1], g_order[j]);
  }
}

u64
bounded_count(const category &category, u64 requested) noexcept
{
  const u64 maximum = LIVE_BUDGET / category.hi;
  return std::max<u64>(1, std::min<u64>({ requested, MAX_COUNT, maximum }));
}

template<typename One>
percentiles
latency_of(One &&one, u64 requested, u64 &count) noexcept
{
  count = std::min<u64>(LATENCY_SAMPLES, std::max<u64>(1, requested));
  for ( u64 i = 0; i < count; ++i ) {
    const u64 begin = ticks();
    one(i);
    const u64 end = ticks();
    g_latencies[i] = end - begin;
  }
  return summarize(g_latencies, count);
}

outcome
finish(sample throughput, percentiles latency, u64 latency_count) noexcept
{
  return { throughput, latency, latency_count };
}

percentiles
latency_for_bulk_phase(const allocator &allocator, u64 count, bool free_phase, bool randomized, u64 &written) noexcept
{
  written = std::min<u64>(LATENCY_SAMPLES, std::max<u64>(1, count));
  if ( free_phase ) {
    for ( u64 i = 0; i < count; ++i ) {
      g_ptrs[i] = allocator.allocate(g_sizes[i]);
      clobber(g_ptrs[i]);
    }
    for ( u64 i = 0; i < written; ++i ) {
      const u64 index = randomized ? g_order[i] : i;
      const u64 begin = ticks();
      allocator.deallocate(g_ptrs[index]);
      const u64 end = ticks();
      g_ptrs[index] = nullptr;
      g_latencies[i] = end - begin;
    }
    for ( u64 i = 0; i < count; ++i ) {
      if ( g_ptrs[i] != nullptr ) allocator.deallocate(g_ptrs[i]);
      g_ptrs[i] = nullptr;
    }
  } else {
    for ( u64 i = 0; i < written; ++i ) {
      const u64 begin = ticks();
      void *pointer = allocator.allocate(g_sizes[i]);
      const u64 end = ticks();
      clobber(pointer);
      allocator.deallocate(pointer);
      g_latencies[i] = end - begin;
    }
  }
  return summarize(g_latencies, written);
}

outcome
run_hot(const allocator &allocator, const category &category, u64 count, u64 seed) noexcept
{
  fill_sizes(category, count, seed);
  auto one = [&](u64 i) {
    void *pointer = allocator.allocate(g_sizes[i % count]);
    clobber(pointer);
    allocator.deallocate(pointer);
  };
  const sample throughput = measure([&]() {
    for ( u64 i = 0; i < count; ++i ) one(i);
  }, 2 * count);
  u64 latency_count = 0;
  const percentiles latency = latency_of(one, count, latency_count);
  return finish(throughput, latency, latency_count);
}

struct bulk_outcome {
  outcome allocate;
  outcome deallocate;
};

bulk_outcome
run_bulk(const allocator &allocator, const category &category, u64 count, u64 seed, bool randomized) noexcept
{
  fill_sizes(category, count, seed);
  if ( randomized ) fill_order(count, seed ^ 0xA5A5A5A5A5A5A5A5ULL);
  auto alloc_all = [&]() {
    for ( u64 i = 0; i < count; ++i ) {
      g_ptrs[i] = allocator.allocate(g_sizes[i]);
      clobber(g_ptrs[i]);
    }
  };
  auto free_all = [&]() {
    for ( u64 i = 0; i < count; ++i ) {
      const u64 index = randomized ? g_order[i] : i;
      allocator.deallocate(g_ptrs[index]);
      g_ptrs[index] = nullptr;
    }
  };
  const sample alloc_sample = measure_lifecycle([] {}, alloc_all, free_all, count);
  const sample free_sample = measure_lifecycle(alloc_all, free_all, [] {}, count);

  u64 alloc_latencies = 0;
  u64 free_latencies = 0;
  const percentiles alloc_latency = latency_for_bulk_phase(allocator, count, false, randomized, alloc_latencies);
  const percentiles free_latency = latency_for_bulk_phase(allocator, count, true, randomized, free_latencies);
  return { finish(alloc_sample, alloc_latency, alloc_latencies), finish(free_sample, free_latency, free_latencies) };
}

outcome
run_interleaved(const allocator &allocator, const category &category, u64 count, u64 seed) noexcept
{
  fill_sizes(category, count, seed);
  const u64 window = std::max<u64>(16, std::min<u64>(256, count / 2));
  auto clear = [&]() {
    for ( u64 i = 0; i < window; ++i ) {
      if ( g_ptrs[i] != nullptr ) allocator.deallocate(g_ptrs[i]);
      g_ptrs[i] = nullptr;
    }
  };
  auto pass = [&]() {
    u64 state = seed ^ 0x55AA55AA55AA55AAULL;
    for ( u64 i = 0; i < count; ++i ) {
      const u64 slot = next_random(state) % window;
      if ( g_ptrs[slot] != nullptr ) {
        allocator.deallocate(g_ptrs[slot]);
        g_ptrs[slot] = nullptr;
      } else {
        g_ptrs[slot] = allocator.allocate(g_sizes[i]);
        clobber(g_ptrs[slot]);
      }
    }
  };
  const sample throughput = measure_lifecycle([] {}, pass, clear, count);
  auto one = [&](u64 i) {
    const std::size_t size = category.lo + next_random(seed) % (category.hi - category.lo + 1);
    void *pointer = allocator.allocate(size);
    clobber(pointer);
    allocator.deallocate(pointer);
  };
  u64 latency_count = 0;
  const percentiles latency = latency_of(one, count, latency_count);
  return finish(throughput, latency, latency_count);
}

outcome
run_fragmented(const allocator &allocator, const category &category, u64 count, u64 seed) noexcept
{
  fill_sizes(category, count, seed);
  fill_order(count, seed ^ 0x0F0F0F0F0F0F0F0FULL);
  const u64 holes = std::max<u64>(1, count / 2);
  auto setup = [&]() {
    for ( u64 i = 0; i < count; ++i ) {
      g_ptrs[i] = allocator.allocate(g_sizes[i]);
      clobber(g_ptrs[i]);
    }
    for ( u64 i = 0; i < holes; ++i ) {
      allocator.deallocate(g_ptrs[g_order[i]]);
      g_ptrs[g_order[i]] = nullptr;
    }
  };
  auto refill = [&]() {
    for ( u64 i = 0; i < holes; ++i ) {
      const u64 slot = g_order[i];
      g_ptrs[slot] = allocator.allocate(g_sizes[i]);
      clobber(g_ptrs[slot]);
    }
    for ( u64 i = 0; i < holes; ++i ) {
      const u64 slot = g_order[i];
      allocator.deallocate(g_ptrs[slot]);
      g_ptrs[slot] = nullptr;
    }
  };
  auto cleanup = [&]() {
    for ( u64 i = 0; i < count; ++i ) {
      if ( g_ptrs[i] != nullptr ) allocator.deallocate(g_ptrs[i]);
      g_ptrs[i] = nullptr;
    }
  };
  const sample throughput = measure_lifecycle(setup, refill, cleanup, 2 * holes);
  auto one = [&](u64 i) {
    void *pointer = allocator.allocate(g_sizes[i % count]);
    clobber(pointer);
    allocator.deallocate(pointer);
  };
  u64 latency_count = 0;
  const percentiles latency = latency_of(one, holes, latency_count);
  return finish(throughput, latency, latency_count);
}

u64
operations_for(const char *workload, const char *phase, u64 count) noexcept
{
  if ( std::strcmp(phase, "alloc") == 0 || std::strcmp(phase, "free") == 0 ||
       std::strcmp(phase, "bulk-free") == 0 || std::strcmp(phase, "random-free") == 0 )
    return count;
  if ( std::strcmp(workload, "interleaved") == 0 || std::strcmp(workload, "fragmented") == 0 ) return count;
  return 2 * count;
}

void
print_row(const char *workload, const char *phase, const allocator &allocator, const category &category, u64 count,
          const outcome &outcome, double ratio) noexcept
{
  const percentiles &p = outcome.latency;
  std::printf("row suite=comparison mode=st allocator=%s workload=%s phase=%s category=%s size=%zu count=%llu "
              "operations=%llu cycles_per_op=%.2f ns_per_op=%.2f mops_s=%.2f p50_ns=%.2f p90_ns=%.2f "
              "p99_ns=%.2f p999_ns=%.2f max_ns=%.2f ratio=%.4f samples=5 latency_samples=%llu\n",
              allocator.name, workload, phase, category.name, category.hi, static_cast<unsigned long long>(count),
              static_cast<unsigned long long>(operations_for(workload, phase, count)), outcome.throughput.cycles_per_op, outcome.throughput.ns_per_op,
              outcome.throughput.mops, static_cast<double>(p.p50) * g_ns_per_tick, static_cast<double>(p.p90) * g_ns_per_tick,
              static_cast<double>(p.p99) * g_ns_per_tick, static_cast<double>(p.p999) * g_ns_per_tick,
              static_cast<double>(p.max) * g_ns_per_tick, ratio, static_cast<unsigned long long>(outcome.latency_count));
}

double
ratio_to(const outcome &value, const outcome &abc) noexcept
{
  return abc.throughput.cycles_per_op > 0.0 ? value.throughput.cycles_per_op / abc.throughput.cycles_per_op : 0.0;
}

template<typename Run>
void
run_matrix(const char *workload, const category &category, u64 count, u64 seed, Run &&run) noexcept
{
  outcome results[sizeof(ALLOCATORS) / sizeof(ALLOCATORS[0])];
  for ( u64 i = 0; i < sizeof(ALLOCATORS) / sizeof(ALLOCATORS[0]); ++i ) results[i] = run(ALLOCATORS[i], seed);
  for ( u64 i = 0; i < sizeof(ALLOCATORS) / sizeof(ALLOCATORS[0]); ++i )
    print_row(workload, "roundtrip", ALLOCATORS[i], category, count, results[i], i == 0 ? 1.0 : ratio_to(results[i], results[0]));
}

void
run_suite() noexcept
{
  for ( const category &category : CATEGORIES ) {
    u64 previous_count = 0;
    for ( u64 requested : COUNTS ) {
      const u64 count = bounded_count(category, requested);
      if ( count == previous_count ) continue;
      previous_count = count;
      const u64 seed = 0xABC00000ULL ^ (static_cast<u64>(category.hi) << 8) ^ count;
      run_matrix("hot", category, count, seed ^ 0x11, [&](const allocator &a, u64 s) {
        return run_hot(a, category, count, s);
      });

      bulk_outcome bulk[sizeof(ALLOCATORS) / sizeof(ALLOCATORS[0])];
      for ( const allocator &a : ALLOCATORS ) {
        const u64 index = static_cast<u64>(&a - &ALLOCATORS[0]);
        bulk[index] = run_bulk(a, category, count, seed ^ 0x22, false);
      }
      for ( u64 i = 0; i < sizeof(ALLOCATORS) / sizeof(ALLOCATORS[0]); ++i ) {
        print_row("serial", "alloc", ALLOCATORS[i], category, count, bulk[i].allocate,
                  i == 0 ? 1.0 : ratio_to(bulk[i].allocate, bulk[0].allocate));
        print_row("serial", "free", ALLOCATORS[i], category, count, bulk[i].deallocate,
                  i == 0 ? 1.0 : ratio_to(bulk[i].deallocate, bulk[0].deallocate));
      }

      for ( bool randomized : { true } ) {
        bulk_outcome random_bulk[sizeof(ALLOCATORS) / sizeof(ALLOCATORS[0])];
        for ( u64 i = 0; i < sizeof(ALLOCATORS) / sizeof(ALLOCATORS[0]); ++i )
          random_bulk[i] = run_bulk(ALLOCATORS[i], category, count, seed ^ (randomized ? 0x44 : 0x33), randomized);
        const char *workload = "randomized";
        const char *phase = "random-free";
        for ( u64 i = 0; i < sizeof(ALLOCATORS) / sizeof(ALLOCATORS[0]); ++i )
          print_row(workload, phase, ALLOCATORS[i], category, count, random_bulk[i].deallocate,
                    i == 0 ? 1.0 : ratio_to(random_bulk[i].deallocate, random_bulk[0].deallocate));
      }

      run_matrix("interleaved", category, count, seed ^ 0x55, [&](const allocator &a, u64 s) {
        return run_interleaved(a, category, count, s);
      });
      run_matrix("fragmented", category, count, seed ^ 0x66, [&](const allocator &a, u64 s) {
        return run_fragmented(a, category, count, s);
      });
      run_matrix("launder", category, count, seed ^ 0x77, [&](const allocator &a, u64 s) {
        return run_hot(a, category, count, s);
      });

      outcome ordinary = run_hot(ALLOCATORS[0], category, count, seed ^ 0x77);
      print_row("launder", "ordinary", ALLOCATORS[0], category, count, ordinary, 1.0);
      // The local temporal primitive is intentionally limited to the regular
      // allocation tiers; it exits on the huge tier.  Keep the ordinary
      // huge-tier comparison row, but do not present an unsupported launder
      // measurement as if it were valid.
      if ( category.hi <= 32768 ) {
        outcome laundered = run_hot(ABC_LAUNDER, category, count, seed ^ 0x77);
        print_row("launder", "roundtrip", ABC_LAUNDER, category, count, laundered, ratio_to(laundered, ordinary));
      }
    }
  }
}

void
calibrate() noexcept
{
  volatile u64 value = 0;
  const u64 c0 = ticks();
  const u64 n0 = monotonic_ns();
  for ( u64 i = 0; i < 2'000'000; ++i ) value += i;
  const u64 n1 = monotonic_ns();
  const u64 c1 = ticks();
  g_sink += value;
  g_ns_per_tick = c1 > c0 ? static_cast<double>(n1 - n0) / static_cast<double>(c1 - c0) : 1.0;
}

};      // namespace

int
main()
{
  const int cpu = pin_from_environment();
  calibrate();
  std::printf("# suite=abcmalloc comparison pathway benchmark\n");
  std::printf("# implementation=local src/cmalloc.hpp\n");
  std::printf("# mode=single-threaded cpu=%d allocator_locks=disabled tsc_ns=%.6f\n", cpu, g_ns_per_tick);
  std::printf("# allocators=abc,glibc,mimalloc,jemalloc,tcmalloc,tbbmalloc\n");
  std::printf("# workloads=hot,serial,randomized,interleaved,fragmented,launder\n");
  std::printf("# launder_semantics=abc-launder is measured through the large tier; huge uses ordinary allocation only\n");
  std::printf("# ratio_semantics=cycles_per_op / abc_cycles_per_op; values below 1.0 favor the comparison allocator\n");
  run_suite();
  std::printf("# sink=%llu\n", static_cast<unsigned long long>(g_sink));
  return 0;
}
