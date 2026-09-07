// Multi-threaded half of the hosted comparison suite.  The worker pool uses
// pthreads only for orchestration; all timed allocation calls go through the
// explicitly selected allocator symbol.
#define ABCMALLOC_DISABLE 1

#include "bench_common.hpp"
#include "../src/cmalloc.hpp"

static_assert(abc::__default_multithread_safe, "multi-thread comparison must retain allocator locks");

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <pthread.h>

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

constexpr unsigned MAX_THREADS = 8;
constexpr u64 MAX_OPS = 4096;
constexpr u64 LIVE_BUDGET = 32ULL << 20;
constexpr unsigned THREAD_COUNTS[] = { 1, 2, 4, 8 };

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

enum class workload : unsigned { hot, serial, randomized, interleaved, fragmented, launder };

struct context {
  const allocator *allocator_ref;
  workload kind;
  const category *category_ref;
  u64 count;
  u64 seed;
  pthread_barrier_t *barrier;
  std::size_t sizes[MAX_OPS]{};
  void *pointers[MAX_OPS]{};
  u64 cycles = 0;
  u64 nanos = 0;
  u64 operations = 0;
};

struct mt_result {
  double cycles_per_op;
  double ns_per_op;
  double mops;
  u64 operations;
};

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
fill_context(context &context) noexcept
{
  u64 state = context.seed;
  for ( u64 i = 0; i < context.count; ++i )
    context.sizes[i] = context.category_ref->lo + next_random(state) % (context.category_ref->hi - context.category_ref->lo + 1);
}

void
execute(context &context) noexcept
{
  const allocator &allocator = *context.allocator_ref;
  const u64 count = context.count;
  switch ( context.kind ) {
    case workload::hot:
    case workload::launder:
      for ( u64 i = 0; i < count; ++i ) {
        void *pointer = allocator.allocate(context.sizes[i]);
        clobber(pointer);
        allocator.deallocate(pointer);
      }
      context.operations = 2 * count;
      return;

    case workload::serial:
      for ( u64 i = 0; i < count; ++i ) {
        context.pointers[i] = allocator.allocate(context.sizes[i]);
        clobber(context.pointers[i]);
      }
      for ( u64 i = 0; i < count; ++i ) {
        allocator.deallocate(context.pointers[i]);
        context.pointers[i] = nullptr;
      }
      context.operations = 2 * count;
      return;

    case workload::randomized:
      for ( u64 i = 0; i < count; ++i ) {
        context.pointers[i] = allocator.allocate(context.sizes[i]);
        clobber(context.pointers[i]);
      }
      for ( u64 i = 0; i < count; ++i ) {
        const u64 index = (i * 2654435761ULL) % count;
        allocator.deallocate(context.pointers[index]);
        context.pointers[index] = nullptr;
      }
      context.operations = 2 * count;
      return;

    case workload::interleaved: {
      const u64 window = std::max<u64>(16, std::min<u64>(256, count / 2));
      u64 state = context.seed ^ 0x55AA55AA55AA55AAULL;
      for ( u64 i = 0; i < count; ++i ) {
        const u64 slot = next_random(state) % window;
        if ( context.pointers[slot] != nullptr ) {
          allocator.deallocate(context.pointers[slot]);
          context.pointers[slot] = nullptr;
        } else {
          context.pointers[slot] = allocator.allocate(context.sizes[i]);
          clobber(context.pointers[slot]);
        }
      }
      for ( u64 i = 0; i < window; ++i ) {
        if ( context.pointers[i] != nullptr ) {
          allocator.deallocate(context.pointers[i]);
          ++context.operations;
        }
        context.pointers[i] = nullptr;
      }
      context.operations += count;
      return;
    }

    case workload::fragmented: {
      const u64 holes = std::max<u64>(1, count / 2);
      u64 operations = 0;
      for ( u64 i = 0; i < count; ++i ) {
        context.pointers[i] = allocator.allocate(context.sizes[i]);
        clobber(context.pointers[i]);
        ++operations;
      }
      for ( u64 i = 0; i < holes; ++i ) {
        allocator.deallocate(context.pointers[2 * i]);
        context.pointers[2 * i] = nullptr;
        ++operations;
      }
      for ( u64 i = 0; i < holes; ++i ) {
        context.pointers[2 * i] = allocator.allocate(context.sizes[i]);
        clobber(context.pointers[2 * i]);
        ++operations;
      }
      for ( u64 i = 0; i < holes; ++i ) {
        allocator.deallocate(context.pointers[2 * i]);
        context.pointers[2 * i] = nullptr;
        ++operations;
      }
      for ( u64 i = 1; i < count; i += 2 ) {
        allocator.deallocate(context.pointers[i]);
        context.pointers[i] = nullptr;
        ++operations;
      }
      context.operations = operations;
      return;
    }
  }
}

