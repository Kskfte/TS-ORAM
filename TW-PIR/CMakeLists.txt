project(TW-PIR)
cmake_minimum_required(VERSION 3.4.1)

enable_language(C CXX)
set(CMAKE_CXX_STANDARD 14)


set(SRCS
    ../DPF-CPP/AES.cpp
    ../DPF-CPP/Defines.cpp
    ../DPF-CPP/Log.cpp
    ../DPF-CPP/PRNG.cpp
    ../DPF-CPP/dpf.cpp
    pir_read.cpp
    pir_write.cpp
    utils.cpp
    )

set(CMAKE_C_FLAGS "-ffunction-sections -Wall  -maes -msse2 -msse4.1 -mavx2 -mpclmul -Wfatal-errors -pthread -Wno-strict-overflow  -fPIC -Wno-ignored-attributes")
set(CMAKE_CXX_FLAGS  "${CMAKE_C_FLAGS}  -std=c++14")

# Select flags.
SET(CMAKE_CXX_FLAGS_RELEASE "-O3")
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO " -O2 -g -ggdb -rdynamic")
SET(CMAKE_CXX_FLAGS_DEBUG  "-O0 -g -ggdb -rdynamic")

add_executable(pir ${SRCS} main.cpp)
target_link_libraries(pir crypto)

add_executable(test_pir_read ${SRCS} test_pir_read.cpp)
target_link_libraries(test_pir_read crypto)

add_executable(test_pir_write ${SRCS} test_pir_write.cpp)
target_link_libraries(test_pir_write crypto)