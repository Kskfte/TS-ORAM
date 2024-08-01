#include "server_0.h"

Server_0::Server_0(boost::asio::io_context& io_context, short port, size_t N, size_t element_size, size_t tag_size): 
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

size_t Server_0::get_tableLen(size_t level) {
    if (level == L) {
        return std::ceil(N / tth_b);
    }
    else {
        return std::ceil(pow(lg_N, level) / tth_b);
    }
}

/**
 * dire = 0, decending order; dire = 1, ascending order
 * flag = 0, (pos, addr) sort; flag = 1, (pos, addr, excess) sort*/ 
void Server_0::compAndSwap(std::vector<std::tuple<bool, size_t, size_t, size_t>>& A, size_t ii, size_t jj, size_t flag, size_t dire) { // A: (addr, pos)
    // Write elements of tuple A[ii]
    boost::asio::write(socket_, boost::asio::buffer(&std::get<0>(A[ii]), sizeof(bool)));
    boost::asio::write(socket_, boost::asio::buffer(&std::get<1>(A[ii]), sizeof(size_t)));
    boost::asio::write(socket_, boost::asio::buffer(&std::get<2>(A[ii]), sizeof(size_t)));

    // Write elements of tuple A[jj]
    boost::asio::write(socket_, boost::asio::buffer(&std::get<0>(A[jj]), sizeof(bool)));
    boost::asio::write(socket_, boost::asio::buffer(&std::get<1>(A[jj]), sizeof(size_t)));
    boost::asio::write(socket_, boost::asio::buffer(&std::get<2>(A[jj]), sizeof(size_t)));

    // Read elements back into tuple A[ii]
    boost::asio::read(socket_, boost::asio::buffer(&std::get<0>(A[ii]), sizeof(bool)));
    boost::asio::read(socket_, boost::asio::buffer(&std::get<1>(A[ii]), sizeof(size_t)));
    boost::asio::read(socket_, boost::asio::buffer(&std::get<2>(A[ii]), sizeof(size_t)));

    // Read elements back into tuple A[jj]
    boost::asio::read(socket_, boost::asio::buffer(&std::get<0>(A[jj]), sizeof(bool)));
    boost::asio::read(socket_, boost::asio::buffer(&std::get<1>(A[jj]), sizeof(size_t)));
    boost::asio::read(socket_, boost::asio::buffer(&std::get<2>(A[jj]), sizeof(size_t)));
}

void Server_0::bitonicToOrder(std::vector<std::tuple<bool, size_t, size_t, size_t>>& A, size_t start, size_t end, size_t flag, size_t dire) {
    if(end - start > 1) {
        size_t middle = (end - start) >> 1;
        for (size_t i = 0; i < middle; i++) {
            compAndSwap(A, i + start, i + start + middle, flag, dire);
        }
        bitonicToOrder(A, start, start + middle, flag, dire);
        bitonicToOrder(A, start + middle, end, flag, dire);
    }
}

void Server_0::bitonicMerge(std::vector<std::tuple<bool, size_t, size_t, size_t>>& A, size_t start, size_t end, size_t flag, size_t dire) {
    if(end - start > 1) {
        size_t middle = (end - start) >> 1;
        bitonicMerge(A, start, start + middle, flag, dire);
        bitonicMerge(A, start + middle, end, flag, dire ^ 1);
        bitonicToOrder(A, start, end, flag, dire);
    }
}

