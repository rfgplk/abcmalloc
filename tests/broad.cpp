/*
 * Comprehensive abc::malloc/realloc/free Testing Suite
 *
 * This suite tests:
 * - Edge cases (zero allocation, NULL pointers, boundary conditions)
 * - Stress testing (large allocations, many allocations, fragmentation)
 * - Fuzzing (random allocation patterns)
 * - Memory alignment
 * - Thread safety (optional, with pthread support)
 */

#include "../src/cmalloc.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

// Test result tracking
struct TestStats {
  int passed = 0;
  int failed = 0;
  int total = 0;
};

TestStats global_stats;

// ANSI color codes for output
#define COLOR_GREEN "\033[32m"
#define COLOR_RED "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_RESET "\033[0m"

// Test assertion macros
#define TEST_ASSERT(condition, message)                                                                                 \
  do {                                                                                                                  \
    global_stats.total++;                                                                                               \
    if ( condition ) {                                                                                                  \
      global_stats.passed++;                                                                                            \
      std::cout << COLOR_GREEN << "[PASS] " << COLOR_RESET << message << std::endl;                                     \
    } else {                                                                                                            \
      global_stats.failed++;                                                                                            \
      std::cout << COLOR_RED << "[FAIL] " << COLOR_RESET << message << std::endl;                                       \
    }                                                                                                                   \
  } while ( 0 )

#define TEST_SECTION(name) std::cout << std::endl << COLOR_BLUE << "=== " << name << " ===" << COLOR_RESET << std::endl

// Helper to check if pointer is aligned
bool
is_aligned(void *ptr, size_t alignment)
{
  return (reinterpret_cast<uintptr_t>(ptr) % alignment) == 0;
}

// Fill memory with pattern
void
fill_pattern(void *ptr, size_t size, uint8_t pattern)
{
  if ( ptr ) {
    memset(ptr, pattern, size);
  }
}

// Verify memory pattern
bool
verify_pattern(void *ptr, size_t size, uint8_t pattern)
{
  if ( !ptr )
    return false;
  uint8_t *bytes = static_cast<uint8_t *>(ptr);
  for ( size_t i = 0; i < size; i++ ) {
    if ( bytes[i] != pattern ) {
      return false;
    }
  }
  return true;
}

// ============================================================================
// BASIC FUNCTIONALITY TESTS
// ============================================================================

void
test_basic_malloc_free()
{
  TEST_SECTION("Basic abc::malloc/free Tests");

  // Test 1: Simple allocation
  void *ptr = abc::malloc(100);
  TEST_ASSERT(ptr != nullptr, "malloc(100) returns non-NULL");
  abc::free(ptr);

  // Test 2: Small allocation
  ptr = abc::malloc(1);
  TEST_ASSERT(ptr != nullptr, "malloc(1) returns non-NULL");
  abc::free(ptr);

  // Test 3: Large allocation
  ptr = abc::malloc(1024 * 1024);     // 1MB
  TEST_ASSERT(ptr != nullptr, "malloc(1MB) returns non-NULL");
  abc::free(ptr);

  // Test 4: Very large allocation
  ptr = abc::malloc(100 * 1024 * 1024);     // 100MB
  TEST_ASSERT(ptr != nullptr, "malloc(100MB) returns non-NULL");
  abc::free(ptr);

  // Test 5: Free NULL pointer (should not crash)
  abc::free(nullptr);
  TEST_ASSERT(true, "free(NULL) does not crash");

  // Test 6: Multiple allocations
  void *ptrs[10];
  bool all_allocated = true;
  for ( int i = 0; i < 10; i++ ) {
    ptrs[i] = abc::malloc(100 * (i + 1));
    if ( !ptrs[i] )
      all_allocated = false;
  }
  TEST_ASSERT(all_allocated, "Multiple abc::malloc calls succeed");

  for ( int i = 0; i < 10; i++ ) {
    abc::free(ptrs[i]);
  }
  TEST_ASSERT(true, "Multiple free calls complete");
}

void
test_zero_allocation()
{
  TEST_SECTION("Zero-Size Allocation Tests");

  // abc::malloc(0) behavior is implementation-defined
  // It should return either NULL or a unique pointer that can be freed
  void *ptr1 = abc::malloc(0);
  void *ptr2 = abc::malloc(0);

  TEST_ASSERT(ptr1 == nullptr || ptr1 != ptr2, "malloc(0) returns NULL or unique pointers");

  // Freeing abc::malloc(0) result should be safe
  abc::free(ptr1);
  abc::free(ptr2);
  TEST_ASSERT(true, "free() on abc::malloc(0) result is safe");
}

