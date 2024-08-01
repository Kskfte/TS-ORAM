#ifndef UTILS_H
#define UTILS_H

#include <utility>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <random>
#include <iostream>
#include <unordered_map>
#include <x86intrin.h>
#include "alignment_allocator.h"

const size_t MAX_SIZE_T = static_cast<size_t>(-1);

// Size of Server's storage cell 
typedef __m256i cell_type;
// 8-byte addr
typedef size_t addr_type;

typedef std::vector<cell_type, AlignmentAllocator<cell_type, sizeof(cell_type)> > element_type;
typedef std::vector<cell_type, AlignmentAllocator<cell_type, sizeof(cell_type)> > element_array_type;
typedef std::vector<cell_type, AlignmentAllocator<cell_type, sizeof(cell_type)> > tag_type;
typedef std::vector<cell_type, AlignmentAllocator<cell_type, sizeof(cell_type)> > tag_array_type;

size_t get_random_index(size_t length);
uint8_t* get_random_uint8t(size_t uint_size);

// Helper function for element area
element_type get_random_element(size_t vir_addr, size_t element_size);
uint8_t* convert_addrVal_to_bytes(std::pair<size_t, uint8_t*> addr_value, size_t element_size);
std::pair<size_t,uint8_t*> convert_bytes_to_addrVal(uint8_t* bytes, size_t element_size);
element_type convert_share_to_element(const element_type& element1, const element_type& element2);
element_type convert_addrVal_to_element(const std::pair<size_t, uint8_t*>& addr_value, size_t element_size);
std::pair<size_t, uint8_t*> convert_element_to_addrVal(const element_type& element, size_t element_size);
element_type convert_bytes_to_element(const uint8_t* bytes, size_t element_size);
void print_element(const element_type& element, size_t element_size);

// Helper function for tag area
tag_type get_random_tag(size_t tag_size);
tag_type convert_share_to_tag(const tag_type& tag1, const tag_type& tag2);
std::pair<tag_type, tag_type> convert_tag_to_share(const tag_type& tag);
tag_type convert_bytes_to_tag(const uint8_t* bytes, size_t tag_size);
uint8_t* convert_tag_to_bytes(const tag_type& tag);
void print_tag(const tag_type& tag, size_t tag_size);

// Helper function for transformation and printing
std::string convert_uint8_to_ascStr(const uint8_t* data, size_t size);

// For ORAM:: full_flag
void set_bit(size_t& full_flag, size_t index, bool bit);
bool is_index_bit_1(size_t full_flag, size_t i);

element_type pir_read(element_array_type& element_table, std::vector<uint8_t>& indexing, size_t element_num, size_t element_size);
element_type pir_read_with_offset(element_array_type& element_table, std::vector<uint8_t>& indexing, size_t element_num, size_t element_size, size_t offset);
void pir_write(tag_array_type& tag_table, std::vector<uint8_t>& indexing, tag_type& modyfied_tag, size_t tag_num, size_t tag_size);
void pir_write2(tag_array_type& tag_table, std::vector<uint8_t>& indexing, tag_type& modyfied_tag, size_t tag_num, size_t tag_size);
void pir_write_with_offset(tag_array_type& tag_table, std::vector<uint8_t>& indexing, tag_type& modyfied_tag, size_t tag_num, size_t tag_size, size_t offset);

uint8_t* read_data(element_array_type& data_array, size_t index, size_t data_size);
element_type read_data_to_cell(element_array_type& data_array, size_t index, size_t data_size);
void write_data(element_array_type& data_array, size_t index, size_t data_size, element_type& data);
size_t getDPF_keySize(size_t array_length);

void clr(element_array_type& data_array);
bool areTagsEqual(const tag_type& tag1, const tag_type& tag2);

std::vector<uint8_t> xor_half_vector(const std::vector<uint8_t>& indexing, size_t half_size);
std::vector<uint8_t> xor_half_vector_avx(const std::vector<uint8_t>& indexing, size_t half_size);

size_t nextPowerOf2(size_t n);
size_t countOnes(size_t number);

void changeFirstSizeT(element_type& element, size_t new_value);

#endif // UTILS_H
