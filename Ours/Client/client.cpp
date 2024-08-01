#include "client.h"

const std::pair<std::string, int> Server0_IPAndPort = {"127.0.0.1", 4000};
const std::pair<std::string, int> Server1_IPAndPort = {"127.0.0.1", 6000};

Client::Client(size_t N, size_t element_size, size_t tag_size): 
    N(N), element_size(element_size), tag_size(tag_size), 
    lg_N(static_cast<size_t>(log2(N))),cuckoo_alpha(lg_N*lg_N), cuckoo_epsilon(0),
    ell(std::min(static_cast<size_t>(log2(lg_N)) << 1, lg_N - 1)), L(lg_N),
    full_flag(0), len_S(1), len_B(1), access_counter(0),
    master_level_key(_mm_set_epi32(0x01234567, 0x89abcdef, 0xfedcba98, 0x76543210)),
    master_tag_key(_mm_set_epi32(0x89abcdef, 0xfedcba98, 0x01234567, 0x76543210)),
    aes_level(master_level_key),aes_tag(master_tag_key),
    ell_epoch(0),L_epoch(0),
    realEll_tableLen(static_cast<size_t>(std::ceil(((1 << (ell + 1)) + log2(N) * (L - ell)) * (1 + cuckoo_epsilon)))),
    baseEll_tableLen(static_cast<size_t>(std::ceil((1 << ell) * (1 + cuckoo_epsilon)))),
    setup_bandwidth(0), access_bandwidth(0),
    server0_socket(io_context), server1_socket(io_context)
{
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

size_t Client::get_epoch(size_t level) {
    if (level == ell) {return ell_epoch;}
    else if (level == L) {return L_epoch;}
    else {return std::max(static_cast<long long>(0), static_cast<long long>(std::floor((access_counter - (1 << level)) / (1 << (level + 1)))));} 
}

key_type Client::get_level_key(size_t level, size_t epoch, int h0Orh1) {
    key_type level_block = _mm_set_epi64x(static_cast<long long>(epoch), (static_cast<long long>(level) << 1) | static_cast<long long>(h0Orh1));
    key_type level_key = aes_level.encryptECB_MMO(level_block);
    return level_key;
}

tag_type Client::get_tag(addr_type vir_addr) {
    size_t num_cells = tag_size / sizeof(cell_type);
    tag_type tag(num_cells);
    for (size_t i = 0; i < (num_cells << 1); i += 2) {
        key_type addr_block1 = _mm_set_epi64x(static_cast<long long>(i), static_cast<long long>(vir_addr));
        key_type addr_block2 = _mm_set_epi64x(static_cast<long long>(i+1), static_cast<long long>(vir_addr));
        key_type tag_block1 = aes_tag.encryptECB_MMO(addr_block1);
        key_type tag_block2 = aes_tag.encryptECB_MMO(addr_block2);
        
        std::memcpy(reinterpret_cast<char*>(tag.data()) + i * 16, &tag_block1, sizeof(key_type));
        std::memcpy(reinterpret_cast<char*>(tag.data()) + (i + 1) * 16, &tag_block2, sizeof(key_type));
    }
    return tag;
}

void Client::insert_data(element_type& ele, size_t level) {
    size_t ell_epoch = get_epoch(ell);
    size_t level_epoch = get_epoch(level);
    key_type ell_key_0 = get_level_key(ell, ell_epoch, 0);
    key_type ell_key_1 = get_level_key(ell, ell_epoch, 1);
    key_type level_key_0 = get_level_key(level, level_epoch, 0);
    key_type level_key_1 = get_level_key(level, level_epoch, 1);

    tag_type tag = get_tag(*reinterpret_cast<addr_type*>(ele.data()));
    std::pair<tag_type, tag_type> shared_tag = convert_tag_to_share(tag);

    // Compute the tableLen and position 
    size_t ell_pos_0 = hash_function_tag(realEll_tableLen - 1, ell_key_0, tag) + 1;
    size_t ell_pos_1 = hash_function_tag(realEll_tableLen - 1, ell_key_1, tag) + 1;
    size_t level_pos_0 = hash_function_tag((baseEll_tableLen << (level - ell)) - 1, level_key_0, tag) + 1;
    size_t level_pos_1 = hash_function_tag((baseEll_tableLen << (level - ell)) - 1, level_key_1, tag) + 1;

    // Send each part separately to server_0
    boost::asio::write(server0_socket, boost::asio::buffer((ele.data()), element_size)); 
    boost::asio::write(server0_socket, boost::asio::buffer((shared_tag.first.data()), tag_size));
    boost::asio::write(server0_socket, boost::asio::buffer((&ell_pos_0), sizeof(size_t)));
    boost::asio::write(server0_socket, boost::asio::buffer((&ell_pos_1), sizeof(size_t)));
    boost::asio::write(server0_socket, boost::asio::buffer((&level_pos_0), sizeof(size_t)));
    boost::asio::write(server0_socket, boost::asio::buffer((&level_pos_1), sizeof(size_t)));

    // Send each part separately to server_1
    boost::asio::write(server1_socket, boost::asio::buffer((ele.data()), element_size)); 
    boost::asio::write(server1_socket, boost::asio::buffer((shared_tag.second.data()), tag_size));
    boost::asio::write(server1_socket, boost::asio::buffer((&ell_pos_0), sizeof(size_t)));
    boost::asio::write(server1_socket, boost::asio::buffer((&ell_pos_1), sizeof(size_t)));
    boost::asio::write(server1_socket, boost::asio::buffer((&level_pos_0), sizeof(size_t)));
    boost::asio::write(server1_socket, boost::asio::buffer((&level_pos_1), sizeof(size_t)));

    setup_bandwidth += (element_size + tag_size + (sizeof(size_t) << 2)) << 1;
}

void Client::Setup() {
    size_t num_cells = element_size / sizeof(cell_type);
    element_type ele(num_cells);
    // std::cout << "ell: " << ell << " ; L: " << L << "\n";
    /**
     * Setup: Insert random N elements to the server
     */
    for (size_t i = 0 ;i < N; i++) {
        ele = get_random_element(i + 1, element_size);
        // Insert data into the client and send to servers
        insert_data(ele, L);
    }

    boost::asio::read(server0_socket, boost::asio::buffer(&len_S, sizeof(size_t)));
    boost::asio::read(server0_socket, boost::asio::buffer(&full_flag, sizeof(size_t)));

}


element_type Client::Access(char op, addr_type req_addr, uint8_t* write_value) {
    // accessed element
    element_type accessed_element(element_size / sizeof(cell_type));
    bool found_flag = false;
    // size_t found_level = -10; // EB: -2; ES: -1; level: found_level
    tag_type req_tag = get_tag(req_addr);
    std::pair<tag_type, tag_type> shared_req_tag = convert_tag_to_share(req_tag);
    tag_type modified_tag = get_random_tag(tag_size);
    size_t pos_buffer = 0;
    size_t pos_stash = 0;
    // Send the modified_tag before the accessing
    boost::asio::write(server0_socket, boost::asio::buffer((modified_tag.data()), tag_size));
    boost::asio::write(server1_socket, boost::asio::buffer((modified_tag.data()), tag_size));

    access_bandwidth += tag_size << 1;

    /**
     * First, access the element buffer in reverse order and modify the tag
     */
    
    if (len_B > 1) {
        for (size_t i = len_B - 1; i > 0; i--) {
            // Extract element
            element_type tmp_ele(element_size / sizeof(cell_type));
            boost::asio::read(server0_socket, boost::asio::buffer(tmp_ele.data(), element_size));
            if (!found_flag && *reinterpret_cast<addr_type*>(tmp_ele.data()) == req_addr) {
                accessed_element = tmp_ele;
                found_flag = true;
                pos_buffer = i;
            }
        }
        auto keys_buffer = DPF::Gen(pos_buffer, std::ceil(log2(len_B)));
        boost::asio::write(server0_socket, boost::asio::buffer((keys_buffer.first.data()), keys_buffer.first.size()));
        boost::asio::write(server1_socket, boost::asio::buffer((keys_buffer.second.data()), keys_buffer.second.size()));
        
        access_bandwidth += (element_size * (len_B - 1)) + (keys_buffer.first.size() << 1); 
    }

    /**
     * Second, access the element stash in reverse order and modify the tag
     */
    if (len_S > 1) {
        for (size_t i = len_S - 1; i > 0; i--) {
            // Extract element
            element_type tmp_ele(element_size / sizeof(cell_type));
            boost::asio::read(server0_socket, boost::asio::buffer(tmp_ele.data(), element_size));
            if (!found_flag && *reinterpret_cast<addr_type*>(tmp_ele.data()) == req_addr) {
                accessed_element = tmp_ele;
                found_flag = true;
                pos_stash = i;
            }
        }
        auto keys_stash = DPF::Gen(pos_stash, std::ceil(log2(len_S)));
        boost::asio::write(server0_socket, boost::asio::buffer((keys_stash.first.data()), keys_stash.first.size())); 
        boost::asio::write(server1_socket, boost::asio::buffer((keys_stash.second.data()), keys_stash.second.size()));
        
        access_bandwidth += (element_size * (len_S - 1)) + (keys_stash.first.size() << 1); 
    }

    /**
     * Third, access each non-empty level and modify the tag
     */

    // Send the DPF keys of ell-th level to servers and modify the tag
    if (is_index_bit_1(full_flag, 0)) {
        // (1). PIR Read
        key_type ell_key_0 = get_level_key(ell, ell_epoch, 0);
        key_type ell_key_1 = get_level_key(ell, ell_epoch, 1);
        size_t ell_pos_0 = hash_function_tag(realEll_tableLen - 1, ell_key_0, req_tag) + 1;
        size_t ell_pos_1 = hash_function_tag(realEll_tableLen - 1, ell_key_1, req_tag) + 1;

        auto keys_0 = DPF::Gen(ell_pos_0, std::ceil(log2(realEll_tableLen)));
        auto keys_1 = DPF::Gen(ell_pos_1, std::ceil(log2(realEll_tableLen)));

        boost::asio::write(server0_socket, boost::asio::buffer((keys_0.first.data()), keys_0.first.size())); 
        boost::asio::write(server0_socket, boost::asio::buffer((keys_1.first.data()), keys_1.first.size())); 
        boost::asio::write(server1_socket, boost::asio::buffer((keys_0.second.data()), keys_0.second.size())); 
        boost::asio::write(server1_socket, boost::asio::buffer((keys_1.second.data()), keys_1.second.size()));
        // Extract element from table 0
        element_type tmp_ele(element_size / sizeof(cell_type));
        element_type tmp_ele_shared0(element_size / sizeof(cell_type));
        element_type tmp_ele_shared1(element_size / sizeof(cell_type));

        boost::asio::read(server0_socket, boost::asio::buffer(tmp_ele_shared0.data(), element_size));
        boost::asio::read(server1_socket, boost::asio::buffer(tmp_ele_shared1.data(), element_size));
        tmp_ele = convert_share_to_element(tmp_ele_shared0, tmp_ele_shared1);

        if (!found_flag && *reinterpret_cast<addr_type*>(tmp_ele.data()) == req_addr) {
            accessed_element = tmp_ele;
            found_flag = true;
        }   
        else {
            ell_pos_0 = 0;
        }

        // Extract element from table 1
        boost::asio::read(server0_socket, boost::asio::buffer(tmp_ele_shared0.data(), element_size));
        boost::asio::read(server1_socket, boost::asio::buffer(tmp_ele_shared1.data(), element_size));
        tmp_ele = convert_share_to_element(tmp_ele_shared0, tmp_ele_shared1);

        if (!found_flag && *reinterpret_cast<addr_type*>(tmp_ele.data()) == req_addr) {
            accessed_element = tmp_ele;
            found_flag = true;
        }   
        else {
            ell_pos_1 = 0;
        }
        
        // (2). PIR Write
        keys_0 = DPF::Gen(ell_pos_0, std::ceil(log2(realEll_tableLen)));
        keys_1 = DPF::Gen(ell_pos_1, std::ceil(log2(realEll_tableLen)));

        boost::asio::write(server0_socket, boost::asio::buffer((keys_0.first.data()), keys_0.first.size())); 
        boost::asio::write(server0_socket, boost::asio::buffer((keys_1.first.data()), keys_1.first.size())); 
        boost::asio::write(server1_socket, boost::asio::buffer((keys_0.second.data()), keys_0.second.size())); 
        boost::asio::write(server1_socket, boost::asio::buffer((keys_1.second.data()), keys_1.second.size()));
        
        access_bandwidth += (keys_0.first.size() << 3) + (element_size << 2); 
    }
    
    // Send the DPF keys of other levels to servers and modify the tag
    for (size_t i = ell + 1; i <= L; i++) {
        if (is_index_bit_1(full_flag, i - ell)) {
            // (1). PIR Read
            size_t level_epoch = get_epoch(i);
            key_type level_key_0 = get_level_key(i, level_epoch, 0);
            key_type level_key_1 = get_level_key(i, level_epoch, 1);
            // Send DPF keys
            size_t level_pos_0 = hash_function_tag((baseEll_tableLen << (i - ell)) - 1, level_key_0, req_tag) + 1;
            size_t level_pos_1 = hash_function_tag((baseEll_tableLen << (i - ell)) - 1, level_key_1, req_tag) + 1;
            auto keys_0 = DPF::Gen(level_pos_0, std::ceil(log2(baseEll_tableLen << (i - ell))));
            auto keys_1 = DPF::Gen(level_pos_1, std::ceil(log2(baseEll_tableLen << (i - ell))));
            boost::asio::write(server0_socket, boost::asio::buffer((keys_0.first.data()), keys_0.first.size())); 
            boost::asio::write(server0_socket, boost::asio::buffer((keys_1.first.data()), keys_1.first.size())); 
            boost::asio::write(server1_socket, boost::asio::buffer((keys_0.second.data()), keys_0.second.size())); 
            boost::asio::write(server1_socket, boost::asio::buffer((keys_1.second.data()), keys_1.second.size())); 
        
            // Extract element from table 0
            element_type tmp_ele(element_size / sizeof(cell_type));
            element_type tmp_ele_shared0(element_size / sizeof(cell_type));
            element_type tmp_ele_shared1(element_size / sizeof(cell_type));
            boost::asio::read(server0_socket, boost::asio::buffer(tmp_ele_shared0.data(), element_size));
            boost::asio::read(server1_socket, boost::asio::buffer(tmp_ele_shared1.data(), element_size));
            tmp_ele = convert_share_to_element(tmp_ele_shared0, tmp_ele_shared1);
            if (!found_flag && *reinterpret_cast<addr_type*>(tmp_ele.data()) == req_addr) {
                accessed_element = tmp_ele;
                found_flag = true;
            }   
            else {
                level_pos_0 = 0;
            }
            // Extract element from table 1
            boost::asio::read(server0_socket, boost::asio::buffer(tmp_ele_shared0.data(), element_size));
            boost::asio::read(server1_socket, boost::asio::buffer(tmp_ele_shared1.data(), element_size));
            tmp_ele = convert_share_to_element(tmp_ele_shared0, tmp_ele_shared1);
            if (!found_flag && *reinterpret_cast<addr_type*>(tmp_ele.data()) == req_addr) {
                accessed_element = tmp_ele;
                found_flag = true;
            }   
            else {
                level_pos_1 = 0;
            }

            // (2). PIR Write
            keys_0 = DPF::Gen(level_pos_0, std::ceil(log2(baseEll_tableLen << (i - ell))));
            keys_1 = DPF::Gen(level_pos_1, std::ceil(log2(baseEll_tableLen << (i - ell))));
            boost::asio::write(server0_socket, boost::asio::buffer((keys_0.first.data()), keys_0.first.size())); 
            boost::asio::write(server0_socket, boost::asio::buffer((keys_1.first.data()), keys_1.first.size())); 
            boost::asio::write(server1_socket, boost::asio::buffer((keys_0.second.data()), keys_0.second.size())); 
            boost::asio::write(server1_socket, boost::asio::buffer((keys_1.second.data()), keys_1.second.size()));
            
            access_bandwidth += (keys_0.first.size() << 3) + (element_size << 2); 
        }
    }

    /**
     * Fourth, write the element and tag to the buffer
     */
    assert(found_flag == true);
    element_type written_element = accessed_element;
    if (op == 'w') {
        written_element = convert_addrVal_to_element(std::make_pair(req_addr, write_value), element_size);
    }

    boost::asio::write(server0_socket, boost::asio::buffer((written_element.data()), element_size)); 
    boost::asio::write(server0_socket, boost::asio::buffer((shared_req_tag.first.data()), tag_size)); 
    boost::asio::write(server1_socket, boost::asio::buffer((written_element.data()), element_size)); 
    boost::asio::write(server1_socket, boost::asio::buffer((shared_req_tag.second.data()), tag_size));
    
    access_bandwidth += (element_size << 1) + (tag_size << 1); 

    len_B++;
    access_counter++;

    /**
     * Fifth, rebuild
     */
    if (access_counter % (1 << L) == 0) {
        RebuildL();
    }
    else if (access_counter % (1 << (ell + 1)) == 0) {
        for (size_t i = L; i > ell; i--) {
            if (access_counter % (1 << i) == 0) {
                RebuildLevel(i);
                break;
            }
        }
    }
    else if (len_B - 1 ==  lg_N) {
        RebuildEll();
    }
    return accessed_element;
}


void Client::RebuildEll() {
    /**
     * Parameters
     */
    element_type dummy_ele = get_random_element(MAX_SIZE_T, element_size);

    element_type glo_ele(element_size / sizeof(cell_type));
    tag_type glo_tag(tag_size / sizeof(cell_type));;
    tag_type glo_shared_tag_0(tag_size / sizeof(cell_type));
    tag_type glo_shared_tag_1(tag_size / sizeof(cell_type));
    addr_type glo_vir_addr;

    ell_epoch++;
    key_type ell_key_0 = get_level_key(ell, ell_epoch, 0);
    key_type ell_key_1 = get_level_key(ell, ell_epoch, 1);
    size_t ell_pos_0, ell_pos_1;

    /**
     * Receive buffer
     */
    for (size_t i = 1; i < len_B; i++) {
        // Read element and tag
        boost::asio::read(server0_socket, boost::asio::buffer(glo_ele.data(), element_size));
        boost::asio::read(server0_socket, boost::asio::buffer(glo_shared_tag_0.data(), tag_size));
        boost::asio::read(server1_socket, boost::asio::buffer(glo_shared_tag_1.data(), tag_size));

        glo_vir_addr = *reinterpret_cast<addr_type*>(glo_ele.data());
        glo_tag = convert_share_to_tag(glo_shared_tag_0, glo_shared_tag_1);

        if (glo_vir_addr != MAX_SIZE_T && areTagsEqual(glo_tag, get_tag(glo_vir_addr))) { //
            ell_pos_0 = hash_function_tag(realEll_tableLen - 1, ell_key_0, glo_tag) + 1;
            ell_pos_1 = hash_function_tag(realEll_tableLen - 1, ell_key_1, glo_tag) + 1;
        }    
        else {
            ell_pos_0 = get_random_position(realEll_tableLen - 1) + 1;
            ell_pos_1 = get_random_position(realEll_tableLen - 1) + 1;
            glo_ele = dummy_ele;
        }

        boost::asio::write(server0_socket, boost::asio::buffer((glo_ele.data()), element_size));
        boost::asio::write(server0_socket, boost::asio::buffer((&ell_pos_0), sizeof(size_t)));
        boost::asio::write(server0_socket, boost::asio::buffer((&ell_pos_1), sizeof(size_t)));
        boost::asio::write(server1_socket, boost::asio::buffer((glo_ele.data()), element_size));
        boost::asio::write(server1_socket, boost::asio::buffer((&ell_pos_0), sizeof(size_t)));
        boost::asio::write(server1_socket, boost::asio::buffer((&ell_pos_1), sizeof(size_t)));
        
        access_bandwidth += (element_size << 1) + element_size + (tag_size << 1) + (sizeof(size_t) << 2); 
    }   

    /**
     * Receive stash
     */
    for (size_t i = 1; i < len_S; i++) {
        // Read element and tag
        boost::asio::read(server0_socket, boost::asio::buffer(glo_ele.data(), element_size));
        boost::asio::read(server0_socket, boost::asio::buffer(glo_shared_tag_0.data(), tag_size));
        boost::asio::read(server1_socket, boost::asio::buffer(glo_shared_tag_1.data(), tag_size));
        glo_vir_addr = *reinterpret_cast<addr_type*>(glo_ele.data());
        glo_tag = convert_share_to_tag(glo_shared_tag_0, glo_shared_tag_1);
        
        if (glo_vir_addr != MAX_SIZE_T && areTagsEqual(glo_tag, get_tag(glo_vir_addr))) { //  
            ell_pos_0 = hash_function_tag(realEll_tableLen - 1, ell_key_0, glo_tag) + 1;
            ell_pos_1 = hash_function_tag(realEll_tableLen - 1, ell_key_1, glo_tag) + 1;
        }    
        else {
            ell_pos_0 = get_random_position(realEll_tableLen - 1) + 1;
            ell_pos_1 = get_random_position(realEll_tableLen - 1) + 1;
            glo_ele = dummy_ele;
        }
    
        boost::asio::write(server0_socket, boost::asio::buffer((glo_ele.data()), element_size));
        boost::asio::write(server0_socket, boost::asio::buffer((&ell_pos_0), sizeof(size_t)));
        boost::asio::write(server0_socket, boost::asio::buffer((&ell_pos_1), sizeof(size_t)));
        boost::asio::write(server1_socket, boost::asio::buffer((glo_ele.data()), element_size));
        boost::asio::write(server1_socket, boost::asio::buffer((&ell_pos_0), sizeof(size_t)));
        boost::asio::write(server1_socket, boost::asio::buffer((&ell_pos_1), sizeof(size_t)));
        
        access_bandwidth += (element_size << 1) + element_size + (tag_size << 1) + (sizeof(size_t) << 2); 
    }    

    /**
     * Recieve table
     */
    size_t len_posD0, len_posD1;
    boost::asio::read(server0_socket, boost::asio::buffer(&len_posD0, sizeof(size_t)));
    boost::asio::read(server0_socket, boost::asio::buffer(&len_posD1, sizeof(size_t)));
    
    access_bandwidth += sizeof(size_t) << 1; 

    for (size_t i = 0; i < len_posD0 + len_posD1; i++) {
        // Read element and tag
        boost::asio::read(server0_socket, boost::asio::buffer(glo_ele.data(), element_size));
        boost::asio::read(server0_socket, boost::asio::buffer(glo_shared_tag_0.data(), tag_size));
        boost::asio::read(server1_socket, boost::asio::buffer(glo_shared_tag_1.data(), tag_size));
        glo_vir_addr = *reinterpret_cast<addr_type*>(glo_ele.data());
        glo_tag = convert_share_to_tag(glo_shared_tag_0, glo_shared_tag_1);
        
        if (glo_vir_addr != MAX_SIZE_T && areTagsEqual(glo_tag, get_tag(glo_vir_addr))) { //  
            ell_pos_0 = hash_function_tag(realEll_tableLen - 1, ell_key_0, glo_tag) + 1;
            ell_pos_1 = hash_function_tag(realEll_tableLen - 1, ell_key_1, glo_tag) + 1;
        }    
        else {
            ell_pos_0 = get_random_position(realEll_tableLen - 1) + 1;
            ell_pos_1 = get_random_position(realEll_tableLen - 1) + 1;
            glo_ele = dummy_ele;
        }
    
        boost::asio::write(server0_socket, boost::asio::buffer((glo_ele.data()), element_size));
        boost::asio::write(server0_socket, boost::asio::buffer((&ell_pos_0), sizeof(size_t)));
        boost::asio::write(server0_socket, boost::asio::buffer((&ell_pos_1), sizeof(size_t)));
        boost::asio::write(server1_socket, boost::asio::buffer((glo_ele.data()), element_size));
        boost::asio::write(server1_socket, boost::asio::buffer((&ell_pos_0), sizeof(size_t)));
        boost::asio::write(server1_socket, boost::asio::buffer((&ell_pos_1), sizeof(size_t)));
         
        access_bandwidth += (element_size << 1) + element_size + (tag_size << 1) + (sizeof(size_t) << 2);
    }
    /**
     * Parameters
     */
    boost::asio::read(server0_socket, boost::asio::buffer(&len_S, sizeof(size_t)));
    len_B = 1;
    set_bit(full_flag, 0, 1);
    
    access_bandwidth += sizeof(size_t); 
}


void Client::RebuildLevel(size_t level) {
    /**
     * Parameters
     */
    element_type dummy_ele = get_random_element(MAX_SIZE_T, element_size);
    element_type glo_ele(element_size / sizeof(cell_type));
    tag_type glo_tag(tag_size / sizeof(cell_type));
    tag_type glo_shared_tag_0(tag_size / sizeof(cell_type));
    tag_type glo_shared_tag_1(tag_size / sizeof(cell_type));
    addr_type glo_vir_addr;

    ell_epoch++;
    key_type ell_key_0 = get_level_key(ell, ell_epoch, 0);
    key_type ell_key_1 = get_level_key(ell, ell_epoch, 1);
    size_t ell_pos_0, ell_pos_1;
    size_t level_epoch = get_epoch(level);
    key_type level_key_0 = get_level_key(level, level_epoch, 0);
    key_type level_key_1 = get_level_key(level, level_epoch, 1);
    size_t level_pos_0, level_pos_1;

    /**
     * Receive buffer
     */
    for (size_t i = 1; i < len_B; i++) {
        // Read element and tag
        boost::asio::read(server0_socket, boost::asio::buffer(glo_ele.data(), element_size));
        boost::asio::read(server0_socket, boost::asio::buffer(glo_shared_tag_0.data(), tag_size));
        boost::asio::read(server1_socket, boost::asio::buffer(glo_shared_tag_1.data(), tag_size));
        glo_vir_addr = *reinterpret_cast<addr_type*>(glo_ele.data());
        glo_tag = convert_share_to_tag(glo_shared_tag_0, glo_shared_tag_1);
        
        if (glo_vir_addr != MAX_SIZE_T && areTagsEqual(glo_tag, get_tag(glo_vir_addr))) {
            ell_pos_0 = hash_function_tag(realEll_tableLen - 1, ell_key_0, glo_tag) + 1;
            ell_pos_1 = hash_function_tag(realEll_tableLen - 1, ell_key_1, glo_tag) + 1;
            level_pos_0 = hash_function_tag((baseEll_tableLen << (level - ell)) - 1, level_key_0, glo_tag) + 1;
            level_pos_1 = hash_function_tag((baseEll_tableLen << (level - ell)) - 1, level_key_1, glo_tag) + 1; 
        }    
        else {
            ell_pos_0 = get_random_position(realEll_tableLen - 1) + 1;
            ell_pos_1 = get_random_position(realEll_tableLen - 1) + 1;
            level_pos_0 = get_random_position((baseEll_tableLen << (level - ell)) - 1) + 1;
            level_pos_1 = get_random_position((baseEll_tableLen << (level - ell)) - 1) + 1;
            glo_ele = dummy_ele;
        }
    
        boost::asio::write(server0_socket, boost::asio::buffer((glo_ele.data()), element_size));
        boost::asio::write(server0_socket, boost::asio::buffer((&ell_pos_0), sizeof(size_t)));
        boost::asio::write(server0_socket, boost::asio::buffer((&ell_pos_1), sizeof(size_t)));
        boost::asio::write(server0_socket, boost::asio::buffer((&level_pos_0), sizeof(size_t)));
        boost::asio::write(server0_socket, boost::asio::buffer((&level_pos_1), sizeof(size_t)));
        
        boost::asio::write(server1_socket, boost::asio::buffer((glo_ele.data()), element_size));
        boost::asio::write(server1_socket, boost::asio::buffer((&ell_pos_0), sizeof(size_t)));
        boost::asio::write(server1_socket, boost::asio::buffer((&ell_pos_1), sizeof(size_t)));
        boost::asio::write(server1_socket, boost::asio::buffer((&level_pos_0), sizeof(size_t)));
        boost::asio::write(server1_socket, boost::asio::buffer((&level_pos_1), sizeof(size_t)));
        
        access_bandwidth += (element_size << 1) + element_size + (tag_size << 1) + (sizeof(size_t) << 3);
    }   

    /**
     * Receive stash
     */
    for (size_t i = 1; i < len_S; i++) {
        // Read element and tag
        boost::asio::read(server0_socket, boost::asio::buffer(glo_ele.data(), element_size));
        boost::asio::read(server0_socket, boost::asio::buffer(glo_shared_tag_0.data(), tag_size));
        boost::asio::read(server1_socket, boost::asio::buffer(glo_shared_tag_1.data(), tag_size));
        glo_vir_addr = *reinterpret_cast<addr_type*>(glo_ele.data());
        glo_tag = convert_share_to_tag(glo_shared_tag_0, glo_shared_tag_1);
        if (glo_vir_addr != MAX_SIZE_T && areTagsEqual(glo_tag, get_tag(glo_vir_addr))) {
            ell_pos_0 = hash_function_tag(realEll_tableLen - 1, ell_key_0, glo_tag) + 1;
            ell_pos_1 = hash_function_tag(realEll_tableLen - 1, ell_key_1, glo_tag) + 1;
            level_pos_0 = hash_function_tag((baseEll_tableLen << (level - ell)) - 1, level_key_0, glo_tag) + 1;
            level_pos_1 = hash_function_tag((baseEll_tableLen << (level - ell)) - 1, level_key_1, glo_tag) + 1; 
        }    
        else {
            ell_pos_0 = get_random_position(realEll_tableLen - 1) + 1;
            ell_pos_1 = get_random_position(realEll_tableLen - 1) + 1;
            level_pos_0 = get_random_position((baseEll_tableLen << (level - ell)) - 1) + 1;
            level_pos_1 = get_random_position((baseEll_tableLen << (level - ell)) - 1) + 1;
            glo_ele = dummy_ele;
        }
    
        boost::asio::write(server0_socket, boost::asio::buffer((glo_ele.data()), element_size));
        boost::asio::write(server0_socket, boost::asio::buffer((&ell_pos_0), sizeof(size_t)));
        boost::asio::write(server0_socket, boost::asio::buffer((&ell_pos_1), sizeof(size_t)));
        boost::asio::write(server0_socket, boost::asio::buffer((&level_pos_0), sizeof(size_t)));
        boost::asio::write(server0_socket, boost::asio::buffer((&level_pos_1), sizeof(size_t)));
        
        boost::asio::write(server1_socket, boost::asio::buffer((glo_ele.data()), element_size));
        boost::asio::write(server1_socket, boost::asio::buffer((&ell_pos_0), sizeof(size_t)));
        boost::asio::write(server1_socket, boost::asio::buffer((&ell_pos_1), sizeof(size_t)));
        boost::asio::write(server1_socket, boost::asio::buffer((&level_pos_0), sizeof(size_t)));
        boost::asio::write(server1_socket, boost::asio::buffer((&level_pos_1), sizeof(size_t)));
        
        access_bandwidth += (element_size << 1) + element_size + (tag_size << 1) + (sizeof(size_t) << 3);
    }    

    /**
     * Recieve table
     */
    size_t len_posD0, len_posD1;
    for (size_t cur_level = ell; cur_level < level; cur_level++) {
        boost::asio::read(server0_socket, boost::asio::buffer(&len_posD0, sizeof(size_t)));
        boost::asio::read(server0_socket, boost::asio::buffer(&len_posD1, sizeof(size_t)));

        access_bandwidth += sizeof(size_t) << 1;

        for (size_t i = 0; i < len_posD0 + len_posD1; i++) {
            // Read element and tag
            boost::asio::read(server0_socket, boost::asio::buffer(glo_ele.data(), element_size));
            boost::asio::read(server0_socket, boost::asio::buffer(glo_shared_tag_0.data(), tag_size));
            boost::asio::read(server1_socket, boost::asio::buffer(glo_shared_tag_1.data(), tag_size));
            glo_vir_addr = *reinterpret_cast<addr_type*>(glo_ele.data());
            glo_tag = convert_share_to_tag(glo_shared_tag_0, glo_shared_tag_1);

            assert(glo_vir_addr != 0);

            if (glo_vir_addr != MAX_SIZE_T && areTagsEqual(glo_tag, get_tag(glo_vir_addr))) {
                ell_pos_0 = hash_function_tag(realEll_tableLen - 1, ell_key_0, glo_tag) + 1;
                ell_pos_1 = hash_function_tag(realEll_tableLen - 1, ell_key_1, glo_tag) + 1;
                level_pos_0 = hash_function_tag((baseEll_tableLen << (level - ell)) - 1, level_key_0, glo_tag) + 1;
                level_pos_1 = hash_function_tag((baseEll_tableLen << (level - ell)) - 1, level_key_1, glo_tag) + 1; 
            }    
            else {
                ell_pos_0 = get_random_position(realEll_tableLen - 1) + 1;
                ell_pos_1 = get_random_position(realEll_tableLen - 1) + 1;
                level_pos_0 = get_random_position((baseEll_tableLen << (level - ell)) - 1) + 1;
                level_pos_1 = get_random_position((baseEll_tableLen << (level - ell)) - 1) + 1;
                glo_ele = dummy_ele;
            }
        
            boost::asio::write(server0_socket, boost::asio::buffer((glo_ele.data()), element_size));
            boost::asio::write(server0_socket, boost::asio::buffer((&ell_pos_0), sizeof(size_t)));
            boost::asio::write(server0_socket, boost::asio::buffer((&ell_pos_1), sizeof(size_t)));
            boost::asio::write(server0_socket, boost::asio::buffer((&level_pos_0), sizeof(size_t)));
            boost::asio::write(server0_socket, boost::asio::buffer((&level_pos_1), sizeof(size_t)));
            
            boost::asio::write(server1_socket, boost::asio::buffer((glo_ele.data()), element_size));
            boost::asio::write(server1_socket, boost::asio::buffer((&ell_pos_0), sizeof(size_t)));
            boost::asio::write(server1_socket, boost::asio::buffer((&ell_pos_1), sizeof(size_t)));
            boost::asio::write(server1_socket, boost::asio::buffer((&level_pos_0), sizeof(size_t)));
            boost::asio::write(server1_socket, boost::asio::buffer((&level_pos_1), sizeof(size_t)));
            
            access_bandwidth += (element_size << 1) + element_size + (tag_size << 1) + (sizeof(size_t) << 3);
        }
    }

    /**
     * Parameters
     */
    boost::asio::read(server0_socket, boost::asio::buffer(&len_S, sizeof(size_t)));
    boost::asio::read(server0_socket, boost::asio::buffer(&full_flag, sizeof(size_t)));
    len_B = 1;
    
    access_bandwidth += sizeof(size_t) << 1;
}


void Client::RebuildL() {
    // Parameters
    element_type dummy_ele = get_random_element(MAX_SIZE_T, element_size);
    element_type glo_ele(element_size / sizeof(cell_type));
    tag_type glo_shared_tag_0(tag_size / sizeof(cell_type));
    tag_type glo_shared_tag_1(tag_size / sizeof(cell_type));
    tag_type glo_tag(tag_size / sizeof(cell_type));;
    std::pair<tag_type, tag_type> shared_pair_tag;
    addr_type glo_vir_addr;
    for (size_t i = 0; i < L - ell; i++){
        set_bit(full_flag, i, 0);
    }

    ell_epoch++;
    key_type ell_key_0 = get_level_key(ell, ell_epoch, 0);
    key_type ell_key_1 = get_level_key(ell, ell_epoch, 1);
    size_t ell_pos_0, ell_pos_1;
    L_epoch++;
    key_type L_key_0 = get_level_key(L, L_epoch, 0);
    key_type L_key_1 = get_level_key(L, L_epoch, 1);
    size_t L_pos_0, L_pos_1;

    size_t all_ele_num = 0;

    /**
     * Step 1: Receive element from server_0&1
     */
    // Receive buffer
    for (size_t i = 1; i < len_B; i++) {
        // Read element and tag
        all_ele_num++;
        boost::asio::read(server0_socket, boost::asio::buffer(glo_ele.data(), element_size));
        boost::asio::read(server0_socket, boost::asio::buffer(glo_shared_tag_0.data(), tag_size));
        boost::asio::read(server1_socket, boost::asio::buffer(glo_shared_tag_1.data(), tag_size));
        glo_vir_addr = *reinterpret_cast<addr_type*>(glo_ele.data());
        glo_tag = convert_share_to_tag(glo_shared_tag_0, glo_shared_tag_1);
        if (glo_vir_addr == MAX_SIZE_T || !areTagsEqual(glo_tag, get_tag(glo_vir_addr))) {glo_ele = dummy_ele;}
        boost::asio::write(server0_socket, boost::asio::buffer((glo_ele.data()), element_size));

        access_bandwidth += (element_size << 1) + (tag_size << 1);
    }   

    
    // Receive stash
    for (size_t i = 1; i < len_S; i++) {
        // Read element and tag
        all_ele_num++;
        boost::asio::read(server0_socket, boost::asio::buffer(glo_ele.data(), element_size));
        boost::asio::read(server0_socket, boost::asio::buffer(glo_shared_tag_0.data(), tag_size));
        boost::asio::read(server1_socket, boost::asio::buffer(glo_shared_tag_1.data(), tag_size));
        glo_vir_addr = *reinterpret_cast<addr_type*>(glo_ele.data());
        glo_tag = convert_share_to_tag(glo_shared_tag_0, glo_shared_tag_1);
        if (glo_vir_addr == MAX_SIZE_T || !areTagsEqual(glo_tag, get_tag(glo_vir_addr))) {glo_ele = dummy_ele;}
        boost::asio::write(server0_socket, boost::asio::buffer((glo_ele.data()), element_size));

        access_bandwidth += (element_size << 1) + (tag_size << 1);
    }   
    
    // Receive table
    size_t len_posD0, len_posD1;
    for (size_t cur_level = ell; cur_level <= L; cur_level++) {
        boost::asio::read(server0_socket, boost::asio::buffer(&len_posD0, sizeof(size_t)));
        boost::asio::read(server0_socket, boost::asio::buffer(&len_posD1, sizeof(size_t)));
        
        access_bandwidth += sizeof(size_t) << 1;

        for (size_t i = 0; i < len_posD0 + len_posD1; i++) {
            // Read element and tag
            all_ele_num++;
            boost::asio::read(server0_socket, boost::asio::buffer(glo_ele.data(), element_size));
            boost::asio::read(server0_socket, boost::asio::buffer(glo_shared_tag_0.data(), tag_size));
            boost::asio::read(server1_socket, boost::asio::buffer(glo_shared_tag_1.data(), tag_size));
            glo_vir_addr = *reinterpret_cast<addr_type*>(glo_ele.data());
            glo_tag = convert_share_to_tag(glo_shared_tag_0, glo_shared_tag_1);
            
            if (glo_vir_addr == MAX_SIZE_T || !areTagsEqual(glo_tag, get_tag(glo_vir_addr))) {glo_ele = dummy_ele;}
            boost::asio::write(server0_socket, boost::asio::buffer((glo_ele.data()), element_size));
            
            access_bandwidth += (element_size << 1) + (tag_size << 1);
        }
    }
    

    /**
     * Step 2: Receive shuffled elements from server_0, send non-dummy elements to server_1
     */
    for (size_t i = 0; i < all_ele_num; i++) {
        boost::asio::read(server0_socket, boost::asio::buffer(glo_ele.data(), element_size));
        if (*reinterpret_cast<addr_type*>(glo_ele.data()) != MAX_SIZE_T) {
            boost::asio::write(server1_socket, boost::asio::buffer((glo_ele.data()), element_size));
        }
    }
    
    access_bandwidth += element_size * (all_ele_num + N);

    /**
     * Step 3: Receive elements from server_1, send position to server_0&1
     */
    for (size_t i = 0; i < N; i++) {
        boost::asio::read(server1_socket, boost::asio::buffer(glo_ele.data(), element_size));
        glo_tag = get_tag(*reinterpret_cast<addr_type*>(glo_ele.data()));
        shared_pair_tag = convert_tag_to_share(glo_tag);
        ell_pos_0 = hash_function_tag(realEll_tableLen - 1, ell_key_0, glo_tag) + 1;
        ell_pos_1 = hash_function_tag(realEll_tableLen - 1, ell_key_1, glo_tag) + 1;
        L_pos_0 = hash_function_tag((baseEll_tableLen << (L - ell)) - 1, L_key_0, glo_tag) + 1;
        L_pos_1 = hash_function_tag((baseEll_tableLen << (L - ell)) - 1, L_key_1, glo_tag) + 1;

        boost::asio::write(server0_socket, boost::asio::buffer((glo_ele.data()), element_size));
        boost::asio::write(server0_socket, boost::asio::buffer((shared_pair_tag.first.data()), tag_size));
        boost::asio::write(server0_socket, boost::asio::buffer((&ell_pos_0), sizeof(size_t)));
        boost::asio::write(server0_socket, boost::asio::buffer((&ell_pos_1), sizeof(size_t)));
        boost::asio::write(server0_socket, boost::asio::buffer((&L_pos_0), sizeof(size_t)));
        boost::asio::write(server0_socket, boost::asio::buffer((&L_pos_1), sizeof(size_t)));
        
        boost::asio::write(server1_socket, boost::asio::buffer((shared_pair_tag.second.data()), tag_size));
        boost::asio::write(server1_socket, boost::asio::buffer((&ell_pos_0), sizeof(size_t)));
        boost::asio::write(server1_socket, boost::asio::buffer((&ell_pos_1), sizeof(size_t)));
        boost::asio::write(server1_socket, boost::asio::buffer((&L_pos_0), sizeof(size_t)));
        boost::asio::write(server1_socket, boost::asio::buffer((&L_pos_1), sizeof(size_t)));

        
        access_bandwidth += (element_size << 1) + (tag_size << 1) + (sizeof(size_t) << 3); 
    }


    /**
     * Step 4: Parameters
     */
    boost::asio::read(server0_socket, boost::asio::buffer(&len_S, sizeof(size_t)));
    boost::asio::read(server0_socket, boost::asio::buffer(&full_flag, sizeof(size_t)));
    len_B = 1;

    access_bandwidth += sizeof(size_t) << 1;
}