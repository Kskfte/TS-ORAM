#include "server_1.h"

Server_1::Server_1(boost::asio::io_context& io_context, short port, size_t N, size_t element_size, size_t tag_size):
    N(N), element_size(element_size), tag_size(tag_size), 
    num_cell_per_element(static_cast<size_t>(element_size / sizeof(cell_type))),
    num_cell_per_tag(static_cast<size_t>(tag_size / sizeof(cell_type))),
    lg_N(static_cast<size_t>(log2(N))),cuckoo_alpha(lg_N*lg_N), cuckoo_epsilon(0),
    parametr_c(lg_N << 1), max_level(1 + static_cast<size_t>(std::ceil(log2(N/parametr_c)))),
    ell_cuckoo(std::min(static_cast<size_t>(log2(lg_N)) << 1, max_level + 1)), access_counter(0), full_flag(0), 
    standard_BC(static_cast<size_t>(3*lg_N/log2(lg_N))),
    baseCuckoo_tableLen(static_cast<size_t>(parametr_c * (1 << (ell_cuckoo - 1)) * (1 + cuckoo_epsilon))),
    written_stash(0), len_written_stash(0),
    acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
    socket_(io_context)
{
    /**
     * Initialize the table
     */
    std::cout << "ell_cuckoo: " << ell_cuckoo << " ;  max_level: " << max_level <<std::endl;
    Stash.resize(lg_N * num_cell_per_element);
    Standard_Table.resize(count_odd_no1(ell_cuckoo - 1));
    for (size_t i = 0; i < count_odd_no1(ell_cuckoo - 1); i++) {
        Standard_Table[i].resize(parametr_c * (1 << ((1 + i) << 1)));
        for (size_t j = 0; j < Standard_Table[i].size(); j++) {
            Standard_Table[i][j].resize(standard_BC * num_cell_per_element);
        }
    }
    if (max_level > ell_cuckoo - 1) {
        size_t tmp_cuckoo_num = count_odd_no1(max_level) - count_odd_no1(ell_cuckoo - 1);
        Cuckoo_Table.resize(tmp_cuckoo_num);
        size_t tmp_base_tabLen = baseCuckoo_tableLen << ((ell_cuckoo + 1) & 1);
        for (size_t i = 0; i < tmp_cuckoo_num; i++) {
            Cuckoo_Table[i].resize(2);
            Cuckoo_Table[i][0].resize((tmp_base_tabLen << (i << 1)) * num_cell_per_element);
            Cuckoo_Table[i][1].resize((tmp_base_tabLen << (i << 1)) * num_cell_per_element);
        }
    }
    
    acceptor_.accept(socket_);
}

bool Server_1::try_insert_standard(std::vector<element_array_type>& element_array, std::unordered_map<addr_type, size_t>& pos_dict, element_type& ele, size_t write_pos) {
    if (pos_dict[write_pos] < standard_BC) {
        write_data(element_array[write_pos], pos_dict[write_pos], element_size, ele);
        pos_dict[write_pos]++;
        return true;
    }
    else {
        return false;
    }
}    

bool Server_1::try_insert_cuckoo(std::vector<element_array_type>& element_array, std::vector<std::unordered_map<addr_type, size_t>>& pos_dict_array, 
    element_type& ele, size_t write_pos, size_t kicked_pos, size_t h0Orh1, size_t depth) {
    if (depth > cuckoo_alpha) {
        return false; // Insertion failed
    }
    auto element_table_ptr = element_array[h0Orh1].data() + write_pos * element_size / sizeof(cell_type);
    if (*reinterpret_cast<addr_type*>(element_table_ptr) == 0) {
        std::memcpy(element_table_ptr, ele.data(), element_size);
        pos_dict_array[h0Orh1][write_pos] = kicked_pos;
        return true;
    }
    // Perform a kick-out
    element_type kicked_out_ele(element_size / sizeof(cell_type));
    std::memcpy(kicked_out_ele.data(), element_table_ptr, element_size);
    size_t next_write_pos = pos_dict_array[h0Orh1][write_pos];
    // Try to insert the kicked-out element in the alternate table
    if (try_insert_cuckoo(element_array, pos_dict_array, kicked_out_ele, next_write_pos, write_pos, h0Orh1 ^ 1, depth + 1)) {
        std::memcpy(element_table_ptr, ele.data(), element_size);
        pos_dict_array[h0Orh1][write_pos] = kicked_pos;
        return true;
    }
    return false;
}

