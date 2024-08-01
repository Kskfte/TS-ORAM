#include "utils.h"

size_t count_even(size_t n) {
    return n >> 1; 
}

size_t count_odd_no1(size_t n) {
    return ((n + 1) >> 1) - 1;
}