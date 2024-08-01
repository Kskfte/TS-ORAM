#include <iostream>
#include <chrono>
#include "client.h"

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

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <log_database_size> <access_times> <element_size> <tag_size>" << std::endl;
        return -1;
    }

    size_t log_database_size = std::strtoull(argv[1], nullptr, 10);
    size_t N = 1UL << log_database_size;
    size_t access_times = std::strtoull(argv[2], nullptr, 10);
    size_t element_size = std::strtoull(argv[3], nullptr, 10);
    size_t tag_size = std::strtoull(argv[4], nullptr, 10);

    size_t progress_interval = access_times / 10;

    std::chrono::duration<double> setupT;
    std::chrono::duration<double> accessT;
    setupT = std::chrono::duration<double>::zero();
    accessT = std::chrono::duration<double>::zero();
    auto time1 = std::chrono::high_resolution_clock::now();
    Client client(N, element_size, tag_size);
    client.Setup();
    auto time2 = std::chrono::high_resolution_clock::now();
    setupT += time2 - time1;

    for (size_t i = 0; i < access_times; i++) {
        element_type ele = client.Access('r', get_random_index(N), get_random_uint8t(element_size - sizeof(addr_type)));
        
        if (i % progress_interval == 0 || i == access_times - 1) {
            int progress = static_cast<int>((static_cast<float>(i + 1) / access_times) * 100);
            printProgress(progress);
        }
    }
    std::cout << std::endl;

    auto time3 = std::chrono::high_resolution_clock::now();
    accessT += time3 - time2;
    std::cout << "setup_time:" << setupT.count() << " sec" << std::endl;
    std::cout << "setup_bandwidth:" << client.setup_bandwidth << std::endl;
    std::cout << "amortized_access_time:" << accessT.count() / access_times << " sec" << std::endl;
    std::cout << "amortized_access_bandwidth:" << client.access_bandwidth / access_times << std::endl;
    return 0;
}
