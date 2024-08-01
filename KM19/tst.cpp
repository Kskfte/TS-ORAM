#include <iostream>
#include <vector>
#include <utility>
#include <algorithm>
const size_t base_excess_addr = 10;
/**
 * dire = 0, decending order; dire = 1, ascending order
 * flag = 0, (pos, addr) sort; flag = 1, (pos, addr, excess) sort*/ 
void compAndSwap(std::vector<std::pair<size_t, size_t>>& A, size_t ii, size_t jj, size_t flag, size_t dire) { // A: (addr, pos)
    size_t case_identifier = ((flag & 1) << 1) | (dire & 1);
    std::pair<size_t, size_t> addr_pos0 = A[ii];
    std::pair<size_t, size_t> addr_pos1 = A[jj];
    switch (case_identifier) {
        case 3:
            if (addr_pos0.second >= base_excess_addr || 
                (addr_pos0.second < base_excess_addr && addr_pos1.second < base_excess_addr && (
                    (addr_pos0.second > addr_pos1.second || 
                    ((addr_pos0.second == addr_pos1.second) && (addr_pos0.first > addr_pos1.first)))
                ))) {
                    std::swap(A[ii], A[jj]);
                }
            break;
        case 2:
            if (addr_pos1.second >= base_excess_addr || 
                (addr_pos0.second < base_excess_addr && addr_pos1.second < base_excess_addr && (
                    (addr_pos0.second < addr_pos1.second || 
                    ((addr_pos0.second == addr_pos1.second) && (addr_pos0.first < addr_pos1.first)))
                ))) {
                    std::swap(A[ii], A[jj]);
                }
            break;
        case 1:  
            if (addr_pos0.second > addr_pos1.second || ((addr_pos0.second == addr_pos1.second) && (addr_pos0.first > addr_pos1.first))) {
                std::swap(A[ii], A[jj]);
            }
            break;
        case 0:
            if (addr_pos0.second < addr_pos1.second || ((addr_pos0.second == addr_pos1.second) && (addr_pos0.first < addr_pos1.first))) {
                std::swap(A[ii], A[jj]);
            }
            break;
    }
}

void bitonicToOrder(std::vector<std::pair<size_t, size_t>>& A, size_t start, size_t end, size_t flag, size_t dire) {
    if(end - start > 1) {
        size_t middle = (end - start) >> 1;
        for (size_t i = 0; i < middle; i++) {
            compAndSwap(A, i + start, i + start + middle, flag, dire);
        }
        bitonicToOrder(A, start, start + middle, flag, dire);
        bitonicToOrder(A, start + middle, end, flag, dire);
    }
}

void bitonicMerge(std::vector<std::pair<size_t, size_t>>& A, size_t start, size_t end, size_t flag, size_t dire) {
    if(end - start > 1) {
        size_t middle = (end - start) >> 1;
        bitonicMerge(A, start, start + middle, flag, dire);
        bitonicMerge(A, start + middle, end, flag, dire ^ 1);
        bitonicToOrder(A, start, end, flag, dire);
    }
}

// 打印vector内容
void printVector(const std::vector<std::pair<size_t, size_t>>& A) {
    for(const auto& p : A) {
        std::cout << "(" << p.first << ", " << p.second << ") ";
    }
    std::cout << std::endl;
}

size_t nextPowerOf2(size_t n) {
    if (n == 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return ++n;
}

int main() {
    // 初始化测试向量
    std::vector<std::pair<size_t, size_t>> A = {
        {1, 15}, {3, 10}, {5, 8}, {7, 12}, {6, 8}, {4, 8}, {1, 8}, {0, 8}, 
        {11, 14}, {13, 9}, {15, 4}, {17, 11}, {10, 11}, {9, 11}, {12, 11}, {19, 11}
    };

    std::cout << nextPowerOf2(128) << "\n";
    std::cout << "Initial vector: ";
    printVector(A);

    // 调用bitonicMerge进行排序
    bitonicMerge(A, 0, A.size(), 0, 1); // flag = 0, dire = 1 (ascending order)
    
    std::cout << "Sorted vector (ascending): ";
    printVector(A);

    // 调用bitonicMerge进行排序
    bitonicMerge(A, 0, A.size(), 1, 1); // flag = 1, dire = 1 (ascending order)
    
    std::cout << "Sorted vector (ascending): ";
    printVector(A);

    return 0;
}