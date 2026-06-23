#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include <string>
#include <memory>
#include <unordered_map>

class CodeGenerator {
public:
    CodeGenerator();
    std::string generate(const std::unique_ptr<CompUnit>& unit);

private:
    std::string output;
    int labelCounter;
    int stackOffset;
    std::unordered_map<std::string, int> varOffsets;

    void emit(const std::string& code);
    std::string newLabel();

    void genCompUnit(const std::unique_ptr<CompUnit>& unit);
    void genFuncDef(const std::unique_ptr<FuncDef>& func);
    void genStmt(const std::unique_ptr<Stmt>& stmt);
    std::string genExpr(const std::unique_ptr<Expr>& expr);
    void genPrologue(const std::string& name, int stackSize);
    void genEpilogue(const std::string& name, int stackSize);
};

#endif
