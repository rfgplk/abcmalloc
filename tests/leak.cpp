#include "../src/cmalloc.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <vector>

int
main()
{
  std::vector<void *> live_allocations;

  for ( int i = 0; i < 1000; ++i ) {
    void *p = abc::malloc(16 + (i % 64));
    assert(p != nullptr);
    live_allocations.push_back(p);
  }

  std::random_shuffle(live_allocations.begin(), live_allocations.end());
  for ( size_t i = 0; i < live_allocations.size() / 2; ++i ) {
    abc::free(live_allocations[i]);
    live_allocations[i] = nullptr;
  }

  for ( size_t i = live_allocations.size() / 2; i < live_allocations.size(); ++i ) {
    abc::free(live_allocations[i]);
    live_allocations[i] = nullptr;
  }

  bool leak = false;
  for ( auto ptr : live_allocations ) {
    if ( ptr != nullptr ) {
      std::cerr << "Memory leak detected at: " << ptr << "\n";
      leak = true;
    }
  }

  if ( leak ) {
    std::cerr << "Memory leak test FAILED\n";
    return 1;
  }

  std::cout << "Memory leak test PASSED\n";
  return 0;
}
