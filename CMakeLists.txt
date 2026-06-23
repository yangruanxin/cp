cmake_minimum_required(VERSION 3.20)
project(ToyC-Compiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 主编译器
add_executable(compiler
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/semantic.cpp
    src/symbol_table.cpp
    src/codegen.cpp
    src/optimizer.cpp
    src/utils.cpp
)

# 各模块独立测试（每人自测自己的模块）
add_executable(test_lexer
    tests/test_lexer.cpp
    src/lexer.cpp
)
target_include_directories(test_lexer PRIVATE src)

add_executable(test_parser
    tests/test_parser.cpp
    src/lexer.cpp
    src/parser.cpp
)
target_include_directories(test_parser PRIVATE src)

add_executable(test_semantic
    tests/test_semantic.cpp
    src/semantic.cpp
    src/symbol_table.cpp
)
target_include_directories(test_semantic PRIVATE src)

add_executable(test_codegen
    tests/test_codegen.cpp
    src/codegen.cpp
)
target_include_directories(test_codegen PRIVATE src)

add_executable(test_integration
    tests/test_integration.cpp
    src/lexer.cpp
    src/parser.cpp
    src/semantic.cpp
    src/symbol_table.cpp
    src/codegen.cpp
    src/optimizer.cpp
    src/utils.cpp
)
target_include_directories(test_integration PRIVATE src)
