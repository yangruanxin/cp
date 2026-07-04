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
    bool hasErrors() const { return hasError; }

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

    // 作用域遮蔽管理：为每个变量分配唯一 IR 名称
    int declIdCounter;
    std::vector<std::unordered_map<std::string, std::string>> nameStack;
    void enterScope();
    void exitScope();
    std::string getIRName(const std::string& srcName);
    std::string declareIRName(const std::string& srcName);

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
