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


class Server_0 {
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
    size_t base_dummy_addr;
    size_t base_filler_addr; // Note filler == empty
    size_t base_excess_addr;
    
    Server_0(boost::asio::io_context& io_context, short port, size_t N, size_t element_size, size_t tag_size);
    void Setup();
    void Access();
    void Rebuild(size_t level, size_t bukcetID);
    void RebuildL();

private:
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::ip::tcp::socket socket_;
    std::array<char, 1024> recv_buffer_;
    
    // Storage
    element_array_type Top_Table;
    std::vector<std::vector<std::vector<element_array_type>>> Middle_Table0; // (Level - 1) + Bucket + Position + element_array
    std::vector<std::vector<std::vector<element_array_type>>> Middle_Table1; // (Level - 1) + Bucket + Position + element_array
    std::vector<element_array_type> Bottom_Table0;
    std::vector<element_array_type> Bottom_Table1;

    size_t get_tableLen(size_t level);
    void compAndSwap(std::vector<std::tuple<bool, size_t, size_t, size_t>>& A, size_t ii, size_t jj, size_t flag, size_t dire);
    void bitonicToOrder(std::vector<std::tuple<bool, size_t, size_t, size_t>>& A, size_t start, size_t end, size_t flag, size_t dire);
    void bitonicMerge(std::vector<std::tuple<bool, size_t, size_t, size_t>>& A, size_t start, size_t end, size_t flag, size_t dire);
    void oblivious_hash(size_t level, size_t bucketID, std::vector<std::tuple<bool, size_t, size_t, size_t>>& obliv_table0, std::vector<std::tuple<bool, size_t, size_t, size_t>>& obliv_table1);
    
};