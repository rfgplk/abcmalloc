#include "../src/cmalloc.hpp"

#include <cstdlib>
#include <cassert>
#include <iostream>
#include <vector>
#include <algorithm>
#include <random>

int main() {
    constexpr int num_blocks = 1000;
    std::vector<void*> blocks;
    std::mt19937 rng(42); // fixed seed for reproducibility
    std::uniform_int_distribution<std::size_t> size_dist(8, 512);

    for (int i = 0; i < num_blocks; ++i) {
        std::size_t sz = size_dist(rng);
        void* p = abc::malloc(sz);
        assert(p != nullptr);
        blocks.push_back(p);
    }

    std::shuffle(blocks.begin(), blocks.end(), rng);

    for (int i = 0; i < num_blocks / 2; ++i) {
        abc::free(blocks[i]);
        blocks[i] = nullptr;
    }

    std::shuffle(blocks.begin() + num_blocks / 2, blocks.end(), rng);
    for (int i = num_blocks / 2; i < num_blocks; ++i) {
        abc::free(blocks[i]);
        blocks[i] = nullptr;
    }

    for (auto ptr : blocks) {
        assert(ptr == nullptr);
    }

    std::cout << "Randomized allocation/deallocation test PASSED\n";
    return 0;
}