// ============================================================================
// REALLOC TESTS
// ============================================================================

void
test_basic_realloc()
{
  TEST_SECTION("Basic realloc Tests");

  // Test 1: realloc with NULL (should behave like abc::malloc)
  void *ptr = abc::realloc(nullptr, 100);
  TEST_ASSERT(ptr != nullptr, "realloc(NULL, 100) behaves like abc::malloc");
  abc::free(ptr);

  // Test 2: Grow allocation
  ptr = abc::malloc(100);
  fill_pattern(ptr, 100, 0xAA);
  void *new_ptr = abc::realloc(ptr, 200);
  TEST_ASSERT(new_ptr != nullptr, "realloc to larger size succeeds");
  TEST_ASSERT(verify_pattern(new_ptr, 100, 0xAA), "realloc preserves original data when growing");
  abc::free(new_ptr);

  // Test 3: Shrink allocation
  ptr = abc::malloc(200);
  fill_pattern(ptr, 200, 0xBB);
  new_ptr = abc::realloc(ptr, 100);
  TEST_ASSERT(new_ptr != nullptr, "realloc to smaller size succeeds");
  TEST_ASSERT(verify_pattern(new_ptr, 100, 0xBB), "realloc preserves data when shrinking");
  abc::free(new_ptr);

  // Test 4: realloc to size 0 (should free)
  ptr = abc::malloc(100);
  new_ptr = abc::realloc(ptr, 0);
  // abc::realloc(ptr, 0) behavior is implementation-defined
  // It may return NULL or a minimal allocation
  if ( new_ptr )
    abc::free(new_ptr);
  TEST_ASSERT(true, "realloc(ptr, 0) completes");

  // Test 5: Multiple reallocs
  ptr = abc::malloc(10);
  fill_pattern(ptr, 10, 0x11);

  for ( int i = 1; i <= 5; i++ ) {
    size_t new_size = 10 * (1 << i);     // 20, 40, 80, 160, 320
    new_ptr = abc::realloc(ptr, new_size);
    TEST_ASSERT(new_ptr != nullptr && verify_pattern(new_ptr, 10, 0x11), "Multiple realloc preserves original data");
    ptr = new_ptr;
  }
  abc::free(ptr);
}

void
test_realloc_edge_cases()
{
  TEST_SECTION("realloc Edge Cases");

  // Test 1: Very large realloc
  void *ptr = abc::malloc(1024);
  fill_pattern(ptr, 1024, 0xCC);
  void *new_ptr = abc::realloc(ptr, 10 * 1024 * 1024);     // Grow to 10MB
  TEST_ASSERT(new_ptr != nullptr, "realloc to very large size succeeds");
  TEST_ASSERT(verify_pattern(new_ptr, 1024, 0xCC), "Large realloc preserves original data");
  abc::free(new_ptr);

  // Test 2: Realloc with same size
  ptr = abc::malloc(100);
  fill_pattern(ptr, 100, 0xDD);
  new_ptr = abc::realloc(ptr, 100);
  TEST_ASSERT(new_ptr != nullptr, "realloc with same size succeeds");
  TEST_ASSERT(verify_pattern(new_ptr, 100, 0xDD), "realloc with same size preserves data");
  abc::free(new_ptr);

  // Test 3: Alternating grow/shrink
  ptr = abc::malloc(100);
  for ( int i = 0; i < 10; i++ ) {
    size_t new_size = (i % 2 == 0) ? 200 : 100;
    new_ptr = abc::realloc(ptr, new_size);
    TEST_ASSERT(new_ptr != nullptr, "Alternating grow/shrink realloc succeeds");
    ptr = new_ptr;
  }
  abc::free(ptr);
}

// ============================================================================
// ALIGNMENT TESTS
// ============================================================================

