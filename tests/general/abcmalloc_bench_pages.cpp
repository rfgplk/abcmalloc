#include <micron/io/console.hpp>
#include <micron/std.hpp>

void *volatile escaped;
#include <random>

int
main()
{
  abc::__arena arena;
  if constexpr ( true ) {
    for ( size_t n = 0; n < 10000; ++n ) {
      void *dont_optimize = std::malloc(4096);
      escaped = dont_optimize;
    }
  }
  return 0;
}
