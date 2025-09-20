#include "../src/cmalloc.hpp"
#include <micron/io/console.hpp>
#include <micron/std.hpp>

#include <cassert>
#include <cstdlib>

int
main()
{
  constexpr size_t SIZE = 1024;

  void *ptr = abc::malloc(SIZE);
  assert(ptr != nullptr);

  unsigned char *mem = static_cast<unsigned char *>(ptr);
  for ( size_t i = 0; i < SIZE; ++i ) {
    mem[i] = static_cast<unsigned char>(i & 0xFF);
    assert(mem[i] == static_cast<unsigned char>(i & 0xFF));
  }

  abc::free(ptr);

  mc::infolog("Tests passed");
  return 0;
}
