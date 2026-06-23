#include "symbol_table.h"

SymbolTable::SymbolTable() {
    scopes.emplace_back(); // global scope
}

void SymbolTable::enterScope() {
    scopes.emplace_back();
}

void SymbolTable::exitScope() {
    if (scopes.size() > 1) scopes.pop_back();
}

void SymbolTable::addVariable(const std::string& name, bool isConst,
                               bool isGlobal, int value, bool valueKnown) {
    Symbol sym;
    sym.name = name;
    sym.isConst = isConst;
    sym.isGlobal = isGlobal;
    sym.value = value;
    sym.valueKnown = valueKnown;
    sym.isFunction = false;
    scopes.back()[name] = sym;
}

void SymbolTable::addFunction(const std::string& name, FuncType returnType,
                               const std::vector<FuncType>& paramTypes) {
    Symbol sym;
    sym.name = name;
    sym.isConst = false;
    sym.isGlobal = true;
    sym.valueKnown = false;
    sym.isFunction = true;
    sym.funcType = returnType;
    sym.paramTypes = paramTypes;
    scopes[0][name] = sym;
}

Symbol* SymbolTable::lookup(const std::string& name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return &found->second;
        }
    }
    return nullptr;
}

bool SymbolTable::isGlobalScope() const {
    return scopes.size() == 1;
}
