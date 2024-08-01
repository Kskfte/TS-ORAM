#include "server_0.h"

Server_0::Server_0(boost::asio::io_context& io_context, short port, size_t N, size_t element_size, size_t tag_size):
    N(N), element_size(element_size), tag_size(tag_size),
    num_cell_per_element(static_cast<size_t>(element_size / sizeof(cell_type))), num_cell_per_tag(static_cast<size_t>(tag_size / sizeof(cell_type))),
    lg_N(static_cast<size_t>(log2(N))),cuckoo_alpha(lg_N*lg_N), cuckoo_epsilon(0),
    ell(std::min(static_cast<size_t>(log2(lg_N)) << 1, lg_N - 1)), L(lg_N),
    full_flag(0), len_S(1), len_B(1), access_counter(0),
    realEll_tableLen(static_cast<size_t>(std::ceil(((1 << (ell + 1)) + lg_N * (L - ell)) * (1 + cuckoo_epsilon)))),
    baseEll_tableLen(static_cast<size_t>(std::ceil((1 << ell) * (1 + cuckoo_epsilon)))),
    acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
    socket_(io_context)
{
    /**
     * Initialize the table
     */
    element_table.resize(L - ell + 1);
    tag_table.resize(L - ell + 1);

    for (size_t i = 0; i < L - ell + 1; ++i) {
        size_t table_size = (i == 0) ? realEll_tableLen : baseEll_tableLen << i;
        element_table[i].resize(2);
        element_table[i][0].resize(table_size * num_cell_per_element);
        element_table[i][1].resize(table_size * num_cell_per_element);
        tag_table[i].resize(2); 
        tag_table[i][0].resize(table_size * num_cell_per_tag);
        tag_table[i][1].resize(table_size * num_cell_per_tag);
    }

    element_stash.resize((lg_N + 1) * num_cell_per_element);
    element_buffer.resize((lg_N + 1) * num_cell_per_element);
    tag_stash.resize((lg_N + 1) * num_cell_per_tag);
    tag_buffer.resize((lg_N + 1) * num_cell_per_tag);

    /**
     * Initialize the position map
     */
    pos_dict.resize(L - ell + 1);
    for (size_t i = 0; i < L - ell + 1; ++i) {
        pos_dict[i].resize(2);
    }

    acceptor_.accept(socket_);
}


void Server_0::insert_data(size_t level) {
    // Extract element
    element_type ele(element_size / sizeof(cell_type));
    boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));

    // Extract shared tag
    tag_type shared_tag(tag_size / sizeof(cell_type));
    boost::asio::read(socket_, boost::asio::buffer(shared_tag.data(), tag_size));

    // Extract positions
    size_t ell_pos_0, ell_pos_1, level_pos_0, level_pos_1;
    boost::asio::read(socket_, boost::asio::buffer(&ell_pos_0, sizeof(size_t)));
    boost::asio::read(socket_, boost::asio::buffer(&ell_pos_1, sizeof(size_t)));
    boost::asio::read(socket_, boost::asio::buffer(&level_pos_0, sizeof(size_t)));
    boost::asio::read(socket_, boost::asio::buffer(&level_pos_1, sizeof(size_t)));

    // Attempt to insert the element
    size_t layer = level - ell;
    if (try_insert(element_table[layer], tag_table[layer], pos_dict[layer], layer, ele, shared_tag, level_pos_0, level_pos_1, 0, 0)) {set_bit(full_flag, layer, 1);}
    else if (try_insert(element_table[0], tag_table[0], pos_dict[0], 0, ele, shared_tag, ell_pos_0, ell_pos_1, 0, 0)) {set_bit(full_flag, 0, 1);}
    else {
        if (len_S > lg_N + 1) {
            throw std::runtime_error("Stash overflow");
        }
        else {
            // Insert into stash
            auto element_stash_ptr = element_stash.data() + len_S * element_size / sizeof(cell_type);
            auto tag_stash_ptr = tag_stash.data() + len_S * tag_size / sizeof(cell_type);
            std::memcpy(element_stash_ptr, ele.data(), element_size);
            std::memcpy(tag_stash_ptr, shared_tag.data(), tag_size);
            len_S++;
        }
    }
}

