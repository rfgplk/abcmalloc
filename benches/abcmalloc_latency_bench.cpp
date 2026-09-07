// Per-call latency distribution for the local src/ implementation.
#include "bench_common.hpp"
#include "../src/cmalloc.hpp"

static_assert(!abc::__default_multithread_safe, "single-thread latency benchmark must disable allocator locks");

#include <cstddef>

namespace
{

using namespace abcmalloc_bench;
using byte = unsigned char;

constexpr u64 SAMPLES = 4096;
constexpr u64 SLOTS = 256;
void *g_slots[SLOTS]{};
u64 g_sizes[SLOTS]{};
u64 g_alloc_values[SAMPLES]{};
u64 g_free_values[SAMPLES]{};
double g_ns_per_tick = 1.0;

u64
next_random(u64 &state) noexcept
{
  state += 0x9E3779B97F4A7C15ULL;
  u64 value = state;
  value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
  value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
  return value ^ (value >> 31);
}

std::size_t
size_for(const char *workload, u64 &state) noexcept
{
  if ( workload[0] == 't' ) return 1 + next_random(state) % 1024;
  if ( workload[0] == 'b' ) return 2048 + next_random(state) % 63488;
  const u64 selector = next_random(state) % 100;
  return selector < 50 ? 1 + next_random(state) % 256
                       : (selector < 85 ? 257 + next_random(state) % 768 : 1025 + next_random(state) % 15360);
}

void
report(const char *workload, const char *phase, const percentiles &p) noexcept
{
  std::printf("row suite=latency allocator=abc workload=%s phase=%s samples=%llu p50_ns=%.2f p90_ns=%.2f "
              "p99_ns=%.2f p999_ns=%.2f max_ns=%.2f\n",
              workload, phase, static_cast<unsigned long long>(SAMPLES), static_cast<double>(p.p50) * g_ns_per_tick,
              static_cast<double>(p.p90) * g_ns_per_tick, static_cast<double>(p.p99) * g_ns_per_tick,
              static_cast<double>(p.p999) * g_ns_per_tick, static_cast<double>(p.max) * g_ns_per_tick);
}

void
run_workload(const char *workload, u64 seed) noexcept
{
  for ( u64 i = 0; i < SLOTS; ++i ) g_slots[i] = nullptr;
  u64 state = seed;
  u64 alloc_count = 0;
  u64 free_count = 0;
  while ( alloc_count < SAMPLES || free_count < SAMPLES ) {
    const u64 slot = next_random(state) % SLOTS;
    if ( g_slots[slot] == nullptr && alloc_count < SAMPLES ) {
      g_sizes[slot] = size_for(workload, state);
      const u64 begin = ticks();
      g_slots[slot] = abc::alloc(g_sizes[slot]);
      const u64 end = ticks();
      clobber(g_slots[slot]);
      g_alloc_values[alloc_count++] = end - begin;
    } else if ( g_slots[slot] != nullptr && free_count < SAMPLES ) {
      const u64 begin = ticks();
      abc::dealloc(static_cast<byte *>(g_slots[slot]));
      const u64 end = ticks();
      g_slots[slot] = nullptr;
      g_free_values[free_count++] = end - begin;
    }
  }
  report(workload, "alloc", summarize(g_alloc_values, alloc_count));
  for ( u64 i = 0; i < SLOTS; ++i )
    if ( g_slots[i] != nullptr ) abc::dealloc(static_cast<byte *>(g_slots[i]));
  report(workload, "free", summarize(g_free_values, free_count));
}

void
calibrate() noexcept
{
  volatile u64 value = 0;
  const u64 c0 = ticks();
  const u64 n0 = monotonic_ns();
  for ( u64 i = 0; i < 2'000'000; ++i ) value += i;
  const u64 c1 = ticks();
  const u64 n1 = monotonic_ns();
  g_ns_per_tick = c1 > c0 ? static_cast<double>(n1 - n0) / static_cast<double>(c1 - c0) : 1.0;
  std::printf("# calibration_sink=%llu\n", static_cast<unsigned long long>(value));
}

};      // namespace

int
main()
{
  const int cpu = pin_from_environment();
  calibrate();
  std::printf("# suite=abcmalloc latency benchmark\n# implementation=local src/cmalloc.hpp\n# cpu=%d\n"
              "# threading=single-threaded allocator_locks=disabled tsc_ns=%.6f\n",
              cpu, g_ns_per_tick);
  run_workload("tlsf", 0x1A7E0C5ULL);
  run_workload("buddy", 0x2B8F1D6ULL);
  run_workload("mixed", 0x3C902E7ULL);
  return 0;
}
