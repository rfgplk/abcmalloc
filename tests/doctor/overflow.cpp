// [abcmalloc mirror] canonical umbrella first: cmalloc.hpp #defines
// MICRON_ABCMALLOC_DISABLE_STD so micron-core headers use THIS standalone
// allocator instead of pulling their own in-tree copy.
#include "../../src/cmalloc.hpp"
#include <micron/io/console.hpp>
#include <micron/std.hpp>

int
main(void)
{

  byte *p = abc::alloc(24);
  if ( p )
    for ( int i = 24; i < 40; ++i ) p[i] = static_cast<byte>(0xEE);
  mc::console(">>> H5 fsck:");
  abc::doctor::fsck();

  byte *q = abc::alloc(200);
  if ( q ) {
    unsigned bad = 0x11223344u;
    __builtin_memcpy(q - 32, &bad, 4);
  }
  mc::console(">>> H4 fsck:");
  abc::doctor::fsck();
  return 1;
}
