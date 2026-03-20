#include "../src/cmalloc.hpp"

#include <iostream>

int
main()
{
  // comp with debug_notices = true to verify correct patterns
  for ( u64 i = 0; i < 100'000'000; ++i ) {
    abc::free(abc::malloc(i));
  }
  std::cout << "Done" << std::endl;
  return 0;
}
