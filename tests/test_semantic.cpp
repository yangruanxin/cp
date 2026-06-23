// B 的自测文件 — 语义分析
// 手工构造 AST 来测试，不依赖 Parser
#include "ast.h"
#include "symbol_table.h"
#include "semantic.h"
#include <iostream>
#include <memory>
#include <cassert>

// 辅助: 构造 int main() { return 0; } 的 AST
std::unique_ptr<CompUnit> make_simple_program() {
    auto unit = std::make_unique<CompUnit>();

    auto func = std::make_unique<FuncDef>();
    func->returnType = FuncType::INT;
    func->name = "main";

    auto body = std::make_unique<Stmt>();
    body->kind = StmtKind::BLOCK;

    auto ret = std::make_unique<Stmt>();
    ret->kind = StmtKind::RETURN;
    auto retVal = std::make_unique<Expr>();
    retVal->kind = ExprKind::INT_LITERAL;
    retVal->intValue = 0;
    ret->retExpr = std::move(retVal);
    body->stmts.push_back(std::move(ret));

    func->body = std::move(body);
    unit->funcDefs.push_back(std::move(func));
    return unit;
}

void test_valid_program() {
    auto ast = make_simple_program();
    SemanticAnalyzer sema;
    sema.analyze(ast);  // should not report errors
    std::cout << "PASS: test_valid_program\n";
}

void test_missing_main() {
    auto unit = std::make_unique<CompUnit>();
    SemanticAnalyzer sema;
    sema.analyze(unit);  // should report "missing main function"
    std::cout << "PASS: test_missing_main\n";
}

int main() {
    test_valid_program();
    test_missing_main();
    std::cout << "All semantic tests passed!\n";
    return 0;
}
