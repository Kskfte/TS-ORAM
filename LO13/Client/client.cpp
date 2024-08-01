#include "client.h"

const std::pair<std::string, int> Server0_IPAndPort = {"127.0.0.1", 5000};
const std::pair<std::string, int> Server1_IPAndPort = {"127.0.0.1", 5001};

Client::Client(size_t N, size_t element_size, size_t tag_size): 
    N(N), element_size(element_size), tag_size(tag_size), 
    num_cell_per_element(static_cast<size_t>(element_size / sizeof(cell_type))),
    num_cell_per_tag(static_cast<size_t>(tag_size / sizeof(cell_type))),
    lg_N(static_cast<size_t>(log2(N))),cuckoo_alpha(lg_N*lg_N), cuckoo_epsilon(0),
    parametr_c(lg_N << 1), max_level(1 + static_cast<size_t>(std::ceil(log2(N/(parametr_c))))),
    ell_cuckoo(std::min(static_cast<size_t>(log2(lg_N)) << 1, max_level + 1)), access_counter(0), dummy_counter(0), full_flag(0), 
    standard_BC(static_cast<size_t>(3*lg_N / log2(lg_N))), maxL_epoch(0), 
    written_stash(0), len_written_stash(0),
    master_level_key(_mm_set_epi32(0x01234567, 0x89abcdef, 0xfedcba98, 0x76543210)),
    master_tag_key(_mm_set_epi32(0x89abcdef, 0xfedcba98, 0x01234567, 0x76543210)),
    aes_level(master_level_key),aes_tag(master_tag_key),
    setup_bandwidth(0), access_bandwidth(0),
    server0_socket(io_context), server1_socket(io_context)
{
    connect_to_server(server0_socket, Server0_IPAndPort.first, Server0_IPAndPort.second);
    connect_to_server(server1_socket, Server1_IPAndPort.first, Server1_IPAndPort.second);
    sockets_list.push_back(&server0_socket);
    sockets_list.push_back(&server1_socket);
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
    if (level == max_level) {
        return maxL_epoch;
    }
    else {    
        size_t first_rebuild = lg_N * (1 << (level - 2));
        return std::floor((access_counter - first_rebuild) / (first_rebuild << 1));
    }
}

key_type Client::get_level_key(size_t level, size_t epoch, int h0Orh1) {
    key_type level_block = _mm_set_epi64x(static_cast<long long>(epoch), (static_cast<long long>(level) << 1) | static_cast<long long>(h0Orh1));
    key_type level_key = aes_level.encryptECB_MMO(level_block);
    return level_key;
}

tag_type Client::get_tag(size_t level, size_t epoch, addr_type vir_addr) {
    tag_type tag(num_cell_per_tag);
    for (size_t i = 0; i < (num_cell_per_tag << 1); i += 2) {
        key_type addr_block1 = _mm_set_epi32(static_cast<int>(vir_addr), static_cast<int>(i) + static_cast<int>(vir_addr), static_cast<int>(level) + static_cast<int>(vir_addr), static_cast<int>(epoch) + static_cast<int>(vir_addr));
        key_type addr_block2 = _mm_set_epi32(static_cast<int>(vir_addr), static_cast<int>(i+1) + static_cast<int>(vir_addr), static_cast<int>(level) + static_cast<int>(vir_addr), static_cast<int>(epoch) + static_cast<int>(vir_addr));
        key_type tag_block1 = aes_tag.encryptECB_MMO(addr_block1);
        key_type tag_block2 = aes_tag.encryptECB_MMO(addr_block2);
        std::memcpy(reinterpret_cast<char*>(tag.data()) + i * 16, &tag_block1, sizeof(key_type));
        std::memcpy(reinterpret_cast<char*>(tag.data()) + (i + 1) * 16, &tag_block2, sizeof(key_type));
    }
    return tag;
}

size_t Client::get_level_ele_num(size_t level) {
    if (level < ell_cuckoo) {
        return parametr_c * (1 << (level - 1)) * standard_BC;
    }
    else {
        return static_cast<size_t>(parametr_c * (1 << level) * (1 + cuckoo_epsilon));
    }
}

