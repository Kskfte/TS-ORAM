#include <cassert>
#include "pir_write.h"
#include "utils.h"

PIR_WRITE::PIR_WRITE(size_t tag_num, size_t tag_size) {
    this->tag_num = tag_num;
    this->tag_size = tag_size;
    //this->tag_cell_array.reserve(tag_num * (tag_size / sizeof(cell_type)));
}

PIR_WRITE::~PIR_WRITE() {
    // Destructor implementation (if needed)
}

size_t PIR_WRITE::size() const {
    return this->tag_cell_array.size() * sizeof(cell_type) / this->tag_size;
}

void PIR_WRITE::push_back(const tag_type tag) {
    size_t num_cell_per_tag = this->tag_size / sizeof(cell_type);
    for (size_t i = 0; i < num_cell_per_tag; ++i) {
        this->tag_cell_array.push_back(tag[i]);
    }
}

tag_type PIR_WRITE::read_tag(size_t index) {
    if (index >= this->tag_cell_array.size() / (this->tag_size / sizeof(cell_type))) {
        throw std::out_of_range("Index out of range");
    }

    size_t num_cell_per_tag = this->tag_size / sizeof(cell_type);
    size_t start_idx = index * num_cell_per_tag;
    
    tag_type tag(num_cell_per_tag);
    for (size_t i = 0; i < num_cell_per_tag; ++i) {
        tag[i] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&this->tag_cell_array[start_idx + i]));
        
    }
    
    return tag;
}

void PIR_WRITE::pir_write(const std::vector<uint8_t>& indexing, const tag_type& tag) {
    size_t num_cell_per_tag = tag_size / sizeof(cell_type);
    size_t i;
    for (i = 0; i + 8 < tag_num; i += 8) {
        size_t tmp = indexing[i / 8];
        for (size_t k = 0; k < num_cell_per_tag; ++k) {
            tag_cell_array[(i + 0) * num_cell_per_tag + k] = _mm256_xor_si256(tag_cell_array[(i + 0) * num_cell_per_tag + k], _mm256_and_si256(tag[k], _mm256_set1_epi64x(-((tmp >> 0) & 1))));       
            tag_cell_array[(i + 1) * num_cell_per_tag + k] = _mm256_xor_si256(tag_cell_array[(i + 1) * num_cell_per_tag + k], _mm256_and_si256(tag[k], _mm256_set1_epi64x(-((tmp >> 1) & 1))));       
            tag_cell_array[(i + 2) * num_cell_per_tag + k] = _mm256_xor_si256(tag_cell_array[(i + 2) * num_cell_per_tag + k], _mm256_and_si256(tag[k], _mm256_set1_epi64x(-((tmp >> 2) & 1))));       
            tag_cell_array[(i + 3) * num_cell_per_tag + k] = _mm256_xor_si256(tag_cell_array[(i + 3) * num_cell_per_tag + k], _mm256_and_si256(tag[k], _mm256_set1_epi64x(-((tmp >> 3) & 1))));       
            tag_cell_array[(i + 4) * num_cell_per_tag + k] = _mm256_xor_si256(tag_cell_array[(i + 4) * num_cell_per_tag + k], _mm256_and_si256(tag[k], _mm256_set1_epi64x(-((tmp >> 4) & 1))));       
            tag_cell_array[(i + 5) * num_cell_per_tag + k] = _mm256_xor_si256(tag_cell_array[(i + 5) * num_cell_per_tag + k], _mm256_and_si256(tag[k], _mm256_set1_epi64x(-((tmp >> 5) & 1))));       
            tag_cell_array[(i + 6) * num_cell_per_tag + k] = _mm256_xor_si256(tag_cell_array[(i + 6) * num_cell_per_tag + k], _mm256_and_si256(tag[k], _mm256_set1_epi64x(-((tmp >> 6) & 1))));       
            tag_cell_array[(i + 7) * num_cell_per_tag + k] = _mm256_xor_si256(tag_cell_array[(i + 7) * num_cell_per_tag + k], _mm256_and_si256(tag[k], _mm256_set1_epi64x(-((tmp >> 7) & 1))));       
            }
    }      

    // Process the remaining tags
    if (i < tag_num) {
        uint64_t tmp = indexing[i / 8];
        for (size_t j = 0; j < tag_num - i; ++j) {
            for (size_t k = 0; k < num_cell_per_tag; ++k) {
                tag_cell_array[(i + j) * num_cell_per_tag + k] = _mm256_xor_si256(tag_cell_array[(i + j) * num_cell_per_tag + k], _mm256_and_si256(tag[k], _mm256_set1_epi64x(-((tmp >> j) & 1))));       
            }
        }
    }
}
