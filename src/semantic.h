#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"
#include "symbol_table.h"
#include <memory>

class SemanticAnalyzer {
public:
    SemanticAnalyzer();
    void analyze(const std::unique_ptr<CompUnit>& unit);

private:
    SymbolTable symTable;
    bool inLoop;
    FuncType currentFuncReturnType;
    bool hasReturn;

    void visitCompUnit(const std::unique_ptr<CompUnit>& unit);
    void visitFuncDef(const std::unique_ptr<FuncDef>& func);
    void visitStmt(const std::unique_ptr<Stmt>& stmt);
    int visitExpr(const std::unique_ptr<Expr>& expr);
    void checkReturnPaths(const std::unique_ptr<Stmt>& stmt);
};

#endif
