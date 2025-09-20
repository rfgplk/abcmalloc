#include "../src/cmalloc.hpp"
#include <micron/io/console.hpp>
#include <micron/std.hpp>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <random>

int
main()
{
  constexpr int NUM_PTRS = 1024;
  constexpr int MAX_SIZE = 256;

  void *ptrs[NUM_PTRS] = { nullptr };
  std::mt19937 rng(42);
  std::uniform_int_distribution<int> size_dist(1, MAX_SIZE);
  std::uniform_int_distribution<int> index_dist(0, NUM_PTRS - 1);
  std::uniform_int_distribution<int> op_dist(0, 2);

  for ( int iter = 0; iter < 100000; ++iter ) {
    int idx = index_dist(rng);
    int op = op_dist(rng);

    if ( op == 0 ) {
      if ( ptrs[idx] == nullptr ) {
        size_t sz = size_dist(rng);
        ptrs[idx] = abc::malloc(sz);
        assert(ptrs[idx] != nullptr);
        memset(ptrs[idx], 0xAA, sz);
      }
    } else if ( op == 1 ) {
      if ( ptrs[idx] != nullptr ) {
        size_t sz = size_dist(rng);
        void *new_ptr = abc::realloc(ptrs[idx], sz);
        assert(new_ptr != nullptr);
        ptrs[idx] = new_ptr;
        memset(ptrs[idx], 0xBB, sz);
      }
    } else {
      if ( ptrs[idx] != nullptr ) {
        abc::free(ptrs[idx]);
        ptrs[idx] = nullptr;
      }
    }
  }

  for ( int i = 0; i < NUM_PTRS; ++i ) {
    if ( ptrs[i] != nullptr )
      abc::free(ptrs[i]);
  }
  mc::infolog("Tests passed");
  return 0;
}
