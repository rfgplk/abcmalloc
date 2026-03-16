#include "../../src/cmalloc.hpp"
#include <micron/io/console.hpp>
#include <micron/std.hpp>

// wont compile for you accessing relatively sorry
#include "../../../bbench/src/bench.hpp"
#include "../../../bbench/src/clock.hpp"

void *volatile escaped;
#include <random>

using namespace bbench;

volatile void *
rand_fn(void)
{
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> dist(1, 1e6);
  for ( size_t n = 0; n < 5000; ++n ) {     // 1-5gb
    void *dont_optimize = abc::malloc(dist(gen));
    escaped = dont_optimize;
  }
  return escaped;
}


volatile void *
fn(void)
{
  for ( size_t n = 0; n < 100; ++n ) {
    void *dont_optimize = std::malloc(4096);
    escaped = dont_optimize;
  }
  return escaped;
}

int
main()
{
  auto benched = bbench::benchmark<bbench::time_resolution::ns>(fn);
  mc::console("Cycles: ", benched.cycles);
  mc::console("Instructions: ", benched.instructions);
  mc::console("CPU Time: ", benched.cpu_time);
  mc::console("Time: ", benched.time);
  mc::console("Cycle: ", per_cycle(benched));
  mc::console("Inst: ", per_instruction(benched));
  mc::console("Miss branch: ", miss_percent(benched));
  mc::console("C/Inst: ", cycles_per_instruction(benched));
  return 0;
}
