#include "../src/cmalloc.hpp"
#include <cstdio>        // for printf
#include <iostream>        // for printf
#include <cstdlib>       // for abc::malloc, abc::free, rand, srand
#include <ctime>         // for time
#include <pthread.h>     // for threading
#include <vector>

constexpr int NUM_THREADS = 2;
constexpr int OPS_PER_THREAD = 1000;
constexpr int MAX_ALLOC = 512;     // max bytes per allocation

struct ThreadStats {
  size_t allocated_bytes = 0;
  size_t freed_bytes = 0;
};

void *
thread_func(void *arg)
{
  ThreadStats *stats = static_cast<ThreadStats *>(arg);
  std::vector<void *> allocations;
  allocations.reserve(100);

  for ( int i = 0; i < OPS_PER_THREAD; ++i ) {
    size_t size = rand() % MAX_ALLOC + 1;
    void *ptr = nullptr;
    try {
      ptr = abc::malloc(size);
    } catch ( micron::except::memory_error &e ) {
      std::cout << e.what() << std::endl;
    }
    if ( ptr ) {
      stats->allocated_bytes += size;
      allocations.push_back(ptr);
    }

    // Randomly abc::free some previous allocations
    if ( !allocations.empty() && (rand() % 2 == 0) ) {
      size_t idx = rand() % allocations.size();
      abc::free(allocations[idx]);
      stats->freed_bytes += size;     // approximate abc::freed bytes
      allocations[idx] = allocations.back();
      allocations.pop_back();
    }
  }

  // abc::free any remaining allocations
  for ( void *ptr : allocations ) {
    abc::free(ptr);
  }

  return nullptr;
}

int
main()
{
  srand(static_cast<unsigned int>(time(nullptr)));

  pthread_t threads[NUM_THREADS];
  ThreadStats stats[NUM_THREADS];

  for ( int i = 0; i < NUM_THREADS; ++i )
    pthread_create(&threads[i], nullptr, thread_func, &stats[i]);

  for ( int i = 0; i < NUM_THREADS; ++i )
    pthread_join(threads[i], nullptr);

  size_t total_alloc = 0, total_freed = 0;
  for ( int i = 0; i < NUM_THREADS; ++i ) {
    total_alloc += stats[i].allocated_bytes;
    total_freed += stats[i].freed_bytes;
  }

  printf("Approx total allocated: %zu bytes\n", total_alloc);
  printf("Approx total abc::freed:     %zu bytes\n", total_freed);
  return 0;
}
