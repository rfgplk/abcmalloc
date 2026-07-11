// [abcmalloc mirror] canonical umbrella first: cmalloc.hpp #defines
// MICRON_ABCMALLOC_DISABLE_STD so micron-core headers use THIS standalone
// allocator instead of pulling their own in-tree copy.
#include "../../src/cmalloc.hpp"
#include <micron/io/console.hpp>
#include <micron/std.hpp>

void *volatile escaped;

#include <cstdlib>
#include <random>

int
main()
{
  if constexpr ( true ) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(1, 1e6);
    abc::__arena arena;
    for ( size_t n = 0; n < 5000; ++n ) {
      void *dont_optimize = malloc(dist(gen));
      escaped = dont_optimize;
    }
  }
  return 0;
}
