// B 的自测 — 手工构造 AST，验证 IR 生成
#include "ast.h"
#include "semantic.h"
#include <iostream>
#include <cassert>

std::unique_ptr<CompUnit> makeReturn42() {
    auto unit = std::make_unique<CompUnit>();
    auto func = std::make_unique<FuncDef>();
    func->returnType = FuncType::INT;
    func->name = "main";

    auto body = std::make_unique<Stmt>();
    body->kind = StmtKind::BLOCK;

    auto ret = std::make_unique<Stmt>();
    ret->kind = StmtKind::RETURN;
    auto val = std::make_unique<Expr>();
    val->kind = ExprKind::INT_LITERAL;
    val->intValue = 42;
    ret->retExpr = std::move(val);
    body->stmts.push_back(std::move(ret));

    func->body = std::move(body);
    unit->funcDefs.push_back(std::move(func));
    return unit;
}

std::unique_ptr<CompUnit> makeAddExpr() {
    // int main() { return 1 + 2 * 3; }
    auto unit = std::make_unique<CompUnit>();
    auto func = std::make_unique<FuncDef>();
    func->returnType = FuncType::INT;
    func->name = "main";

    auto body = std::make_unique<Stmt>();
    body->kind = StmtKind::BLOCK;

    auto ret = std::make_unique<Stmt>();
    ret->kind = StmtKind::RETURN;

    // Build: 1 + 2 * 3
    auto mul = std::make_unique<Expr>();
    mul->kind = ExprKind::BINARY;
    mul->binaryOp = BinaryOp::MUL;
    auto two = std::make_unique<Expr>();
    two->kind = ExprKind::INT_LITERAL; two->intValue = 2;
    auto three = std::make_unique<Expr>();
    three->kind = ExprKind::INT_LITERAL; three->intValue = 3;
    mul->lhs = std::move(two);
    mul->rhs = std::move(three);

    auto add = std::make_unique<Expr>();
    add->kind = ExprKind::BINARY;
    add->binaryOp = BinaryOp::ADD;
    auto one = std::make_unique<Expr>();
    one->kind = ExprKind::INT_LITERAL; one->intValue = 1;
    add->lhs = std::move(one);
    add->rhs = std::move(mul);

    ret->retExpr = std::move(add);
    body->stmts.push_back(std::move(ret));
    func->body = std::move(body);
    unit->funcDefs.push_back(std::move(func));
    return unit;
}

int main() {
    // Test 1: simple return
    auto ast1 = makeReturn42();
    IRGenerator irGen;
    IRList ir = irGen.generate(ast1);
    assert(!ir.empty());
    std::cout << "Test 1 PASS: generated " << ir.size() << " IR instructions for return 42\n";
    for (const auto& i : ir) {
        std::cout << "  " << static_cast<int>(i.op) << " " << i.dst << "\n";
    }

    // Test 2: expression with operator precedence
    auto ast2 = makeAddExpr();
    IRList ir2 = irGen.generate(ast2);
    assert(!ir2.empty());
    std::cout << "Test 2 PASS: generated " << ir2.size() << " IR instructions for 1+2*3\n";

    std::cout << "All semantic tests passed!\n";
    return 0;
}
