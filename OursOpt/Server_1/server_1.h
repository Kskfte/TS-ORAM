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

class Server_1 {
public:
    // Input
    size_t N;
    size_t element_size;
    size_t tag_size;
    // cell per_block and per_tag
    size_t num_cell_per_element;
    size_t num_cell_per_tag;
    // Cuckoo information
    size_t lg_N;
    size_t cuckoo_alpha;
    double cuckoo_epsilon;
    // Others
    size_t ell;
    size_t L;
    size_t full_flag;
    size_t len_S;
    size_t len_B;
    size_t access_counter;
    // Table length
    size_t realEll_tableLen;
    size_t baseEll_tableLen;

    Server_1(boost::asio::io_context& io_context, short port, size_t N, size_t element_size, size_t tag_size);
    void Setup();
    void Access();
    void RebuildEll();
    void RebuildLevel(size_t level);
    void RebuildL();

private:
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::ip::tcp::socket socket_;
    std::array<char, 1024> recv_buffer_;
    
    std::vector<std::vector<element_array_type>> element_table;
    element_array_type element_stash;
    element_array_type element_buffer;
    std::vector<std::vector<tag_array_type>> tag_table;
    tag_array_type tag_stash;
    tag_array_type tag_buffer;

    // Position dictionary
    std::vector<std::vector<std::unordered_map<size_t, size_t>>> pos_dict;

    bool try_insert(std::vector<element_array_type>& element_array, std::vector<tag_array_type>& tag_array, std::vector<std::unordered_map<addr_type, size_t>>& pos_dict_array, size_t layer, element_type& ele, tag_type& tag, size_t write_pos, size_t kicked_pos, size_t h0Orh1, size_t depth);
    void insert_data(size_t level);
};