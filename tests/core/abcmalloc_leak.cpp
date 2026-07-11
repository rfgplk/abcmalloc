// [abcmalloc mirror] canonical umbrella first: cmalloc.hpp #defines
// MICRON_ABCMALLOC_DISABLE_STD so micron-core headers use THIS standalone
// allocator instead of pulling their own in-tree copy.
#include "../../src/cmalloc.hpp"

#include "../../src/cmalloc.hpp"
#include <micron/io/console.hpp>
#include <micron/std.hpp>

#include <micron/string/strings.hpp>
#include "../snowball/snowball.hpp"

struct s {
  int x;
  int y;
};

int
main()
{
  byte *a = 0;
  for ( ;; )
    for ( int i = 1; i < 500; i++ ) {
      a = abc::alloc(1025);
      abc::dealloc(a);
    }

  return 1;
}
