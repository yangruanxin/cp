// A 的自测文件 — 语法分析
// 编译: g++ -std=c++20 -I../src ../src/lexer.cpp ../src/parser.cpp test_parser.cpp -o test_parser
#include "lexer.h"
#include "parser.h"
#include <cassert>
#include <iostream>

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

void test_parse_nested_control_flow() {
    Lexer lexer("const int MAX = 10; int x = 1; int main() { if (x < MAX) { while (x < 3) { x = x + 1; } } return x; }");
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();

    assert(ast != nullptr);
    assert(ast->decls.size() == 2);
    assert(ast->funcDefs.size() == 1);

    auto& func = ast->funcDefs[0];
    assert(func->body != nullptr);
    assert(func->body->kind == StmtKind::BLOCK);
    assert(func->body->stmts.size() == 2);
    assert(func->body->stmts[0]->kind == StmtKind::IF);
    assert(func->body->stmts[0]->thenStmt != nullptr);
    assert(func->body->stmts[0]->thenStmt->kind == StmtKind::BLOCK);
    assert(func->body->stmts[0]->thenStmt->stmts[0]->kind == StmtKind::WHILE);

    std::string dump = dumpAst(ast);
    assert(dump.find("FuncDef") != std::string::npos);
    std::cout << "PASS: test_parse_nested_control_flow\n";
}

int main() {
    test_parse_function();
    test_parse_nested_control_flow();
    std::cout << "All parser tests passed!\n";
    return 0;
}
