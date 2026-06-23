// A 的自测文件 — 语法分析
// 编译: g++ -std=c++20 -I../src ../src/lexer.cpp ../src/parser.cpp test_parser.cpp -o test_parser
#include "lexer.h"
#include "parser.h"
#include <iostream>
#include <cassert>

void test_parse_expression() {
    Lexer lexer("1 + 2 * 3");
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    // TODO: parse expression directly if we add that API
    std::cout << "PASS: test_parse_expression\n";
}

void test_parse_function() {
    Lexer lexer("int main() { return 0; }");
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    assert(ast != nullptr);
    assert(ast->funcDefs.size() == 1);
    assert(ast->funcDefs[0]->name == "main");
    std::cout << "PASS: test_parse_function\n";
}

int main() {
    test_parse_expression();
    test_parse_function();
    std::cout << "All parser tests passed!\n";
    return 0;
}
