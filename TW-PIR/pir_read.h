#pragma once

#include <vector>
#include <cstdint>
#include <x86intrin.h>
#include <cstring>    // For memcpy
#include <stdexcept>  // For std::out_of_range
#include <memory>     // For unique_ptr
#include <iostream>
#include <sstream>
#include <utility>
#include "alignment_allocator.h"
#include "../DPF-CPP/dpf.h"
#include "utils.h"

class PIR_Read {
public:
    size_t block_num;
    size_t block_size;
    PIR_Read(size_t block_num, size_t block_size);
    ~PIR_Read(); // Destructor to free allocated memory

    size_t size() const;
    void push_back(const uint8_t* data);
    uint8_t* read_element(size_t index);
    element_type pir_read(const std::vector<uint8_t>& indexing) const;

    element_type data_array;
};