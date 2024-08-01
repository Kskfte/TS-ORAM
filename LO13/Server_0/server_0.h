#include <x86intrin.h>
#include <cmath>
#include <vector>
#include <iostream>
#include <iomanip>
#include <unordered_map>
#include <../boost/asio.hpp>
#include "../../DPF-CPP/AES.h"
#include "../../DPF-CPP/dpf.h"
#include "../../TW-PIR/utils.h"
#include "../../TW-PIR/alignment_allocator.h"
#include "../../Hash-Function/utils.h"
#include "../Utils/utils.h"

class Server_0 {
public:
    // Given the pararmeters
    size_t N;
    size_t element_size;
    size_t tag_size;
    size_t num_cell_per_element;
    size_t num_cell_per_tag;
    size_t lg_N;
    // Cuckoo information
    size_t cuckoo_alpha;
    double cuckoo_epsilon;
    // Other parameters
    size_t parametr_c;
    size_t max_level;
    size_t ell_cuckoo;
    size_t access_counter;
    size_t full_flag;
    size_t standard_BC; 
    size_t baseCuckoo_tableLen;
    size_t written_stash;
    size_t len_written_stash;
    
    Server_0(boost::asio::io_context& io_context, short port, size_t N, size_t element_size, size_t tag_size);
    void Setup();
    void Access();
    void Rebuild(size_t level);
    void RebuildMaxLevel();

private:
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::ip::tcp::socket socket_;
    std::array<char, 1024> recv_buffer_;
    
    // Storage
    element_array_type Stash;
    std::vector<std::vector<element_array_type>> Standard_Table; // Level + position + element_array
    std::vector<std::vector<element_array_type>> Cuckoo_Table; // Level + table0_Or_1 + element_array
    
    bool try_insert_standard(std::vector<element_array_type>& element_array, std::unordered_map<addr_type, size_t>& pos_dict, element_type& ele, size_t write_pos);
    bool try_insert_cuckoo(std::vector<element_array_type>& element_array, std::vector<std::unordered_map<addr_type, size_t>>& pos_dict_array, element_type& ele, size_t write_pos, size_t kicked_pos, size_t h0Orh1, size_t depth);
    void write_element_to_socket(element_array_type& data_array, size_t count); // Send element to client
    void send_standard_table(size_t level, size_t rec_ele_num); // Construct the table and send them to client
    void send_cuckoo_table(size_t level, size_t rec_ele_num); // Construct the table and send them to client
    void receive_standard_table(size_t level, std::vector<element_array_type>& element_array); // Receive the constructed table
    void receive_cuckoo_table(size_t level, std::vector<element_array_type>& element_array); // Receive the constructed table
    void receive_shuffle_resend_standard_table(size_t level);
    void receive_shuffle_resend_cuckoo_table(size_t level);
};