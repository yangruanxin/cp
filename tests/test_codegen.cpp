// C 的自测文件 — 代码生成
// 手工构造 AST 来测试，不依赖 Parser 或 Semantic
#include "ast.h"
#include "codegen.h"
#include <iostream>
#include <cassert>

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
    retVal->intValue = 42;
    ret->retExpr = std::move(retVal);
    body->stmts.push_back(std::move(ret));

    func->body = std::move(body);
    unit->funcDefs.push_back(std::move(func));
    return unit;
}

void test_simple_return() {
    auto ast = make_simple_program();
    CodeGenerator cg;
    std::string asmOutput = cg.generate(ast);
    // Should contain: li a0, 42
    assert(asmOutput.find("li") != std::string::npos);
    std::cout << "PASS: test_simple_return\n";
    std::cout << "Generated assembly:\n" << asmOutput << "\n";
}

int main() {
    test_simple_return();
    std::cout << "All codegen tests passed!\n";
    return 0;
}