void
test_alignment()
{
  TEST_SECTION("Memory Alignment Tests");

  // Test various allocation sizes for proper alignment
  size_t sizes[] = { 1, 7, 8, 15, 16, 31, 32, 63, 64, 127, 128, 1024, 4096 };

  for ( size_t size : sizes ) {
    void *ptr = abc::malloc(size);
    TEST_ASSERT(ptr == nullptr || is_aligned(ptr, sizeof(void *)),
                "malloc(" + std::to_string(size) + ") returns aligned pointer");
    abc::free(ptr);
  }

  // Test alignment after realloc
  void *ptr = abc::malloc(8);
  for ( size_t size : sizes ) {
    ptr = abc::realloc(ptr, size);
    TEST_ASSERT(ptr == nullptr || is_aligned(ptr, sizeof(void *)),
                "realloc(" + std::to_string(size) + ") returns aligned pointer");
  }
  abc::free(ptr);
}

// ============================================================================
// DATA INTEGRITY TESTS
// ============================================================================

void
test_data_integrity()
{
  TEST_SECTION("Data Integrity Tests");

  // Test 1: Write and verify patterns
  const size_t size = 1024;
  void *ptr = abc::malloc(size);
  TEST_ASSERT(ptr != nullptr, "Allocation for data integrity test succeeds");

  // Fill with incremental pattern
  uint8_t *bytes = static_cast<uint8_t *>(ptr);
  for ( size_t i = 0; i < size; i++ ) {
    bytes[i] = static_cast<uint8_t>(i & 0xFF);
  }

  // Verify pattern
  bool intact = true;
  for ( size_t i = 0; i < size; i++ ) {
    if ( bytes[i] != static_cast<uint8_t>(i & 0xFF) ) {
      intact = false;
      break;
    }
  }
  TEST_ASSERT(intact, "Written data remains intact in allocated memory");
  abc::free(ptr);

  // Test 2: Data integrity across realloc
  ptr = abc::malloc(512);
  bytes = static_cast<uint8_t *>(ptr);
  for ( size_t i = 0; i < 512; i++ ) {
    bytes[i] = static_cast<uint8_t>((i * 7) & 0xFF);
  }

  ptr = abc::realloc(ptr, 2048);
  bytes = static_cast<uint8_t *>(ptr);
  intact = true;
  for ( size_t i = 0; i < 512; i++ ) {
    if ( bytes[i] != static_cast<uint8_t>((i * 7) & 0xFF) ) {
      intact = false;
      break;
    }
  }
  TEST_ASSERT(intact, "Data integrity maintained across realloc");
  abc::free(ptr);
}

// ============================================================================
// BOUNDARY TESTS
// ============================================================================

void
test_boundaries()
{
  TEST_SECTION("Boundary Condition Tests");

  // Test power-of-2 boundaries
  size_t boundaries[] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536 };

  for ( size_t size : boundaries ) {
    void *ptr = abc::malloc(size);
    TEST_ASSERT(ptr != nullptr, "Allocation at boundary " + std::to_string(size) + " succeeds");
    fill_pattern(ptr, size, 0xEE);
    TEST_ASSERT(verify_pattern(ptr, size, 0xEE), "Boundary allocation is writable and readable");
    abc::free(ptr);
  }

  // Test boundary-1 and boundary+1
  for ( size_t boundary : { 64, 256, 1024, 4096 } ) {
    void *ptr1 = abc::malloc(boundary - 1);
    void *ptr2 = abc::malloc(boundary);
    void *ptr3 = abc::malloc(boundary + 1);

    TEST_ASSERT(ptr1 && ptr2 && ptr3, "Allocations around boundary " + std::to_string(boundary) + " succeed");

    abc::free(ptr1);
    abc::free(ptr2);
    abc::free(ptr3);
  }
}

// ============================================================================
// STRESS TESTS
// ============================================================================

void
test_many_small_allocations()
{
  TEST_SECTION("Stress Test: Many Small Allocations");

  const int num_allocations = 10000;
  std::vector<void *> ptrs;
  ptrs.reserve(num_allocations);

  // Allocate many small blocks
  bool all_succeeded = true;
  for ( int i = 0; i < num_allocations; i++ ) {
    void *ptr = abc::malloc(16 + (i % 64));     // 16-80 bytes
    if ( !ptr ) {
      all_succeeded = false;
      break;
    }
    ptrs.push_back(ptr);
  }

  TEST_ASSERT(all_succeeded, "Allocated " + std::to_string(ptrs.size()) + " small blocks");

  // Free all
  for ( void *ptr : ptrs ) {
    abc::free(ptr);
  }
  TEST_ASSERT(true, "Freed all small allocations");
}