void *worker(void *argument) noexcept
{
  context &context = *static_cast<::context *>(argument);
  fill_context(context);
  execute(context);
  pthread_barrier_wait(context.barrier);
  const u64 c0 = ticks();
  const u64 n0 = monotonic_ns();
  execute(context);
  const u64 n1 = monotonic_ns();
  const u64 c1 = ticks();
  context.cycles = c1 - c0;
  context.nanos = n1 - n0;
  pthread_barrier_wait(context.barrier);
  return nullptr;
}

u64
count_for(const category &category, unsigned threads) noexcept
{
  const u64 maximum = LIVE_BUDGET / (category.hi * threads);
  return std::max<u64>(1, std::min<u64>(MAX_OPS, maximum));
}

mt_result
run_config(const allocator &allocator, workload kind, const category &category, unsigned threads, u64 count, u64 seed) noexcept
{
  pthread_barrier_t barrier;
  pthread_barrier_init(&barrier, nullptr, threads);
  pthread_t handles[MAX_THREADS]{};
  context contexts[MAX_THREADS]{};
  for ( unsigned i = 0; i < threads; ++i ) {
    contexts[i].allocator_ref = &allocator;
    contexts[i].kind = kind;
    contexts[i].category_ref = &category;
    contexts[i].count = count;
    contexts[i].seed = seed ^ (0x9E3779B97F4A7C15ULL * (i + 1));
    contexts[i].barrier = &barrier;
    const int create_result = pthread_create(&handles[i], nullptr, worker, &contexts[i]);
    if ( create_result != 0 ) {
      std::fprintf(stderr, "benchmark: pthread_create failed: %s\n", std::strerror(create_result));
      std::abort();
    }
  }
  u64 total_cycles = 0;
  u64 total_nanos = 0;
  u64 total_ops = 0;
  for ( unsigned i = 0; i < threads; ++i ) {
    pthread_join(handles[i], nullptr);
    total_cycles += contexts[i].cycles;
    total_nanos = std::max(total_nanos, contexts[i].nanos);
    total_ops += contexts[i].operations;
  }
  pthread_barrier_destroy(&barrier);
  const double ops = static_cast<double>(total_ops);
  const double ns = static_cast<double>(total_nanos);
  return { ops > 0 ? static_cast<double>(total_cycles) / ops : 0.0, ops > 0 ? ns / ops : 0.0,
           ns > 0.0 ? ops * 1000.0 / ns : 0.0, total_ops };
}

const char *
workload_name(workload kind) noexcept
{
  switch ( kind ) {
    case workload::hot: return "hot";
    case workload::serial: return "serial";
    case workload::randomized: return "randomized";
    case workload::interleaved: return "interleaved";
    case workload::fragmented: return "fragmented";
    case workload::launder: return "launder";
  }
  return "unknown";
}

