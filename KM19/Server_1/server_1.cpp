#include "server_1.h"

Server_1::Server_1(boost::asio::io_context& io_context, short port, size_t N, size_t element_size, size_t tag_size): 
    N(N), element_size(element_size), tag_size(tag_size), 
    num_cell_per_element(std::ceil(element_size / sizeof(cell_type))),
    num_cell_per_tag(std::ceil(tag_size / sizeof(cell_type))),
    lg_N(std::ceil(log2(N))), L(std::ceil(lg_N/log2(lg_N))),
    tth_b(std::ceil(pow(lg_N, 0.6))), eleNum_oneBins(5 * tth_b), topLevel_size(0), access_counter(0), 
    base_dummy_addr(1UL << 60), base_filler_addr(1UL << 61), base_excess_addr(1UL << 62), 
    acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
    socket_(io_context)
{
    full_flag.resize(L);
    /**
     * Initialize the table
     */
    size_t num_bins;
    std::cout << "L: " << L << "\n";
    std::cout << "lg_N: " << lg_N << "\n";
    std::cout << "tth_b: " << tth_b << "\n";
    std::cout << "eleNum_oneBins: " << eleNum_oneBins << "\n";
    Top_Table.resize(lg_N * num_cell_per_element);
    Middle_Table0.resize(L - 1);
    Middle_Table1.resize(L - 1);
    for (size_t i = 0; i < Middle_Table0.size(); i++) {
        Middle_Table0[i].resize(lg_N - 1);
        Middle_Table1[i].resize(lg_N - 1);
        for (size_t j = 0; j < lg_N - 1; j++) {
            num_bins = std::ceil(pow(lg_N, i + 1) / tth_b);
            Middle_Table0[i][j].resize(num_bins);
            Middle_Table1[i][j].resize(num_bins);
            for (size_t k = 0; k < num_bins; k++) {
                Middle_Table0[i][j][k].resize(eleNum_oneBins*num_cell_per_element);
                Middle_Table1[i][j][k].resize(eleNum_oneBins*num_cell_per_element);
            }
        }
    }
    num_bins = std::ceil(N / tth_b);
    Bottom_Table0.resize(num_bins);
    Bottom_Table1.resize(num_bins);
    for (size_t k = 0; k < num_bins; k++) {
        Bottom_Table0[k].resize(eleNum_oneBins*num_cell_per_element);
        Bottom_Table1[k].resize(eleNum_oneBins*num_cell_per_element);
    }
    
    acceptor_.accept(socket_);
}

size_t Server_1::get_tableLen(size_t level) {
    if (level == L) {
        return std::ceil(N / tth_b);
    }
    else {
        return std::ceil(pow(lg_N, level) / tth_b);
    }
}

void Server_1::oblivious_hash(size_t level, size_t bucketID, std::vector<element_type>& obliv_table) {
    /**
     * First, reshuffle and send the table
     */
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(obliv_table.begin(), obliv_table.end(), g);
    for (size_t i = 0; i < obliv_table.size(); i++) {
        boost::asio::write(socket_, boost::asio::buffer(obliv_table[i].data(), element_size));
    }
    // Finally, receive the table to S0 by client
    element_type ele(num_cell_per_element);
    if (level < L) {
        for (size_t i = 0; i < get_tableLen(level); i++) {
            for (size_t j = 0; j < eleNum_oneBins; j++) {
                boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
                write_data(Middle_Table0[level - 1][bucketID][i], j, element_size, ele);
            }
        }
        for (size_t i = 0; i < get_tableLen(level); i++) {
            for (size_t j = 0; j < eleNum_oneBins; j++) {
                boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
                write_data(Middle_Table1[level - 1][bucketID][i], j, element_size, ele);
            }
        }
    }
    else {
        for (size_t i = 0; i < get_tableLen(level); i++) {
            for (size_t j = 0; j < eleNum_oneBins; j++) {
                boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
                write_data(Bottom_Table0[i], j, element_size, ele);
            }
        }
        for (size_t i = 0; i < get_tableLen(level); i++) {
            for (size_t j = 0; j < eleNum_oneBins; j++) {
                boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
                write_data(Bottom_Table1[i], j, element_size, ele);
            }
        }
    }
}

void Server_1::Setup() {
    element_type ele(num_cell_per_element);
    size_t num_bins = get_tableLen(L);
    std::vector<element_type> obliv_table((num_bins * eleNum_oneBins) << 1, element_type(num_cell_per_element));
    for (size_t i = 0; i < N; i++) {
        boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
        obliv_table[i] = ele;
    }
    for (size_t i = N; i < ((num_bins * eleNum_oneBins) << 1); i++) {
        ele = get_random_element(base_filler_addr + i - N, element_size);
        obliv_table[i] = ele;
    }

    /**
     * Second, oblivious hash
     */
    oblivious_hash(L, 0, obliv_table);
    full_flag[L - 1] = 1;
}


