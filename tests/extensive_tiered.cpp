#include "../src/cmalloc.hpp"

#include <iostream>

int
main()
{
  // comp with debug_notices = true to verify correct patterns
  for ( u64 i = 0; i < 1'000'000; ++i ) {
    void* a = abc::malloc(i);
    void* b = abc::malloc(i*3);
    void* c = abc::malloc(i*100);
    abc::free(a);
    abc::free(b);
    abc::free(c);
  }
  std::cout << "Done" << std::endl;
  return 0;
}
