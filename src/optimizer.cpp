#include "optimizer.h"
#include <unordered_map>
#include <utility>

// GCC 6.3 没有 std::optional，用 pair 替代
struct OptInt {
    bool has = false;
    int value = 0;
    OptInt() = default;
    OptInt(int v) : has(true), value(v) {}
};

IRList Optimizer::optimize(const IRList& ir) {
    IRList result = constantFolding(ir);
    while (true) {
        IRList next = deadCodeElimination(result);
        if (next.size() == result.size()) {
            result = std::move(next);
            break;
        }
        result = std::move(next);
    }
    return result;
}

namespace {

bool isTemp(const std::string& name) {
    return !name.empty() && name[0] == '%';
}

bool hasControlFlowBoundary(IROp op) {
    switch (op) {
        case IROp::FUNC_BEGIN:
        case IROp::FUNC_END:
        case IROp::LABEL:
        case IROp::GOTO:
        case IROp::IF_GOTO:
        case IROp::CALL:
            return true;
        default:
            return false;
    }
}

bool hasSideEffect(IROp op) {
    switch (op) {
        case IROp::STORE:
        case IROp::CALL:
        case IROp::RETURN:
        case IROp::GOTO:
        case IROp::IF_GOTO:
        case IROp::LABEL:
        case IROp::FUNC_BEGIN:
        case IROp::FUNC_END:
        case IROp::DECL:
        case IROp::ARG:
            return true;
        default:
            return false;
    }
}

bool isBinaryOp(IROp op) {
    switch (op) {
        case IROp::ADD: case IROp::SUB: case IROp::MUL:
        case IROp::DIV: case IROp::MOD:
        case IROp::LT:  case IROp::GT:  case IROp::LE:
        case IROp::GE:  case IROp::EQ:  case IROp::NE:
        case IROp::AND: case IROp::OR:
            return true;
        default:
            return false;
    }
}

OptInt foldBinary(IROp op, int lhs, int rhs) {
    switch (op) {
        case IROp::ADD: return lhs + rhs;
        case IROp::SUB: return lhs - rhs;
        case IROp::MUL: return lhs * rhs;
        case IROp::DIV:
            if (rhs == 0) return OptInt();
            return lhs / rhs;
        case IROp::MOD:
            if (rhs == 0) return OptInt();
            return lhs % rhs;
        case IROp::LT:  return lhs <  rhs ? 1 : 0;
        case IROp::GT:  return lhs >  rhs ? 1 : 0;
        case IROp::LE:  return lhs <= rhs ? 1 : 0;
        case IROp::GE:  return lhs >= rhs ? 1 : 0;
        case IROp::EQ:  return lhs == rhs ? 1 : 0;
        case IROp::NE:  return lhs != rhs ? 1 : 0;
        case IROp::AND: return (lhs != 0 && rhs != 0) ? 1 : 0;
        case IROp::OR:  return (lhs != 0 || rhs != 0) ? 1 : 0;
        default:        return OptInt();
    }
}

OptInt foldUnary(IROp op, int value) {
    switch (op) {
        case IROp::NEG: return -value;
        case IROp::NOT: return value == 0 ? 1 : 0;
        default:        return OptInt();
    }
}

} // namespace

IRList Optimizer::constantFolding(const IRList& ir) {
    IRList result;
    std::unordered_map<std::string, int> constValues;

    auto getConst = [&](const std::string& name) -> OptInt {
        auto it = constValues.find(name);
        if (it == constValues.end()) return OptInt();
        return it->second;
    };

    for (const auto& instr : ir) {
        if (hasControlFlowBoundary(instr.op)) {
            constValues.clear();
            result.push_back(instr);
            continue;
        }

        if (instr.op == IROp::LOAD_IMM) {
            constValues[instr.dst] = instr.intValue;
            result.push_back(instr);
        } else if (isBinaryOp(instr.op)) {
            auto lhs = getConst(instr.src1);
            auto rhs = getConst(instr.src2);
            if (lhs.has && rhs.has) {
                auto folded = foldBinary(instr.op, lhs.value, rhs.value);
                if (folded.has) {
                    constValues[instr.dst] = folded.value;
                    result.push_back(IRInstr::li(instr.dst, folded.value));
                } else {
                    constValues.erase(instr.dst);
                    result.push_back(instr);
                }
            } else {
                constValues.erase(instr.dst);
                result.push_back(instr);
            }
        } else if (instr.op == IROp::NEG || instr.op == IROp::NOT) {
            auto value = getConst(instr.src1);
            if (value.has) {
                auto folded = foldUnary(instr.op, value.value);
                constValues[instr.dst] = folded.value;
                result.push_back(IRInstr::li(instr.dst, folded.value));
            } else {
                constValues.erase(instr.dst);
                result.push_back(instr);
            }
        } else if (instr.op == IROp::ASSIGN) {
            auto value = getConst(instr.src1);
            if (value.has) {
                constValues[instr.dst] = value.value;
                result.push_back(IRInstr::li(instr.dst, value.value));
            } else {
                constValues.erase(instr.dst);
                result.push_back(instr);
            }
        } else {
            if (!instr.dst.empty()) constValues.erase(instr.dst);
            result.push_back(instr);
        }
    }

    return result;
}

IRList Optimizer::deadCodeElimination(const IRList& ir) {
    // Count uses of each temporary variable
    std::unordered_map<std::string, int> useCount;
    for (const auto& instr : ir) {
        if (isTemp(instr.src1))
            useCount[instr.src1]++;
        if (isTemp(instr.src2))
            useCount[instr.src2]++;
    }

    IRList result;
    for (const auto& instr : ir) {
        // If dst is a temp with 0 uses, skip this instruction
        bool isDead = false;
        if (isTemp(instr.dst)) {
            if (useCount[instr.dst] == 0) {
                isDead = !hasSideEffect(instr.op);
            }
        }
        if (!isDead) {
            result.push_back(instr);
        }
    }
    return result;
}