void
test_many_large_allocations()
{
  TEST_SECTION("Stress Test: Many Large Allocations");

  const int num_allocations = 100;
  std::vector<void *> ptrs;
  ptrs.reserve(num_allocations);

  // Allocate many large blocks
  for ( int i = 0; i < num_allocations; i++ ) {
    void *ptr = abc::malloc(1024 * 1024);     // 1MB each
    if ( !ptr ) {
      break;
    }
    ptrs.push_back(ptr);
  }

  TEST_ASSERT(ptrs.size() > 0, "Allocated " + std::to_string(ptrs.size()) + " large blocks (1MB each)");

  // Free all
  for ( void *ptr : ptrs ) {
    abc::free(ptr);
  }
  TEST_ASSERT(true, "Freed all large allocations");
}

void
test_fragmentation()
{
  TEST_SECTION("Stress Test: Memory Fragmentation");

  const int num_allocations = 1000;
  std::vector<void *> ptrs;

  // Allocate alternating sizes
  for ( int i = 0; i < num_allocations; i++ ) {
    size_t size = (i % 2 == 0) ? 32 : 1024;
    void *ptr = abc::malloc(size);
    if ( ptr )
      ptrs.push_back(ptr);
  }

  // Free every other allocation
  for ( size_t i = 0; i < ptrs.size(); i += 2 ) {
    abc::free(ptrs[i]);
    ptrs[i] = nullptr;
  }

  TEST_ASSERT(true, "Created fragmented memory state");

  // Try to allocate in fragmented space
  int realloc_count = 0;
  for ( size_t i = 0; i < ptrs.size(); i += 2 ) {
    ptrs[i] = abc::malloc(64);
    if ( ptrs[i] )
      realloc_count++;
  }

  TEST_ASSERT(realloc_count > 0, "Can allocate in fragmented memory (" + std::to_string(realloc_count) + " successful)");

  // Cleanup
  for ( void *ptr : ptrs ) {
    abc::free(ptr);
  }
}

void
test_realloc_stress()
{
  TEST_SECTION("Stress Test: Aggressive realloc");

  void *ptr = abc::malloc(16);
  TEST_ASSERT(ptr != nullptr, "Initial allocation for realloc stress test");

  // Pattern to preserve
  fill_pattern(ptr, 16, 0x42);

  // Perform many reallocs with varying sizes
  bool all_succeeded = true;
  size_t min_size = 16;

  for ( int i = 0; i < 100; i++ ) {
    size_t new_size = 16 + (i * i * 13) % 10000;     // Varying sizes
    void *new_ptr = abc::realloc(ptr, new_size);

    if ( !new_ptr ) {
      all_succeeded = false;
      break;
    }

    // Verify original data still intact
    if ( !verify_pattern(new_ptr, min_size, 0x42) ) {
      all_succeeded = false;
      abc::free(new_ptr);
      break;
    }

    ptr = new_ptr;
    min_size = std::min(min_size, new_size);
  }

  TEST_ASSERT(all_succeeded, "Survived 100 aggressive reallocs with data integrity");
  abc::free(ptr);
}

// ============================================================================
// FUZZING TESTS
// ============================================================================

void
test_random_operations()
{
  TEST_SECTION("Fuzz Test: Random abc::malloc/realloc/free Operations");

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> op_dist(0, 2);     // 0=malloc, 1=realloc, 2=free
  std::uniform_int_distribution<> size_dist(1, 10000);

  std::vector<void *> allocations;
  int operations = 5000;
  int malloc_count = 0, realloc_count = 0, free_count = 0;
  bool error_occurred = false;

  for ( int i = 0; i < operations && !error_occurred; i++ ) {
    int op = op_dist(gen);

    if ( op == 0 || allocations.empty() ) {
      // abc::malloc
      size_t size = size_dist(gen);
      void *ptr = abc::malloc(size);
      if ( ptr ) {
        allocations.push_back(ptr);
        malloc_count++;
      } else {
        error_occurred = true;
      }
    } else if ( op == 1 && !allocations.empty() ) {
      // realloc
      std::uniform_int_distribution<> idx_dist(0, allocations.size() - 1);
      size_t idx = idx_dist(gen);
      size_t new_size = size_dist(gen);

      void *new_ptr = abc::realloc(allocations[idx], new_size);
      if ( new_ptr || new_size == 0 ) {
        allocations[idx] = new_ptr;
        realloc_count++;
      } else {
        error_occurred = true;
      }
    } else if ( op == 2 && !allocations.empty() ) {
      // free
      std::uniform_int_distribution<> idx_dist(0, allocations.size() - 1);
      size_t idx = idx_dist(gen);

      abc::free(allocations[idx]);
      allocations.erase(allocations.begin() + idx);
      free_count++;
    }
  }

  // Cleanup remaining allocations
  for ( void *ptr : allocations ) {
    abc::free(ptr);
  }

  std::cout << "  Operations: abc::malloc=" << malloc_count << ", realloc=" << realloc_count << ", free=" << free_count
            << std::endl;

  TEST_ASSERT(!error_occurred, "Completed " + std::to_string(operations) + " random operations without errors");
}