bool Server_0::try_insert(std::vector<element_array_type>& element_array, std::vector<tag_array_type>& tag_array, std::vector<std::unordered_map<addr_type, size_t>>& pos_dict_array, 
    size_t layer, element_type& ele, tag_type& tag, size_t write_pos, size_t kicked_pos, size_t h0Orh1, size_t depth) {
    if (depth > cuckoo_alpha) {
        return false; // Insertion failed
    }

    auto element_table_ptr = element_array[h0Orh1].data() + write_pos * element_size / sizeof(cell_type);
    auto tag_table_ptr = tag_array[h0Orh1].data() + write_pos * tag_size / sizeof(cell_type);
    
    if (*reinterpret_cast<addr_type*>(element_table_ptr) == 0) {
        std::memcpy(element_table_ptr, ele.data(), element_size);
        std::memcpy(tag_table_ptr, tag.data(), tag_size);
        pos_dict_array[h0Orh1][write_pos] = kicked_pos;
        return true;
    }

    // Perform a kick-out
    element_type kicked_out_ele(element_size / sizeof(cell_type));
    tag_type kicked_out_tag(tag_size / sizeof(cell_type));
    std::memcpy(kicked_out_ele.data(), element_table_ptr, element_size);
    std::memcpy(kicked_out_tag.data(), tag_table_ptr, tag_size);
    size_t next_write_pos = pos_dict_array[h0Orh1][write_pos];

    // Try to insert the kicked-out element in the alternate table
    if (try_insert(element_array, tag_array, pos_dict_array, layer, kicked_out_ele, kicked_out_tag, next_write_pos, write_pos, h0Orh1 ^ 1, depth + 1)) {
        std::memcpy(element_table_ptr, ele.data(), element_size);
        std::memcpy(tag_table_ptr, tag.data(), tag_size);
        pos_dict_array[h0Orh1][write_pos] = kicked_pos;
        return true;
    }

    return false;
}

void Server_0::Setup() {
    for (size_t i = 0 ;i < N; i++) {
        insert_data(L);
    }   

    boost::asio::write(socket_, boost::asio::buffer((&len_S), sizeof(size_t)));
    boost::asio::write(socket_, boost::asio::buffer((&full_flag), sizeof(size_t)));
}


