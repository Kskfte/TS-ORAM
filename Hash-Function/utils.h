#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../DPF-CPP/AES.h"
#include "../TW-PIR/utils.h"
#include "../TW-PIR/alignment_allocator.h"

typedef block key_type;

size_t hash_function_tag(size_t table_length, key_type& hash_key, tag_type& tag);
size_t hash_function_addr(size_t table_length, key_type& hash_key, size_t addr);
size_t get_random_position(size_t table_length);

