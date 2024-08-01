#include "client.h"

const std::pair<std::string, int> Server0_IPAndPort = {"127.0.0.1", 5000};
const std::pair<std::string, int> Server1_IPAndPort = {"127.0.0.1", 5001};

Client::Client(size_t N, size_t element_size, size_t tag_size): 
    N(N), element_size(element_size), tag_size(tag_size), 
    num_cell_per_element(std::ceil(element_size / sizeof(cell_type))),
    num_cell_per_tag(std::ceil(tag_size / sizeof(cell_type))),
    lg_N(std::ceil(log2(N))), L(std::ceil(lg_N/log2(lg_N))),
    tth_b(std::ceil(pow(lg_N, 0.6))), eleNum_oneBins(5 * tth_b), topLevel_size(0),
    access_counter(0), maxL_epoch(0),
    base_dummy_addr(1UL << 60), base_filler_addr(1UL << 61), base_excess_addr(1UL << 62), 
    master_level_key(_mm_set_epi32(0x01234567, 0x89abcdef, 0xfedcba98, 0x76543210)),
    master_tag_key(_mm_set_epi32(0x89abcdef, 0xfedcba98, 0x01234567, 0x76543210)),
    aes_level(master_level_key), aes_tag(master_tag_key),
    setup_bandwidth(0), access_bandwidth(0), 
    server0_socket(io_context), server1_socket(io_context)
{
    std::cout << "L: " << L << "\n";
    std::cout << "lg_N: " << lg_N << "\n";
    std::cout << "tth_b: " << tth_b << "\n";
    std::cout << "eleNum_oneBins: " << eleNum_oneBins << "\n";
    full_flag.resize(L);
    connect_to_server(server0_socket, Server0_IPAndPort.first, Server0_IPAndPort.second);
    connect_to_server(server1_socket, Server1_IPAndPort.first, Server1_IPAndPort.second);
}