void Server_0::Access() {
    // Extract written tag
    tag_type modified_tag(tag_size / sizeof(cell_type));
    boost::asio::read(socket_, boost::asio::buffer(modified_tag.data(), tag_size));
    std::vector<uint8_t> aaaa;
    std::vector<uint8_t> bbbb;
    /**
     * First, element/tag buffer
     */
    if (len_B > 1) {
        for (size_t i = len_B - 1; i > 0; i--) {
            boost::asio::write(socket_, boost::asio::buffer((read_data(element_buffer, i, element_size)), element_size));
        }
        size_t dpf_key_size = getDPF_keySize(len_B);
        std::vector<uint8_t> dpf_key(dpf_key_size);
        boost::asio::read(socket_, boost::asio::buffer(dpf_key.data(), dpf_key.size()));
        aaaa = DPF::EvalFull(dpf_key, std::ceil(log2(len_B)));
        pir_write(tag_buffer, aaaa, modified_tag, len_B, tag_size);
    }

    /**
     * Second, element/tag stash
     */
    if (len_S > 1) {
        for (size_t i = len_S - 1; i > 0; i--) {
            boost::asio::write(socket_, boost::asio::buffer((read_data(element_stash, i, element_size)), element_size));
        }
        size_t dpf_key_size = getDPF_keySize(len_S);
        std::vector<uint8_t> dpf_key(dpf_key_size);
        boost::asio::read(socket_, boost::asio::buffer(dpf_key.data(), dpf_key_size));
        aaaa = DPF::EvalFull(dpf_key, std::ceil(log2(len_B)));
        pir_write(tag_stash, aaaa, modified_tag, len_S, tag_size);
    }

    /**
     * Third, each non-empty level
     */

    // level ell
    if (is_index_bit_1(full_flag, 0)) {
        size_t dpf_key_size = getDPF_keySize(realEll_tableLen);

        std::vector<uint8_t> dpf_key_0(dpf_key_size);
        std::vector<uint8_t> dpf_key_1(dpf_key_size);
        boost::asio::read(socket_, boost::asio::buffer(dpf_key_0.data(), dpf_key_size));
        boost::asio::read(socket_, boost::asio::buffer(dpf_key_1.data(), dpf_key_size));
        
        size_t log_tab_length = std::ceil(log2(realEll_tableLen));
        if(log_tab_length > 10) {
            aaaa = DPF::EvalFull8(dpf_key_0, log_tab_length);
            bbbb = DPF::EvalFull8(dpf_key_1, log_tab_length);
        }    
        else {
            aaaa = DPF::EvalFull(dpf_key_0, log_tab_length);
            bbbb = DPF::EvalFull(dpf_key_1, log_tab_length);
        }

        element_type element_0 = pir_read(element_table[0][0], aaaa, realEll_tableLen, element_size);
        element_type element_1 = pir_read(element_table[0][1], bbbb, realEll_tableLen, element_size);
        boost::asio::write(socket_, boost::asio::buffer((element_0.data()), element_size));
        boost::asio::write(socket_, boost::asio::buffer((element_1.data()), element_size));
    }

    // Other levels
    size_t offset_0 = -1;
    size_t offset_1 = -1;
    size_t glo_read_dpf_key_size = getDPF_keySize(baseEll_tableLen << (L - ell));
    std::vector<uint8_t> glo_read_dpf_key_0(glo_read_dpf_key_size);
    std::vector<uint8_t> glo_read_dpf_key_1(glo_read_dpf_key_size);
    boost::asio::read(socket_, boost::asio::buffer(glo_read_dpf_key_0.data(), glo_read_dpf_key_size));
    boost::asio::read(socket_, boost::asio::buffer(glo_read_dpf_key_1.data(), glo_read_dpf_key_size));
    std::vector<std::vector<uint8_t>> levels_aaaa(L - ell);
    std::vector<std::vector<uint8_t>> levels_bbbb(L - ell);
    if (L > 10) { 
        levels_aaaa[L - ell - 1] = DPF::EvalFull8(glo_read_dpf_key_0, L);
        levels_bbbb[L - ell - 1] = DPF::EvalFull8(glo_read_dpf_key_1, L);
    }
    else {
        levels_aaaa[L - ell - 1] = DPF::EvalFull(glo_read_dpf_key_0, L);
        levels_bbbb[L - ell - 1] = DPF::EvalFull(glo_read_dpf_key_1, L);
    }
    for (size_t i = L - 1; i > ell; i--) {
        levels_aaaa[i - ell - 1] = xor_half_vector_avx(levels_aaaa[i - ell], 1 << (i - 3));
        levels_bbbb[i - ell - 1] = xor_half_vector_avx(levels_bbbb[i - ell], 1 << (i - 3));
    }
    
    for (size_t i = ell + 1; i <= L; i++) {
        if (is_index_bit_1(full_flag, i - ell)) {
            // element
            boost::asio::read(socket_, boost::asio::buffer(&offset_0, sizeof(size_t)));
            boost::asio::read(socket_, boost::asio::buffer(&offset_1, sizeof(size_t)));
            element_type element_0 = pir_read_with_offset(element_table[i - ell][0], levels_aaaa[i - ell - 1], baseEll_tableLen << (i - ell), element_size, offset_0);           
            element_type element_1 = pir_read_with_offset(element_table[i - ell][1], levels_bbbb[i - ell - 1], baseEll_tableLen << (i - ell), element_size, offset_1);
            boost::asio::write(socket_, boost::asio::buffer((element_0.data()), element_size));
            boost::asio::write(socket_, boost::asio::buffer((element_1.data()), element_size));
        }
    }
    
    /**
     * Fourth, PIR Write
     */
    size_t level_log_array_length;
    size_t level_dpf_key_size;
    if (is_index_bit_1(full_flag, 0)) {
        level_log_array_length = static_cast<size_t>(std::ceil(log2(realEll_tableLen)) + 1);
        level_dpf_key_size = getDPF_keySize(1 << level_log_array_length);
        std::vector<uint8_t> level_dpf_keys(level_dpf_key_size);
        boost::asio::read(socket_, boost::asio::buffer(level_dpf_keys.data(), level_dpf_key_size));
        std::vector<uint8_t> level_aaaa = DPF::EvalFull(level_dpf_keys, level_log_array_length);
        pir_write(tag_table[0][0], level_aaaa, modified_tag, realEll_tableLen, tag_size);
        pir_write2(tag_table[0][1], level_aaaa, modified_tag, realEll_tableLen, tag_size);
    }

    level_log_array_length = L + 2;
    level_dpf_key_size = getDPF_keySize(1 << level_log_array_length);
    std::vector<uint8_t> level_dpf_keys(level_dpf_key_size);
    boost::asio::read(socket_, boost::asio::buffer(level_dpf_keys.data(), level_dpf_key_size));
    std::vector<uint8_t> level_aaaa = DPF::EvalFull(level_dpf_keys, level_log_array_length);
    for (size_t i = ell + 1; i <= L; i++) {
        if (is_index_bit_1(full_flag, i - ell)) {
            pir_write_with_offset(tag_table[i - ell][0], level_aaaa, modified_tag, 1 << i, tag_size, 0);
            pir_write_with_offset(tag_table[i - ell][1], level_aaaa, modified_tag, 1 << i, tag_size, 1 << (L + 1));
        }
    }
    /**
     * Fifth, write the element and tag to the buffer
     */
    // Extract element
    element_type ele(element_size / sizeof(cell_type));
    boost::asio::read(socket_, boost::asio::buffer(ele.data(), element_size));
    // Extract shared tag
    tag_type shared_tag(tag_size / sizeof(cell_type));
    boost::asio::read(socket_, boost::asio::buffer(shared_tag.data(), tag_size));

    write_data(element_buffer, len_B, element_size, ele);
    write_data(tag_buffer, len_B, tag_size, shared_tag);

    len_B++;
    access_counter++;

    /**
     * Fifth, rebuild
     */
    if (access_counter % (1 << L) == 0) {
        // std::cout << "Rebuild: " << L << "\n";
        RebuildL();
    }
    else if (access_counter % (1 << (ell + 1)) == 0) {
        for (size_t i = L; i > ell; i--) {
            if (access_counter % (1 << i) == 0) {
                // std::cout << "Rebuild: " << i << "\n";
                RebuildLevel(i);
                break;
            }
        }
    }
    else if (len_B - 1 ==  lg_N) {
        //std::cout << "Rebuild: " << ell << "\n";
        RebuildEll();
    }
} 

