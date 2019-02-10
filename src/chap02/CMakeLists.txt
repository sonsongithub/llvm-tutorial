cmake_minimum_required(VERSION 3.12)

include(${CMAKE_CURRENT_SOURCE_DIR}/../shared.cmake)

SET_LLVM_COMPILE_CONFIG()

set(CMAKE_C_LINK_EXECUTABLE "/usr/local/opt/llvm/bin/clang++")
set(CMAKE_CXX_COMPILER "/usr/local/opt/llvm/bin/clang++")
set(CMAKE_CXX_FLAGS ${LLVM_COMPILE_CONFIG})

add_executable(main main.cpp)
