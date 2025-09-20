
#include "../src/cmalloc.hpp"
#include <micron/io/console.hpp>
#include <micron/std.hpp>

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <random>

struct node_t {
  unsigned char *str;
  node_t *next;
  size_t size;
  unsigned char pattern;
};

void
init_pattern(unsigned char *ptr, size_t size, unsigned char pattern)
{
  for ( size_t i = 0; i < size; ++i )
    ptr[i] = pattern;
}

bool
verify_pattern(const unsigned char *ptr, size_t size, unsigned char pattern)
{
  for ( size_t i = 0; i < size; ++i )
    if ( ptr[i] != pattern )
      return false;
  return true;
}

int
main()
{
  constexpr int NUM_ITER = 1000000;
  constexpr int MAX_STR_SIZE = 128;

  node_t *head = nullptr;
  std::mt19937 rng(2025);
  std::uniform_int_distribution<int> size_dist(1, MAX_STR_SIZE);
  std::uniform_int_distribution<int> op_dist(0, 5);
  std::uniform_int_distribution<int> pat_dist(1, 255);

  for ( int iter = 0; iter < NUM_ITER; ++iter ) {
    int op = op_dist(rng);

    if ( op > 0 ) {     // insert
      size_t sz = size_dist(rng);
      unsigned char pat = static_cast<unsigned char>(pat_dist(rng));

      node_t *node = (node_t *)abc::malloc(sizeof(node_t));
      assert(node != nullptr);
      node->size = sz;
      node->pattern = pat;
      node->next = head;
      head = node;

      node->str = (unsigned char *)abc::malloc(sz);
      assert(node->str != nullptr);

      init_pattern(node->str, sz, pat);

      // verify only immediately initialized memory
      assert(verify_pattern(node->str, node->size, node->pattern));
    } else {     // delete head
      if ( head ) {
        node_t *tmp = head;
        head = head->next;

        assert(verify_pattern(tmp->str, tmp->size, tmp->pattern));

        abc::free(tmp->str);
        abc::free(tmp);
      }
    }
  }

  while ( head ) {
    node_t *tmp = head;
    head = head->next;
    assert(verify_pattern(tmp->str, tmp->size, tmp->pattern));
    abc::free(tmp->str);
    abc::free(tmp);
  }

  mc::infolog("Test passed");
}
