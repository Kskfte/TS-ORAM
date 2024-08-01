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

void test_pir_read(size_t log_element_num, size_t element_size, std::vector<std::vector<double>>& results) {
    size_t element_num = 1UL << log_element_num;
    size_t value_size = element_size - sizeof(size_t);

    // Initialize data_array with the form (addr, value)
    std::vector<std::pair<size_t, uint8_t*>> data_array;
    data_array.reserve(element_num);
    for (size_t i = 0; i < element_num; ++i) {
        size_t addr = i;
        uint8_t* value = new uint8_t[value_size];
        generate_random_chars(value, value_size);
        data_array.emplace_back(addr, value);
    }

    std::default_random_engine generator;
    std::uniform_int_distribution<size_t> distribution(0, element_num - 1);

    double total_buildT = 0;
    size_t total_transfer_bytes = 0;

    for (int trial = 0; trial < 10; ++trial) {  // run 10 trials for each configuration
        size_t read_index = distribution(generator);
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

        total_buildT += buildT.count();
        total_transfer_bytes += transfer_bytes;

        std::pair<size_t, uint8_t*> read_addr_value = convert_element_to_addrVal(rrres, element_size);

        assert(data_array[read_index].first == read_addr_value.first && convert_uint8_to_ascStr(data_array[read_index].second, element_size - sizeof(cell_type)) == convert_uint8_to_ascStr(read_addr_value.second, element_size - sizeof(cell_type)));
    }

    // Save average results in the 2D vector
    double avg_buildT = total_buildT / 10.0;
    double avg_transfer_bytes = total_transfer_bytes / 10.0;
    results.push_back({(double)log_element_num, (double)element_size, avg_buildT, avg_transfer_bytes});

    // Free the allocated memory for data_array values
    for (auto& item : data_array) {
        delete[] item.second;
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

    std::vector<size_t> log_element_nums = {10, 15, 20};
    std::vector<size_t> element_sizes = {32, 64, 256, 1024, 4096};
    std::vector<std::vector<double>> results;

    for (size_t log_element_num : log_element_nums) {
        for (size_t element_size : element_sizes) {
            std::cout << log_element_num << " " << element_size << "\n";
            test_pir_read(log_element_num, element_size, results);
        }
    }
  
    // Save results to a CSV file
    std::ofstream outfile("../Result/pir_read_results.csv");
    outfile << "log_element_num, element_size, buildT (s), transfer_bytes (B)\n";
    for (const auto& result : results) {
        outfile << result[0] << "," << result[1] << "," << result[2] << "," << result[3] << "\n";
    }
    outfile.close();

    // Read the CSV file
    std::string filename = "../Result/pir_read_results.csv"; // Replace with your CSV filename
    auto data = readCSV(filename);
    printCSV(data);
    return 0;
    return 0;
}
