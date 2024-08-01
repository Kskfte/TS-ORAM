#include <cassert>
#include "utils.h"

// Helper function to generate random bytes
void fill_random_element_bytes(uint8_t* buffer, size_t size) {
    const char charset[] = 
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = sizeof(charset) - 1;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dis(0, max_index);

    for (size_t i = 0; i < size; ++i) {
        buffer[i] = charset[dis(gen)];
    }
}

void fill_random_tag_bytes(uint8_t* buffer, size_t size) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dis(0, 255);
    for (size_t i = 0; i < size; ++i) {
        buffer[i] = dis(gen);
    }
}

// Random index from [1, length]
size_t get_random_index(size_t length) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dis(1, length);

    return dis(gen);

}
uint8_t* get_random_uint8t(size_t uint_size) {
    uint8_t* random_array = new uint8_t[uint_size];
    fill_random_element_bytes(random_array, uint_size);
    return random_array;
}

element_type get_random_element(size_t vir_addr, size_t element_size) {
    element_type element(element_size / sizeof(cell_type));
    std::memcpy(&element[0], &vir_addr, sizeof(addr_type));
    fill_random_element_bytes(reinterpret_cast<uint8_t*>(&element[0]) + sizeof(addr_type), element_size - sizeof(addr_type));
    return element;
}

uint8_t* convert_addrVal_to_bytes(std::pair<size_t, uint8_t*> addr_value, size_t element_size) {
    uint8_t* bytes = new uint8_t[element_size];
    std::memcpy(bytes, &addr_value.first, sizeof(size_t));
    std::memcpy(bytes + sizeof(size_t), addr_value.second, element_size - sizeof(size_t));
    return bytes;
}

std::pair<size_t, uint8_t*> convert_bytes_to_addrVal(uint8_t* bytes, size_t element_size) {
    size_t addr;
    uint8_t* value = new uint8_t[element_size-sizeof(addr)];
    std::memcpy(&addr, bytes, sizeof(size_t));
    std::memcpy(value, bytes + sizeof(size_t), element_size - sizeof(addr));
    return std::make_pair(addr, value);
}

element_type convert_share_to_element(const element_type& element1, const element_type& element2) {
    assert(element1.size() == element2.size());
    element_type result(element1.size());
    for (size_t i = 0; i < element1.size(); ++i) {
        result[i] = _mm256_xor_si256(element1[i], element2[i]);
    }
    return result;
}

element_type convert_addrVal_to_element(const std::pair<size_t, uint8_t*>& addr_value, size_t element_size) {
    element_type element;
    element.reserve(element_size / sizeof(cell_type));
    size_t num_cells = element_size / sizeof(cell_type);
    for (size_t i = 0; i < num_cells; ++i) {
        cell_type cell = _mm256_setzero_si256();
        if (i == 0) {
            std::memcpy(&cell, &addr_value.first, sizeof(size_t));
            std::memcpy(reinterpret_cast<uint8_t*>(&cell) + sizeof(size_t), addr_value.second, sizeof(cell_type) - sizeof(size_t));
        } else {
            std::memcpy(&cell, addr_value.second + i * sizeof(cell_type) - sizeof(size_t), sizeof(cell_type));
        }
        element.push_back(cell);
    }
    return element;
}

std::pair<size_t, uint8_t*> convert_element_to_addrVal(const element_type& element, size_t element_size) {
    size_t num_cells = element_size / sizeof(cell_type);

    if (element.size() != num_cells) {
        throw std::invalid_argument("Element size does not match the expected number of cells.");
    }

    // 提取 size_t 地址
    size_t addr;
    std::memcpy(&addr, &element[0], sizeof(size_t));

    // 提取 uint8_t* 值
    size_t value_size = element_size - sizeof(size_t);
    uint8_t* value = new uint8_t[value_size];

    // 从第一个 cell 中提取数据
    std::memcpy(value, reinterpret_cast<const uint8_t*>(&element[0]) + sizeof(size_t), sizeof(cell_type) - sizeof(size_t));

    // 从剩余 cells 中提取数据
    for (size_t i = 1; i < num_cells; ++i) {
        std::memcpy(value + i * sizeof(cell_type) - sizeof(size_t), &element[i], sizeof(cell_type));
    }

    return std::make_pair(addr, value);
}


element_type convert_bytes_to_element(const uint8_t* bytes, size_t element_size) {
    size_t num_cells = element_size / sizeof(cell_type);
    element_type element(num_cells);
    for (size_t i = 0; i < num_cells; ++i) {
        element[i] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(bytes + i * sizeof(__m256i)));
    }
    return element;
}