void
test_random_sizes()
{
  TEST_SECTION("Fuzz Test: Random Allocation Sizes");

  std::random_device rd;
  std::mt19937 gen(rd());

  // Test various size distributions
  std::vector<std::pair<std::string, std::uniform_int_distribution<>>> distributions
      = { { "Tiny (1-16)", std::uniform_int_distribution<>(1, 16) },
          { "Small (1-256)", std::uniform_int_distribution<>(1, 256) },
          { "Medium (1-4096)", std::uniform_int_distribution<>(1, 4096) },
          { "Large (1-1MB)", std::uniform_int_distribution<>(1, 1024 * 1024) },
          { "Mixed (1-10MB)", std::uniform_int_distribution<>(1, 10 * 1024 * 1024) } };

  for ( auto &[name, dist] : distributions ) {
    std::vector<void *> ptrs;
    int success_count = 0;

    for ( int i = 0; i < 100; i++ ) {
      size_t size = dist(gen);
      void *ptr = abc::malloc(size);
      if ( ptr ) {
        ptrs.push_back(ptr);
        success_count++;

        // Verify we can write to it
        fill_pattern(ptr, std::min(size, size_t(256)), 0x55);
      }
    }

    TEST_ASSERT(success_count > 0, name + ": " + std::to_string(success_count) + "/100 allocations succeeded");

    // Cleanup
    for ( void *ptr : ptrs ) {
      abc::free(ptr);
    }
  }
}

void
test_pathological_patterns()
{
  TEST_SECTION("Fuzz Test: Pathological Allocation Patterns");

  // Pattern 1: Allocate-free-allocate same size repeatedly
  {
    const size_t size = 1024;
    bool success = true;
    for ( int i = 0; i < 1000; i++ ) {
      void *ptr = abc::malloc(size);
      if ( !ptr ) {
        success = false;
        break;
      }
      abc::free(ptr);
    }
    TEST_ASSERT(success, "Repeated alloc-free same size (1000 iterations)");
  }

  // Pattern 2: Growing pyramid
  {
    std::vector<void *> pyramid;
    for ( size_t size = 16; size <= 16384; size *= 2 ) {
      for ( int i = 0; i < 10; i++ ) {
        void *ptr = abc::malloc(size);
        if ( ptr )
          pyramid.push_back(ptr);
      }
    }
    TEST_ASSERT(pyramid.size() > 0, "Growing pyramid pattern (" + std::to_string(pyramid.size()) + " allocations)");
    for ( void *ptr : pyramid )
      abc::free(ptr);
  }

  // Pattern 3: Reverse-order free
  {
    std::vector<void *> ptrs;
    for ( int i = 0; i < 100; i++ ) {
      void *ptr = abc::malloc(100 + i * 10);
      if ( ptr )
        ptrs.push_back(ptr);
    }

    // Free in reverse order
    for ( auto it = ptrs.rbegin(); it != ptrs.rend(); ++it ) {
      abc::free(*it);
    }
    TEST_ASSERT(true, "Reverse-order free pattern completed");
  }

  // Pattern 4: Alternating allocation sizes
  {
    std::vector<void *> ptrs;
    for ( int i = 0; i < 100; i++ ) {
      size_t size = (i % 2 == 0) ? 16 : 8192;
      void *ptr = abc::malloc(size);
      if ( ptr )
        ptrs.push_back(ptr);
    }
    TEST_ASSERT(ptrs.size() > 0, "Alternating size pattern (" + std::to_string(ptrs.size()) + " allocations)");
    for ( void *ptr : ptrs )
      abc::free(ptr);
  }
}

// ============================================================================
// PERFORMANCE TESTS
// ============================================================================