void Server_1::write_element_to_socket(element_array_type& data_array, size_t count) {
    for (size_t i = 0; i < count; i++) {
        element_type ele = read_data_to_cell(data_array, i, element_size);
        boost::asio::write(socket_, boost::asio::buffer((ele.data()), element_size));
    }
}

void Server_1::send_standard_table(size_t level, size_t rec_ele_num) {
    size_t level_tabLen = parametr_c * (1 << (level - 1));
    element_type ele(num_cell_per_element);
    tag_type tag(num_cell_per_tag);
    std::unordered_map<addr_type, size_t> tmp_pos_dict;
    std::vector<element_array_type> tmp_standard_table(level_tabLen, element_array_type(standard_BC * num_cell_per_element));
    size_t tmpEleNum_inStash = 0;
    element_array_type tmp_stash(lg_N * num_cell_per_element);
    size_t level_pos_0;
    key_type level_key_0;
    boost::asio::read(socket_, boost::asio::buffer(&level_key_0, sizeof(key_type)));
    for (size_t i = 0 ;i < rec_ele_num; i++) {
        boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
        boost::asio::read(socket_, boost::asio::buffer(tag.data(), tag_size));
        level_pos_0 = hash_function_tag(level_tabLen, level_key_0, tag);
        if (!try_insert_standard(tmp_standard_table, tmp_pos_dict, ele, level_pos_0)) {
            if (tmpEleNum_inStash == lg_N) {
                throw std::runtime_error("Stash overflow");
            }
            else {
                // Insert into stash
                auto element_stash_ptr = tmp_stash.data() + tmpEleNum_inStash * num_cell_per_element;
                std::memcpy(element_stash_ptr, ele.data(), element_size);
                tmpEleNum_inStash++;
            }

        }
    }

    write_element_to_socket(tmp_stash, lg_N);
    for (size_t i = 0; i < level_tabLen; i++) {
        write_element_to_socket(tmp_standard_table[i], standard_BC);
    }
}

void Server_1::send_cuckoo_table(size_t level, size_t rec_ele_num) {
    size_t level_tabLen = static_cast<size_t>(parametr_c * (1 << (level - 1)) * (1 + cuckoo_epsilon));
    element_type ele(num_cell_per_element);
    tag_type tag(num_cell_per_tag);
    std::vector<std::unordered_map<addr_type, size_t>> tmp_pos_dict_array(2);
    std::vector<element_array_type> tmp_cuckoo_table(2, element_array_type(level_tabLen * num_cell_per_element));
    size_t tmpEleNum_inStash = 0;
    element_array_type tmp_stash(lg_N * num_cell_per_element);
    size_t level_pos_0;
    size_t level_pos_1;
    key_type level_key_0;
    key_type level_key_1;
    boost::asio::read(socket_, boost::asio::buffer(&level_key_0, sizeof(key_type)));
    boost::asio::read(socket_, boost::asio::buffer(&level_key_1, sizeof(key_type)));
    for (size_t i = 0; i < rec_ele_num; i++) {
        boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
        boost::asio::read(socket_, boost::asio::buffer(tag.data(), tag_size));
        level_pos_0 = hash_function_tag(level_tabLen, level_key_0, tag);
        level_pos_1 = hash_function_tag(level_tabLen, level_key_1, tag);
        if (!try_insert_cuckoo(tmp_cuckoo_table, tmp_pos_dict_array, ele, level_pos_0, level_pos_1, 0, 0)) {
            if (tmpEleNum_inStash == lg_N) {
                throw std::runtime_error("Stash overflow");
            }
            else {
                // Insert into stash
                auto element_stash_ptr = tmp_stash.data() + tmpEleNum_inStash * num_cell_per_element;
                std::memcpy(element_stash_ptr, ele.data(), element_size);
                tmpEleNum_inStash++;
            }
        }
    }
    write_element_to_socket(tmp_stash, lg_N);
    write_element_to_socket(tmp_cuckoo_table[0], level_tabLen);
    write_element_to_socket(tmp_cuckoo_table[1], level_tabLen);
}

