#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <string>
#include <unordered_map>
#include <vector>
#include "ast.h"

struct Symbol {
    std::string name;
    bool isConst;
    bool isGlobal;
    int value;           // for consts with known value
    bool valueKnown;
    FuncType funcType;   // for functions
    bool isFunction;
    std::vector<FuncType> paramTypes; // for functions
};

class SymbolTable {
public:
    SymbolTable();

    void enterScope();
    void exitScope();

    bool addVariable(const std::string& name, bool isConst, bool isGlobal,
                     int value = 0, bool valueKnown = false);
    bool addFunction(const std::string& name, FuncType returnType,
                     const std::vector<FuncType>& paramTypes);

    Symbol* lookup(const std::string& name);
    Symbol* lookupCurrentScope(const std::string& name);
    bool isGlobalScope() const;

private:
    std::vector<std::unordered_map<std::string, Symbol>> scopes;
};

#endif