void Server_0::oblivious_hash(size_t level, size_t bucketID, std::vector<std::tuple<bool, size_t, size_t, size_t>>& obliv_table0, std::vector<std::tuple<bool, size_t, size_t, size_t>>& obliv_table1) {
    size_t real_array_len, pad_array_len;
    size_t table_ele_num = get_tableLen(level) * eleNum_oneBins;
    if (level == L) {
        real_array_len = table_ele_num + N;
    }
    else {
        real_array_len = table_ele_num + std::ceil(pow(lg_N, level));
    }
    pad_array_len = nextPowerOf2(real_array_len);
    assert(pad_array_len == obliv_table0.size());
    /**
     * 1:::Sort table_0 according to the (pos, addr)
     */
    bitonicMerge(obliv_table0, 0, pad_array_len, 0, 1);

    for (size_t i = 0; i < pad_array_len; i++) {
        boost::asio::write(socket_, boost::asio::buffer(&std::get<0>(obliv_table0[i]), sizeof(bool)));
        boost::asio::write(socket_, boost::asio::buffer(&std::get<2>(obliv_table0[i]), sizeof(size_t)));
        
        boost::asio::read(socket_, boost::asio::buffer(&std::get<0>(obliv_table0[i]), sizeof(bool)));
        boost::asio::read(socket_, boost::asio::buffer(&std::get<2>(obliv_table0[i]), sizeof(size_t)));
    }
    /**
     * 2:::Sort table_0 according to the (excess, pos, addr)
     */
    bitonicMerge(obliv_table0, 0, pad_array_len, 1, 1);

    /**
     * 3:::Sort table_1 according to the (pos, addr)
     */
    for (size_t i = table_ele_num; i < pad_array_len; i++) {
        obliv_table1[i] = {false, std::get<1>(obliv_table0[i]), std::get<3>(obliv_table0[i]), std::get<2>(obliv_table0[i])};
    }
    bitonicMerge(obliv_table1, 0, pad_array_len, 0, 1);
    for (size_t i = 0; i < pad_array_len; i++) {
        boost::asio::write(socket_, boost::asio::buffer(&std::get<0>(obliv_table1[i]), sizeof(bool)));
        boost::asio::write(socket_, boost::asio::buffer(&std::get<2>(obliv_table1[i]), sizeof(size_t)));
        
        boost::asio::read(socket_, boost::asio::buffer(&std::get<0>(obliv_table1[i]), sizeof(bool)));
        boost::asio::read(socket_, boost::asio::buffer(&std::get<2>(obliv_table1[i]), sizeof(size_t)));
    }
    /**
     * 4:::Sort table_1 according to the (excess, pos, addr)
     */
    bitonicMerge(obliv_table1, 0, pad_array_len, 1, 1);

    /**
     * 5:::Match
     */
    // Send addr to client and receive tag
    element_type ele(num_cell_per_element);
    tag_type tag(num_cell_per_tag);
    std::vector<tag_type> tag_tab(table_ele_num << 1, tag_type(num_cell_per_tag));
    for (size_t i = 0; i < table_ele_num; i++) {
        boost::asio::write(socket_, boost::asio::buffer(&std::get<1>(obliv_table0[i]), sizeof(size_t)));
        boost::asio::read(socket_, boost::asio::buffer(tag.data(), tag_size));
        tag_tab[i] = tag;
    }
    for (size_t i = 0; i < table_ele_num; i++) {
        boost::asio::write(socket_, boost::asio::buffer(&std::get<1>(obliv_table1[i]), sizeof(size_t)));
        boost::asio::read(socket_, boost::asio::buffer(tag.data(), tag_size));
        tag_tab[i + table_ele_num] = tag;
    }

    // Receive tag and ele
    for (size_t i = 0; i < (table_ele_num << 1); i++) {
        boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
        boost::asio::read(socket_, boost::asio::buffer(tag.data(), tag_size));
        for (size_t j = 0; j < tag_tab.size(); j++) {
            if (areTagsEqual(tag_tab[j], tag)) {
                size_t case_identifier = ((1UL & (j < table_ele_num)) << 1) | (level < L);
                size_t binID, locInbin;
                switch (case_identifier) {
                    case 3: // first table of level
                        binID = std::floor(j / eleNum_oneBins);
                        locInbin = j % eleNum_oneBins;
                        write_data(Middle_Table0[level - 1][bucketID][binID], locInbin, element_size, ele);
                        break;
                    case 2: // first table of L
                        binID = std::floor(j / eleNum_oneBins);
                        locInbin = j % (eleNum_oneBins);
                        write_data(Bottom_Table0[binID], locInbin, element_size, ele);
                        break;
                    case 1: // second table of level
                        binID = std::floor((j - table_ele_num) / eleNum_oneBins);
                        locInbin = (j - table_ele_num) % eleNum_oneBins;
                        write_data(Middle_Table1[level - 1][bucketID][binID], locInbin, element_size, ele);
                        break;
                    case 0: // second table of L
                        binID = std::floor((j - table_ele_num) / eleNum_oneBins);
                        locInbin = (j - table_ele_num) % eleNum_oneBins;
                        write_data(Bottom_Table1[binID], locInbin, element_size, ele);
                        break;
                }
                break;
            }
        }
    }

    // Finally, send the table to S1 by client
    if (level < L) {
        for (size_t i = 0; i < get_tableLen(level); i++) {
            for (size_t j = 0; j < eleNum_oneBins; j++) {
                ele = read_data_to_cell(Middle_Table0[level - 1][bucketID][i], j, element_size);
                boost::asio::write(socket_, boost::asio::buffer(ele.data(), element_size));
            }
        }
        for (size_t i = 0; i < get_tableLen(level); i++) {
            for (size_t j = 0; j < eleNum_oneBins; j++) {
                ele = read_data_to_cell(Middle_Table1[level - 1][bucketID][i], j, element_size);
                boost::asio::write(socket_, boost::asio::buffer(ele.data(), element_size));
            }
        }
    }
    else {
        for (size_t i = 0; i < get_tableLen(level); i++) {
            for (size_t j = 0; j < eleNum_oneBins; j++) {
                ele = read_data_to_cell(Bottom_Table0[i], j, element_size);
                boost::asio::write(socket_, boost::asio::buffer(ele.data(), element_size));
            }
        }
        for (size_t i = 0; i < get_tableLen(level); i++) {
            for (size_t j = 0; j < eleNum_oneBins; j++) {
                ele = read_data_to_cell(Bottom_Table1[i], j, element_size);
                boost::asio::write(socket_, boost::asio::buffer(ele.data(), element_size));
            }
        }
    }
}
        