void print_element(const element_type& element, size_t element_size) {
    uint8_t* byte_array = new uint8_t[element_size];
    
    for (size_t i = 0; i < element.size(); ++i) {
        std::memcpy(byte_array + i * sizeof(cell_type), &element[i], sizeof(cell_type));
    }
    std::pair<size_t, uint8_t*> rr = convert_bytes_to_addrVal(byte_array, element_size);
    std::cout << rr.first << " " << convert_uint8_to_ascStr(rr.second, element_size - sizeof(size_t)) << "\n";
}

/**
 * 
 * 
 * 
 */

tag_type get_random_tag(size_t tag_size) {
    tag_type tag(tag_size / sizeof(cell_type));
    fill_random_tag_bytes(reinterpret_cast<uint8_t*>(tag.data()), tag_size);
    return tag;
}

std::pair<tag_type, tag_type> convert_tag_to_share(const tag_type& tag) {
    tag_type share1 = get_random_tag(tag.size() * sizeof(cell_type));
    tag_type share2(tag.size());
    for (size_t i = 0; i < tag.size(); ++i) {
        share2[i] = _mm256_xor_si256(tag[i], share1[i]);
    }
    return std::make_pair(share1, share2);
}

tag_type convert_share_to_tag(const tag_type& tag1, const tag_type& tag2) {
    tag_type tag(tag1.size());
    for (size_t i = 0; i < tag1.size(); ++i) {
        tag[i] = _mm256_xor_si256(tag1[i], tag2[i]);
    }
    return tag;
}

tag_type convert_bytes_to_tag(const uint8_t* bytes, size_t tag_size) {
    size_t num_cells = tag_size / sizeof(cell_type);
    tag_type tag(num_cells);
    for (size_t i = 0; i < num_cells; ++i) {
        tag[i] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(bytes + i * sizeof(__m256i)));
    }
    return tag;
}

uint8_t* convert_tag_to_bytes(const tag_type& tag) {
    return reinterpret_cast<uint8_t*>(const_cast<cell_type*>(tag.data()));
}

std::string convert_uint8_to_ascStr(const uint8_t* data, size_t size) {
    std::string result;
    result.reserve(size);
    for (size_t i = 0; i < size; ++i) {
        result += static_cast<char>(data[i]);
    }
    return result;
}

void print_tag(const tag_type& tag, size_t tag_size) {
    uint8_t* byte_array = new uint8_t[tag_size];
    
    for (size_t i = 0; i < tag.size(); ++i) {
        std::memcpy(byte_array + i * sizeof(cell_type), &tag[i], sizeof(cell_type));
    }
    std::cout << convert_uint8_to_ascStr(byte_array, tag_size) << "\n";
}

void set_bit(size_t& full_flag, size_t index, bool bit) {
    size_t mask = 1UL << index;
    full_flag = (full_flag & ~mask) | (static_cast<size_t>(bit) << index);
}

bool is_index_bit_1(size_t full_flag, size_t i) {
    return (full_flag & (1 << i)) != 0;
}

element_type pir_read(element_array_type& element_table, std::vector<uint8_t>& indexing, size_t element_num, size_t element_size) {
    size_t num_cell_per_element = element_size / sizeof(cell_type);
    element_type result(num_cell_per_element, _mm256_set_epi64x(0, 0, 0, 0));
    size_t i;
    for (i = 0; i + 7 < element_num; i += 8) {
        uint64_t tmp = indexing[i / 8];
        for (size_t k = 0; k < num_cell_per_element; ++k) {
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(element_table[(i + 0) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 0) & 1))));       
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(element_table[(i + 1) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 1) & 1))));
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(element_table[(i + 2) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 2) & 1))));
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(element_table[(i + 3) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 3) & 1))));
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(element_table[(i + 4) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 4) & 1))));
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(element_table[(i + 5) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 5) & 1))));
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(element_table[(i + 6) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 6) & 1))));
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(element_table[(i + 7) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 7) & 1))));  
            }
    }
    // Process the remaining elements
    if (i < element_num) {
        uint64_t tmp = indexing[i / 8];
        for (size_t j = 0; j < element_num - i; ++j) {
            for (size_t k = 0; k < num_cell_per_element; ++k) {
                result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(element_table[(i + j) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> j) & 1))));
            }
        }
    }
    return result;
}

