#include "utils.h"

size_t hash_function_tag(size_t table_length, key_type& hash_key, tag_type& tag) {
    AES aes(hash_key);

    // Initialize the hash state to a predefined value
    block hash_state = _mm_set1_epi32(0);

    // Process each cell in the tag
    for (const auto& cell : tag) {
        // Split the 256-bit cell into two 128-bit blocks
        block cell_block1 = _mm256_extracti128_si256(cell, 0);
        block cell_block2 = _mm256_extracti128_si256(cell, 1);

        // Apply AES MMO to each 128-bit block and XOR with hash_state
        block encrypted_block1 = aes.encryptECB_MMO(cell_block1);
        block encrypted_block2 = aes.encryptECB_MMO(cell_block2);

        hash_state = _mm_xor_si128(hash_state, encrypted_block1);
        hash_state = _mm_xor_si128(hash_state, encrypted_block2);
    }

    // Convert the final hash state to a size_t value
    uint64_t hash_result[2];
    _mm_storeu_si128(reinterpret_cast<__m128i*>(hash_result), hash_state);

    // Use the hash result to determine the index in the table
    return static_cast<size_t>((hash_result[0] ^ hash_result[1]) % table_length);
}

size_t hash_function_addr(size_t table_length, key_type& hash_key, size_t addr) {
    AES aes(hash_key);
    block plaintex_block = _mm_set1_epi32(addr);
    block encrypted_block = aes.encryptECB_MMO(plaintex_block);
    // Convert the final hash state to a size_t value
    uint64_t hash_result[2];
    _mm_storeu_si128(reinterpret_cast<__m128i*>(hash_result), encrypted_block);

    // Use the hash result to determine the index in the table
    return static_cast<size_t>((hash_result[0] ^ hash_result[1]) % table_length);
}

size_t get_random_position(size_t table_length) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dis(0, table_length - 1);
    return dis(gen);
}