void Client::connect_to_server(boost::asio::ip::tcp::socket& socket, const std::string& server_ip, int server_port) {
    try {
        boost::asio::ip::tcp::resolver resolver(io_context);
        boost::asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(server_ip, std::to_string(server_port));
        boost::asio::connect(socket, endpoints);
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
}

size_t Client::get_epoch(size_t level, size_t bucketID) {
    if (level == L) {
        return maxL_epoch;
    }
    else {    
        size_t first_rebuild = std::ceil(pow(lg_N, level)) * (bucketID + 1);
        return std::floor((access_counter - first_rebuild) / std::ceil(pow(lg_N, level + 1)));
    }
}

size_t Client::get_tableLen(size_t level) {
    if (level == L) {
        return std::ceil(N / tth_b);
    }
    else {
        return std::ceil(pow(lg_N, level) / tth_b);
    }
}

key_type Client::get_level_key(size_t level, size_t bucketID, size_t epoch, int h0Orh1)
{
    key_type level_block = _mm_set_epi64x(level<< 32 | bucketID, epoch << 1 | static_cast<size_t>(h0Orh1));
    key_type level_key = aes_level.encryptECB_MMO(level_block);
    return level_key;
}

tag_type Client::get_tag(size_t level, size_t bucketID, size_t epoch, addr_type vir_addr) {
    tag_type tag(num_cell_per_tag);
    for (size_t i = 0; i < (num_cell_per_tag << 1); i += 2) {
        key_type addr_block1 = _mm_set_epi64x(vir_addr, level << 50 | bucketID << 40 | epoch << 30 | i);
        key_type addr_block2 = _mm_set_epi64x(vir_addr, level << 50 | bucketID << 40 | epoch << 30 | (i + 1));
        // key_type addr_block2 = _mm_set_epi32(static_cast<int>(i + 1 + vir_addr), static_cast<int>(level + vir_addr), static_cast<int>(bucketID + vir_addr), static_cast<int>(epoch + vir_addr));
        key_type tag_block1 = aes_tag.encryptECB_MMO(addr_block1);
        key_type tag_block2 = aes_tag.encryptECB_MMO(addr_block2);
        std::memcpy(reinterpret_cast<char*>(tag.data()) + i * 16, &tag_block1, sizeof(key_type));
        std::memcpy(reinterpret_cast<char*>(tag.data()) + (i + 1) * 16, &tag_block2, sizeof(key_type));
    }
    return tag;
}

/**
 * dire = 0, decending order; dire = 1, ascending order
 * flag = 0, (pos, addr) sort; flag = 1, (pos, addr, excess) sort*/ 
void Client::compAndSwap(size_t array_len, size_t ii, size_t jj, size_t flag, size_t dire) { // A: (addr, pos)
    size_t case_identifier = ((flag & 1) << 1) | (dire & 1);
    bool ef_0, ef_1;
    size_t addr_0, pos_0, addr_1, pos_1;
    boost::asio::read(server0_socket, boost::asio::buffer(&ef_0, sizeof(bool)));
    boost::asio::read(server0_socket, boost::asio::buffer(&addr_0, sizeof(size_t)));
    boost::asio::read(server0_socket, boost::asio::buffer(&pos_0, sizeof(size_t)));
    boost::asio::read(server0_socket, boost::asio::buffer(&ef_1, sizeof(bool)));
    boost::asio::read(server0_socket, boost::asio::buffer(&addr_1, sizeof(size_t)));
    boost::asio::read(server0_socket, boost::asio::buffer(&pos_1, sizeof(size_t)));
    switch (case_identifier) {
        case 3:
            if (ef_0 || 
                (!ef_0 && !ef_1 && (
                    (pos_0 > pos_1 || 
                    ((pos_0 == pos_1) && (addr_0 > addr_1)))
                ))) {
                boost::asio::write(server0_socket, boost::asio::buffer(&ef_1, sizeof(bool)));
                boost::asio::write(server0_socket, boost::asio::buffer(&addr_1, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&pos_1, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&ef_0, sizeof(bool)));
                boost::asio::write(server0_socket, boost::asio::buffer(&addr_0, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&pos_0, sizeof(size_t)));
            }
            else {
                boost::asio::write(server0_socket, boost::asio::buffer(&ef_0, sizeof(bool)));
                boost::asio::write(server0_socket, boost::asio::buffer(&addr_0, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&pos_0, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&ef_1, sizeof(bool)));
                boost::asio::write(server0_socket, boost::asio::buffer(&addr_1, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&pos_1, sizeof(size_t)));
            }
            break;
        case 2:
            if (ef_1 || 
                (!ef_0 && !ef_1 && (
                    (pos_0 < pos_1 || 
                    ((pos_0 == pos_1) && (addr_0 < addr_1)))
                ))) {
                boost::asio::write(server0_socket, boost::asio::buffer(&ef_1, sizeof(bool)));
                boost::asio::write(server0_socket, boost::asio::buffer(&addr_1, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&pos_1, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&ef_0, sizeof(bool)));
                boost::asio::write(server0_socket, boost::asio::buffer(&addr_0, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&pos_0, sizeof(size_t)));
            }
            else {
                boost::asio::write(server0_socket, boost::asio::buffer(&ef_0, sizeof(bool)));
                boost::asio::write(server0_socket, boost::asio::buffer(&addr_0, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&pos_0, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&ef_1, sizeof(bool)));
                boost::asio::write(server0_socket, boost::asio::buffer(&addr_1, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&pos_1, sizeof(size_t)));
            }
            break;
        case 1:  
            if (pos_0 > pos_1 || ((pos_0 == pos_1) && (addr_0 > addr_1))) {
                boost::asio::write(server0_socket, boost::asio::buffer(&ef_1, sizeof(bool)));
                boost::asio::write(server0_socket, boost::asio::buffer(&addr_1, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&pos_1, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&ef_0, sizeof(bool)));
                boost::asio::write(server0_socket, boost::asio::buffer(&addr_0, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&pos_0, sizeof(size_t)));
            }
            else {
                boost::asio::write(server0_socket, boost::asio::buffer(&ef_0, sizeof(bool)));
                boost::asio::write(server0_socket, boost::asio::buffer(&addr_0, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&pos_0, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&ef_1, sizeof(bool)));
                boost::asio::write(server0_socket, boost::asio::buffer(&addr_1, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&pos_1, sizeof(size_t)));
            }
            break;
        case 0:
            if (pos_0 < pos_1 || ((pos_0 == pos_1) && (addr_0 < addr_1))) {
                boost::asio::write(server0_socket, boost::asio::buffer(&ef_1, sizeof(bool)));
                boost::asio::write(server0_socket, boost::asio::buffer(&addr_1, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&pos_1, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&ef_0, sizeof(bool)));
                boost::asio::write(server0_socket, boost::asio::buffer(&addr_0, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&pos_0, sizeof(size_t)));
            }
            else {
                boost::asio::write(server0_socket, boost::asio::buffer(&ef_0, sizeof(bool)));
                boost::asio::write(server0_socket, boost::asio::buffer(&addr_0, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&pos_0, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&ef_1, sizeof(bool)));
                boost::asio::write(server0_socket, boost::asio::buffer(&addr_1, sizeof(size_t)));
                boost::asio::write(server0_socket, boost::asio::buffer(&pos_1, sizeof(size_t)));
            }
            break;
    }

    if (access_counter == 0) {
        setup_bandwidth +=  ((sizeof(bool) << 2) + (sizeof(size_t) << 3));
    }
    else {
        access_bandwidth +=  ((sizeof(bool) << 2) + (sizeof(size_t) << 3));
    }
}

void Client::bitonicToOrder(size_t array_len, size_t start, size_t end, size_t flag, size_t dire) {
    if(end - start > 1) {
        size_t middle = (end - start) >> 1;
        for (size_t i = 0; i < middle; i++) {
            compAndSwap(array_len, i + start, i + start + middle, flag, dire);
        }
        bitonicToOrder(array_len, start, start + middle, flag, dire);
        bitonicToOrder(array_len, start + middle, end, flag, dire);
    }
}

void Client::bitonicMerge(size_t array_len, size_t start, size_t end, size_t flag, size_t dire) {
    if(end - start > 1) {
        size_t middle = (end - start) >> 1;
        bitonicMerge(array_len, start, start + middle, flag, dire);
        bitonicMerge(array_len, start + middle, end, flag, dire ^ 1);
        bitonicToOrder(array_len, start, end, flag, dire);
    }
}

void Client::oblivious_hash(size_t level, size_t bucketID) {
    size_t real_ele_num, table_ele_num, real_array_len, pad_array_len;
    size_t addr, pos;
    bool ef;
    table_ele_num = get_tableLen(level) * eleNum_oneBins;
    if (level == L) {
        real_ele_num = N;
        real_array_len = table_ele_num + N;
    }
    else {
        real_ele_num = std::ceil(pow(lg_N, level));
        real_array_len = table_ele_num + std::ceil(pow(lg_N, level));
    }
    pad_array_len = nextPowerOf2(real_array_len);
    /**
     * 1:::Sort table_0 according to the (pos, addr)
     */
    bitonicMerge(pad_array_len, 0, pad_array_len, 0, 1);
    size_t sameBin_counter = 0;
    size_t samBinId = 0;
    for (size_t i = 0; i < pad_array_len; i++) {
        boost::asio::read(server0_socket, boost::asio::buffer(&ef, sizeof(bool)));
        boost::asio::read(server0_socket, boost::asio::buffer(&pos, sizeof(size_t)));
        if (pos == samBinId && sameBin_counter >= eleNum_oneBins) {
            ef = true;
            sameBin_counter++;
        }
        else if (pos == samBinId && sameBin_counter < eleNum_oneBins) {
            sameBin_counter++;
        }
        else {
            samBinId = pos;
            sameBin_counter = 1;
        }
        boost::asio::write(server0_socket, boost::asio::buffer(&ef, sizeof(bool)));
        boost::asio::write(server0_socket, boost::asio::buffer(&pos, sizeof(size_t)));
    }

    if (access_counter == 0) {
        setup_bandwidth += pad_array_len * ((sizeof(bool) << 1) + (sizeof(size_t) << 1));
    }
    else {
        access_bandwidth += pad_array_len * ((sizeof(bool) << 1) + (sizeof(size_t) << 1));
    }
    /**
     * 2:::Sort table_0 according to the (excess, pos, addr)
     */
    bitonicMerge(pad_array_len, 0, pad_array_len, 1, 1);
    /**
     * 3:::Sort table_1 according to the (pos, addr)
     */
    bitonicMerge(pad_array_len, 0, pad_array_len, 0, 1);
    sameBin_counter = 0;
    samBinId = 0;
    for (size_t i = 0; i < pad_array_len; i++) {
        boost::asio::read(server0_socket, boost::asio::buffer(&ef, sizeof(bool)));
        boost::asio::read(server0_socket, boost::asio::buffer(&pos, sizeof(size_t)));
        if (pos == samBinId && sameBin_counter >= eleNum_oneBins) {
            ef = true;
            sameBin_counter++;
        }
        else if (pos == samBinId && sameBin_counter < eleNum_oneBins) {
            sameBin_counter++;
        }
        else {
            samBinId = pos;
            sameBin_counter = 1;
        }
        boost::asio::write(server0_socket, boost::asio::buffer(&ef, sizeof(bool)));
        boost::asio::write(server0_socket, boost::asio::buffer(&pos, sizeof(size_t)));
    }
    
    if (access_counter == 0) {
        setup_bandwidth += pad_array_len * ((sizeof(bool) << 1) + (sizeof(size_t) << 1));
    }
    else {
        access_bandwidth += pad_array_len * ((sizeof(bool) << 1) + (sizeof(size_t) << 1));
    }
    /**
     * 4:::Sort table_1 according to the (excess, pos, addr)
     */
    bitonicMerge(pad_array_len, 0, pad_array_len, 1, 1);
    /**
     * 5:::Match
     */
    element_type ele(num_cell_per_element);
    tag_type tag(num_cell_per_tag);
    // Table 0&1
    size_t empty_num_tag = 0; //(table_ele_num - real_ele_num) << 1;
    for (size_t i = 0; i < (table_ele_num << 1); i++) {
        boost::asio::read(server0_socket, boost::asio::buffer(&addr, sizeof(size_t)));
        if (addr < base_filler_addr) {
            tag = get_tag(level, bucketID, get_epoch(level, bucketID), addr);
        }
        else {
            tag = get_tag(level, bucketID, get_epoch(level, bucketID), base_filler_addr + empty_num_tag);
            empty_num_tag++;
        }
        boost::asio::write(server0_socket, boost::asio::buffer(tag.data(), tag_size));
    }
    
    if (access_counter == 0) {
        setup_bandwidth += (table_ele_num << 1) * (sizeof(size_t) + tag_size);
    }
    else {
        access_bandwidth += (table_ele_num << 1) * (sizeof(size_t) + tag_size);
    }

    assert(empty_num_tag == (table_ele_num << 1) - real_ele_num);
    // Receive ele from Server 1, compute tag, and send to server0
    for (size_t i = 0; i < (table_ele_num << 1); i++) {
        boost::asio::read(server1_socket, boost::asio::buffer(ele.data(), element_size));
        tag = get_tag(level, bucketID, get_epoch(level, bucketID), *reinterpret_cast<addr_type*>(ele.data()));
        boost::asio::write(server0_socket, boost::asio::buffer(ele.data(), element_size));
        boost::asio::write(server0_socket, boost::asio::buffer(tag.data(), tag_size));
    }
    
    if (access_counter == 0) {
        setup_bandwidth += (table_ele_num << 1) * (element_size + element_size + tag_size);
    }
    else {
        access_bandwidth += (table_ele_num << 1) * (element_size + element_size + tag_size);
    }
    
    // Recieve table from S0, and send to S1
    for (size_t i = 0; i < (table_ele_num << 1); i++) {
        boost::asio::read(server0_socket, boost::asio::buffer(ele.data(), element_size));
        boost::asio::write(server1_socket, boost::asio::buffer(ele.data(), element_size));
    }
    
    if (access_counter == 0) {
        setup_bandwidth += (table_ele_num << 1) * (element_size + element_size);
    }
    else {
        access_bandwidth += (table_ele_num << 1) * (element_size + element_size);
    }
}

void Client::Setup() {
    element_type ele(num_cell_per_element);
    /**
     * First, send addr + pos to S0, send ele to S1
     */
    std::vector<size_t> numbers(N);
    for (size_t i = 0; i < N; ++i) {
        numbers[i] = i + 1;
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(numbers.begin(), numbers.end(), g);

    key_type level_key_0 = get_level_key(L, 0, get_epoch(L, 0), 0);
    key_type level_key_1 = get_level_key(L, 0, get_epoch(L, 0), 1);
    size_t level_pos_0;
    size_t level_pos_1;
    for (size_t i = 0 ;i < N; i++) {
        ele = get_random_element(numbers[i], element_size);
        level_pos_0 = hash_function_addr(get_tableLen(L), level_key_0, numbers[i]);
        level_pos_1 = hash_function_addr(get_tableLen(L), level_key_1, numbers[i]);
        boost::asio::write(server0_socket, boost::asio::buffer(&numbers[i], sizeof(size_t)));
        boost::asio::write(server0_socket, boost::asio::buffer(&level_pos_0, sizeof(size_t)));
        boost::asio::write(server0_socket, boost::asio::buffer(&level_pos_1, sizeof(size_t)));
        boost::asio::write(server1_socket, boost::asio::buffer(ele.data(), element_size));
    }
    setup_bandwidth += N * (3 * sizeof(size_t) + element_size);
    /**
     * Second, oblivious hash, S0 has the addr, S1 has the ele
     */
    oblivious_hash(L, 0);
    full_flag[L - 1] = 1;
}

element_type Client::Access(char op, addr_type req_addr, uint8_t* write_value) {
    // accessed element
    bool found_flag = false;
    element_type accessed_element(num_cell_per_element);
    element_type ele(num_cell_per_element);
    element_type ele_shared0(num_cell_per_element);
    element_type ele_shared1(num_cell_per_element);

    /**
     * First, access the top level
     */
    for (size_t i = 0; i < access_counter % lg_N; i++) {
        boost::asio::read(server0_socket, boost::asio::buffer(ele.data(), element_size));
        if (!found_flag && *reinterpret_cast<addr_type*>(ele.data()) == req_addr) {
            accessed_element = ele;
            found_flag =true;
        }

        access_bandwidth += element_size;
    }
    /**
     * Second, access each table
     */
    key_type level_key_0;
    key_type level_key_1;
    size_t level_pos_0, level_pos_1;
    size_t dpf_loc = 0;
    size_t num_bins, tmp_accessed_addr;
    size_t level_ele_counter;
    size_t rewritten_addr;
    // Middle table
    for (size_t i = 1; i < L; i++) {
        num_bins = get_tableLen(i);
        level_ele_counter = 0;
        dpf_loc = 0;
        for (size_t j = 0; j < lg_N - 1; j++) {
            if (is_index_bit_1(full_flag[i - 1], j)) {
                level_key_0 = get_level_key(i, j, get_epoch(i, j), 0);
                if (found_flag) {level_pos_0 = get_random_position(num_bins);}
                else {
                    level_pos_0 = hash_function_addr(num_bins, level_key_0, req_addr);
                }
                boost::asio::write(server0_socket, boost::asio::buffer(&level_pos_0, sizeof(size_t)));
                boost::asio::write(server1_socket, boost::asio::buffer(&level_pos_0, sizeof(size_t)));

                access_bandwidth += sizeof(size_t) << 1;

                for (size_t k = 0; k < eleNum_oneBins; k++) {
                    boost::asio::read(server0_socket, boost::asio::buffer(&tmp_accessed_addr, sizeof(size_t)));
                    if (!found_flag && tmp_accessed_addr == req_addr) {
                        dpf_loc = level_ele_counter;
                        found_flag = true;
                        rewritten_addr = base_dummy_addr + access_counter;
                    }
                    else {
                        rewritten_addr = tmp_accessed_addr;
                    }
                    boost::asio::write(server0_socket, boost::asio::buffer(&rewritten_addr, sizeof(size_t)));
                    boost::asio::write(server1_socket, boost::asio::buffer(&rewritten_addr, sizeof(size_t)));
                    level_ele_counter++;
                }
                
                access_bandwidth += eleNum_oneBins * sizeof(size_t) * 3;

                level_key_1 = get_level_key(i, j, get_epoch(i, j), 1);
                if (found_flag) {level_pos_1 = get_random_position(num_bins);}
                else {
                    level_pos_1 = hash_function_addr(num_bins, level_key_1, req_addr);
                }
                boost::asio::write(server0_socket, boost::asio::buffer(&level_pos_1, sizeof(size_t)));
                boost::asio::write(server1_socket, boost::asio::buffer(&level_pos_1, sizeof(size_t)));

                access_bandwidth += sizeof(size_t) << 1;

                for (size_t k = 0; k < eleNum_oneBins; k++) {
                    boost::asio::read(server0_socket, boost::asio::buffer(&tmp_accessed_addr, sizeof(size_t)));
                    if (!found_flag && tmp_accessed_addr == req_addr) {
                        dpf_loc = level_ele_counter;
                        found_flag = true;
                        rewritten_addr = base_dummy_addr + access_counter;
                    }
                    else {
                        rewritten_addr = tmp_accessed_addr;
                    }
                    boost::asio::write(server0_socket, boost::asio::buffer(&rewritten_addr, sizeof(size_t)));
                    boost::asio::write(server1_socket, boost::asio::buffer(&rewritten_addr, sizeof(size_t)));
                    level_ele_counter++;
                }

                access_bandwidth += eleNum_oneBins * sizeof(size_t) * 3;
            }
        }
        if (level_ele_counter != 0) {
            auto keys_0 = DPF::Gen(dpf_loc, std::ceil(log2(level_ele_counter)));
            boost::asio::write(server0_socket, boost::asio::buffer((keys_0.first.data()), keys_0.first.size()));
            boost::asio::write(server1_socket, boost::asio::buffer((keys_0.second.data()), keys_0.second.size())); 
            boost::asio::read(server0_socket, boost::asio::buffer(ele_shared0.data(), element_size));
            boost::asio::read(server1_socket, boost::asio::buffer(ele_shared1.data(), element_size));
            
            access_bandwidth += (keys_0.first.size() << 1) + (element_size << 1);

            ele = convert_share_to_element(ele_shared0, ele_shared1);
            if (*reinterpret_cast<addr_type*>(ele.data()) == req_addr) {
                accessed_element = ele;
            } 
        }
    }

    // Bottom table
    num_bins = get_tableLen(L);
    level_ele_counter = 0;
    dpf_loc = 0;
    assert(is_index_bit_1(full_flag[L - 1], 0) == true);
    if (is_index_bit_1(full_flag[L - 1], 0)) {
        level_key_0 = get_level_key(L, 0, get_epoch(L, 0), 0);
        if (found_flag) {level_pos_0 = get_random_position(num_bins);}
        else {
            level_pos_0 = hash_function_addr(num_bins, level_key_0, req_addr);
        }
        boost::asio::write(server0_socket, boost::asio::buffer(&level_pos_0, sizeof(size_t)));
        boost::asio::write(server1_socket, boost::asio::buffer(&level_pos_0, sizeof(size_t)));

        access_bandwidth += sizeof(size_t) << 1;

        for (size_t k = 0; k < eleNum_oneBins; k++) {
            boost::asio::read(server0_socket, boost::asio::buffer(&tmp_accessed_addr, sizeof(size_t)));
            if (!found_flag && tmp_accessed_addr == req_addr) {
                dpf_loc = level_ele_counter;
                found_flag = true;
                rewritten_addr = base_dummy_addr + access_counter;
            }
            else {
                rewritten_addr = tmp_accessed_addr;
            }
            boost::asio::write(server0_socket, boost::asio::buffer(&rewritten_addr, sizeof(size_t)));
            boost::asio::write(server1_socket, boost::asio::buffer(&rewritten_addr, sizeof(size_t)));
            level_ele_counter++;
        }

        access_bandwidth += eleNum_oneBins * sizeof(size_t) * 3;

        level_key_1 = get_level_key(L, 0, get_epoch(L, 0), 1);
        if (found_flag) {level_pos_1 = get_random_position(num_bins);}
        else {
            level_pos_1 = hash_function_addr(num_bins, level_key_1, req_addr);
        }
        boost::asio::write(server0_socket, boost::asio::buffer(&level_pos_1, sizeof(size_t)));
        boost::asio::write(server1_socket, boost::asio::buffer(&level_pos_1, sizeof(size_t)));

        access_bandwidth += sizeof(size_t) << 1;

        for (size_t k = 0; k < eleNum_oneBins; k++) {
            boost::asio::read(server0_socket, boost::asio::buffer(&tmp_accessed_addr, sizeof(size_t)));
            if (!found_flag && tmp_accessed_addr == req_addr) {
                dpf_loc = level_ele_counter;
                found_flag = true;
                rewritten_addr = base_dummy_addr + access_counter;
            }
            else {
                rewritten_addr = tmp_accessed_addr;
            }
            boost::asio::write(server0_socket, boost::asio::buffer(&rewritten_addr, sizeof(size_t)));
            boost::asio::write(server1_socket, boost::asio::buffer(&rewritten_addr, sizeof(size_t)));
            level_ele_counter++;
        }

        
        access_bandwidth += eleNum_oneBins * sizeof(size_t) * 3;

        auto keys_0 = DPF::Gen(dpf_loc, std::ceil(log2(level_ele_counter)));
        boost::asio::write(server0_socket, boost::asio::buffer((keys_0.first.data()), keys_0.first.size()));
        boost::asio::write(server1_socket, boost::asio::buffer((keys_0.second.data()), keys_0.second.size())); 

        boost::asio::read(server0_socket, boost::asio::buffer(ele_shared0.data(), element_size));
        boost::asio::read(server1_socket, boost::asio::buffer(ele_shared1.data(), element_size));

        access_bandwidth += (keys_0.first.size() << 1) + (element_size << 1);

        ele = convert_share_to_element(ele_shared0, ele_shared1);
        if (*reinterpret_cast<addr_type*>(ele.data()) == req_addr) {
            accessed_element = ele;
        }
    }

    /**
     * Third, write the ele back to the top level
     */
    assert(found_flag == true);
    element_type written_element = accessed_element;
    if (op == 'w') {
        written_element = convert_addrVal_to_element(std::make_pair(req_addr, write_value), element_size);
    }

    for (size_t i = 0; i < access_counter % lg_N; i++) {
        boost::asio::read(server0_socket, boost::asio::buffer(ele.data(), element_size));
        if (*reinterpret_cast<addr_type*>(ele.data()) == req_addr) {
            boost::asio::write(server0_socket, boost::asio::buffer(written_element.data(), element_size));
            boost::asio::write(server1_socket, boost::asio::buffer(written_element.data(), element_size));
            written_element = get_random_element(base_dummy_addr + access_counter, element_size);
        }
        else {
            boost::asio::write(server0_socket, boost::asio::buffer(ele.data(), element_size));
            boost::asio::write(server1_socket, boost::asio::buffer(ele.data(), element_size));
        }

        access_bandwidth += element_size + (element_size << 1);

    }
    boost::asio::write(server0_socket, boost::asio::buffer(written_element.data(), element_size));
    boost::asio::write(server1_socket, boost::asio::buffer(written_element.data(), element_size));

    access_bandwidth +=  element_size << 1;

    access_counter++;
    
    if (access_counter % static_cast<size_t>(pow(lg_N, L)) == 0) {
        std::cout << "REBUILD MAX LEVEL::  " << L << "\n";
        RebuildL();
    }
    else {
        for (size_t i = L - 1; i > 0; i--) {
            if (access_counter % static_cast<size_t>(pow(lg_N, i)) == 0) {
                size_t reb_bucketID = (static_cast<size_t>(access_counter / static_cast<size_t>(pow(lg_N, i))) % lg_N) - 1;
                // std::cout << "REBUILD LEVEL::  " << i  << " ;  REBUILD BucketID::  " << reb_bucketID << "\n";
                Rebuild(i, reb_bucketID);
                break;
            }
        }
    }

    return accessed_element;
}

void Client::Rebuild(size_t level, size_t bucketID) {
    // Parameters
    element_type ele(num_cell_per_element);
    size_t nonEmptyCounter = 0;
    for (size_t i = 0; i < lg_N; i++) {
        boost::asio::read(server0_socket, boost::asio::buffer(ele.data(), element_size));
        access_bandwidth +=  element_size;
        if (*reinterpret_cast<addr_type*>(ele.data()) < base_filler_addr) {
            boost::asio::write(server1_socket, boost::asio::buffer(ele.data(), element_size));
            access_bandwidth +=  element_size;
            nonEmptyCounter++;
        }
    }
    for (size_t i = 1; i < level; i++) {
        for (size_t j = 0; j < lg_N - 1; j++) {
            for (size_t k = 0; k < get_tableLen(i); k++) {
                for (size_t l = 0; l < eleNum_oneBins; l++) {
                    boost::asio::read(server0_socket, boost::asio::buffer(ele.data(), element_size));
                    access_bandwidth +=  element_size;
                    if (*reinterpret_cast<addr_type*>(ele.data()) < base_filler_addr) {
                        boost::asio::write(server1_socket, boost::asio::buffer(ele.data(), element_size));
                        access_bandwidth +=  element_size;
                        nonEmptyCounter++;
                    }
                    boost::asio::read(server0_socket, boost::asio::buffer(ele.data(), element_size));
                    access_bandwidth +=  element_size;
                    if (*reinterpret_cast<addr_type*>(ele.data()) < base_filler_addr) {
                        boost::asio::write(server1_socket, boost::asio::buffer(ele.data(), element_size));
                        access_bandwidth +=  element_size;
                        nonEmptyCounter++;
                    }
                }
            }
        }
    }
    assert(nonEmptyCounter == static_cast<size_t>(pow(lg_N, level)));

    key_type level_key_0 = get_level_key(level, bucketID, get_epoch(level, bucketID), 0);
    key_type level_key_1 = get_level_key(level, bucketID, get_epoch(level, bucketID), 1);
    size_t level_pos_0;
    size_t level_pos_1;
    size_t acc_addr;
    for (size_t i = 0 ;i < nonEmptyCounter; i++) {
        boost::asio::read(server1_socket, boost::asio::buffer(&acc_addr, sizeof(size_t)));
        level_pos_0 = hash_function_addr(get_tableLen(level), level_key_0, acc_addr);
        level_pos_1 = hash_function_addr(get_tableLen(level), level_key_1, acc_addr);
        boost::asio::write(server0_socket, boost::asio::buffer(&acc_addr, sizeof(size_t)));
        boost::asio::write(server0_socket, boost::asio::buffer(&level_pos_0, sizeof(size_t)));
        boost::asio::write(server0_socket, boost::asio::buffer(&level_pos_1, sizeof(size_t)));
    }
    access_bandwidth +=  nonEmptyCounter * (sizeof(size_t) << 2);
    /**
     * Second, oblivious hash, S0 has the addr, S1 has the ele
     */
    oblivious_hash(level, bucketID);
    for (size_t i = 1; i < level; i++) {
        full_flag[i - 1] = 0; 
    }
    set_bit(full_flag[level - 1], bucketID, 1);
}

void Client::RebuildL() {
    // Parameters
    element_type ele(num_cell_per_element);
    size_t nonEmptyCounter = 0;
    for (size_t i = 0; i < lg_N; i++) {
        boost::asio::read(server0_socket, boost::asio::buffer(ele.data(), element_size));
        access_bandwidth +=  element_size;
        if (*reinterpret_cast<addr_type*>(ele.data()) < base_dummy_addr) {
            boost::asio::write(server1_socket, boost::asio::buffer(ele.data(), element_size));
            access_bandwidth +=  element_size;
            nonEmptyCounter++;
        }
    }
    for (size_t i = 1; i < L; i++) {
        for (size_t j = 0; j < lg_N - 1; j++) {
            for (size_t k = 0; k < get_tableLen(i); k++) {
                for (size_t l = 0; l < eleNum_oneBins; l++) {
                    boost::asio::read(server0_socket, boost::asio::buffer(ele.data(), element_size));
                    access_bandwidth +=  element_size;
                    if (*reinterpret_cast<addr_type*>(ele.data()) < base_dummy_addr) {
                        boost::asio::write(server1_socket, boost::asio::buffer(ele.data(), element_size));
                        access_bandwidth +=  element_size;
                        nonEmptyCounter++;
                    }
                    boost::asio::read(server0_socket, boost::asio::buffer(ele.data(), element_size));
                    access_bandwidth +=  element_size;
                    if (*reinterpret_cast<addr_type*>(ele.data()) < base_dummy_addr) {
                        boost::asio::write(server1_socket, boost::asio::buffer(ele.data(), element_size));
                        access_bandwidth +=  element_size;
                        nonEmptyCounter++;
                    }
                }
            }
        }
    }
    for (size_t k = 0; k < get_tableLen(L); k++) {
        for (size_t l = 0; l < eleNum_oneBins; l++) {
            boost::asio::read(server0_socket, boost::asio::buffer(ele.data(), element_size));
            access_bandwidth +=  element_size;
            if (*reinterpret_cast<addr_type*>(ele.data()) < base_dummy_addr) {
                boost::asio::write(server1_socket, boost::asio::buffer(ele.data(), element_size));
                access_bandwidth +=  element_size;
                nonEmptyCounter++;
            }
            boost::asio::read(server0_socket, boost::asio::buffer(ele.data(), element_size));
            access_bandwidth +=  element_size;
            if (*reinterpret_cast<addr_type*>(ele.data()) < base_dummy_addr) {
                boost::asio::write(server1_socket, boost::asio::buffer(ele.data(), element_size));
                access_bandwidth +=  element_size;
                nonEmptyCounter++;
            }
        }
    }

    assert(nonEmptyCounter == N);

    key_type level_key_0 = get_level_key(L, 0, get_epoch(L, 0), 0);
    key_type level_key_1 = get_level_key(L, 0, get_epoch(L, 0), 1);
    size_t level_pos_0;
    size_t level_pos_1;
    size_t acc_addr;
    for (size_t i = 0 ;i < nonEmptyCounter; i++) {
        boost::asio::read(server1_socket, boost::asio::buffer(&acc_addr, sizeof(size_t)));
        level_pos_0 = hash_function_addr(get_tableLen(L), level_key_0, acc_addr);
        level_pos_1 = hash_function_addr(get_tableLen(L), level_key_1, acc_addr);
        boost::asio::write(server0_socket, boost::asio::buffer(&acc_addr, sizeof(size_t)));
        boost::asio::write(server0_socket, boost::asio::buffer(&level_pos_0, sizeof(size_t)));
        boost::asio::write(server0_socket, boost::asio::buffer(&level_pos_1, sizeof(size_t)));
    }
    access_bandwidth +=  nonEmptyCounter * (sizeof(size_t) << 2);
    /**
     * Second, oblivious hash, S0 has the addr, S1 has the ele
     */
    oblivious_hash(L, 0);
    for (size_t i = 1; i < L; i++) {
        full_flag[i - 1] = 0; 
    }
    full_flag[L - 1] = 1;
    maxL_epoch++;
}