void Server_0::RebuildEll() {
    /**
     * Initialize new tables
     */

    std::vector<element_array_type> tmp_elementTable; 
    tmp_elementTable.resize(2);
    tmp_elementTable[0].resize(realEll_tableLen * num_cell_per_element);
    tmp_elementTable[1].resize(realEll_tableLen * num_cell_per_element); 
    element_array_type tmp_elementStash;
    tmp_elementStash.resize((lg_N + 1) * num_cell_per_element);
    std::vector<tag_array_type> tmp_tagTable;
    tmp_tagTable.resize(2);
    tmp_tagTable[0].resize(realEll_tableLen * num_cell_per_tag);
    tmp_tagTable[1].resize(realEll_tableLen * num_cell_per_tag);
    tag_array_type tmp_tagStash;
    tmp_tagStash.resize((lg_N + 1) * num_cell_per_tag);
    std::vector<std::unordered_map<addr_type, size_t>> tmp_posDict;
    tmp_posDict.resize(2);
    size_t tmp_len_S = 1;
    set_bit(full_flag, 0, 0);

    // parameter
    size_t ell_pos_0, ell_pos_1;
    element_type glo_ele(element_size / sizeof(cell_type));
    tag_type glo_tag(tag_size / sizeof(cell_type));
    size_t len_posD;

    /**
     * Send buffer
     */
    for (size_t i = 1; i < len_B; i++) {
        glo_ele = read_data_to_cell(element_buffer, i, element_size);
        glo_tag = read_data_to_cell(tag_buffer, i, tag_size);

        boost::asio::write(socket_, boost::asio::buffer((glo_ele.data()), element_size));
        boost::asio::write(socket_, boost::asio::buffer((glo_tag.data()), tag_size));

        boost::asio::read(socket_, boost::asio::buffer(glo_ele.data(), element_size));
        boost::asio::read(socket_, boost::asio::buffer(&ell_pos_0, sizeof(size_t)));
        boost::asio::read(socket_, boost::asio::buffer(&ell_pos_1, sizeof(size_t)));

        if (try_insert(tmp_elementTable, tmp_tagTable, tmp_posDict, 0, glo_ele, glo_tag, ell_pos_0, ell_pos_1, 0, 0)) {set_bit(full_flag, 0, 1);}
        else {
            if (tmp_len_S > lg_N + 1) {
                throw std::runtime_error("Stash overflow");
            }
            else {
                // Insert into stash
                auto element_stash_ptr = tmp_elementStash.data() + tmp_len_S * element_size / sizeof(cell_type);
                auto tag_stash_ptr = tmp_tagStash.data() + tmp_len_S * tag_size / sizeof(cell_type);
                std::memcpy(element_stash_ptr, glo_ele.data(), element_size);
                std::memcpy(tag_stash_ptr, glo_tag.data(), tag_size);
                tmp_len_S++;
            }
        }
    }


    /**
     * Send stash
     */
    for (size_t i = 1; i < len_S; i++) {
        glo_ele = read_data_to_cell(element_stash, i, element_size);
        glo_tag = read_data_to_cell(tag_stash, i, tag_size);

        boost::asio::write(socket_, boost::asio::buffer((glo_ele.data()), element_size));
        boost::asio::write(socket_, boost::asio::buffer((glo_tag.data()), tag_size));

        boost::asio::read(socket_, boost::asio::buffer(glo_ele.data(), element_size));
        boost::asio::read(socket_, boost::asio::buffer(&ell_pos_0, sizeof(size_t)));
        boost::asio::read(socket_, boost::asio::buffer(&ell_pos_1, sizeof(size_t)));

        if (try_insert(tmp_elementTable, tmp_tagTable, tmp_posDict, 0, glo_ele, glo_tag, ell_pos_0, ell_pos_1, 0, 0)) {set_bit(full_flag, 0, 1);}
        else {
            if (tmp_len_S > lg_N + 1) {
                throw std::runtime_error("Stash overflow");
            }
            else {
                // Insert into stash
                auto element_stash_ptr = tmp_elementStash.data() + tmp_len_S * element_size / sizeof(cell_type);
                auto tag_stash_ptr = tmp_tagStash.data() + tmp_len_S * tag_size / sizeof(cell_type);
                std::memcpy(element_stash_ptr, glo_ele.data(), element_size);
                std::memcpy(tag_stash_ptr, glo_tag.data(), tag_size);
                tmp_len_S++;
            }
        }
    }

    /**
     * Send table
     */
    len_posD = pos_dict[0][0].size();
    boost::asio::write(socket_, boost::asio::buffer(&len_posD, sizeof(size_t)));
    len_posD = pos_dict[0][1].size();
    boost::asio::write(socket_, boost::asio::buffer(&len_posD, sizeof(size_t)));
    for (size_t i =0; i < 2; i++) {
        for (const auto& pair : pos_dict[0][i]) {
            glo_ele = read_data_to_cell(element_table[0][i], pair.first, element_size);
            glo_tag = read_data_to_cell(tag_table[0][i], pair.first, tag_size);

            boost::asio::write(socket_, boost::asio::buffer((glo_ele.data()), element_size));
            boost::asio::write(socket_, boost::asio::buffer((glo_tag.data()), tag_size));

            boost::asio::read(socket_, boost::asio::buffer(glo_ele.data(), element_size));
            boost::asio::read(socket_, boost::asio::buffer(&ell_pos_0, sizeof(size_t)));
            boost::asio::read(socket_, boost::asio::buffer(&ell_pos_1, sizeof(size_t)));

            if (try_insert(tmp_elementTable, tmp_tagTable, tmp_posDict, 0, glo_ele, glo_tag, ell_pos_0, ell_pos_1, 0, 0)) {set_bit(full_flag, 0, 1);}
            else {
                if (tmp_len_S > lg_N + 1) {
                    throw std::runtime_error("Stash overflow");
                }
                else {
                    // Insert into stash
                    auto element_stash_ptr = tmp_elementStash.data() + tmp_len_S * element_size / sizeof(cell_type);
                    auto tag_stash_ptr = tmp_tagStash.data() + tmp_len_S * tag_size / sizeof(cell_type);
                    std::memcpy(element_stash_ptr, glo_ele.data(), element_size);
                    std::memcpy(tag_stash_ptr, glo_tag.data(), tag_size);
                    tmp_len_S++;
                }
            }
        } 
    }

    /**
     * Re-initilization
     */
    clr(element_table[0][0]);
    clr(element_table[0][1]);
    clr(tag_table[0][0]);
    clr(tag_table[0][1]);
    pos_dict[0][0].clear();
    pos_dict[0][1].clear();
    
    clr(element_buffer);
    clr(tag_buffer);
    element_table[0] = tmp_elementTable;
    element_stash = tmp_elementStash;
    tag_table[0] = tmp_tagTable;
    tag_stash = tmp_tagStash;
    pos_dict[0] = tmp_posDict;
    len_B = 1;
    len_S = tmp_len_S;

    boost::asio::write(socket_, boost::asio::buffer((&len_S), sizeof(size_t)));
}

