#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"
#include "symbol_table.h"
#include "ir.h"
#include <memory>
#include <vector>

class IRGenerator {
public:
    IRGenerator();
    IRList generate(const std::unique_ptr<CompUnit>& unit);

private:
    SymbolTable symTable;
    IRList ir;
    int labelCounter;
    int tempCounter;
    bool inLoop;
    FuncType currentFuncReturnType;
    bool hasReturn;
    std::string loopBeginLabel;
    std::string loopEndLabel;

    std::string newLabel();
    std::string newTemp();

    void visitCompUnit(const std::unique_ptr<CompUnit>& unit);
    void visitFuncDef(const std::unique_ptr<FuncDef>& func);
    void visitStmt(const std::unique_ptr<Stmt>& stmt);
    std::string visitExpr(const std::unique_ptr<Expr>& expr);
};

#endif
