cmake_minimum_required(VERSION 3.20)
project(ToyC-Compiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 所有源文件
set(COMPILER_SOURCES
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/symbol_table.cpp
    src/semantic.cpp
    src/codegen.cpp
    src/optimizer.cpp
    src/utils.cpp
)

# 主编译器
add_executable(compiler ${COMPILER_SOURCES})
target_include_directories(compiler PRIVATE src)

# A 的自测
add_executable(test_lexer tests/test_lexer.cpp src/lexer.cpp)
target_include_directories(test_lexer PRIVATE src)

add_executable(test_parser tests/test_parser.cpp src/lexer.cpp src/parser.cpp)
target_include_directories(test_parser PRIVATE src)

# B 的自测
add_executable(test_semantic
    tests/test_semantic.cpp
    src/semantic.cpp
    src/symbol_table.cpp
)
target_include_directories(test_semantic PRIVATE src)

# C 的自测
add_executable(test_codegen tests/test_codegen.cpp src/codegen.cpp)
target_include_directories(test_codegen PRIVATE src)

# D 的自测
add_executable(test_optimizer tests/test_optimizer.cpp src/optimizer.cpp)
target_include_directories(test_optimizer PRIVATE src)

# D 的集成测试
add_executable(test_integration tests/test_integration.cpp ${COMPILER_SOURCES})
target_include_directories(test_integration PRIVATE src)