void Server_0::RebuildLevel(size_t level) {
    /**
     * Initialize new tables
     */
    std::vector<element_array_type> tmp_ellElementTable; 
    tmp_ellElementTable.resize(2);
    tmp_ellElementTable[0].resize(realEll_tableLen * num_cell_per_element);
    tmp_ellElementTable[1].resize(realEll_tableLen * num_cell_per_element); 
    std::vector<element_array_type> tmp_levelElementTable; 
    tmp_levelElementTable.resize(2);
    tmp_levelElementTable[0].resize((baseEll_tableLen << (level - ell)) * num_cell_per_element);
    tmp_levelElementTable[1].resize((baseEll_tableLen << (level - ell)) * num_cell_per_element);
    element_array_type tmp_elementStash;
    tmp_elementStash.resize((lg_N + 1) * num_cell_per_element);

    std::vector<tag_array_type> tmp_ellTagTable;
    tmp_ellTagTable.resize(2);
    tmp_ellTagTable[0].resize(realEll_tableLen * num_cell_per_tag);
    tmp_ellTagTable[1].resize(realEll_tableLen * num_cell_per_tag);
    std::vector<tag_array_type> tmp_levelTagTable;
    tmp_levelTagTable.resize(2);
    tmp_levelTagTable[0].resize((baseEll_tableLen << (level - ell)) * num_cell_per_tag);
    tmp_levelTagTable[1].resize((baseEll_tableLen << (level - ell)) * num_cell_per_tag);
    tag_array_type tmp_tagStash;
    tmp_tagStash.resize((lg_N + 1) * num_cell_per_tag);

    std::vector<std::unordered_map<addr_type, size_t>> tmp_ellPosDict;
    tmp_ellPosDict.resize(2);
    std::vector<std::unordered_map<addr_type, size_t>> tmp_levelPosDict;
    tmp_levelPosDict.resize(2);

    size_t tmp_len_S = 1;

    // parameter
    size_t ell_pos_0, ell_pos_1;
    size_t level_pos_0, level_pos_1;
    element_type glo_ele(element_size / sizeof(cell_type));
    tag_type glo_tag(tag_size / sizeof(cell_type));
    size_t len_posD;
    for (size_t i = 0; i < level - ell; i++){
        set_bit(full_flag, i, 0);
    }

    /**
     * Send buffer
     */
    for (size_t i = 1; i < len_B; i++) {
        glo_ele = read_data_to_cell(element_buffer, i, element_size);
        glo_tag = read_data_to_cell(tag_buffer, i, tag_size);

        boost::asio::write(socket_, boost::asio::buffer((glo_ele.data()), element_size));
        boost::asio::write(socket_, boost::asio::buffer((glo_tag.data()), tag_size));

        boost::asio::read(socket_, boost::asio::buffer(glo_ele.data(), element_size));
        boost::asio::read(socket_, boost::asio::buffer(&ell_pos_0, sizeof(size_t)));
        boost::asio::read(socket_, boost::asio::buffer(&ell_pos_1, sizeof(size_t)));
        boost::asio::read(socket_, boost::asio::buffer(&level_pos_0, sizeof(size_t)));
        boost::asio::read(socket_, boost::asio::buffer(&level_pos_1, sizeof(size_t)));

        if (try_insert(tmp_levelElementTable, tmp_levelTagTable, tmp_levelPosDict, level - ell, glo_ele, glo_tag, level_pos_0, level_pos_1, 0, 0)) {set_bit(full_flag, level - ell, 1);}
        else if (try_insert(tmp_ellElementTable, tmp_ellTagTable, tmp_ellPosDict, 0, glo_ele, glo_tag, ell_pos_0, ell_pos_1, 0, 0)) {set_bit(full_flag, 0, 1);}
        else {
            if (tmp_len_S > lg_N + 1) {
                throw std::runtime_error("Stash overflow");
            }
            else {
                // Insert into stash
                auto element_stash_ptr = tmp_elementStash.data() + tmp_len_S * element_size / sizeof(cell_type);
                auto tag_stash_ptr = tmp_tagStash.data() + tmp_len_S * tag_size / sizeof(cell_type);
                std::memcpy(element_stash_ptr, glo_ele.data(), element_size);
                std::memcpy(tag_stash_ptr, glo_tag.data(), tag_size);
                tmp_len_S++;
            }
        }
    }

    /**
     * Send stash
     */
    for (size_t i = 1; i < len_S; i++) {
        glo_ele = read_data_to_cell(element_stash, i, element_size);
        glo_tag = read_data_to_cell(tag_stash, i, tag_size);

        boost::asio::write(socket_, boost::asio::buffer((glo_ele.data()), element_size));
        boost::asio::write(socket_, boost::asio::buffer((glo_tag.data()), tag_size));

        boost::asio::read(socket_, boost::asio::buffer(glo_ele.data(), element_size));
        boost::asio::read(socket_, boost::asio::buffer(&ell_pos_0, sizeof(size_t)));
        boost::asio::read(socket_, boost::asio::buffer(&ell_pos_1, sizeof(size_t)));
        boost::asio::read(socket_, boost::asio::buffer(&level_pos_0, sizeof(size_t)));
        boost::asio::read(socket_, boost::asio::buffer(&level_pos_1, sizeof(size_t)));

        if (try_insert(tmp_levelElementTable, tmp_levelTagTable, tmp_levelPosDict, level - ell, glo_ele, glo_tag, level_pos_0, level_pos_1, 0, 0)) {set_bit(full_flag, level - ell, 1);}
        else if (try_insert(tmp_ellElementTable, tmp_ellTagTable, tmp_ellPosDict, 0, glo_ele, glo_tag, ell_pos_0, ell_pos_1, 0, 0)) {set_bit(full_flag, 0, 1);}
        else {
            if (tmp_len_S > lg_N + 1) {
                throw std::runtime_error("Stash overflow");
            }
            else {
                // Insert into stash
                auto element_stash_ptr = tmp_elementStash.data() + tmp_len_S * element_size / sizeof(cell_type);
                auto tag_stash_ptr = tmp_tagStash.data() + tmp_len_S * tag_size / sizeof(cell_type);
                std::memcpy(element_stash_ptr, glo_ele.data(), element_size);
                std::memcpy(tag_stash_ptr, glo_tag.data(), tag_size);
                tmp_len_S++;
            }
        }
    }

    /**
     * Send table
     */
    for (size_t cur_level = ell; cur_level < level; cur_level++) {
        len_posD = pos_dict[cur_level - ell][0].size();
        boost::asio::write(socket_, boost::asio::buffer(&len_posD, sizeof(size_t)));
        len_posD = pos_dict[cur_level - ell][1].size();
        boost::asio::write(socket_, boost::asio::buffer(&len_posD, sizeof(size_t)));
        for (size_t i =0; i < 2; i++) {
            for (const auto& pair : pos_dict[cur_level - ell][i]) {
                glo_ele = read_data_to_cell(element_table[cur_level - ell][i], pair.first, element_size);
                glo_tag = read_data_to_cell(tag_table[cur_level - ell][i], pair.first, tag_size);

                boost::asio::write(socket_, boost::asio::buffer((glo_ele.data()), element_size));
                boost::asio::write(socket_, boost::asio::buffer((glo_tag.data()), tag_size));

                boost::asio::read(socket_, boost::asio::buffer(glo_ele.data(), element_size));
                boost::asio::read(socket_, boost::asio::buffer(&ell_pos_0, sizeof(size_t)));
                boost::asio::read(socket_, boost::asio::buffer(&ell_pos_1, sizeof(size_t)));
                boost::asio::read(socket_, boost::asio::buffer(&level_pos_0, sizeof(size_t)));
                boost::asio::read(socket_, boost::asio::buffer(&level_pos_1, sizeof(size_t)));

                if (try_insert(tmp_levelElementTable, tmp_levelTagTable, tmp_levelPosDict, level - ell, glo_ele, glo_tag, level_pos_0, level_pos_1, 0, 0)) {set_bit(full_flag, level - ell, 1);}
                else if (try_insert(tmp_ellElementTable, tmp_ellTagTable, tmp_ellPosDict, 0, glo_ele, glo_tag, ell_pos_0, ell_pos_1, 0, 0)) {set_bit(full_flag, 0, 1);}
                else {
                    if (tmp_len_S > lg_N + 1) {
                        throw std::runtime_error("Stash overflow");
                    }
                    else {
                        // Insert into stash
                        auto element_stash_ptr = tmp_elementStash.data() + tmp_len_S * element_size / sizeof(cell_type);
                        auto tag_stash_ptr = tmp_tagStash.data() + tmp_len_S * tag_size / sizeof(cell_type);
                        std::memcpy(element_stash_ptr, glo_ele.data(), element_size);
                        std::memcpy(tag_stash_ptr, glo_tag.data(), tag_size);
                        tmp_len_S++;
                    }
                }
            } 
        }
    }

    /**
     * Re-initilization
     */
    element_table[0] = tmp_ellElementTable;
    element_table[level - ell] = tmp_levelElementTable;
    element_stash = tmp_elementStash;
    clr(element_buffer);
    tag_table[0] = tmp_ellTagTable;
    tag_table[level - ell] = tmp_levelTagTable;
    tag_stash = tmp_tagStash;
    clr(tag_buffer);
    pos_dict[0] = tmp_ellPosDict;
    pos_dict[level - ell] = tmp_levelPosDict;
    for (size_t i = ell + 1; i < level; i++) {
        clr(element_table[i - ell][0]);
        clr(element_table[i - ell][1]);
        clr(tag_table[i - ell][0]);
        clr(tag_table[i - ell][1]);
        pos_dict[i - ell][0].clear();
        pos_dict[i - ell][1].clear();
    }

    len_B = 1;
    len_S = tmp_len_S;
    boost::asio::write(socket_, boost::asio::buffer((&len_S), sizeof(size_t)));
    boost::asio::write(socket_, boost::asio::buffer((&full_flag), sizeof(size_t)));
}

