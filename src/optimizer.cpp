#include "optimizer.h"
#include <unordered_set>
#include <unordered_map>

IRList Optimizer::optimize(const IRList& ir) {
    IRList result = constantFolding(ir);
    result = deadCodeElimination(result);
    return result;
}

IRList Optimizer::constantFolding(const IRList& ir) {
    IRList result;
    std::unordered_map<std::string, int> constValues;

    auto isConst = [&](const std::string& name) -> bool {
        return constValues.find(name) != constValues.end();
    };

    for (const auto& instr : ir) {
        if (instr.op == IROp::LOAD_IMM) {
            constValues[instr.dst] = instr.intValue;
            result.push_back(instr);
        } else if (instr.op == IROp::ADD || instr.op == IROp::SUB ||
                   instr.op == IROp::MUL || instr.op == IROp::DIV ||
                   instr.op == IROp::MOD) {
            bool lConst = isConst(instr.src1);
            bool rConst = isConst(instr.src2);
            if (lConst && rConst) {
                int lv = constValues[instr.src1];
                int rv = constValues[instr.src2];
                int res = 0;
                switch (instr.op) {
                    case IROp::ADD: res = lv + rv; break;
                    case IROp::SUB: res = lv - rv; break;
                    case IROp::MUL: res = lv * rv; break;
                    case IROp::DIV: res = lv / rv; break;
                    case IROp::MOD: res = lv % rv; break;
                }
                constValues[instr.dst] = res;
                result.push_back(IRInstr::li(instr.dst, res));
            } else {
                result.push_back(instr);
            }
        } else if (instr.op == IROp::ASSIGN) {
            if (isConst(instr.src1)) {
                constValues[instr.dst] = constValues[instr.src1];
            }
            result.push_back(instr);
        } else {
            result.push_back(instr);
        }
    }

    return result;
}

IRList Optimizer::deadCodeElimination(const IRList& ir) {
    // Count uses of each temporary variable
    std::unordered_map<std::string, int> useCount;
    for (const auto& instr : ir) {
        if (!instr.src1.empty() && instr.src1[0] == '%')
            useCount[instr.src1]++;
        if (!instr.src2.empty() && instr.src2[0] == '%')
            useCount[instr.src2]++;
    }

    IRList result;
    for (const auto& instr : ir) {
        // If dst is a temp with 0 uses, skip this instruction
        bool isDead = false;
        if (!instr.dst.empty() && instr.dst[0] == '%') {
            if (useCount[instr.dst] == 0) {
                // Skip instructions whose result is never used
                // (except for control flow / store / call / return)
                switch (instr.op) {
                    case IROp::STORE: case IROp::CALL: case IROp::RETURN:
                    case IROp::GOTO: case IROp::IF_GOTO: case IROp::LABEL:
                    case IROp::FUNC_BEGIN: case IROp::FUNC_END:
                        isDead = false;
                        break;
                    default:
                        isDead = true;
                        break;
                }
            }
        }
        if (!isDead) {
            result.push_back(instr);
        }
    }
    return result;
}
