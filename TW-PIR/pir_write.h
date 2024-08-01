#pragma once


#include <vector>
#include <cstdint>
#include <x86intrin.h>
#include <cstring>    // For memcpy
#include <stdexcept>  // For std::out_of_range
#include <memory>     // For unique_ptr
#include <iostream>
#include <sstream>
#include "alignment_allocator.h"
#include "../DPF-CPP/dpf.h"
#include "utils.h"

class PIR_WRITE {
public:
    size_t tag_num;
    size_t tag_size;

    PIR_WRITE(size_t tag_num, size_t tag_size);
    ~PIR_WRITE(); // Destructor to free allocated memory

    size_t size() const;
    void push_back(const tag_type tag);
    tag_type read_tag(size_t index); 
    void pir_write(const std::vector<uint8_t>& indexing, const tag_type& tag);

    tag_type tag_cell_array;
};