// A 的自测文件 — 词法分析
// 编译: g++ -std=c++20 -I../src ../src/lexer.cpp test_lexer.cpp -o test_lexer
#include "lexer.h"
#include <cassert>
#include <iostream>
#include <stdexcept>

void test_basic_tokens() {
    Lexer lexer("int main() { return 0; }");
    auto tokens = lexer.tokenize();
    assert(tokens.size() > 0);
    assert(tokens[0].type == TokenType::INT);
    assert(tokens[1].type == TokenType::ID && tokens[1].value == "main");
    std::cout << "PASS: test_basic_tokens\n";
}

void test_comments_and_operators() {
    Lexer lexer("int a = 1; // comment\nint b = 2; /* block */ int c = 3;\nif (a && b || c == 3 != 4) { return a; }");
    auto tokens = lexer.tokenize();

    bool sawAnd = false;
    bool sawOr = false;
    bool sawEq = false;
    bool sawNe = false;
    for (const auto& token : tokens) {
        if (token.type == TokenType::AND) sawAnd = true;
        if (token.type == TokenType::OR) sawOr = true;
        if (token.type == TokenType::EQ) sawEq = true;
        if (token.type == TokenType::NE) sawNe = true;
    }

    assert(sawAnd && sawOr && sawEq && sawNe);
    std::cout << "PASS: test_comments_and_operators\n";
}

void test_invalid_character() {
    bool threw = false;
    try {
        Lexer lexer("int x = 1 @ 2;");
        (void)lexer.tokenize();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
    std::cout << "PASS: test_invalid_character\n";
}

int main() {
    test_basic_tokens();
    test_comments_and_operators();
    test_invalid_character();
    std::cout << "All lexer tests passed!\n";
    return 0;
}
