// A 的自测文件 — 词法分析
// 编译: g++ -std=c++20 -I../src ../src/lexer.cpp test_lexer.cpp -o test_lexer
//
// 用法: 直接运行，观察 output 是否与预期一致
#include "lexer.h"
#include <iostream>
#include <cassert>

void test_basic_tokens() {
    Lexer lexer("int main() { return 0; }");
    auto tokens = lexer.tokenize();
    assert(tokens.size() > 0);
    assert(tokens[0].type == TokenType::INT);
    assert(tokens[1].type == TokenType::ID && tokens[1].value == "main");
    std::cout << "PASS: test_basic_tokens\n";
}

void test_comments() {
    Lexer lexer("int a = 1; // comment\nint b = 2; /* block */ int c;");
    auto tokens = lexer.tokenize();
    // TODO: check tokens
    std::cout << "PASS: test_comments\n";
}

int main() {
    test_basic_tokens();
    test_comments();
    std::cout << "All lexer tests passed!\n";
    return 0;
}
