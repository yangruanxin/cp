#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"
#include "symbol_table.h"
#include "ir.h"
#include <memory>
#include <vector>

struct ConstEvalResult {
    bool known = false;
    int value = 0;
};

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
    bool hasError;
    std::string loopBeginLabel;
    std::string loopEndLabel;

    std::string newLabel();
    std::string newTemp();
    void error(const std::string& message);
    ConstEvalResult evalConstExpr(const std::unique_ptr<Expr>& expr);
    void defineVariable(const std::string& name, bool isConst, bool isGlobal,
                        int value = 0, bool valueKnown = false);
    std::string makeBool(const std::string& value);

    void visitCompUnit(const std::unique_ptr<CompUnit>& unit);
    void visitFuncDef(const std::unique_ptr<FuncDef>& func);
    bool visitStmt(const std::unique_ptr<Stmt>& stmt);
    std::string visitExpr(const std::unique_ptr<Expr>& expr, bool allowVoid = false);
};

#endif
