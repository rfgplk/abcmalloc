// Shared hosted benchmark support.  The timed kernels only use the small
// helpers in this file; reporting and affinity setup stay outside them.
#pragma once

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sched.h>

namespace abcmalloc_bench
{

using u64 = std::uint64_t;

inline u64
monotonic_ns() noexcept
{
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  return static_cast<u64>(ts.tv_sec) * 1'000'000'000ULL + static_cast<u64>(ts.tv_nsec);
}

[[gnu::always_inline]] inline u64
ticks() noexcept
{
#if defined(__x86_64__) || defined(__i386__)
  std::uint32_t lo = 0;
  std::uint32_t hi = 0;
  __asm__ __volatile__("lfence\n\trdtsc\n\tlfence" : "=a"(lo), "=d"(hi) : : "memory");
  return (static_cast<u64>(hi) << 32) | lo;
#elif defined(__aarch64__)
  u64 value = 0;
  __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(value));
  return value;
#else
  return monotonic_ns();
#endif
}

inline bool
pin_cpu(int cpu) noexcept
{
  if ( cpu < 0 ) return true;
  if ( cpu >= CPU_SETSIZE ) {
    errno = EINVAL;
    return false;
  }
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);
  return sched_setaffinity(0, sizeof(set), &set) == 0;
}

inline int
current_cpu() noexcept
{
  return sched_getcpu();
}

inline int
pin_from_environment() noexcept
{
  const char *text = std::getenv("ABCMALLOC_BENCH_CPU");
  if ( text == nullptr || *text == '\0' ) return current_cpu();
  char *end = nullptr;
  const long value = std::strtol(text, &end, 10);
  if ( end == text || *end != '\0' || value < 0 || value > 1'000'000 ) return current_cpu();
  if ( !pin_cpu(static_cast<int>(value)) ) {
    std::fprintf(stderr, "benchmark: sched_setaffinity(cpu=%ld) failed: %s\n", value, std::strerror(errno));
    return current_cpu();
  }
  return current_cpu();
}

struct sample {
  double cycles_per_op;
  double ns_per_op;
  double mops;
};

inline double
median(double *values, unsigned count) noexcept
{
  std::sort(values, values + count);
  return values[count / 2];
}

template<typename Kernel>
sample
measure(Kernel &&kernel, u64 operations, unsigned measurements = 5) noexcept
{
  constexpr unsigned warmups = 2;
  for ( unsigned i = 0; i < warmups; ++i ) kernel();

  double cycles[9]{};
  double nanos[9]{};
  if ( measurements > 9 ) measurements = 9;
  for ( unsigned i = 0; i < measurements; ++i ) {
    const u64 c0 = ticks();
    const u64 n0 = monotonic_ns();
    kernel();
    const u64 n1 = monotonic_ns();
    const u64 c1 = ticks();
    cycles[i] = operations ? static_cast<double>(c1 - c0) / static_cast<double>(operations) : 0.0;
    nanos[i] = operations ? static_cast<double>(n1 - n0) / static_cast<double>(operations) : 0.0;
  }
  const double ns = median(nanos, measurements);
  return { median(cycles, measurements), ns, ns > 0.0 ? 1000.0 / ns : 0.0 };
}

template<typename Setup, typename Kernel, typename Cleanup>
sample
measure_lifecycle(Setup &&setup, Kernel &&kernel, Cleanup &&cleanup, u64 operations, unsigned measurements = 5) noexcept
{
  constexpr unsigned warmups = 2;
  for ( unsigned i = 0; i < warmups; ++i ) {
    setup();
    kernel();
    cleanup();
  }

  double cycles[9]{};
  double nanos[9]{};
  if ( measurements > 9 ) measurements = 9;
  for ( unsigned i = 0; i < measurements; ++i ) {
    setup();
    const u64 c0 = ticks();
    const u64 n0 = monotonic_ns();
    kernel();
    const u64 n1 = monotonic_ns();
    const u64 c1 = ticks();
    cleanup();
    cycles[i] = operations ? static_cast<double>(c1 - c0) / static_cast<double>(operations) : 0.0;
    nanos[i] = operations ? static_cast<double>(n1 - n0) / static_cast<double>(operations) : 0.0;
  }
  const double ns = median(nanos, measurements);
  return { median(cycles, measurements), ns, ns > 0.0 ? 1000.0 / ns : 0.0 };
}

struct percentiles {
  u64 p50;
  u64 p90;
  u64 p99;
  u64 p999;
  u64 max;
};

inline percentiles
summarize(u64 *values, u64 count) noexcept
{
  std::sort(values, values + count);
  const auto at = [values, count](u64 numerator, u64 denominator) noexcept -> u64 {
    if ( count == 0 ) return 0;
    u64 index = (count * numerator) / denominator;
    if ( index >= count ) index = count - 1;
    return values[index];
  };
  return { at(50, 100), at(90, 100), at(99, 100), at(999, 1000), count ? values[count - 1] : 0 };
}

[[gnu::always_inline]] inline void
clobber(const void *pointer) noexcept
{
  __asm__ __volatile__("" : : "r"(pointer) : "memory");
}

inline void
print_header(const char *name, const char *description) noexcept
{
  std::printf("# benchmark=%s\n# description=%s\n", name, description);
}

};      // namespace abcmalloc_bench
