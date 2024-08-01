#include <iostream>
#include "server_1.h" // Include "server_1.h" for server 1

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cout << "Usage: ./ours_server1_opt <log_database_size> <access_times> <element_size> <tag_size>" << std::endl;
        return -1;
    }
    try {
        size_t log_database_size = std::strtoull(argv[1], nullptr, 10);
        size_t N = 1UL << log_database_size;
        size_t access_times = std::strtoull(argv[2], nullptr, 10);
        size_t element_size = std::strtoull(argv[3], nullptr, 10);
        size_t tag_size = std::strtoull(argv[4], nullptr, 10);

        boost::asio::io_context io_context;
        Server_1 server(io_context, 6000, N, element_size, tag_size); // Port 4000 for server 0, Port 6000 for server 1        
        server.Setup();  

        for (size_t i = 0; i < access_times; i++) {
            server.Access();
        } 
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