void Server_0::Setup() {
    element_type ele(num_cell_per_element);
    size_t addr;
    size_t level_pos_0, level_pos_1;
    size_t num_bins = get_tableLen(L);
    std::vector<std::tuple<bool, size_t, size_t, size_t>> obliv_table0(nextPowerOf2(num_bins * eleNum_oneBins + N)); // (excess_flag, addr, pos0, pos1)
    std::vector<std::tuple<bool, size_t, size_t, size_t>> obliv_table1(nextPowerOf2(num_bins * eleNum_oneBins + N));
    for (size_t i = 0; i < num_bins * eleNum_oneBins; i++) {
        obliv_table0[i] = {false, base_filler_addr + i, std::floor(i / eleNum_oneBins), std::floor(i / eleNum_oneBins)};
        obliv_table1[i] = {false, base_filler_addr + i, std::floor(i / eleNum_oneBins), std::floor(i / eleNum_oneBins)};
    }
    for (size_t i = num_bins * eleNum_oneBins; i < obliv_table0.size() - N; i++) {
        obliv_table0[i] = {false, base_filler_addr + i, std::floor(i / eleNum_oneBins), std::floor(i / eleNum_oneBins)};
    }
    for (size_t i = obliv_table0.size() - N; i < obliv_table0.size(); i++) {
        boost::asio::read(socket_, boost::asio::buffer(&addr, sizeof(size_t)));
        boost::asio::read(socket_, boost::asio::buffer(&level_pos_0, sizeof(size_t)));
        boost::asio::read(socket_, boost::asio::buffer(&level_pos_1, sizeof(size_t)));
        obliv_table0[i] = {false, addr, level_pos_0, level_pos_1};
    }
    
    /**
     * Second, oblivious hash
     */
    oblivious_hash(L, 0, obliv_table0, obliv_table1);
    full_flag[L - 1] = 1;
}

