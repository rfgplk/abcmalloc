#include "../src/cmalloc.hpp"
#include <micron/io/console.hpp>
#include <micron/std.hpp>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

struct Allocation {
  void *ptr = nullptr;
  size_t size = 0;
  uint8_t pattern = 0;
};

int
main()
{
  constexpr int MAX_PTRS = 1024;
  constexpr int MAX_SIZE = 4096;

  std::vector<Allocation> allocs(MAX_PTRS);
  std::mt19937 rng(12345);     // deterministic seed
  std::uniform_int_distribution<int> idx_dist(0, MAX_PTRS - 1);
  std::uniform_int_distribution<int> op_dist(0, 2);
  std::uniform_int_distribution<int> size_dist(0, MAX_SIZE);
  std::uniform_int_distribution<int> pat_dist(1, 255);

  for ( int iter = 0; iter < 200000; ++iter ) {
    int idx = idx_dist(rng);
    int op = op_dist(rng);

    Allocation &a = allocs[idx];

    if ( op == 0 ) {     // abc::malloc
      if ( !a.ptr ) {
        size_t sz = size_dist(rng);
        void *p = abc::malloc(sz);
        assert(p != nullptr || sz == 0);
        a.ptr = p;
        a.size = sz;
        a.pattern = pat_dist(rng);
        if ( p && sz )
          memset(p, a.pattern, sz);
      }
    } else if ( op == 1 ) {                     // abc::realloc
      if ( a.ptr || size_dist(rng) < 50 ) {     // sometimes abc::realloc NULL ptr
        size_t new_sz = size_dist(rng);
        void *p = abc::realloc(a.ptr, new_sz);
        assert(p != nullptr || new_sz == 0);
        if ( p && a.ptr ) {     // check old data is still intact
          size_t min_sz = a.size < new_sz ? a.size : new_sz;
          for ( size_t i = 0; i < min_sz; ++i ) {
            uint8_t *b = reinterpret_cast<uint8_t *>(p);
            if ( i < a.size )
              assert(b[i] == a.pattern);
          }
        }
        a.ptr = p;
        a.size = new_sz;
        a.pattern = pat_dist(rng);
        if ( p && new_sz )
          memset(p, a.pattern, new_sz);
      }
    } else {     // abc::free
      if ( a.ptr ) {
        abc::free(a.ptr);
        a.ptr = nullptr;
        a.size = 0;
        a.pattern = 0;
      }
    }
  }

  for ( auto &a : allocs ) {
    if ( a.ptr )
      abc::free(a.ptr);
  }

  mc::infolog("Tests passed");
  return 0;
}
