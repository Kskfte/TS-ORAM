#include <cassert>
#include "pir_read.h"

PIR_Read::PIR_Read(size_t block_num, size_t block_size) {
    /*
    block = (8-byte addr, other-value)
    block_size >= 32
    32, 64, 256, 1024...
    */
    this->block_num = block_num;
    this->block_size = block_size;
    this->data_array.reserve(block_num * (block_size / sizeof(cell_type)));
}

PIR_Read::~PIR_Read() {
    // Destructor implementation (if needed)
}

size_t PIR_Read::size() const {
    return data_array.size() * sizeof(cell_type) / block_size;
}

void PIR_Read::push_back(const uint8_t* data) {
    size_t num_blocks = block_size / sizeof(cell_type);
    for (size_t i = 0; i < num_blocks; ++i) {
        cell_type block;
        memcpy(&block, data + i * sizeof(cell_type), sizeof(cell_type));
        data_array.push_back(block);
    }
}

uint8_t* PIR_Read::read_element(size_t index) {
    if (index >= data_array.size() / (block_size / sizeof(cell_type))) {
        throw std::out_of_range("Index out of range");
    }

    size_t blocks_per_full_block = block_size / sizeof(cell_type);
    size_t start_idx = index * blocks_per_full_block;
    
    uint8_t* block = new uint8_t[block_size];

    for (size_t i = 0; i < blocks_per_full_block; ++i) {
        std::memcpy(block + i * sizeof(cell_type), &data_array[start_idx + i], sizeof(cell_type));
    }

    return block;
}

element_type PIR_Read::pir_read(const std::vector<uint8_t>& indexing) const {
    size_t num_cell_per_element = block_size / sizeof(cell_type);
    element_type result(num_cell_per_element, _mm256_set_epi64x(0, 0, 0, 0));
    size_t i;
    // assert(data_array.size() % 8 == 0);
    for (i = 0; i + 8 < block_num; i += 8) {
        uint64_t tmp = indexing[i / 8];
        for (size_t k = 0; k < num_cell_per_element; ++k) {
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(data_array[(i + 0) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 0) & 1))));       
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(data_array[(i + 1) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 1) & 1))));
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(data_array[(i + 2) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 2) & 1))));
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(data_array[(i + 3) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 3) & 1))));
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(data_array[(i + 4) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 4) & 1))));
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(data_array[(i + 5) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 5) & 1))));
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(data_array[(i + 6) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 6) & 1))));
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(data_array[(i + 7) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 7) & 1))));  
            }
    }

    // Process the remaining elements
    if (i < block_num) {
        uint64_t tmp = indexing[i / 8];
        for (size_t j = 0; j < block_num - i; ++j) {
            for (size_t k = 0; k < num_cell_per_element; ++k) {
                result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(data_array[(i + j) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> j) & 1))));
            }
        }
    }
    return result;
}