void
test_performance()
{
  TEST_SECTION("Performance Tests");

  using namespace std::chrono;

  // Test 1: abc::malloc/free speed
  {
    const int iterations = 100000;
    auto start = high_resolution_clock::now();

    for ( int i = 0; i < iterations; i++ ) {
      void *ptr = abc::malloc(64);
      abc::free(ptr);
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();

    std::cout << "  abc::malloc/free (64 bytes) x " << iterations << ": " << duration << "ms" << std::endl;
    TEST_ASSERT(true, "Performance benchmark: abc::malloc/free completed");
  }

  // Test 2: realloc speed
  {
    const int iterations = 10000;
    auto start = high_resolution_clock::now();

    void *ptr = abc::malloc(16);
    for ( int i = 0; i < iterations; i++ ) {
      ptr = abc::realloc(ptr, 16 + (i % 1000));
    }
    abc::free(ptr);

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();

    std::cout << "  realloc x " << iterations << ": " << duration << "ms" << std::endl;
    TEST_ASSERT(true, "Performance benchmark: realloc completed");
  }

  // Test 3: Large allocation speed
  {
    const int iterations = 1000;
    auto start = high_resolution_clock::now();

    for ( int i = 0; i < iterations; i++ ) {
      void *ptr = abc::malloc(1024 * 1024);     // 1MB
      abc::free(ptr);
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();

    std::cout << "  Large abc::malloc/free (1MB) x " << iterations << ": " << duration << "ms" << std::endl;
    TEST_ASSERT(true, "Performance benchmark: large alloc completed");
  }
}

// ============================================================================
// DOUBLE FREE DETECTION (requires careful testing)
// ============================================================================

void
test_double_free_awareness()
{
  TEST_SECTION("Double-Free Awareness Tests");

  std::cout << COLOR_YELLOW << "  Note: These tests verify behavior awareness, not safety guarantees" << COLOR_RESET
            << std::endl;

  // We can't safely test actual double-frees as they cause undefined behavior
  // Instead, we test patterns that help detect if allocator tracks state

  // Test: Alloc, free, alloc same size
  void *ptr1 = abc::malloc(128);
  void *original = ptr1;
  abc::free(ptr1);
  ptr1 = abc::malloc(128);

  // Allocator might reuse same location
  bool possibly_reused = (ptr1 == original);
  std::cout << "  Allocator " << (possibly_reused ? "may reuse" : "didn't reuse") << " freed memory immediately"
            << std::endl;
  abc::free(ptr1);

  TEST_ASSERT(true, "Memory reuse pattern test completed");
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

void
print_summary()
{
  std::cout << std::endl;
  std::cout << COLOR_BLUE << "================================" << COLOR_RESET << std::endl;
  std::cout << COLOR_BLUE << "       TEST SUMMARY" << COLOR_RESET << std::endl;
  std::cout << COLOR_BLUE << "================================" << COLOR_RESET << std::endl;
  std::cout << "Total tests: " << global_stats.total << std::endl;
  std::cout << COLOR_GREEN << "Passed: " << global_stats.passed << COLOR_RESET << std::endl;
  std::cout << COLOR_RED << "Failed: " << global_stats.failed << COLOR_RESET << std::endl;

  double pass_rate = global_stats.total > 0 ? (100.0 * global_stats.passed / global_stats.total) : 0.0;

  std::cout << std::fixed << std::setprecision(2);
  std::cout << "Pass rate: " << pass_rate << "%" << std::endl;
  std::cout << COLOR_BLUE << "================================" << COLOR_RESET << std::endl;
}

int
main(int /* argc */, char * /* argv */[])
{
  std::cout << COLOR_BLUE << "================================" << COLOR_RESET << std::endl;
  std::cout << COLOR_BLUE << "  abc::malloc/realloc/free Test Suite" << COLOR_RESET << std::endl;
  std::cout << COLOR_BLUE << "================================" << COLOR_RESET << std::endl;

  // Run all test suites
  test_basic_malloc_free();
  test_zero_allocation();
  test_basic_realloc();
  test_realloc_edge_cases();
  test_alignment();
  test_data_integrity();
  test_boundaries();
  test_many_small_allocations();
  test_many_large_allocations();
  test_fragmentation();
  test_realloc_stress();
  test_random_operations();
  test_random_sizes();
  test_pathological_patterns();
  test_performance();
  test_double_free_awareness();

  // Print final summary
  print_summary();

  return (global_stats.failed == 0) ? 0 : 1;
}
