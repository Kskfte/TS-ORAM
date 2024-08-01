#include <x86intrin.h>
#include <cmath>
#include <../boost/asio.hpp>
#include "../../DPF-CPP/AES.h"
#include "../../DPF-CPP/dpf.h"
#include "../../TW-PIR/utils.h"
#include "../../TW-PIR/alignment_allocator.h"
#include "../../Hash-Function/utils.h"

typedef __m128i key_type;

class Client {
public:
    // Given the pararmeters
    size_t N;
    size_t element_size;
    size_t tag_size;
    size_t num_cell_per_element;
    size_t num_cell_per_tag;
    // Cuckoo information
    size_t lg_N;
    size_t cuckoo_alpha;
    double cuckoo_epsilon;
    // Other parameters
    size_t parametr_c;
    size_t max_level;
    size_t ell_cuckoo;
    size_t access_counter;
    size_t dummy_counter;
    size_t full_flag;
    size_t standard_BC;
    size_t maxL_epoch;
    size_t written_stash;
    size_t len_written_stash;

    // PRF keys
    key_type master_level_key;
    key_type master_tag_key;
    // AES initialization
    AES aes_level;
    AES aes_tag;

    Client(size_t N, size_t element_size, size_t tag_size);
    void Setup();
    element_type Access(char op, addr_type req_addr, uint8_t* write_value);
    void Rebuild(size_t level);
    void RebuildMaxLevel();

    // Cost
    size_t setup_bandwidth;
    size_t access_bandwidth;

private:
    // Server connections
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::socket server0_socket;
    boost::asio::ip::tcp::socket server1_socket;
    std::vector<boost::asio::ip::tcp::socket*> sockets_list;

    // Helper function
    void connect_to_server(boost::asio::ip::tcp::socket& socket, const std::string& server_ip, int server_port);
    size_t get_epoch(size_t level);
    key_type get_level_key(size_t level, size_t epoch, int h0Orh1);
    tag_type get_tag(size_t level, size_t epoch, addr_type vir_adrr);
    size_t get_level_ele_num(size_t level);

};