element_type pir_read_with_offset(element_array_type& element_table, std::vector<uint8_t>& indexing, size_t element_num, size_t element_size, size_t offset) {
    size_t num_cell_per_element = element_size / sizeof(cell_type);
    element_type result(num_cell_per_element, _mm256_set_epi64x(0, 0, 0, 0));
    assert(element_num >> 3 << 3 == element_num);
    for (size_t i = 0; i + 7 < element_num; i += 8) {
        size_t tmp = indexing[i / 8];
        for (size_t k = 0; k < num_cell_per_element; ++k) {
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(element_table[((i + 0 + element_num - offset) % element_num) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 0) & 1)))); 
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(element_table[((i + 1 + element_num - offset) % element_num) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 1) & 1))));   
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(element_table[((i + 2 + element_num - offset) % element_num) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 2) & 1))));   
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(element_table[((i + 3 + element_num - offset) % element_num) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 3) & 1))));   
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(element_table[((i + 4 + element_num - offset) % element_num) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 4) & 1))));   
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(element_table[((i + 5 + element_num - offset) % element_num) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 5) & 1))));   
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(element_table[((i + 6 + element_num - offset) % element_num) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 6) & 1))));   
            result[k] = _mm256_xor_si256(result[k], _mm256_and_si256(element_table[((i + 7 + element_num - offset) % element_num) * num_cell_per_element + k], _mm256_set1_epi64x(-((tmp >> 7) & 1))));   
        }
    }

    return result;
}

void pir_write(tag_array_type& tag_table, std::vector<uint8_t>& indexing, tag_type& modyfied_tag, size_t tag_num, size_t tag_size) {
    size_t num_cell_per_tag = tag_size / sizeof(cell_type);
    size_t i;
    for (i = 0; i + 7 < tag_num; i += 8) {
        size_t tmp = indexing[i / 8];
        for (size_t k = 0; k < num_cell_per_tag; ++k) {
            tag_table[(i + 0) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 0) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 0) & 1))));       
            tag_table[(i + 1) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 1) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 1) & 1))));       
            tag_table[(i + 2) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 2) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 2) & 1))));       
            tag_table[(i + 3) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 3) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 3) & 1))));       
            tag_table[(i + 4) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 4) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 4) & 1))));       
            tag_table[(i + 5) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 5) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 5) & 1))));       
            tag_table[(i + 6) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 6) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 6) & 1))));       
            tag_table[(i + 7) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 7) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 7) & 1))));       
        }
    }    
    // Process the remaining tags
    if (i < tag_num) {
        uint64_t tmp = indexing[i / 8];
        for (size_t j = 0; j < tag_num - i; ++j) {
            for (size_t k = 0; k < num_cell_per_tag; ++k) {
                tag_table[(i + j) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + j) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> j) & 1))));       
            }
        }
    }
}

void pir_write2(tag_array_type& tag_table, std::vector<uint8_t>& indexing, tag_type& modyfied_tag, size_t tag_num, size_t tag_size) {
    size_t num_cell_per_tag = tag_size / sizeof(cell_type);
    size_t i;
    size_t array_length = 1 << static_cast<size_t>(std::ceil(log2(tag_num)));
    for (i = 0; i + 7 < tag_num; i += 8) {
        size_t tmp = indexing[(i + array_length) / 8];
        for (size_t k = 0; k < num_cell_per_tag; ++k) {
            tag_table[(i + 0) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 0) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 0) & 1))));       
            tag_table[(i + 1) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 1) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 1) & 1))));       
            tag_table[(i + 2) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 2) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 2) & 1))));       
            tag_table[(i + 3) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 3) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 3) & 1))));       
            tag_table[(i + 4) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 4) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 4) & 1))));       
            tag_table[(i + 5) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 5) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 5) & 1))));       
            tag_table[(i + 6) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 6) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 6) & 1))));       
            tag_table[(i + 7) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 7) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 7) & 1))));       
        }
    }    
    // Process the remaining tags
    if (i < tag_num) {
        size_t tmp = indexing[(i + array_length) / 8];
        for (size_t j = 0; j < tag_num - i; ++j) {
            for (size_t k = 0; k < num_cell_per_tag; ++k) {
                tag_table[(i + j) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + j) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> j) & 1))));       
            }
        }
    }
}

/**
 * offset % 8 == 0
 * tag_num % 8 == 0
 */
void pir_write_with_offset(tag_array_type& tag_table, std::vector<uint8_t>& indexing, tag_type& modyfied_tag, size_t tag_num, size_t tag_size, size_t offset) {
    size_t num_cell_per_tag = tag_size / sizeof(cell_type);
    size_t i;
    assert(offset >> 3 << 3 == offset);
    assert(tag_num >> 3 << 3 == tag_num);
    for (i = tag_num; i + 7 < (tag_num << 1); i += 8) {
        size_t tmp = indexing[(i + offset) / 8];
        for (size_t k = 0; k < num_cell_per_tag; ++k) { 
            tag_table[(i + 0 - tag_num) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 0 - tag_num) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 0) & 1))));       
            tag_table[(i + 1 - tag_num) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 1 - tag_num) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 1) & 1))));       
            tag_table[(i + 2 - tag_num) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 2 - tag_num) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 2) & 1))));       
            tag_table[(i + 3 - tag_num) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 3 - tag_num) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 3) & 1))));       
            tag_table[(i + 4 - tag_num) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 4 - tag_num) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 4) & 1))));       
            tag_table[(i + 5 - tag_num) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 5 - tag_num) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 5) & 1))));       
            tag_table[(i + 6 - tag_num) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 6 - tag_num) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 6) & 1))));       
            tag_table[(i + 7 - tag_num) * num_cell_per_tag + k] = _mm256_xor_si256(tag_table[(i + 7 - tag_num) * num_cell_per_tag + k], _mm256_and_si256(modyfied_tag[k], _mm256_set1_epi64x(-((tmp >> 7) & 1))));       
        }
        
    }    
}

