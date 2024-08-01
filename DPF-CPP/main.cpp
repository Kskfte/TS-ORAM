#include "dpf.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <cstdint>
#include <x86intrin.h>

int main(int argc, char** argv) {

    if(argc != 2) {
	    std::cout << "Usage: ./dpf <log_tree_size>" << std::endl;
        return -1;
    }
    size_t NN = std::strtoull(argv[1], nullptr, 10);
    std::chrono::duration<double> buildT, evalT, answerT;
    size_t keysizeT = 0;
    std::cout << NN << "\n";
    buildT = evalT = answerT = std::chrono::duration<double>::zero();
    for(size_t N = NN; N >= NN; N--) {
        
        /*
        //hashdatastore store;
        store.reserve(1ULL << N);
        // Fill Datastore with dummy elements for benchmark
        for (size_t i = 0; i < (1ULL << N); i++) {
            store.push_back(_mm256_set_epi64x(i, i, i, i));
        }
        */
        auto time1 = std::chrono::high_resolution_clock::now();
        auto keys = DPF::Gen((1ULL << N)-1, N);
        auto a = keys.first;
        auto b = keys.second;
        
        
        /*
        for (size_t j = 0; j < (1<<N); j++) {
            bool boo_a = DPF::Eval(a,j,N);
            bool boo_b = DPF::Eval(b,j,N);
            std::cout << boo_a << " " << boo_b <<"\n";
        }
        */
       
        keysizeT += a.size();
        auto time2 = std::chrono::high_resolution_clock::now();

        std::vector<uint8_t> aaaa;
        std::vector<uint8_t> bbbb;
        if(N > 10) {
            aaaa = DPF::EvalFull8(a, N);
            bbbb = DPF::EvalFull8(b, N);
        } else {
            aaaa = DPF::EvalFull(a, N);
            bbbb = DPF::EvalFull(b, N);
        }

        /*
        std::cout << "aaaaSize: " << aaaa.size() << "\n";
        for(size_t i = 0; i+7 < (1UL << N); i+=8) {
            uint64_t tmpA = aaaa[i/8];
            uint64_t tmpB = bbbb[i/8];
            std::cout << i << ": " << _mm256_extract_epi64(_mm256_xor_si256(_mm256_set1_epi64x(((tmpB>>0)&1)),_mm256_set1_epi64x(((tmpA>>0)&1))), 0) << std::endl;
            std::cout << i+1 << ": " << _mm256_extract_epi64(_mm256_xor_si256(_mm256_set1_epi64x(((tmpB>>1)&1)),_mm256_set1_epi64x(((tmpA>>1)&1))), 0) << std::endl;
            std::cout << i+2 << ": " << _mm256_extract_epi64(_mm256_xor_si256(_mm256_set1_epi64x(((tmpB>>2)&1)),_mm256_set1_epi64x(((tmpA>>2)&1))), 0) << std::endl;
            std::cout << i+3 << ": " << _mm256_extract_epi64(_mm256_xor_si256(_mm256_set1_epi64x(((tmpB>>3)&1)),_mm256_set1_epi64x(((tmpA>>3)&1))), 0) << std::endl;
            std::cout << i+4 << ": " << _mm256_extract_epi64(_mm256_xor_si256(_mm256_set1_epi64x(((tmpB>>4)&1)),_mm256_set1_epi64x(((tmpA>>4)&1))), 0) << std::endl;
            std::cout << i+5 << ": " << _mm256_extract_epi64(_mm256_xor_si256(_mm256_set1_epi64x(((tmpB>>5)&1)),_mm256_set1_epi64x(((tmpA>>5)&1))), 0) << std::endl;
            std::cout << i+6 << ": " << _mm256_extract_epi64(_mm256_xor_si256(_mm256_set1_epi64x(((tmpB>>6)&1)),_mm256_set1_epi64x(((tmpA>>6)&1))), 0) << std::endl;
            std::cout << i+7 << ": " << _mm256_extract_epi64(_mm256_xor_si256(_mm256_set1_epi64x(((tmpB>>7)&1)),_mm256_set1_epi64x(((tmpA>>7)&1))), 0) << std::endl;


        }
        */
        
        //std::vector<uint8_t> bbbb = DPF::EvalFull(b, N);

        auto time3 = std::chrono::high_resolution_clock::now();
        //hashdatastore::hash_type answerA = store.answer_pir2(aaaa);
        //hashdatastore::hash_type answerB = store.answer_pir2(bbbb);
        //hashdatastore::hash_type answer = _mm256_xor_si256(answerA, answerB);
        auto time4 = std::chrono::high_resolution_clock::now();
        //std::cout << _mm256_extract_epi64(answer, 0) << std::endl;

        buildT += time2 - time1;
        evalT += time3 - time2;
        answerT += time4 - time3;
    }
    std::cout << "DPF.Gen: "        <<  buildT.count() << "sec" << std::endl;
    std::cout << "DPF.Eval: "       << evalT.count() << "sec" << std::endl;
    std::cout << "Inner Prod: "     << answerT.count() << "sec" << std::endl;
    std::cout << "Transfer bytes: " <<  keysizeT << std::endl;

    return 0;

}
