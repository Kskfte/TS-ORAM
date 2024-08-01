project(LO13-Client)
cmake_minimum_required(VERSION 3.4.1)

enable_language(C CXX)
set(CMAKE_CXX_STANDARD 14)


set(SRCS
    ../../DPF-CPP/AES.cpp
    ../../DPF-CPP/Defines.cpp
    ../../DPF-CPP/Log.cpp
    ../../DPF-CPP/PRNG.cpp
    ../../DPF-CPP/dpf.cpp
    ../../TW-PIR/utils.cpp
    ../../Hash-Function/utils.cpp
    ../Utils/utils.cpp
    client.cpp
    )

set(CMAKE_C_FLAGS "-ffunction-sections -Wall  -maes -msse2 -msse4.1 -mavx2 -mpclmul -Wfatal-errors -pthread -Wno-strict-overflow  -fPIC -Wno-ignored-attributes")
set(CMAKE_CXX_FLAGS  "${CMAKE_C_FLAGS}  -std=c++14")

# Select flags.
SET(CMAKE_CXX_FLAGS_RELEASE "-O3")
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO " -O2 -g -ggdb -rdynamic")
SET(CMAKE_CXX_FLAGS_DEBUG  "-O0 -g -ggdb -rdynamic")

add_executable(lo13_client ${SRCS} main.cpp)
target_link_libraries(lo13_client crypto)

add_executable(test_lo13_client ${SRCS} test.cpp)
target_link_libraries(test_lo13_client crypto)