// [abcmalloc mirror] canonical umbrella first: cmalloc.hpp #defines
// MICRON_ABCMALLOC_DISABLE_STD so micron-core headers use THIS standalone
// allocator instead of pulling their own in-tree copy.
#include "../../src/cmalloc.hpp"
#include "../../src/cmalloc.hpp"
#include <micron/io/console.hpp>
#include <micron/std.hpp>

void *volatile escaped;

int
main()
{
  char *dont_optimize = reinterpret_cast<char *>(abc::malloc(1ULL << 32));
  escaped = dont_optimize;
  mc::console(escaped);
  // mc::io::stdout("\n");
  // mc::io::stdout((const char)dont_optimize[345456]);
  // mc::io::stdout("\n");
  // mc::io::stdout((const char)dont_optimize[456]);
  // mc::io::stdout("\n");
  return 0;
}