void Client::Setup() {
    std::cout << "ell_cuckoo: " << ell_cuckoo << " ; max_level: " << max_level << "\n";
    element_type ele(num_cell_per_element);
    tag_type tag(num_cell_per_tag);
    /**
     * First send hash keys
     */
    if (ell_cuckoo >= max_level + 1) {
        key_type level_key_0 = get_level_key(max_level, maxL_epoch, 0);
        boost::asio::write(*sockets_list[(max_level + 1) & 1], boost::asio::buffer((&level_key_0), sizeof(key_type)));
        setup_bandwidth += sizeof(key_type);
    }
    else {
        key_type level_key_0 = get_level_key(max_level, maxL_epoch, 0);
        key_type level_key_1 = get_level_key(max_level, maxL_epoch, 1);
        boost::asio::write(*sockets_list[(max_level + 1) & 1], boost::asio::buffer((&level_key_0), sizeof(key_type)));
        boost::asio::write(*sockets_list[(max_level + 1) & 1], boost::asio::buffer((&level_key_1), sizeof(key_type)));
        setup_bandwidth += sizeof(key_type) << 1;
    }
    /**
     * Second, send the elements and tag to one of the server
     */
    for (size_t i = 0 ;i < N; i++) {
        ele = get_random_element(i + 1, element_size);
        tag = get_tag(max_level, maxL_epoch, i + 1);
        boost::asio::write(*sockets_list[(max_level + 1) & 1], boost::asio::buffer((ele.data()), element_size));
        boost::asio::write(*sockets_list[(max_level + 1) & 1], boost::asio::buffer((tag.data()), tag_size));        
    }
    setup_bandwidth += N * (element_size + tag_size);
    /**
     * Third, receive the stash and table, and send the elements to another table
     */
    size_t ele_num_in_stash = 0;
    for (size_t i = 0; i < lg_N; i++) {
        boost::asio::read(*sockets_list[(max_level + 1) & 1], boost::asio::buffer(ele.data(), element_size));
        if (*reinterpret_cast<addr_type*>(ele.data()) != 0) {ele_num_in_stash++;}
        else {
            ele = get_random_element(MAX_SIZE_T - dummy_counter, element_size);
            dummy_counter++;
        }
        boost::asio::write(*sockets_list[max_level & 1], boost::asio::buffer((ele.data()), element_size));  
    }
    setup_bandwidth += lg_N * (element_size << 1);
    for (size_t i = 0; i < get_level_ele_num(max_level); i++) {
        boost::asio::read(*sockets_list[(max_level + 1) & 1], boost::asio::buffer(ele.data(), element_size));
        if (ele_num_in_stash > 0 && *reinterpret_cast<addr_type*>(ele.data()) == 0) {
            ele = get_random_element(MAX_SIZE_T - dummy_counter, element_size);
            dummy_counter++;
            ele_num_in_stash--;
        }
        boost::asio::write(*sockets_list[max_level & 1], boost::asio::buffer((ele.data()), element_size));
    }
    setup_bandwidth += get_level_ele_num(max_level) * (element_size << 1);
    /**
     * Fourth, set full_flag
     */
    for (size_t i = 2; i < max_level; i++) {set_bit(full_flag, i, 0);}
    set_bit(full_flag, max_level, 1);
    written_stash = (max_level + 1) & 1;
    len_written_stash = 0;   
}

