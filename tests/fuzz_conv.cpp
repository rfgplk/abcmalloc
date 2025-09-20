#include "../src/cmalloc.hpp"
#include <micron/io/console.hpp>
#include <micron/std.hpp>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

struct PtrNode {
  std::vector<void *> chain;     // stores all allocated levels
  size_t size = 0;
  uint8_t pattern = 0;
};

int
main()
{
  constexpr int MAX_PTRS = 256;
  constexpr int MAX_DEPTH = 4;
  constexpr int MAX_SIZE = 1024;

  std::vector<PtrNode> nodes(MAX_PTRS);

  std::mt19937 rng(12345);
  std::uniform_int_distribution<int> idx_dist(0, MAX_PTRS - 1);
  std::uniform_int_distribution<int> op_dist(0, 2);     // abc::malloc, abc::realloc, abc::free
  std::uniform_int_distribution<int> size_dist(1, MAX_SIZE);
  std::uniform_int_distribution<int> depth_dist(1, MAX_DEPTH);
  std::uniform_int_distribution<int> pat_dist(1, 255);

  for ( int iter = 0; iter < 100000; ++iter ) {
    int idx = idx_dist(rng);
    PtrNode &node = nodes[idx];
    int op = op_dist(rng);

    // allocate pointer chain if empty
    if ( node.chain.empty() ) {
      int depth = depth_dist(rng);
      node.chain.resize(depth, nullptr);
      for ( int d = 0; d < depth - 1; ++d ) {
        node.chain[d] = abc::malloc(sizeof(void *));
        assert(node.chain[d] != nullptr);
        // link to next level
        node.chain[d + 1] = nullptr;
        *reinterpret_cast<void **>(node.chain[d]) = node.chain[d + 1];
      }
    }

    void *&final = node.chain.back();

    if ( op == 0 ) {     // abc::malloc at deepest level
      if ( !final ) {
        size_t sz = size_dist(rng);
        final = abc::malloc(sz);
        assert(final != nullptr);
        node.size = sz;
        node.pattern = pat_dist(rng);
        memset(final, node.pattern, sz);
      }
    } else if ( op == 1 ) {     // abc::realloc
      if ( final ) {
        size_t new_sz = size_dist(rng);
        void *new_ptr = abc::realloc(final, new_sz);
        assert(new_ptr != nullptr);
        size_t min_sz = new_sz < node.size ? new_sz : node.size;
        uint8_t *b = reinterpret_cast<uint8_t *>(new_ptr);
        for ( size_t i = 0; i < min_sz; ++i ) {
          if ( i < node.size )
            assert(b[i] == node.pattern);
        }
        final = new_ptr;
        node.size = new_sz;
        node.pattern = pat_dist(rng);
        memset(final, node.pattern, new_sz);
      }
    } else {     // abc::free
      if ( final ) {
        abc::free(final);
        final = nullptr;
        node.size = 0;
        node.pattern = 0;
      }
    }
  }

  // cleanup
  for ( auto &node : nodes ) {
    for ( auto p : node.chain ) {
      if ( p )
        abc::free(p);
    }
    node.chain.clear();
  }
  mc::infolog("Test passed");
  return 0;
}
