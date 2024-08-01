#include "../DPF-CPP/dpf.h"
#include "pir_read.h"
#include "pir_write.h"
#include "utils.h"

#include <chrono>
#include <iostream>
#include <cassert>
#include <random>

void generate_random_chars(uint8_t* data, size_t size) {
    std::random_device rd; 
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 61); 
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    for (size_t i = 0; i < size; ++i) {
        data[i] = charset[dis(gen)];
    }
}


int main(int argc, char** argv) {
    if (argc != 4) {
        std::cout << "Usage: ./pir 0_Or_1 <log_element_num> <bytes_block_size>" << std::endl;
        return -1;
    }

    // Parameters
    size_t program_name = std::strtoull(argv[1], nullptr, 10);
    size_t log_element_num = std::strtoull(argv[2], nullptr, 10);
    size_t element_num = 1UL << log_element_num;
    size_t element_size = std::strtoull(argv[3], nullptr, 10);
    size_t value_size = element_size - sizeof(size_t);

    size_t tag_size = element_size;
    size_t tag_num = element_num;
    size_t log_tag_num = log_element_num;

    if (program_name == 0) {
        // Initialize data_array with the form (addr, value)
        std::vector<std::pair<size_t, uint8_t*>> data_array;
        data_array.reserve(element_num);
        for (size_t i = 0; i < element_num; ++i) {
            size_t addr = i;
            uint8_t* value = new uint8_t[value_size];
            generate_random_chars(value, value_size);
            data_array.emplace_back(addr, value);
        }
        
        size_t read_index = element_num - 1;
        PIR_Read read_pir(element_num, element_size);
        for (size_t i = 0; i < element_num; ++i) {
            read_pir.push_back(convert_addrVal_to_bytes(data_array[i], element_size));
        }

        std::chrono::duration<double> buildT;
        buildT = std::chrono::duration<double>::zero();
        auto time2 = std::chrono::high_resolution_clock::now();

        auto keys = DPF::Gen(read_index, log_element_num);
        auto a = keys.first;
        auto b = keys.second;
        std::vector<uint8_t> aaaa;
        std::vector<uint8_t> bbbb;
        if(log_element_num > 10) {
            aaaa = DPF::EvalFull8(a, log_element_num);
            bbbb = DPF::EvalFull8(b, log_element_num);
        } else {
            aaaa = DPF::EvalFull(a, log_element_num);
            bbbb = DPF::EvalFull(b, log_element_num);
        }
        element_type answerA = read_pir.pir_read(aaaa);
        element_type answerB = read_pir.pir_read(bbbb);
        element_type rrres = convert_share_to_element(answerA, answerB);

        auto time3 = std::chrono::high_resolution_clock::now();
        buildT += time3 - time2;
        size_t transfer_bytes = a.size() + b.size() + element_size + element_size;
        std::cout << buildT.count() << " sec" << std::endl;
        std::cout << transfer_bytes << " bytes" << std::endl;
        //print_element(rrres, element_size);

        std::pair<size_t, uint8_t*> read_addr_value = convert_element_to_addrVal(rrres, element_size);

        assert(data_array[read_index].first == read_addr_value.first && convert_uint8_to_ascStr(data_array[read_index].second, element_size - sizeof(cell_type)) == convert_uint8_to_ascStr(read_addr_value.second, element_size - sizeof(cell_type)));
        
        // Free the allocated memory for data_array values
        for (auto& item : data_array) {
            delete[] item.second;
        }
    }
    else {
        std::vector<uint8_t*> tag_array;
        tag_array.reserve(tag_num);
        for (size_t i = 0; i < tag_num; ++i) {
            uint8_t* tag = new uint8_t[tag_size];
            generate_random_chars(tag, tag_size);
            tag_array.emplace_back(tag);
        }

        PIR_WRITE write_pir1(tag_num, tag_size);
        PIR_WRITE write_pir2(tag_num, tag_size);
        std::pair<tag_type, tag_type> tag_share;
        for (size_t i = 0; i < tag_num; ++i) {
            tag_share = convert_tag_to_share(convert_bytes_to_tag(tag_array[i], tag_size));
            write_pir1.push_back(tag_share.first);
            write_pir2.push_back(tag_share.second);
        }

        size_t write_index = tag_num-1;
        uint8_t* write_tag = new uint8_t[tag_size];
        generate_random_chars(write_tag, tag_size);

        tag_type modified_tag = convert_share_to_tag(convert_bytes_to_tag(write_tag, tag_size), convert_bytes_to_tag(tag_array[write_index], tag_size));
        
        std::chrono::duration<double> buildT;
        buildT = std::chrono::duration<double>::zero();
        auto time2 = std::chrono::high_resolution_clock::now();

        auto keys = DPF::Gen(write_index, log_tag_num);
        auto a = keys.first;
        auto b = keys.second;
        std::vector<uint8_t> aaaa;
        std::vector<uint8_t> bbbb;
        if(log_tag_num > 10) {
            aaaa = DPF::EvalFull8(a, log_tag_num);
            bbbb = DPF::EvalFull8(b, log_tag_num);
        } else {
            aaaa = DPF::EvalFull(a, log_tag_num);
            bbbb = DPF::EvalFull(b, log_tag_num);
        }

        write_pir1.pir_write(aaaa, modified_tag);
        write_pir2.pir_write(bbbb, modified_tag);

        tag_type bbbb_value1 = write_pir1.read_tag(write_index);
        tag_type bbbb_value2 = write_pir2.read_tag(write_index);

        tag_type bnew_tag213 = convert_share_to_tag(bbbb_value1, bbbb_value2);

        //print_tag(bnew_tag213, tag_size);
        
        auto time3 = std::chrono::high_resolution_clock::now();
        buildT += time3 - time2;
        size_t transfer_bytes = a.size() + b.size() + tag_size + tag_size;
        std::cout << buildT.count() << " sec" << std::endl;
        std::cout << transfer_bytes << " bytes" << std::endl;


        assert(convert_uint8_to_ascStr(write_tag, tag_size) == convert_uint8_to_ascStr(convert_tag_to_bytes(bnew_tag213), tag_size));

        // Free the allocated memory for data_array values
        for (auto& item : tag_array) {
            delete[] item;
        }
    }

    return 0;
}
