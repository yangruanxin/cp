#ifndef CODEGEN_H
#define CODEGEN_H

#include "ir.h"
#include <string>
#include <unordered_map>

class CodeGenerator {
public:
    CodeGenerator();
    std::string generate(const IRList& ir);

private:
    std::string output;
    int labelCounter;
    int stackSize;
    std::unordered_map<std::string, int> varOffsets;
    std::unordered_map<std::string, int> globalAddrs;

    void emit(const std::string& code);
    std::string newLabel();
    std::string getReg(const std::string& irOperand);
    int getVarOffset(const std::string& name);

    // Track current function
    std::string currentFunc;
    int currentStackSize;
    bool isGlobalScope;
};

#endif
