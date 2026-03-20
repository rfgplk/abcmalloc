#include "../src/cmalloc.hpp"

int
main()
{
  // comp with debug_notices = true to verify correct patterns
  for ( u64 i = 0; i < 10'000'000; ++i ) {
    abc::free(abc::malloc(i*100));
  }
  return 0;
}
