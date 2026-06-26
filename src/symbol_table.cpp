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

bool SymbolTable::addVariable(const std::string& name, bool isConst,
                               bool isGlobal, int value, bool valueKnown) {
    if (lookupCurrentScope(name)) {
        return false;
    }
    Symbol sym;
    sym.name = name;
    sym.isConst = isConst;
    sym.isGlobal = isGlobal;
    sym.value = value;
    sym.valueKnown = valueKnown;
    sym.isFunction = false;
    scopes.back()[name] = sym;
    return true;
}

bool SymbolTable::addFunction(const std::string& name, FuncType returnType,
                               const std::vector<FuncType>& paramTypes) {
    if (scopes[0].find(name) != scopes[0].end()) {
        return false;
    }
    Symbol sym;
    sym.name = name;
    sym.isConst = false;
    sym.isGlobal = true;
    sym.valueKnown = false;
    sym.isFunction = true;
    sym.funcType = returnType;
    sym.paramTypes = paramTypes;
    scopes[0][name] = sym;
    return true;
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

Symbol* SymbolTable::lookupCurrentScope(const std::string& name) {
    auto found = scopes.back().find(name);
    if (found != scopes.back().end()) {
        return &found->second;
    }
    return nullptr;
}

bool SymbolTable::isGlobalScope() const {
    return scopes.size() == 1;
}
