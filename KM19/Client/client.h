#include <x86intrin.h>
#include <cmath>
#include <../boost/asio.hpp>
#include "../../DPF-CPP/AES.h"
#include "../../DPF-CPP/dpf.h"
#include "../../TW-PIR/utils.h"
#include "../../TW-PIR/alignment_allocator.h"
#include "../../Hash-Function/utils.h"

class Client {
public:
    // Given the pararmeters
    size_t N;
    size_t element_size;
    size_t tag_size;
    size_t num_cell_per_element;
    size_t num_cell_per_tag;
    size_t lg_N;
    size_t L;
    // Two-tier hash information
    size_t tth_b;
    size_t eleNum_oneBins;
    size_t topLevel_size;
    // Others
    std::vector<size_t> full_flag;
    size_t access_counter;
    size_t maxL_epoch;
    size_t base_dummy_addr;
    size_t base_filler_addr; // Note filler == empty
    size_t base_excess_addr;

    // PRF keys
    key_type master_level_key;
    key_type master_tag_key;
    // AES initialization
    AES aes_level;
    AES aes_tag;
    
    // Cost
    size_t setup_bandwidth;
    size_t access_bandwidth;

    Client(size_t N, size_t element_size, size_t tag_size);
    void Setup();
    element_type Access(char op, addr_type req_addr, uint8_t* write_value);
    void Rebuild(size_t level, size_t bukcetID);
    void RebuildL();

private:
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::socket server0_socket;
    boost::asio::ip::tcp::socket server1_socket;

    void connect_to_server(boost::asio::ip::tcp::socket& socket, const std::string& server_ip, int server_port);
    size_t get_epoch(size_t level, size_t bucketID);
    key_type get_level_key(size_t level, size_t bucketID, size_t epoch, int h0Orh1);
    tag_type get_tag(size_t level, size_t bucketID, size_t epoch, addr_type vir_addr);  
    size_t get_tableLen(size_t level);

    void compAndSwap(size_t array_len, size_t ii, size_t jj, size_t flag, size_t dire);
    void bitonicToOrder(size_t array_len, size_t start, size_t end, size_t flag, size_t dire);
    void bitonicMerge(size_t array_len, size_t start, size_t end, size_t flag, size_t dire);
    void oblivious_hash(size_t level, size_t bucketID);
};