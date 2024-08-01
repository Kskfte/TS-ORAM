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
    // Cuckoo information
    size_t lg_N;
    size_t cuckoo_alpha;
    double cuckoo_epsilon;
    // Compute this information
    size_t ell;
    size_t L;
    size_t full_flag;
    size_t len_S;
    size_t len_B;
    size_t access_counter;
    key_type master_level_key;
    key_type master_tag_key;
    // AES initialization
    AES aes_level;
    AES aes_tag;
    // Level epoch information
    size_t ell_epoch;
    size_t L_epoch;
    // Table length
    size_t realEll_tableLen;
    size_t baseEll_tableLen;

    Client(size_t N, size_t element_size, size_t tag_size);
    void Setup();
    element_type Access(char op, addr_type acc_addr, uint8_t* write_value);
    void RebuildEll();
    void RebuildLevel(size_t level);
    void RebuildL();

    // Cost
    size_t setup_bandwidth;
    size_t access_bandwidth;

private:
    // Server connections
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::socket server0_socket;
    boost::asio::ip::tcp::socket server1_socket;

    // Helper function
    void connect_to_server(boost::asio::ip::tcp::socket& socket, const std::string& server_ip, int server_port);
    size_t get_epoch(size_t level);
    key_type get_level_key(size_t level, size_t epoch, int h0Orh1);
    tag_type get_tag(addr_type vir_adrr);
    void insert_data(element_type& ele, size_t level);

};