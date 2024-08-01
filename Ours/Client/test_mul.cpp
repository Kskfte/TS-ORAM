#include <iostream>
#include <chrono>
#include <fstream>
#include <thread>
#include "client.h" // Assuming you have a header file for Client

void printProgress(int progress) {
    constexpr int barWidth = 70;
    std::cout << "\r[";
    int pos = (barWidth * progress) / 100;
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << progress << " %";
    std::cout.flush();
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <log_database_sizes length> <log_database_sizes values> <element_sizes length> <element_sizes values>\n";
        return -1;
    }

    int log_database_sizes_length = std::atoi(argv[1]);
    std::vector<size_t> log_database_sizes;
    for (int i = 0; i < log_database_sizes_length; ++i) {
        log_database_sizes.push_back(std::atoi(argv[2 + i]));
    }

    int element_sizes_length = std::atoi(argv[2 + log_database_sizes_length]);
    std::vector<size_t> element_sizes;
    for (int i = 0; i < element_sizes_length; ++i) {
        element_sizes.push_back(std::atoi(argv[3 + log_database_sizes_length + i]));
    }

    std::vector<std::vector<double>> results;

    for (size_t log_database_size : log_database_sizes) {
        for (size_t element_size : element_sizes) {
            size_t N = 1UL << log_database_size;
            size_t access_times = N;  // Fixed as 2^log_database_size
            size_t tag_size = 32;     // Fixed tag size

            std::chrono::duration<double> setupT;
            std::chrono::duration<double> accessT;
            setupT = std::chrono::duration<double>::zero();
            accessT = std::chrono::duration<double>::zero();
            auto time1 = std::chrono::high_resolution_clock::now();
            Client client(N, element_size, tag_size);
            client.Setup();
            auto time2 = std::chrono::high_resolution_clock::now();
            // Access loop with progress printing
            for (size_t i = 0; i < access_times; ++i) {
                // Access operation
                element_type ele = client.Access('r', get_random_index(N), get_random_uint8t(element_size - sizeof(addr_type)));
        
                // Progress printing
                if ((i + 1) % (access_times / 10) == 0 || i == access_times - 1) {
                    int progress = static_cast<int>((static_cast<float>(i + 1) / access_times) * 100);
                    printProgress(progress);
                }
            }
            auto time3 = std::chrono::high_resolution_clock::now();
            setupT += time2 - time1;
            accessT += time3 - time2;
            std::cout << std::endl;
            results.push_back({(double)log_database_size, (double)element_size, (double)setupT.count(), (double)client.setup_bandwidth, (double)accessT.count() / access_times, (double) client.access_bandwidth / access_times});
            
            // Save results to a CSV file
            std::cout << "outFIle" << "\n";
            std::ofstream outfile("../../Result/ours_results.csv");
            outfile << "log_database_size, element_size, setup_time (s), setup_bandwidth (B), amortized_access_time (s), amortized_access_bandwidth (B)\n";
            for (const auto& result : results) {
                outfile << result[0] << "," << result[1] << "," << result[2] << "," << result[3] << "," << result[4] << "," << result[5] << "\n";
            }
            outfile.close();
            
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        }
    }

    return 0;
}