void Server_1::receive_standard_table(size_t level, std::vector<element_array_type>& element_array) {
    size_t level_tabLen = parametr_c * (1 << (level - 1));
    element_type ele(num_cell_per_element);
    for (size_t i = 0; i < lg_N; i++) {
        boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
        write_data(Stash, i, element_size, ele);
    }
    for (size_t i = 0; i < level_tabLen; i++) {
        for (size_t j = 0; j < standard_BC; j++){
            boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
            write_data(element_array[i], j, element_size, ele);
        }
    }
}

void Server_1::receive_cuckoo_table(size_t level, std::vector<element_array_type>& element_array) {
    size_t level_tabLen = static_cast<size_t>(parametr_c * (1 << (level - 1)) * (1 + cuckoo_epsilon));
    element_type ele(num_cell_per_element);
    for (size_t i = 0; i < lg_N; i++) {
        boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
        write_data(Stash, i, element_size, ele);
    }
    for (size_t i = 0; i < 2; i++) {
        for (size_t j = 0; j < level_tabLen; j++){
            boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
            write_data(element_array[i], j, element_size, ele);
        }
    }
}

void Server_1::receive_shuffle_resend_standard_table(size_t level) {
    size_t total_num_ele = (lg_N << 1) + lg_N * ((1 << level) - 4) * standard_BC;
    size_t tmp_level = level;
    if (level == max_level) {
        tmp_level = max_level + 1;
        total_num_ele += parametr_c * (1 << (max_level - 1)) * standard_BC;
    }
    size_t glo_index = 0;
    element_type ele(num_cell_per_element);
    element_array_type tmp_element_array(total_num_ele * num_cell_per_element);
    /**
     * First, read own stash and table elements to tmp_element_array
     */
    for (size_t i = 0; i < lg_N; i++) {
        ele = read_data_to_cell(Stash, i, element_size);
        write_data(tmp_element_array, glo_index, element_size, ele);
        glo_index++;
    }
    clr(Stash);
    size_t level_tabLen;
    for (size_t i = 3; i < tmp_level; i += 2) {
        level_tabLen = parametr_c * (1 << (i - 1));
        assert(level_tabLen == Standard_Table[(i - 3) >> 1].size());
        for (size_t j = 0; j < Standard_Table[(i - 3) >> 1].size(); j++) {
            for (size_t k = 0; k < standard_BC; k++) {
                ele = read_data_to_cell(Standard_Table[(i - 3) >> 1][j], k, element_size);
                write_data(tmp_element_array, glo_index, element_size, ele);
                glo_index++;
            }
            clr(Standard_Table[(i - 3) >> 1][j]);
        }
    }
    /**
     * Second, read other table elements to tmp_element_array
     */
    while (glo_index < total_num_ele) {
        boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
        write_data(tmp_element_array, glo_index, element_size, ele);
        glo_index++;
    }
    assert(glo_index == total_num_ele);
    /**
     * Third, reshuffle the table and send to client
     */
    std::vector<cell_type*> pointers(total_num_ele);
    for (size_t i = 0; i < total_num_ele; ++i) {
        pointers[i] = tmp_element_array.data() + i * element_size / sizeof(cell_type);
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(pointers.begin(), pointers.end(), g);
    for (auto ptr : pointers) {
        if (ptr == nullptr) {
            std::cerr << "Error: Null pointer encountered in element_pointers" << std::endl;
            continue;
        }
        // Debug information
        std::memcpy(ele.data(), ptr, element_size);
        boost::asio::write(socket_, boost::asio::buffer((ele.data()), element_size));
    }
}

void Server_1::receive_shuffle_resend_cuckoo_table(size_t level) {
    size_t total_num_ele = (lg_N << 1) + lg_N * ((1 << ell_cuckoo) - 4) * standard_BC + lg_N * (((1 << level) - (1 << ell_cuckoo)) << 1);
    size_t tmp_level = level;
    if (level == max_level) {
        tmp_level = max_level + 1;
        total_num_ele += static_cast<size_t>(parametr_c * (1 << max_level) * (1 + cuckoo_epsilon));
    }
    size_t glo_index = 0;
    element_type ele(num_cell_per_element);
    element_array_type tmp_element_array(total_num_ele * num_cell_per_element);
    /**
     * First, read own stash and table elements to tmp_element_array
     */
    for (size_t i = 0; i < lg_N; i++) {
        ele = read_data_to_cell(Stash, i, element_size);
        write_data(tmp_element_array, glo_index, element_size, ele);
        glo_index++;
    }
    clr(Stash);
    size_t level_tabLen;
    for (size_t i = 3; i < ell_cuckoo; i += 2) {
        level_tabLen = parametr_c * (1 << (i - 1));
        assert(level_tabLen == Standard_Table[(i - 3) >> 1].size());
        for (size_t j = 0; j < Standard_Table[(i - 3) >> 1].size(); j++) {
            for (size_t k = 0; k < standard_BC; k++) {
                ele = read_data_to_cell(Standard_Table[(i - 3) >> 1][j], k, element_size);
                write_data(tmp_element_array, glo_index, element_size, ele);
                glo_index++;
            }
            clr(Standard_Table[(i - 3) >> 1][j]);
        }
    }

    for (size_t i = ell_cuckoo + ((ell_cuckoo + 1) & 1); i < tmp_level; i += 2) {
        level_tabLen = static_cast<size_t>(parametr_c * (1 << (i - 1)) * (1 + cuckoo_epsilon));
        for (size_t k = 0; k < level_tabLen; k++) {
            ele = read_data_to_cell(Cuckoo_Table[(i - (ell_cuckoo + ((ell_cuckoo + 1) & 1))) >> 1][0], k, element_size);
            write_data(tmp_element_array, glo_index, element_size, ele);
            glo_index++;
        }
        for (size_t k = 0; k < level_tabLen; k++) {
            ele = read_data_to_cell(Cuckoo_Table[(i - (ell_cuckoo + ((ell_cuckoo + 1) & 1))) >> 1][1], k, element_size);
            write_data(tmp_element_array, glo_index, element_size, ele);
            glo_index++;
        }
        clr(Cuckoo_Table[(i - (ell_cuckoo + ((ell_cuckoo + 1) & 1))) >> 1][0]);
        clr(Cuckoo_Table[(i - (ell_cuckoo + ((ell_cuckoo + 1) & 1))) >> 1][1]);
    }

    /**
     * Second, read other table elements to tmp_element_array
     */
    while (glo_index < total_num_ele) {
        boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
        write_data(tmp_element_array, glo_index, element_size, ele);
        glo_index++;
    }
    assert(glo_index == total_num_ele);
    /**
     * Third, reshuffle the table and send to client
     */
    std::vector<cell_type*> pointers(total_num_ele);
    for (size_t i = 0; i < total_num_ele; ++i) {
        pointers[i] = tmp_element_array.data() + i * element_size / sizeof(cell_type);;
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(pointers.begin(), pointers.end(), g);
    for (auto ptr : pointers) {
        if (ptr == nullptr) {
            std::cerr << "Error: Null pointer encountered in element_pointers" << std::endl;
            continue;
        }
        // Debug information
        std::memcpy(ele.data(), ptr, element_size);
        boost::asio::write(socket_, boost::asio::buffer((ele.data()), element_size));
    }
}

void Server_1::Setup() {
    size_t case_identifier = (((max_level + 1) & 1) << 1) | (ell_cuckoo >= max_level + 1);
    switch (case_identifier) {
        case 3: // max_level & 1 != 0 && ell_cuckoo == max_level + 1
            send_standard_table(max_level, N);
            break;
        case 2: // max_level & 1 != 0 && ell_cuckoo != max_level + 1
            send_cuckoo_table(max_level, N);
            break;
        case 1: // max_level & 1 == 0 && ell_cuckoo == max_level + 1
            receive_standard_table(max_level, Standard_Table[(max_level - 3) >> 1]);
            break;
        case 0: // max_level & 1 == 0 && ell_cuckoo != max_level + 1
            receive_cuckoo_table(max_level, Cuckoo_Table[(max_level - (ell_cuckoo + ((ell_cuckoo + 1) & 1))) >> 1]);
            break;
    } 
    for (size_t i = 2; i < max_level; i++) {set_bit(full_flag, i, 0);}
    set_bit(full_flag, max_level, 1);
    written_stash = (max_level + 1) & 1;
    len_written_stash = 0;     
}

void Server_1::Access() {
    // Parameters
    tag_type tag(num_cell_per_tag);
    element_type ele(num_cell_per_element);
    key_type level_key_0;
    key_type level_key_1;
    size_t level_pos_0;
    size_t level_pos_1;

    /**
     * First, send the stash
     */
    for (size_t i = 0; i < written_stash * len_written_stash + (written_stash ^ 1) * lg_N; i++) {
        ele = read_data_to_cell(Stash, i, element_size);
        boost::asio::write(socket_, boost::asio::buffer((ele.data()), element_size));
    }
    /**
     * Second, send the standard table 
     */ 
    for (size_t i = 3; i < ell_cuckoo; i += 2) {
        if (is_index_bit_1(full_flag, i)) {
            boost::asio::read(socket_, boost::asio::buffer(&level_key_0, sizeof(key_type)));
            boost::asio::read(socket_, boost::asio::buffer(tag.data(), tag_size));
            level_pos_0 = hash_function_tag(parametr_c * (1 << (i - 1)), level_key_0, tag);
            for (size_t j = 0; j < standard_BC; j++) {
                ele = read_data_to_cell(Standard_Table[(i - 3) >> 1][level_pos_0], j, element_size);
                boost::asio::write(socket_, boost::asio::buffer((ele.data()), element_size));
                boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
                write_data(Standard_Table[(i - 3) >> 1][level_pos_0], j, element_size, ele);
            }
        }
    }
    /**
     * Third, send the cuckoo table 
     */ 
    for (size_t i = ell_cuckoo + ((ell_cuckoo + 1) & 1); i < max_level + 1; i += 2) {
        if (is_index_bit_1(full_flag, i)) {
            boost::asio::read(socket_, boost::asio::buffer(&level_key_0, sizeof(key_type)));
            boost::asio::read(socket_, boost::asio::buffer(&level_key_1, sizeof(key_type)));
            boost::asio::read(socket_, boost::asio::buffer(tag.data(), tag_size));
            level_pos_0 = hash_function_tag(static_cast<size_t>(parametr_c * (1 << (i - 1)) * (1 + cuckoo_epsilon)), level_key_0, tag);
            level_pos_1 = hash_function_tag(static_cast<size_t>(parametr_c * (1 << (i - 1)) * (1 + cuckoo_epsilon)), level_key_1, tag);
            
            ele = read_data_to_cell(Cuckoo_Table[(i - (ell_cuckoo + ((ell_cuckoo + 1) & 1))) >> 1][0], level_pos_0, element_size);
            boost::asio::write(socket_, boost::asio::buffer((ele.data()), element_size));
            boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
            write_data(Cuckoo_Table[(i - (ell_cuckoo + ((ell_cuckoo + 1) & 1))) >> 1][0], level_pos_0, element_size, ele);
            
            ele = read_data_to_cell(Cuckoo_Table[(i - (ell_cuckoo + ((ell_cuckoo + 1) & 1))) >> 1][1], level_pos_1, element_size);
            boost::asio::write(socket_, boost::asio::buffer((ele.data()), element_size));
            boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
            write_data(Cuckoo_Table[(i - (ell_cuckoo + ((ell_cuckoo + 1) & 1))) >> 1][1], level_pos_1, element_size, ele);
        }
    }

    /**
     * Fourth, rewrite the top level
     */
    for (size_t i = 0; i < written_stash * len_written_stash + (written_stash ^ 1) * lg_N; i++) {
        ele = read_data_to_cell(Stash, i, element_size);
        boost::asio::write(socket_, boost::asio::buffer((ele.data()), element_size));
        boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
        write_data(Stash, i, element_size, ele);
    }
    if (written_stash) {
        boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
        write_data(Stash, len_written_stash, element_size, ele);
    }
    
    access_counter++;
    len_written_stash++;
    
    /**
     * Fifth, rebuild
     */
    if (access_counter % lg_N == 0) {
        bool tmp_reb = false;
        for (size_t i = 2; i < max_level; i++) {
            if (!is_index_bit_1(full_flag, i)) {
                std::cout << "RebuildLevel:: " << i << std::endl;
                Rebuild(i);
                tmp_reb = true;
                break;
            }
        }
        if (!tmp_reb) {
            std::cout << "RebuildMaxLevel:: " << max_level << std::endl;
            RebuildMaxLevel();
        }
    }
} 


void Server_1::Rebuild(size_t level) {
    size_t level_tabLen;
    size_t case_identifier = (((level + 1) & 1) << 1) | (ell_cuckoo >= level + 1);
    switch (case_identifier) {
        case 3: // level & 1 != 0 && ell_cuckoo == level + 1
            /**
             * First, Send table to client and clear the table
             */
            write_element_to_socket(Stash, lg_N);
            clr(Stash);
            for (size_t i = 3; i < level; i += 2) {
                level_tabLen = parametr_c * (1 << (i - 1));
                assert(level_tabLen == Standard_Table[(i - 3) >> 1].size());
                for (size_t j = 0; j < Standard_Table[(i - 3) >> 1].size(); j++) {
                    write_element_to_socket(Standard_Table[(i - 3) >> 1][j], standard_BC);
                    clr(Standard_Table[(i - 3) >> 1][j]);
                }
            }
            /**
             * Second, construct the hash table and send to client
             */
            send_standard_table(level, lg_N * (1 << (level - 1)));
            break;
        case 2: // level & 1 != 0 && ell_cuckoo != level + 1
            /**
             * First, Send table to client and clear the table
             */
            write_element_to_socket(Stash, lg_N);
            clr(Stash);
            for (size_t i = 3; i < ell_cuckoo; i += 2) {
                level_tabLen = parametr_c * (1 << (i - 1));
                assert(level_tabLen == Standard_Table[(i - 3) >> 1].size());
                for (size_t j = 0; j < Standard_Table[(i - 3) >> 1].size(); j++) {
                    write_element_to_socket(Standard_Table[(i - 3) >> 1][j], standard_BC);
                    clr(Standard_Table[(i - 3) >> 1][j]);
                }
            }
            for (size_t i = ell_cuckoo + ((ell_cuckoo + 1) & 1); i < level; i += 2) {
                level_tabLen = static_cast<size_t>(parametr_c * (1 << (i - 1)) * (1 + cuckoo_epsilon));
                write_element_to_socket(Cuckoo_Table[(i - (ell_cuckoo + ((ell_cuckoo + 1) & 1))) >> 1][0], level_tabLen);
                write_element_to_socket(Cuckoo_Table[(i - (ell_cuckoo + ((ell_cuckoo + 1) & 1))) >> 1][1], level_tabLen);
                clr(Cuckoo_Table[(i - (ell_cuckoo + ((ell_cuckoo + 1) & 1))) >> 1][0]);
                clr(Cuckoo_Table[(i - (ell_cuckoo + ((ell_cuckoo + 1) & 1))) >> 1][1]);
            }
            
            /**
             * Second, construct the hash table and send to client
             */
            send_cuckoo_table(level, lg_N * (1 << (level - 1)) * (1 + cuckoo_epsilon));
            break;
        case 1: // level & 1 == 0 && ell_cuckoo == level + 1
            receive_shuffle_resend_standard_table(level);
            receive_standard_table(level, Standard_Table[(level - 3) >> 1]);
            break;
        case 0: // level & 1 == 0 && ell_cuckoo != level + 1
            receive_shuffle_resend_cuckoo_table(level);
            receive_cuckoo_table(level, Cuckoo_Table[(level - (ell_cuckoo + ((ell_cuckoo + 1) & 1))) >> 1]);
            break;
    }
    for (size_t i = 2; i < level; i++) {set_bit(full_flag, i, 0);}
    set_bit(full_flag, level, 1);
    written_stash = (level + 1) & 1;
    len_written_stash = 0;
}

void Server_1::RebuildMaxLevel() {
    size_t level_tabLen;
    size_t case_identifier = (((max_level + 1) & 1) << 1) | (ell_cuckoo >= max_level + 1);
    switch (case_identifier) {
        case 3: // max_level & 1 != 0 && ell_cuckoo == max_level + 1
            /**
             * First, Send table to client and clear the table
             */
            write_element_to_socket(Stash, lg_N);
            clr(Stash);
            for (size_t i = 3; i < max_level + 1; i += 2) {
                level_tabLen = parametr_c * (1 << (i - 1));
                assert(level_tabLen == Standard_Table[(i - 3) >> 1].size());
                for (size_t j = 0; j < Standard_Table[(i - 3) >> 1].size(); j++) {
                    write_element_to_socket(Standard_Table[(i - 3) >> 1][j], standard_BC);
                    clr(Standard_Table[(i - 3) >> 1][j]);
                }
            }
            /**
             * Second, construct the hash table and send to client
             */
            send_standard_table(max_level, N);
            break;
        case 2: // max_level & 1 != 0 && ell_cuckoo != max_level + 1
            /**
             * First, Send table to client and clear the table
             */
            write_element_to_socket(Stash, lg_N);
            clr(Stash);
            for (size_t i = 3; i < ell_cuckoo; i += 2) {
                level_tabLen = parametr_c * (1 << (i - 1));
                assert(level_tabLen == Standard_Table[(i - 3) >> 1].size());
                for (size_t j = 0; j < Standard_Table[(i - 3) >> 1].size(); j++) {
                    write_element_to_socket(Standard_Table[(i - 3) >> 1][j], standard_BC);
                    clr(Standard_Table[(i - 3) >> 1][j]);
                }
            }
            for (size_t i = ell_cuckoo + ((ell_cuckoo + 1) & 1); i < max_level + 1; i += 2) {
                level_tabLen = static_cast<size_t>(parametr_c * (1 << (i - 1)) * (1 + cuckoo_epsilon));
                write_element_to_socket(Cuckoo_Table[(i - (ell_cuckoo + ((ell_cuckoo + 1) & 1))) >> 1][0], level_tabLen);
                write_element_to_socket(Cuckoo_Table[(i - (ell_cuckoo + ((ell_cuckoo + 1) & 1))) >> 1][1], level_tabLen);
                clr(Cuckoo_Table[(i - (ell_cuckoo + ((ell_cuckoo + 1) & 1))) >> 1][0]);
                clr(Cuckoo_Table[(i - (ell_cuckoo + ((ell_cuckoo + 1) & 1))) >> 1][1]);
            }
            
            /**
             * Second, construct the hash table and send to client
             */
            send_cuckoo_table(max_level, N);
            break;
        case 1: // max_level & 1 == 0 && ell_cuckoo == max_level + 1
            receive_shuffle_resend_standard_table(max_level);
            receive_standard_table(max_level, Standard_Table[(max_level - 3) >> 1]);
            break;
        case 0: // max_level & 1 == 0 && ell_cuckoo != max_level + 1
            receive_shuffle_resend_cuckoo_table(max_level);
            receive_cuckoo_table(max_level, Cuckoo_Table[(max_level - (ell_cuckoo + ((ell_cuckoo + 1) & 1))) >> 1]);
            break;
    }
    for (size_t i = 2; i < max_level; i++) {set_bit(full_flag, i, 0);}
    set_bit(full_flag, max_level, 1);
    written_stash = (max_level + 1) & 1;
    len_written_stash = 0;
}