element_type Client::Access(char op, addr_type req_addr, uint8_t* write_value) {
    // accessed element
    bool found_flag = false;
    tag_type tag(num_cell_per_tag);
    element_type accessed_element(num_cell_per_element);
    element_type ele(num_cell_per_element);

    /**
     * First, access the Stash
     */
    
    for (size_t i = 0; i < len_written_stash; i++) {
        boost::asio::read(*sockets_list[written_stash], boost::asio::buffer(ele.data(), element_size));
        if (!found_flag && *reinterpret_cast<addr_type*>(ele.data()) == req_addr) {
            accessed_element = ele;
            found_flag = true;
        }
    }
    access_bandwidth += len_written_stash * element_size;
    for (size_t i = 0; i < lg_N; i++) {
        boost::asio::read(*sockets_list[written_stash ^ 1], boost::asio::buffer(ele.data(), element_size));
        if (!found_flag && *reinterpret_cast<addr_type*>(ele.data()) == req_addr) {
            accessed_element = ele;
            found_flag = true;
        }
    }
    access_bandwidth += lg_N * element_size;
    /**
     * Second, access the cuckoo level
     */
    for (size_t i = 2; i < ell_cuckoo; i++) {
        if (is_index_bit_1(full_flag, i)) {
            if (found_flag) {tag = get_tag(i, get_epoch(i), get_random_index(MAX_SIZE_T));}
            else {tag = get_tag(i, get_epoch(i), req_addr);}
            key_type level_key_0 = get_level_key(i, get_epoch(i), 0);
            boost::asio::write(*sockets_list[i & 1], boost::asio::buffer((&level_key_0), sizeof(key_type)));
            boost::asio::write(*sockets_list[i & 1], boost::asio::buffer((tag.data()), tag_size));
            
            access_bandwidth += sizeof(key_type) + tag_size;

            for (size_t j = 0; j < standard_BC; j++) {
                boost::asio::read(*sockets_list[i & 1], boost::asio::buffer(ele.data(), element_size));
                if (!found_flag && *reinterpret_cast<addr_type*>(ele.data()) == req_addr) {
                    accessed_element = ele;
                    found_flag = true;
                    ele = get_random_element(MAX_SIZE_T - dummy_counter, element_size);
                    dummy_counter++;
                    boost::asio::write(*sockets_list[i & 1], boost::asio::buffer((ele.data()), element_size));
                }
                else {
                    boost::asio::write(*sockets_list[i & 1], boost::asio::buffer((ele.data()), element_size));
                }
            }
            access_bandwidth += standard_BC * (element_size << 1);
        }
    }
    /**
     * Third, access the cuckoo level
     */
    for (size_t i = ell_cuckoo; i < max_level + 1; i++) {
        if (is_index_bit_1(full_flag, i)) {
            if (found_flag) {tag = get_tag(i, get_epoch(i), get_random_index(MAX_SIZE_T));}
            else {tag = get_tag(i, get_epoch(i), req_addr);}
            key_type level_key_0 = get_level_key(i, get_epoch(i), 0);
            key_type level_key_1 = get_level_key(i, get_epoch(i), 1);
            boost::asio::write(*sockets_list[i & 1], boost::asio::buffer((&level_key_0), sizeof(key_type)));
            boost::asio::write(*sockets_list[i & 1], boost::asio::buffer((&level_key_1), sizeof(key_type)));
            boost::asio::write(*sockets_list[i & 1], boost::asio::buffer((tag.data()), tag_size));

            access_bandwidth += (sizeof(key_type) << 1) + tag_size;

            for (size_t j = 0; j < 2; j++) {
                boost::asio::read(*sockets_list[i & 1], boost::asio::buffer(ele.data(), element_size));
                if (!found_flag && *reinterpret_cast<addr_type*>(ele.data()) == req_addr) {
                    accessed_element = ele;
                    found_flag = true;
                    ele = get_random_element(MAX_SIZE_T - dummy_counter, element_size);
                    dummy_counter++;
                    boost::asio::write(*sockets_list[i & 1], boost::asio::buffer((ele.data()), element_size));
                }
                else {
                    boost::asio::write(*sockets_list[i & 1], boost::asio::buffer((ele.data()), element_size));
                }
            }
            access_bandwidth += 2 * (element_size << 1);
        }
    }

    /**
     * Fourth, read top again and write to it
     */
    assert(found_flag == true);
    element_type written_element = accessed_element;
    if (op == 'w') {
        written_element = convert_addrVal_to_element(std::make_pair(req_addr, write_value), element_size);
    }
    bool overwritten_flag = false;
    for (size_t i = 0; i < len_written_stash; i++) {
        boost::asio::read(*sockets_list[written_stash], boost::asio::buffer(ele.data(), element_size));
        if (*reinterpret_cast<addr_type*>(ele.data()) == req_addr) {
            boost::asio::write(*sockets_list[written_stash], boost::asio::buffer((written_element.data()), element_size));            
            overwritten_flag = true;
        }
        else {
            boost::asio::write(*sockets_list[written_stash], boost::asio::buffer((ele.data()), element_size));            
        }
    }
    access_bandwidth += len_written_stash * (element_size << 1);
    for (size_t i = 0; i < lg_N; i++) {
        boost::asio::read(*sockets_list[written_stash ^ 1], boost::asio::buffer(ele.data(), element_size));
        if (*reinterpret_cast<addr_type*>(ele.data()) == req_addr) {
            boost::asio::write(*sockets_list[written_stash ^ 1], boost::asio::buffer((written_element.data()), element_size));            
            overwritten_flag = true;
        }
        else {
            boost::asio::write(*sockets_list[written_stash ^ 1], boost::asio::buffer((ele.data()), element_size));            
        }
    }
    access_bandwidth += lg_N * (element_size << 1);
    if (!overwritten_flag) {
        boost::asio::write(*sockets_list[written_stash], boost::asio::buffer((written_element.data()), element_size));     
    }
    else {
        ele = get_random_element(MAX_SIZE_T - dummy_counter, element_size);
        dummy_counter++;
        boost::asio::write(*sockets_list[written_stash], boost::asio::buffer((ele.data()), element_size));      
    }
    access_bandwidth += 1;
    access_counter++;
    len_written_stash++;

    /**
     * Fifth, rebuild
     */
    if (access_counter % lg_N == 0) {
        bool tmp_reb = false;
        for (size_t i = 2; i < max_level; i++) {
            if (!is_index_bit_1(full_flag, i)) {
                Rebuild(i);
                tmp_reb = true;
                break;
            }
        }
        if (!tmp_reb) {
            std::cout << "RebuildMaxLevel: " << max_level << "\n";
            RebuildMaxLevel();
        }
    }
    return accessed_element;
}

