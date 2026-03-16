#include "../../src/cmalloc.hpp"
#include <micron/io/console.hpp>
#include <micron/std.hpp>

void *volatile escaped;
#include <random>
int
main()
{
  if constexpr ( true ) {
    for ( size_t n = 0; n < 10000; ++n ) { 
      void *dont_optimize = abc::malloc(4096);
      escaped = dont_optimize;
    }
  }
  return 0;
}