void Server_1::Access() {
    // Parameters
    element_type ele(num_cell_per_element);
    size_t level_pos_0, level_pos_1;
    size_t level_ele_counter;
    size_t tmp_addr;
    size_t valid_bucket_num;
    // Middle table
    for (size_t i = 1; i < L; i++) {
        level_ele_counter = 0;
        valid_bucket_num = countOnes(full_flag[i - 1]);
        element_array_type tmp_ele_array((valid_bucket_num * eleNum_oneBins * num_cell_per_element) << 1);
        for (size_t j = 0; j < lg_N - 1; j++) {
            if (is_index_bit_1(full_flag[i - 1], j)) {
                boost::asio::read(socket_, boost::asio::buffer(&level_pos_0, sizeof(size_t)));
                for (size_t k = 0; k < eleNum_oneBins; k++) {
                    ele = read_data_to_cell(Middle_Table0[i - 1][j][level_pos_0], k, element_size);
                    tmp_addr = *reinterpret_cast<addr_type*>(ele.data());
                    write_data(tmp_ele_array, level_ele_counter, element_size, ele);
                    boost::asio::read(socket_, boost::asio::buffer(&tmp_addr, sizeof(size_t)));
                    changeFirstSizeT(ele, tmp_addr);
                    write_data(Middle_Table0[i - 1][j][level_pos_0], k, element_size, ele);
                    level_ele_counter++;
                }

                boost::asio::read(socket_, boost::asio::buffer(&level_pos_1, sizeof(size_t)));
                for (size_t k = 0; k < eleNum_oneBins; k++) {
                    ele = read_data_to_cell(Middle_Table1[i - 1][j][level_pos_1], k, element_size);
                    tmp_addr = *reinterpret_cast<addr_type*>(ele.data());
                    write_data(tmp_ele_array, level_ele_counter, element_size, ele);
                    boost::asio::read(socket_, boost::asio::buffer(&tmp_addr, sizeof(size_t)));
                    changeFirstSizeT(ele, tmp_addr);
                    write_data(Middle_Table1[i - 1][j][level_pos_1], k, element_size, ele);
                    level_ele_counter++;
                }
            }
        }
        if (level_ele_counter != 0) {
            size_t dpf_key_size = getDPF_keySize(level_ele_counter);
            std::vector<uint8_t> dpf_key(dpf_key_size);
            boost::asio::read(socket_, boost::asio::buffer(dpf_key.data(), dpf_key_size));
            size_t log_tab_length = std::ceil(log2(level_ele_counter));
            std::vector<uint8_t> aaaa;
            if(log_tab_length > 10) {
                aaaa = DPF::EvalFull8(dpf_key, log_tab_length);
            }    
            else {
                aaaa = DPF::EvalFull(dpf_key, log_tab_length);
            }

            ele = pir_read(tmp_ele_array, aaaa, level_ele_counter, element_size);
            boost::asio::write(socket_, boost::asio::buffer(ele.data(), element_size));
        }
    }

    // Bottom table
    level_ele_counter = 0;
    valid_bucket_num = countOnes(full_flag[L - 1]);
    element_array_type tmp_ele_array((valid_bucket_num * eleNum_oneBins * num_cell_per_element) << 1);
    if (is_index_bit_1(full_flag[L - 1], 0)) {
        boost::asio::read(socket_, boost::asio::buffer(&level_pos_0, sizeof(size_t)));
        for (size_t k = 0; k < eleNum_oneBins; k++) {
            ele = read_data_to_cell(Bottom_Table0[level_pos_0], k, element_size);
            tmp_addr = *reinterpret_cast<addr_type*>(ele.data());
            write_data(tmp_ele_array, level_ele_counter, element_size, ele);
            boost::asio::read(socket_, boost::asio::buffer(&tmp_addr, sizeof(size_t)));
            changeFirstSizeT(ele, tmp_addr);
            write_data(Bottom_Table0[level_pos_0], k, element_size, ele);
            level_ele_counter++;
        }

        boost::asio::read(socket_, boost::asio::buffer(&level_pos_1, sizeof(size_t)));
        for (size_t k = 0; k < eleNum_oneBins; k++) {
            ele = read_data_to_cell(Bottom_Table1[level_pos_1], k, element_size);
            tmp_addr = *reinterpret_cast<addr_type*>(ele.data());
            write_data(tmp_ele_array, level_ele_counter, element_size, ele);
            boost::asio::read(socket_, boost::asio::buffer(&tmp_addr, sizeof(size_t)));
            changeFirstSizeT(ele, tmp_addr);
            write_data(Bottom_Table1[level_pos_1], k, element_size, ele);
            level_ele_counter++;
        }

        size_t dpf_key_size = getDPF_keySize(level_ele_counter);
        std::vector<uint8_t> dpf_key(dpf_key_size);
        boost::asio::read(socket_, boost::asio::buffer(dpf_key.data(), dpf_key_size));
        size_t log_tab_length = std::ceil(log2(level_ele_counter));
        std::vector<uint8_t> aaaa;
        if(log_tab_length > 10) {
            aaaa = DPF::EvalFull8(dpf_key, log_tab_length);
        }    
        else {
            aaaa = DPF::EvalFull(dpf_key, log_tab_length);
        }

        ele = pir_read(tmp_ele_array, aaaa, level_ele_counter, element_size);
        boost::asio::write(socket_, boost::asio::buffer(ele.data(), element_size));
    }

    /**
     * Third, write the ele back to the top level
     */
    for (size_t i = 0; i < access_counter % lg_N; i++) {
        boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
        write_data(Top_Table, i, element_size, ele);
        
    }
    boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
    write_data(Top_Table, access_counter % lg_N, element_size, ele);

    access_counter++;
    
    if (access_counter % static_cast<size_t>(pow(lg_N, L)) == 0) {
        RebuildL();
    }
    else {
        for (size_t i = L - 1; i > 0; i--) {
            if (access_counter % static_cast<size_t>(pow(lg_N, i)) == 0) {
                size_t reb_bucketID = (static_cast<size_t>(access_counter / static_cast<size_t>(pow(lg_N, i))) % lg_N) - 1;
                Rebuild(i, reb_bucketID);
                break;
            }
        }
    }
}

