#include "../../src/cmalloc.hpp"

#include <micron/io/console.hpp>
#include <micron/std.hpp>

void *volatile escaped;
int
main()
{
  char *dont_optimize = reinterpret_cast<char*>(abc::malloc(1ULL << 32));
  escaped = dont_optimize;
  mc::console(escaped);
  return 0;
}