void
print_row(const allocator &allocator, workload kind, const category &category, unsigned threads, u64 count,
          const mt_result &result, double ratio, double scaling) noexcept
{
  std::printf("row suite=comparison mode=mt allocator=%s workload=%s phase=throughput category=%s size=%zu "
              "threads=%u count=%llu operations=%llu cycles_per_op=%.2f ns_per_op=%.2f mops_s=%.2f "
              "p50_ns=0 p90_ns=0 p99_ns=0 p999_ns=0 max_ns=0 ratio=%.4f scaling=%.4f samples=1 latency_samples=0\n",
              allocator.name, workload_name(kind), category.name, category.hi, threads,
              static_cast<unsigned long long>(count), static_cast<unsigned long long>(result.operations),
              result.cycles_per_op, result.ns_per_op, result.mops, ratio, scaling);
}

void
run_suite() noexcept
{
  constexpr unsigned allocator_count = sizeof(ALLOCATORS) / sizeof(ALLOCATORS[0]);
  constexpr workload WORKLOADS[] = { workload::hot, workload::serial, workload::randomized,
                                     workload::interleaved, workload::fragmented, workload::launder };
  for ( const category &category : CATEGORIES ) {
    for ( workload kind : WORKLOADS ) {
      double base_one[allocator_count]{};
      for ( unsigned ai = 0; ai < allocator_count; ++ai ) {
        const u64 count = count_for(category, 1);
        const allocator &allocator = ALLOCATORS[ai];
        const mt_result result = run_config(allocator, kind, category, 1, count,
                                             0xABC10000ULL ^ (static_cast<u64>(category.hi) << 8) ^ 1U);
        base_one[ai] = result.mops;
      }
      for ( unsigned threads : THREAD_COUNTS ) {
        mt_result results[allocator_count]{};
        for ( unsigned ai = 0; ai < allocator_count; ++ai ) {
          const u64 count = count_for(category, threads);
          results[ai] = run_config(ALLOCATORS[ai], kind, category, threads, count,
                                   0xABC10000ULL ^ (static_cast<u64>(category.hi) << 8) ^ threads);
        }
        for ( unsigned ai = 0; ai < allocator_count; ++ai ) {
          const u64 count = count_for(category, threads);
          const mt_result &result = results[ai];
          const double ratio = ai == 0 || results[0].cycles_per_op <= 0.0
                                   ? 1.0
                                   : result.cycles_per_op / results[0].cycles_per_op;
          const double scaling = base_one[ai] > 0.0 ? result.mops / base_one[ai] : 0.0;
          print_row(ALLOCATORS[ai], kind, category, threads, count, result, ratio, scaling);
        }
      }
    }
  }
  // The temporal path is emitted as an explicit variant, in the same way as
  // the single-threaded suite, while the six baseline allocators above remain
  // present for every workload matrix cell.
  for ( const category &category : CATEGORIES ) {
    if ( category.hi > 32768 ) continue;
    const u64 count = count_for(category, 1);
    const mt_result value = run_config(ABC_LAUNDER, workload::launder, category, 1, count, 0xABC30000ULL);
    const mt_result ordinary = run_config(ALLOCATORS[0], workload::launder, category, 1, count, 0xABC30000ULL);
    print_row(ABC_LAUNDER, workload::launder, category, 1, count, value,
              ordinary.cycles_per_op > 0.0 ? value.cycles_per_op / ordinary.cycles_per_op : 0.0,
              ordinary.mops > 0.0 ? value.mops / ordinary.mops : 0.0);
  }
}

};      // namespace

int
main()
{
  const int cpu = pin_from_environment();
  std::printf("# suite=abcmalloc comparison pathway benchmark\n");
  std::printf("# implementation=local src/cmalloc.hpp\n");
  std::printf("# mode=multi-threaded cpu=%d allocator_locks=enabled\n", cpu);
  std::printf("# allocators=abc,glibc,mimalloc,jemalloc,tcmalloc,tbbmalloc\n");
  std::printf("# workloads=hot,serial,randomized,interleaved,fragmented,launder\n");
  std::printf("# launder_semantics=abc-launder is measured through the large tier; huge uses ordinary allocation only\n");
  std::printf("# ratio_semantics=cycles_per_op / abc_cycles_per_op; scaling=Mops/s / same allocator's one-thread Mops/s\n");
  run_suite();
  return 0;
}
