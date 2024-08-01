#include <iostream>
#include "../DPF-CPP/AES.h"
#include "../TW-PIR/utils.h"
#include "../TW-PIR/alignment_allocator.h"
#include "utils.h"

// Assuming tag_type is defined appropriately, e.g.,
// using tag_type = std::vector<uint8_t>;

int main() {
    // Define key and tag
    size_t table_length = 10; // Example table length
    size_t num_tags = 100; // Number of tags to test
    size_t cells_per_tag = 4; // Number of cells in each tag

    // Example key
    key_type hash_key = _mm_set_epi32(0x01234567, 0x89abcdef, 0xfedcba98, 0x76543210);

    // Random number generator
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;

    // Generate and test tags
    for (size_t i = 0; i < num_tags; ++i) {
        tag_type tag(cells_per_tag);
        for (size_t j = 0; j < cells_per_tag; ++j) {
            tag[j] = _mm256_set_epi64x(dis(gen), dis(gen), dis(gen), dis(gen));
        }

        // Compute hash for the tag
        size_t hash = hash_function_tag(table_length, hash_key, tag);

        // Print result
        std::cout << "Hash for tag " << i + 1 << ": " << hash << std::endl;
    }

    return 0;
}