void Client::Rebuild(size_t level) {
    element_type ele(num_cell_per_element);
    element_type tag(num_cell_per_tag);
    size_t level_tabLen;
    /**
     * First, receive standard table from Sa, and send to Sb
     */
    for (size_t i = 0; i < lg_N; i++) {
        boost::asio::read(*sockets_list[(level + 1) & 1], boost::asio::buffer(ele.data(), element_size));
        assert(*reinterpret_cast<addr_type*>(ele.data()) != 0);
        boost::asio::write(*sockets_list[level & 1], boost::asio::buffer((ele.data()), element_size));
    }
    access_bandwidth += lg_N * (element_size << 1);
    size_t standard_level = (ell_cuckoo < level + 1) * ell_cuckoo + (ell_cuckoo >= level + 1) * level;
    for (size_t i = 2 + ((level + 1) & 1); i < standard_level; i += 2) {
        level_tabLen = parametr_c * (1 << (i - 1));
        for (size_t j = 0; j < level_tabLen * standard_BC; j++) {
            boost::asio::read(*sockets_list[(level + 1) & 1], boost::asio::buffer(ele.data(), element_size));
            boost::asio::write(*sockets_list[level & 1], boost::asio::buffer((ele.data()), element_size));
        }
        access_bandwidth += level_tabLen * standard_BC * (element_size << 1);
    }
    size_t cuckoo_start_level = ell_cuckoo + (ell_cuckoo & level & 1) + ((ell_cuckoo + 1) & (level + 1) & 1);
    for (size_t i = cuckoo_start_level; i < level; i += 2) {
        level_tabLen = static_cast<size_t>(parametr_c * (1 << (i - 1)) * (1 + cuckoo_epsilon));
        for (size_t j = 0; j < (level_tabLen << 1); j++) {
            boost::asio::read(*sockets_list[(level + 1) & 1], boost::asio::buffer(ele.data(), element_size));
            boost::asio::write(*sockets_list[level & 1], boost::asio::buffer((ele.data()), element_size));
        }
        access_bandwidth += (level_tabLen << 1) * (element_size << 1);
    }

    /**
     * Second, receive standard table from Sb, remove empty, and send to Sa
     */
    key_type level_key_0 = get_level_key(level, get_epoch(level), 0);
    key_type level_key_1 = get_level_key(level, get_epoch(level), 1);
    boost::asio::write(*sockets_list[(level + 1) & 1], boost::asio::buffer((&level_key_0), sizeof(key_type)));
    access_bandwidth += sizeof(key_type);
    if (ell_cuckoo < level + 1) {
        boost::asio::write(*sockets_list[(level + 1) & 1], boost::asio::buffer((&level_key_1), sizeof(key_type)));
        access_bandwidth += sizeof(key_type);
    }
    
    size_t total_num_ele = (lg_N << 1) + lg_N * ((1 << ((ell_cuckoo < level + 1) * ell_cuckoo + (ell_cuckoo >= level + 1) * level)) - 4) * standard_BC + (ell_cuckoo < level + 1) * lg_N * (((1 << level) - (1 << ell_cuckoo)) << 1);
    for (size_t i = 0; i < total_num_ele; i++) {
        boost::asio::read(*sockets_list[level & 1], boost::asio::buffer(ele.data(), element_size));
        access_bandwidth += element_size;
        if (*reinterpret_cast<addr_type*>(ele.data()) != 0) {
            boost::asio::write(*sockets_list[(level + 1) & 1], boost::asio::buffer((ele.data()), element_size));
            tag = get_tag(level, get_epoch(level), *reinterpret_cast<addr_type*>(ele.data()));
            boost::asio::write(*sockets_list[(level + 1) & 1], boost::asio::buffer((tag.data()), tag_size));    
            access_bandwidth += element_size + tag_size;
        }
    }
    /**
     * Third, receive constructed standard table from S0 and send to S1
     */
    size_t ele_num_in_stash = 0;
    for (size_t i = 0; i < lg_N; i++) {
        boost::asio::read(*sockets_list[(level + 1) & 1], boost::asio::buffer(ele.data(), element_size));
        if (*reinterpret_cast<addr_type*>(ele.data()) != 0) {ele_num_in_stash++;}
        else {
            ele = get_random_element(MAX_SIZE_T - dummy_counter, element_size);
            dummy_counter++;
        }
        boost::asio::write(*sockets_list[level & 1], boost::asio::buffer((ele.data()), element_size));  
    }
    access_bandwidth += lg_N * (element_size << 1);
    for (size_t i = 0; i < get_level_ele_num(level); i++) {
        boost::asio::read(*sockets_list[(level + 1) & 1], boost::asio::buffer(ele.data(), element_size));
        if (ele_num_in_stash > 0 && *reinterpret_cast<addr_type*>(ele.data()) == 0) {
            ele = get_random_element(MAX_SIZE_T - dummy_counter, element_size);
            dummy_counter++;
            ele_num_in_stash--;
        }
        boost::asio::write(*sockets_list[level & 1], boost::asio::buffer((ele.data()), element_size));
    }
    access_bandwidth += get_level_ele_num(level) * (element_size << 1);
    for (size_t i = 2; i < level; i++) {set_bit(full_flag, i, 0);}
    set_bit(full_flag, level, 1);
    written_stash = (level + 1) & 1;
    len_written_stash = 0;
}

