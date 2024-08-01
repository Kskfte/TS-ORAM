#include "../DPF-CPP/dpf.h"
#include "pir_read.h"
#include "pir_write.h"
#include "utils.h"

#include <chrono>
#include <iostream>
#include <cassert>
#include <random>
#include <fstream>
#include <vector>
#include <ctime>

void generate_random_chars(uint8_t* data, size_t size) {
    std::random_device rd; 
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 61); 
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    for (size_t i = 0; i < size; ++i) {
        data[i] = charset[dis(gen)];
    }
}

void test_pir_write(size_t log_tag_num, size_t tag_size, std::vector<std::vector<double>>& results) {
    size_t tag_num = 1UL << log_tag_num;

    // Initialize tag_array with random values
    std::vector<uint8_t*> tag_array;
    tag_array.reserve(tag_num);
    for (size_t i = 0; i < tag_num; ++i) {
        uint8_t* tag = new uint8_t[tag_size];
        generate_random_chars(tag, tag_size);
        tag_array.emplace_back(tag);
    }

    std::default_random_engine generator;
    std::uniform_int_distribution<size_t> distribution(0, tag_num - 1);

    double total_buildT = 0;
    size_t total_transfer_bytes = 0;

    for (int trial = 0; trial < 10; ++trial) {  // run 10 trials for each configuration
        size_t write_index = distribution(generator);
        uint8_t* write_tag = new uint8_t[tag_size];
        generate_random_chars(write_tag, tag_size);

        PIR_WRITE write_pir1(tag_num, tag_size);
        PIR_WRITE write_pir2(tag_num, tag_size);

        std::pair<tag_type, tag_type> tag_share;
        for (size_t i = 0; i < tag_num; ++i) {
            tag_share = convert_tag_to_share(convert_bytes_to_tag(tag_array[i], tag_size));
            write_pir1.push_back(tag_share.first);
            write_pir2.push_back(tag_share.second);
        }

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

        auto time3 = std::chrono::high_resolution_clock::now();
        buildT += time3 - time2;
        size_t transfer_bytes = a.size() + b.size() + tag_size + tag_size;

        total_buildT += buildT.count();
        total_transfer_bytes += transfer_bytes;
        
        tag_type rtag1 = write_pir1.read_tag(write_index);
        tag_type rtag2 = write_pir2.read_tag(write_index);
        tag_type rtag = convert_share_to_tag(rtag1, rtag2);
        assert(convert_uint8_to_ascStr(write_tag, tag_size) == convert_uint8_to_ascStr(convert_tag_to_bytes(rtag), tag_size));

        delete[] write_tag;
    }

    // Save average results in the 2D vector
    double avg_buildT = total_buildT / 10.0;
    double avg_transfer_bytes = total_transfer_bytes / 10.0;
    results.push_back({(double)log_tag_num, (double)tag_size, avg_buildT, avg_transfer_bytes});

    // Free the allocated memory for tag_array values
    for (auto& item : tag_array) {
        delete[] item;
    }
}

// Function to read CSV file
std::vector<std::vector<std::string>> readCSV(const std::string& filename) {
    std::vector<std::vector<std::string>> data;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Could not open the file!" << std::endl;
        return data;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::vector<std::string> row;
        std::stringstream ss(line);
        std::string value;
        
        while (std::getline(ss, value, ',')) {
            row.push_back(value);
        }
        
        data.push_back(row);
    }
    
    file.close();
    return data;
}

// Function to print CSV data
void printCSV(const std::vector<std::vector<std::string>>& data) {
    for (const auto& row : data) {
        for (const auto& value : row) {
            std::cout << value << " ";
        }
        std::cout << std::endl;
    }
}

int main(int argc, char** argv) {
    std::vector<size_t> log_tag_nums = {10, 15, 20};
    std::vector<size_t> tag_sizes = {32, 64, 256, 1024, 4096};
    std::vector<std::vector<double>> results;

    for (size_t log_tag_num : log_tag_nums) {
        for (size_t tag_size : tag_sizes) {
            std::cout << log_tag_num << " " << tag_size << "\n";
            test_pir_write(log_tag_num, tag_size, results);
        }
    }
  
    // Save results to a CSV file
    std::ofstream outfile("../Result/pir_write_results.csv");
    outfile << "log_tag_num, tag_size, buildT (s), transfer_bytes (B)\n";
    for (const auto& result : results) {
        outfile << result[0] << "," << result[1] << "," << result[2] << "," << result[3] << "\n";
    }
    outfile.close();

    // Read the CSV file
    std::string filename = "../Result/pir_write_results.csv"; // Replace with your CSV filename
    auto data = readCSV(filename);
    printCSV(data);

    return 0;
}