void Server_0::RebuildL() {
    // Parameters
    size_t ell_pos_0, ell_pos_1;
    size_t L_pos_0, L_pos_1;
    element_type glo_ele(element_size / sizeof(cell_type));
    tag_type glo_tag(tag_size / sizeof(cell_type));
    size_t len_posD;
    for (size_t i = 0; i < L - ell; i++){
        set_bit(full_flag, i, 0);
    }

    /**
     * Step 1: Send element to server_0
     */
    // Send buffer
    for (size_t i = 1; i < len_B; i++) {
        glo_ele = read_data_to_cell(element_buffer, i, element_size);
        glo_tag = read_data_to_cell(tag_buffer, i, tag_size);
        boost::asio::write(socket_, boost::asio::buffer((glo_ele.data()), element_size));
        boost::asio::write(socket_, boost::asio::buffer((glo_tag.data()), tag_size));
        boost::asio::read(socket_, boost::asio::buffer(glo_ele.data(), element_size));
        write_data(element_buffer, i, element_size, glo_ele);            
    }

    // Send stash
    for (size_t i = 1; i < len_S; i++) {
        glo_ele = read_data_to_cell(element_stash, i, element_size);
        glo_tag = read_data_to_cell(tag_stash, i, tag_size);
        boost::asio::write(socket_, boost::asio::buffer((glo_ele.data()), element_size));
        boost::asio::write(socket_, boost::asio::buffer((glo_tag.data()), tag_size));
        boost::asio::read(socket_, boost::asio::buffer(glo_ele.data(), element_size));
        write_data(element_stash, i, element_size, glo_ele);
    }

    // Send table
    for (size_t cur_level = ell; cur_level <= L; cur_level++) {
        len_posD = pos_dict[cur_level - ell][0].size();
        boost::asio::write(socket_, boost::asio::buffer(&len_posD, sizeof(size_t)));
        len_posD = pos_dict[cur_level - ell][1].size();
        boost::asio::write(socket_, boost::asio::buffer(&len_posD, sizeof(size_t)));
        for (size_t i =0; i < 2; i++) {
            for (const auto& pair : pos_dict[cur_level - ell][i]) {
                glo_ele = read_data_to_cell(element_table[cur_level - ell][i], pair.first, element_size);
                glo_tag = read_data_to_cell(tag_table[cur_level - ell][i], pair.first, tag_size);
                boost::asio::write(socket_, boost::asio::buffer((glo_ele.data()), element_size));
                boost::asio::write(socket_, boost::asio::buffer((glo_tag.data()), tag_size));
                boost::asio::read(socket_, boost::asio::buffer(glo_ele.data(), element_size));
                write_data(element_table[cur_level - ell][i], pair.first, element_size, glo_ele);
            } 
        }
    }

    /**
     * Step 2: Shuffle elements and send to client
     */
    std::vector<cell_type*> element_pointers;
    for (size_t i = 1; i < len_B; ++i) {
        element_pointers.push_back(element_buffer.data() + i * element_size / sizeof(cell_type));
    }
    for (size_t i = 1; i < len_S; ++i) {
        element_pointers.push_back(element_stash.data() + i * element_size / sizeof(cell_type));
    }
    for (size_t cur_level = ell; cur_level <= L; cur_level++) {
        for (const auto& pos_pair : pos_dict[cur_level - ell][0]) {
            element_pointers.push_back(element_table[cur_level - ell][0].data() + pos_pair.first * element_size / sizeof(cell_type));
        }
        for (const auto& pos_pair : pos_dict[cur_level - ell][1]) {
            element_pointers.push_back(element_table[cur_level - ell][1].data() + pos_pair.first * element_size / sizeof(cell_type));
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
        std::memcpy(glo_ele.data(), ptr, element_size);
        boost::asio::write(socket_, boost::asio::buffer((glo_ele.data()), element_size));
    }

    // Initialize structure
    clr(element_buffer);
    clr(element_stash);
    clr(tag_buffer);
    clr(tag_stash);
    for (size_t i = ell; i <= L; i++) {
        clr(element_table[i - ell][0]);
        clr(element_table[i - ell][1]);
        clr(tag_table[i - ell][0]);
        clr(tag_table[i - ell][1]);
        pos_dict[i - ell][0].clear();
        pos_dict[i - ell][1].clear();
    }
    len_B = 1;
    len_S = 1;

    /**
     * Step 3: Setup the table
     */
    for (size_t i = 0; i < N; i++) {
        boost::asio::read(socket_, boost::asio::buffer(glo_ele.data(), element_size));
        boost::asio::read(socket_, boost::asio::buffer(glo_tag.data(), tag_size));
        boost::asio::read(socket_, boost::asio::buffer(&ell_pos_0, sizeof(size_t)));
        boost::asio::read(socket_, boost::asio::buffer(&ell_pos_1, sizeof(size_t)));
        boost::asio::read(socket_, boost::asio::buffer(&L_pos_0, sizeof(size_t)));
        boost::asio::read(socket_, boost::asio::buffer(&L_pos_1, sizeof(size_t)));

        // Attempt to insert the element
        if (try_insert(element_table[L - ell], tag_table[L - ell], pos_dict[L - ell], L - ell, glo_ele, glo_tag, L_pos_0, L_pos_1, 0, 0)) {set_bit(full_flag, L - ell, 1);}
        else if (try_insert(element_table[0], tag_table[0], pos_dict[0], 0, glo_ele, glo_tag, ell_pos_0, ell_pos_1, 0, 0)) {set_bit(full_flag, 0, 1);}
        else {
            if (len_S > lg_N + 1) {
                throw std::runtime_error("Stash overflow");
            }
            else {
                // Insert into stash
                auto element_stash_ptr = element_stash.data() + len_S * element_size / sizeof(cell_type);
                auto tag_stash_ptr = tag_stash.data() + len_S * tag_size / sizeof(cell_type);
                std::memcpy(element_stash_ptr, glo_ele.data(), element_size);
                std::memcpy(tag_stash_ptr, glo_tag.data(), tag_size);
                len_S++;
            }
        }
    }
    boost::asio::write(socket_, boost::asio::buffer((&len_S), sizeof(size_t)));
    boost::asio::write(socket_, boost::asio::buffer((&full_flag), sizeof(size_t)));
}