void Client::RebuildMaxLevel() {
    element_type ele(num_cell_per_element);
    element_type tag(num_cell_per_tag);
    size_t level_tabLen;
    maxL_epoch += 1;
    dummy_counter = 0;
    /**
     * First, receive table from Sa, and send to Sb
     */
    for (size_t i = 0; i < lg_N; i++) {
        boost::asio::read(*sockets_list[(max_level + 1) & 1], boost::asio::buffer(ele.data(), element_size));
        assert(*reinterpret_cast<addr_type*>(ele.data()) != 0);
        boost::asio::write(*sockets_list[max_level & 1], boost::asio::buffer((ele.data()), element_size));
    }
    access_bandwidth += lg_N * (element_size << 1);
    size_t standard_level = (ell_cuckoo < max_level + 1) * ell_cuckoo + (ell_cuckoo >= max_level + 1) * max_level;
    for (size_t i = 2 + ((max_level + 1) & 1); i < standard_level; i += 2) {
        level_tabLen = parametr_c * (1 << (i - 1));
        for (size_t j = 0; j < level_tabLen * standard_BC; j++) {
            boost::asio::read(*sockets_list[(max_level + 1) & 1], boost::asio::buffer(ele.data(), element_size));
            boost::asio::write(*sockets_list[max_level & 1], boost::asio::buffer((ele.data()), element_size));
        }
        access_bandwidth += level_tabLen * standard_BC * (element_size << 1);
    }
    size_t cuckoo_start_level = ell_cuckoo + (ell_cuckoo & max_level & 1) + ((ell_cuckoo + 1) & (max_level + 1) & 1);
    for (size_t i = cuckoo_start_level; i < max_level + 1; i += 2) {
        level_tabLen = static_cast<size_t>(parametr_c * (1 << (i - 1)) * (1 + cuckoo_epsilon));
        for (size_t j = 0; j < (level_tabLen << 1); j++) {
            boost::asio::read(*sockets_list[(max_level + 1) & 1], boost::asio::buffer(ele.data(), element_size));
            boost::asio::write(*sockets_list[max_level & 1], boost::asio::buffer((ele.data()), element_size));
        }
        access_bandwidth += (level_tabLen << 1) * (element_size << 1);
    }
    /**
     * Second, receive table from Sb, remove empty, and send to Sa
     */
    key_type level_key_0 = get_level_key(max_level, get_epoch(max_level), 0);
    key_type level_key_1 = get_level_key(max_level, get_epoch(max_level), 1);
    boost::asio::write(*sockets_list[(max_level + 1) & 1], boost::asio::buffer((&level_key_0), sizeof(key_type)));
    access_bandwidth += sizeof(key_type);
    if (ell_cuckoo < max_level + 1) {
        boost::asio::write(*sockets_list[(max_level + 1) & 1], boost::asio::buffer((&level_key_1), sizeof(key_type)));
        access_bandwidth += sizeof(key_type);
    }
    size_t total_num_ele = (lg_N << 1) + lg_N * ((1 << ((ell_cuckoo < max_level + 1) * ell_cuckoo + (ell_cuckoo >= max_level + 1) * (max_level + 1))) - 4) * standard_BC + (ell_cuckoo < max_level + 1) * lg_N * (((1 << (max_level + 1)) - (1 << ell_cuckoo)) << 1);
    for (size_t i = 0; i < total_num_ele; i++) {
        boost::asio::read(*sockets_list[max_level & 1], boost::asio::buffer(ele.data(), element_size));
        access_bandwidth += element_size;
        if (*reinterpret_cast<addr_type*>(ele.data()) > 0 && *reinterpret_cast<addr_type*>(ele.data()) <= N) {
            boost::asio::write(*sockets_list[(max_level + 1) & 1], boost::asio::buffer((ele.data()), element_size));
            tag = get_tag(max_level, get_epoch(max_level), *reinterpret_cast<addr_type*>(ele.data()));
            boost::asio::write(*sockets_list[(max_level + 1) & 1], boost::asio::buffer((tag.data()), tag_size));
            access_bandwidth += element_size + tag_size;
        }
    }

    /**
     * Third, receive constructed standard table from S0 and send to S1
     */
    size_t ele_num_in_stash = 0;
    for (size_t i = 0; i < lg_N; i++) {
        boost::asio::read(*sockets_list[(max_level + 1) & 1], boost::asio::buffer(ele.data(), element_size));
        if (*reinterpret_cast<addr_type*>(ele.data()) != 0) {ele_num_in_stash++;}
        else {
            ele = get_random_element(MAX_SIZE_T - dummy_counter, element_size);
            dummy_counter++;
        }
        boost::asio::write(*sockets_list[max_level & 1], boost::asio::buffer((ele.data()), element_size));  
    }
    access_bandwidth += lg_N * (element_size << 1);
    for (size_t i = 0; i < get_level_ele_num(max_level); i++) {
        boost::asio::read(*sockets_list[(max_level + 1) & 1], boost::asio::buffer(ele.data(), element_size));
        if (ele_num_in_stash > 0 && *reinterpret_cast<addr_type*>(ele.data()) == 0) {
            ele = get_random_element(MAX_SIZE_T - dummy_counter, element_size);
            dummy_counter++;
            ele_num_in_stash--;
        }
        boost::asio::write(*sockets_list[max_level & 1], boost::asio::buffer((ele.data()), element_size));
    }   
    access_bandwidth += get_level_ele_num(max_level) * (element_size << 1);
    for (size_t i = 2; i < max_level; i++) {set_bit(full_flag, i, 0);}
    set_bit(full_flag, max_level, 1);
    written_stash = (max_level + 1) & 1;
    len_written_stash = 0;
}