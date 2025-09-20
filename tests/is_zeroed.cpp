#include "../src/cmalloc.hpp"
#include <micron/io/console.hpp>
#include <micron/std.hpp>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

int
main()
{
  constexpr int NUM_PTRS = 512;
  constexpr int MAX_SIZE = 1024;

  std::vector<void *> ptrs(NUM_PTRS, nullptr);
  std::vector<size_t> sizes(NUM_PTRS, 0);

  std::mt19937 rng(2025);
  std::uniform_int_distribution<int> idx_dist(0, NUM_PTRS - 1);
  std::uniform_int_distribution<int> size_dist(1, MAX_SIZE);

  for ( int i = 0; i < NUM_PTRS; ++i ) {
    size_t sz = size_dist(rng);
    //void *p = abc::calloc(sz, 1);
    void *p = abc::malloc(sz);
    assert(p != nullptr);
    ptrs[i] = p;
    sizes[i] = sz;
  }

  for ( int i = 0; i < NUM_PTRS; ++i ) {
    uint8_t *b = reinterpret_cast<uint8_t *>(ptrs[i]);
    for ( size_t j = 0; j < sizes[i]; ++j ) {
      assert(b[j] == 0);     // memory should be zeroed
    }
  }

  for ( auto p : ptrs ) {
    abc::free(p);
  }

  mc::infolog("Tests passed");
  return 0;
}
