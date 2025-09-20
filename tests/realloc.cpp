#include "../src/cmalloc.hpp"

#include <cstdlib>
#include <cassert>
#include <iostream>
#include <cstring>

int main() {
    constexpr int num_blocks = 500;
    void* blocks[num_blocks];

    // Step 1: Allocate initial blocks
    for (int i = 0; i < num_blocks; ++i) {
        blocks[i] = abc::malloc(16);
        assert(blocks[i] != nullptr);
        std::memset(blocks[i], 0xAA, 16); // initialize memory
    }

    // Step 2: Grow blocks to larger size
    for (int i = 0; i < num_blocks; ++i) {
        std::size_t new_size = 16 + (i % 64);
        void* new_block = abc::malloc(new_size);
        assert(new_block != nullptr);
        std::memcpy(new_block, blocks[i], 16); // copy old content
        abc::free(blocks[i]);
        blocks[i] = new_block;
    }

    // Step 3: Shrink blocks to smaller size
    for (int i = 0; i < num_blocks; ++i) {
        std::size_t shrink_size = 8 + (i % 8);
        void* new_block = abc::malloc(shrink_size);
        assert(new_block != nullptr);
        std::memcpy(new_block, blocks[i], shrink_size); // copy partial content
        abc::free(blocks[i]);
        blocks[i] = new_block;
    }

    // Step 4: Free all blocks
    for (int i = 0; i < num_blocks; ++i) {
        abc::free(blocks[i]);
    }

    std::cout << "Allocation growth/shrink test PASSED\n";
    return 0;
}
