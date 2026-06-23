cmake_minimum_required(VERSION 3.20)
project(ToyC-Compiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
