#include <iostream>
#include "server_1.h" // Assuming you have a header file for Server_1

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <log_database_sizes length> <log_database_sizes values> <element_sizes length> <element_sizes values>\n";
        return 1;
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
            size_t tag_size = 32;     // Fixed tag size
            boost::asio::io_context io_context;
            Server_1 server(io_context, 5001, N, element_size, tag_size);
            size_t access_times = server.lg_N * (1 << (server.max_level - 2));
            server.Setup();
            for (size_t i = 0; i < access_times; i++) {
                server.Access();
            }
        }
    }

    return 0;
}