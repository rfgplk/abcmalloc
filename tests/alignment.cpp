#include "../src/cmalloc.hpp"


#include <cstdlib>
#include <cassert>
#include <iostream>
#include <cstdint>
#include <vector>

bool is_aligned(void* ptr, std::size_t alignment) {
    return reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0;
}

int main() {
    std::vector<std::size_t> sizes = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
    std::vector<void*> allocations;

    // Allocate blocks of varying size
    for (auto size : sizes) {
        void* p = abc::malloc(size);
        assert(p != nullptr);
        allocations.push_back(p);

        // Test alignment: malloc guarantees at least alignof(max_align_t)
        assert(is_aligned(p, alignof(std::max_align_t)));
    }

    // Free all allocations
    for (auto ptr : allocations) {
        abc::free(ptr);
    }

    std::cout << "Memory alignment test PASSED\n";
    return 0;
}
