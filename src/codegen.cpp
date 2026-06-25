#include "codegen.h"
#include <iostream>

CodeGenerator::CodeGenerator() : labelCounter(0), stackSize(0),
    currentStackSize(0), isGlobalScope(true) {}

void CodeGenerator::emit(const std::string& code) {
    output += code + "\n";
}

std::string CodeGenerator::newLabel() {
    return ".L" + std::to_string(labelCounter++);
}

std::string CodeGenerator::getReg(const std::string& irOperand) {
    if (irOperand.empty()) return "zero";
    if (irOperand[0] == '%') {
        // Temporary IR variable -> map to t0-t6
        int idx = std::stoi(irOperand.substr(2));
        return "t" + std::to_string(idx % 7);
    }
    if (irOperand == "a0" || irOperand == "a1" || irOperand == "a7") {
        return irOperand;
    }
    // Named variable -> load via stack
    return "t0";
}

int CodeGenerator::getVarOffset(const std::string& name) {
    auto it = varOffsets.find(name);
    if (it != varOffsets.end()) return it->second;
    int off = currentStackSize;
    varOffsets[name] = off;
    currentStackSize += 4;
    stackSize = currentStackSize;
    return off;
}

std::string CodeGenerator::generate(const IRList& ir) {
    emit(".text");
    isGlobalScope = true;

    for (size_t i = 0; i < ir.size(); i++) {
        const auto& instr = ir[i];

        switch (instr.op) {
            case IROp::FUNC_BEGIN: {
                currentFunc = instr.label;
                currentStackSize = 0;
                varOffsets.clear();
                isGlobalScope = false;
                emit(".globl " + currentFunc);
                emit(currentFunc + ":");
                // Prologue - stack size adjusted at FUNC_END
                break;
            }
            case IROp::FUNC_END: {
                // Align stack to 16 bytes
                int alignedSize = (currentStackSize + 15) / 16 * 16;
                if (alignedSize < 16) alignedSize = 16;

                // Rewrite prologue with correct size
                // For simplicity, use fixed 32-byte frame
                int frameSize = 32;
                emit("    addi sp, sp, -" + std::to_string(frameSize));
                emit("    sw ra, " + std::to_string(frameSize - 4) + "(sp)");
                emit("    sw s0, " + std::to_string(frameSize - 8) + "(sp)");
                emit("    addi s0, sp, " + std::to_string(frameSize));
                // Epilogue
                emit("    lw ra, " + std::to_string(frameSize - 4) + "(sp)");
                emit("    lw s0, " + std::to_string(frameSize - 8) + "(sp)");
                emit("    addi sp, sp, " + std::to_string(frameSize));
                emit("    jalr zero, ra, 0");
                break;
            }
            case IROp::LABEL:
                emit(instr.label + ":");
                break;
            case IROp::GOTO:
                emit("    j " + instr.label);
                break;
            case IROp::IF_GOTO: {
                std::string reg = getReg(instr.src1);
                emit("    bnez " + reg + ", " + instr.label);
                break;
            }
            case IROp::LOAD_IMM: {
                std::string reg = getReg(instr.dst);
                emit("    li " + reg + ", " + std::to_string(instr.intValue));
                break;
            }
            case IROp::ADD: case IROp::SUB: case IROp::MUL:
            case IROp::DIV: case IROp::MOD:
            case IROp::LT: case IROp::GT: case IROp::LE: case IROp::GE:
            case IROp::EQ: case IROp::NE: {
                static const std::unordered_map<IROp, std::string> rvAsm = {
                    {IROp::ADD, "add"}, {IROp::SUB, "sub"}, {IROp::MUL, "mul"},
                    {IROp::DIV, "div"}, {IROp::MOD, "rem"},
                    {IROp::LT, "slt"}, {IROp::GT, "sgt"},
                    {IROp::LE, "sle"}, {IROp::GE, "sge"},
                    {IROp::EQ, "sub"}, {IROp::NE, "sub"},
                };
                std::string d = getReg(instr.dst);
                std::string s1 = getReg(instr.src1);
                std::string s2 = getReg(instr.src2);
                std::string asmOp = rvAsm.at(instr.op);
                emit("    " + asmOp + " " + d + ", " + s1 + ", " + s2);
                if (instr.op == IROp::EQ) {
                    emit("    seqz " + d + ", " + d);
                } else if (instr.op == IROp::NE) {
                    emit("    snez " + d + ", " + d);
                }
                break;
            }
            case IROp::NEG: {
                std::string d = getReg(instr.dst);
                std::string s = getReg(instr.src1);
                emit("    neg " + d + ", " + s);
                break;
            }
            case IROp::NOT: {
                std::string d = getReg(instr.dst);
                std::string s = getReg(instr.src1);
                emit("    seqz " + d + ", " + s);
                break;
            }
            case IROp::ASSIGN: {
                std::string d = getReg(instr.dst);
                std::string s = getReg(instr.src1);
                if (d != s) {
                    emit("    mv " + d + ", " + s);
                }
                break;
            }
            case IROp::DECL: {
                // Variable declaration - allocate stack space later in prologue
                // For now, just track it
                if (instr.src1 == "global") {
                    emit("    .data");
                    emit("    .globl " + instr.dst);
                    emit(instr.dst + ":");
                    emit("    .word " + std::to_string(instr.intValue));
                    emit("    .text");
                }
                break;
            }
            case IROp::LOAD: {
                std::string d = getReg(instr.dst);
                std::string base = getReg(instr.src1);
                int offset = instr.intValue;
                emit("    lw " + d + ", " + std::to_string(offset) + "(" + base + ")");
                break;
            }
            case IROp::STORE: {
                std::string base = getReg(instr.dst);
                std::string val = getReg(instr.src1);
                int offset = instr.intValue;
                emit("    sw " + val + ", " + std::to_string(offset) + "(" + base + ")");
                break;
            }
            case IROp::CALL: {
                // Args already in a0-a7 via ARG instructions
                // For now, naive call
                emit("    jal ra, " + instr.label);
                std::string d = getReg(instr.dst);
                emit("    mv " + d + ", a0");
                break;
            }
            case IROp::RETURN:
                emit("    jalr zero, ra, 0");
                break;
            case IROp::ARG: {
                std::string val = getReg(instr.src1);
                int idx = instr.intValue;
                std::string aReg = "a" + std::to_string(idx);
                emit("    mv " + aReg + ", " + val);
                break;
            }
        }
    }

    return output;
}