void Server_1::Rebuild(size_t level, size_t bucketID) {
    element_type ele(num_cell_per_element);
    size_t num_bins = get_tableLen(level);
    size_t nonEmptyCounter = static_cast<size_t>(pow(lg_N, level));
    std::vector<element_type> obliv_table((num_bins * eleNum_oneBins) << 1, element_type(num_cell_per_element));
    std::vector<size_t> nonEmptyAddrList(nonEmptyCounter);
    for (size_t i = 0; i < nonEmptyCounter; i++) {
        boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
        obliv_table[i] = ele;
        nonEmptyAddrList[i] = *reinterpret_cast<addr_type*>(ele.data());
    }
    for (size_t i = nonEmptyCounter; i < ((num_bins * eleNum_oneBins) << 1); i++) {
        ele = get_random_element(base_filler_addr + i - nonEmptyCounter, element_size);
        obliv_table[i] = ele;
    }
    

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(nonEmptyAddrList.begin(), nonEmptyAddrList.end(), g);
    for (size_t i = 0; i < nonEmptyCounter; i++) {
        boost::asio::write(socket_, boost::asio::buffer(&nonEmptyAddrList[i], sizeof(size_t)));
    }

    /**
     * Second, oblivious hash
     */
    oblivious_hash(level, bucketID, obliv_table);
    for (size_t i = 1; i < level; i++) {
        full_flag[i - 1] = 0; 
    }
    set_bit(full_flag[level - 1], bucketID, 1);
}

void Server_1::RebuildL() {
    element_type ele(num_cell_per_element);
    size_t num_bins = get_tableLen(L);
    size_t nonEmptyCounter = N;
    std::vector<element_type> obliv_table((num_bins * eleNum_oneBins) << 1, element_type(num_cell_per_element));
    std::vector<size_t> nonEmptyAddrList(nonEmptyCounter);
    for (size_t i = 0; i < nonEmptyCounter; i++) {
        boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
        obliv_table[i] = ele;
        nonEmptyAddrList[i] = *reinterpret_cast<addr_type*>(ele.data());
    }
    for (size_t i = nonEmptyCounter; i < ((num_bins * eleNum_oneBins) << 1); i++) {
        ele = get_random_element(base_filler_addr + i - nonEmptyCounter, element_size);
        obliv_table[i] = ele;
    }
    

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(nonEmptyAddrList.begin(), nonEmptyAddrList.end(), g);
    for (size_t i = 0; i < nonEmptyCounter; i++) {
        boost::asio::write(socket_, boost::asio::buffer(&nonEmptyAddrList[i], sizeof(size_t)));
    }

    /**
     * Second, oblivious hash
     */
    oblivious_hash(L, 0, obliv_table);
    clr(Top_Table);
    
    for (size_t i = 1; i < L; i++) {
        full_flag[i - 1] = 0; 
    }
    full_flag[L - 1] = 1;
}