void Server_0::Access() {
    // Parameters
    element_type ele(num_cell_per_element);
    /**
     * First, send the top level
     */
    for (size_t i = 0; i < access_counter % lg_N; i++) {
        ele = read_data_to_cell(Top_Table, i, element_size);
        boost::asio::write(socket_, boost::asio::buffer(ele.data(), element_size));
    }

    /**
     * Second, send ele in table
     */
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
                    boost::asio::write(socket_, boost::asio::buffer(&tmp_addr, sizeof(size_t)));
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
                    boost::asio::write(socket_, boost::asio::buffer(&tmp_addr, sizeof(size_t)));
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
            boost::asio::write(socket_, boost::asio::buffer(&tmp_addr, sizeof(size_t)));
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
            boost::asio::write(socket_, boost::asio::buffer(&tmp_addr, sizeof(size_t)));
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
        ele = read_data_to_cell(Top_Table, i, element_size);
        boost::asio::write(socket_, boost::asio::buffer(ele.data(), element_size));
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

void Server_0::Rebuild(size_t level, size_t bucketID) {
    // Parameters
    element_type ele(num_cell_per_element);
    /**
     * First: Shuffle elements and send to client
     */
    std::vector<cell_type*> element_pointers;
    for (size_t i = 0; i < lg_N; i++) {
        element_pointers.push_back(Top_Table.data() + i * element_size / sizeof(cell_type));
    }
    for (size_t i = 1; i < level; i++) {
        for (size_t j = 0; j < lg_N - 1; j++) {
            for (size_t k = 0; k < get_tableLen(i); k++) {
                for (size_t l = 0; l < eleNum_oneBins; l++) {
                    element_pointers.push_back(Middle_Table0[i - 1][j][k].data() + l * element_size / sizeof(cell_type));
                    element_pointers.push_back(Middle_Table1[i - 1][j][k].data() + l * element_size / sizeof(cell_type));
                }
            }
        }
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(element_pointers.begin(), element_pointers.end(), g);

    // Read
    for (auto ptr : element_pointers) {
        if (ptr == nullptr) {
            std::cerr << "Error: Null pointer encountered in element_pointers" << std::endl;
            continue;
        }
        // Debug information
        std::memcpy(ele.data(), ptr, element_size);
        boost::asio::write(socket_, boost::asio::buffer(ele.data(), element_size));
    }

    /**
     * Second, receive the addr and pos
     */
    size_t nonEmptyCounter = static_cast<size_t>(pow(lg_N, level));
    size_t addr;
    size_t level_pos_0, level_pos_1;
    size_t num_bins = get_tableLen(level);
    std::vector<std::tuple<bool, size_t, size_t, size_t>> obliv_table0(nextPowerOf2(num_bins * eleNum_oneBins + nonEmptyCounter)); // (excess_flag, addr, pos0, pos1)
    std::vector<std::tuple<bool, size_t, size_t, size_t>> obliv_table1(nextPowerOf2(num_bins * eleNum_oneBins + nonEmptyCounter));
    for (size_t i = 0; i < num_bins * eleNum_oneBins; i++) {
        obliv_table0[i] = {false, base_filler_addr + i, std::floor(i / eleNum_oneBins), std::floor(i / eleNum_oneBins)};
        obliv_table1[i] = {false, base_filler_addr + i, std::floor(i / eleNum_oneBins), std::floor(i / eleNum_oneBins)};
    }
    for (size_t i = num_bins * eleNum_oneBins; i < obliv_table0.size() - nonEmptyCounter; i++) {
        obliv_table0[i] = {false, base_filler_addr + i, std::floor(i / eleNum_oneBins), std::floor(i / eleNum_oneBins)};
    }
    for (size_t i = obliv_table0.size() - nonEmptyCounter; i < obliv_table0.size(); i++) {
        boost::asio::read(socket_, boost::asio::buffer(&addr, sizeof(size_t)));
        boost::asio::read(socket_, boost::asio::buffer(&level_pos_0, sizeof(size_t)));
        boost::asio::read(socket_, boost::asio::buffer(&level_pos_1, sizeof(size_t)));
        obliv_table0[i] = {false, addr, level_pos_0, level_pos_1};
    }
    
    /**
     * Second, oblivious hash
     */
    oblivious_hash(level, bucketID, obliv_table0, obliv_table1);
    for (size_t i = 1; i < level; i++) {
        full_flag[i - 1] = 0; 
    }
    set_bit(full_flag[level - 1], bucketID, 1);
}

void Server_0::RebuildL() {
    // Parameters
    element_type ele(num_cell_per_element);
    /**
     * First: Shuffle elements and send to client
     */
    std::vector<cell_type*> element_pointers;
    for (size_t i = 0; i < lg_N; i++) {
        element_pointers.push_back(Top_Table.data() + i * element_size / sizeof(cell_type));
    }
    for (size_t i = 1; i < L; i++) {
        for (size_t j = 0; j < lg_N - 1; j++) {
            for (size_t k = 0; k < get_tableLen(i); k++) {
                for (size_t l = 0; l < eleNum_oneBins; l++) {
                    element_pointers.push_back(Middle_Table0[i - 1][j][k].data() + l * element_size / sizeof(cell_type));
                    element_pointers.push_back(Middle_Table1[i - 1][j][k].data() + l * element_size / sizeof(cell_type));
                }
            }
        }
    }
    for (size_t k = 0; k < get_tableLen(L); k++) {
        for (size_t l = 0; l < eleNum_oneBins; l++) {
            element_pointers.push_back(Bottom_Table0[k].data() + l * element_size / sizeof(cell_type));
            element_pointers.push_back(Bottom_Table1[k].data() + l * element_size / sizeof(cell_type));
        }
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(element_pointers.begin(), element_pointers.end(), g);

    // Read
    for (auto ptr : element_pointers) {
        if (ptr == nullptr) {
            std::cerr << "Error: Null pointer encountered in element_pointers" << std::endl;
            continue;
        }
        // Debug information
        std::memcpy(ele.data(), ptr, element_size);
        boost::asio::write(socket_, boost::asio::buffer(ele.data(), element_size));
    }

    /**
     * Second, receive the addr and pos
     */
    size_t nonEmptyCounter = N;
    size_t addr;
    size_t level_pos_0, level_pos_1;
    size_t num_bins = get_tableLen(L);
    std::vector<std::tuple<bool, size_t, size_t, size_t>> obliv_table0(nextPowerOf2(num_bins * eleNum_oneBins + nonEmptyCounter)); // (excess_flag, addr, pos0, pos1)
    std::vector<std::tuple<bool, size_t, size_t, size_t>> obliv_table1(nextPowerOf2(num_bins * eleNum_oneBins + nonEmptyCounter));
    for (size_t i = 0; i < num_bins * eleNum_oneBins; i++) {
        obliv_table0[i] = {false, base_filler_addr + i, std::floor(i / eleNum_oneBins), std::floor(i / eleNum_oneBins)};
        obliv_table1[i] = {false, base_filler_addr + i, std::floor(i / eleNum_oneBins), std::floor(i / eleNum_oneBins)};
    }
    for (size_t i = num_bins * eleNum_oneBins; i < obliv_table0.size() - nonEmptyCounter; i++) {
        obliv_table0[i] = {false, base_filler_addr + i, std::floor(i / eleNum_oneBins), std::floor(i / eleNum_oneBins)};
    }
    for (size_t i = obliv_table0.size() - nonEmptyCounter; i < obliv_table0.size(); i++) {
        boost::asio::read(socket_, boost::asio::buffer(&addr, sizeof(size_t)));
        boost::asio::read(socket_, boost::asio::buffer(&level_pos_0, sizeof(size_t)));
        boost::asio::read(socket_, boost::asio::buffer(&level_pos_1, sizeof(size_t)));
        obliv_table0[i] = {false, addr, level_pos_0, level_pos_1};
    }
    /**
     * Second, oblivious hash
     */
    oblivious_hash(L, 0, obliv_table0, obliv_table1);
    clr(Top_Table);

    for (size_t i = 1; i < L; i++) {
        full_flag[i - 1] = 0; 
    }
    full_flag[L - 1] = 1;
}