uint8_t* read_data(element_array_type& data_array, size_t index, size_t data_size) {
    if (index >= data_array.size() / (data_size / sizeof(cell_type))) {
        throw std::out_of_range("Index out of range");
    }

    size_t blocks_per_full_block = data_size / sizeof(cell_type);
    size_t start_idx = index * blocks_per_full_block;
    
    uint8_t* element = new uint8_t[data_size];

    for (size_t i = 0; i < blocks_per_full_block; ++i) {
        std::memcpy(element + i * sizeof(cell_type), &data_array[start_idx + i], sizeof(cell_type));
    }

    return element;
}

element_type read_data_to_cell(element_array_type& data_array, size_t index, size_t data_size) {
    if (index >= data_array.size() / (data_size / sizeof(cell_type))) {
        throw std::out_of_range("Index out of range");
    }

    size_t blocks_per_full_block = data_size / sizeof(cell_type);
    size_t start_idx = index * blocks_per_full_block;
    
    element_type element(data_size / sizeof(cell_type));

    for (size_t i = 0; i < blocks_per_full_block; ++i) {
        std::memcpy(&element[i], &data_array[start_idx + i], sizeof(cell_type));
    }

    return element;
}


void write_data(element_array_type& data_array, size_t index, size_t data_size, element_type& data) {
    if (index >= data_array.size() / (data_size / sizeof(cell_type))) {
        throw std::out_of_range("Index out of range");
    }

    size_t blocks_per_full_block = data_size / sizeof(cell_type);
    size_t start_idx = index * blocks_per_full_block;

    for (size_t i = 0; i < blocks_per_full_block; ++i) {
        std::memcpy(&data_array[start_idx + i], &data[i], sizeof(cell_type));
    }
}

size_t getDPF_keySize(size_t array_length) {
    size_t arr_log = std::ceil(std::log2(array_length));
    return 33 + std::max<long>(0, arr_log - 7) * 18;
}

void clr(element_array_type& data_array) {
    std::memset(data_array.data(), 0, data_array.size() * sizeof(cell_type));
}

bool areTagsEqual(const tag_type& tag1, const tag_type& tag2) {
    for (size_t i = 0; i < tag1.size(); ++i) {
        if (memcmp(&tag1[i], &tag2[i], sizeof(cell_type)) != 0) {
            return false;
        }
    }
    return true;
}

std::vector<uint8_t> xor_half_vector(const std::vector<uint8_t>& indexing, size_t half_size) {
    assert(half_size >> 3 << 3 == half_size);
    std::vector<uint8_t> result(half_size);
    for (size_t i = 0; i < half_size; ++i) {
        result[i] = indexing[i] ^ indexing[i + half_size];
    }
    return result;
}

std::vector<uint8_t> xor_half_vector_avx(const std::vector<uint8_t>& indexing, size_t half_size) {
    std::vector<uint8_t> result(half_size);
    size_t i = 0;
    // Process 32 bytes at a time using AVX2 instructions
    for (; i + 31 < half_size; i += 32) {
        __m256i part1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&indexing[i]));
        __m256i part2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&indexing[i + half_size]));
        __m256i xor_result = _mm256_xor_si256(part1, part2);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(&result[i]), xor_result);
    }

    // Process remaining elements
    for (; i < half_size; ++i) {
        result[i] = indexing[i] ^ indexing[i + half_size];
    }

    return result;
}

size_t nextPowerOf2(size_t n) {
    if (n == 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return ++n;
}

size_t countOnes(size_t number) {
    size_t count = 0;
    while (number) {
        count += number & 1; // Add the last bit (0 or 1) to count
        number >>= 1;        // Shift the number right by 1 to check the next bit
    }
    return count;
}

// Function to change the first size_t value in element_type
void changeFirstSizeT(element_type& element, size_t new_value) {
    // Copy new_value into the first sizeof(size_t) bytes of the element
    std::memcpy(element.data(), &new_value, sizeof(size_t));
}