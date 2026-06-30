#include "optimizer.h"
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <utility>

IRList Optimizer::optimize(const IRList& ir) {
    IRList result = ir;
    for (int i = 0; i < 6; i++) {
        IRList next = copyPropagation(result);
        next = constantFolding(next);
        next = simplifyJumps(next);
        while (true) {
            IRList dce = deadCodeElimination(next);
            if (dce.size() == next.size()) {
                next = std::move(dce);
                break;
            }
            next = std::move(dce);
        }
        if (next.size() == result.size()) {
            result = std::move(next);
            break;
        }
        result = std::move(next);
    }
    return result;
}

IRList Optimizer::simplifyJumps(const IRList& ir) {
    IRList result;
    for (size_t i = 0; i < ir.size(); i++) {
        const auto& instr = ir[i];
        bool skip = false;
        if (instr.op == IROp::GOTO) {
            size_t j = i + 1;
            while (j < ir.size() && ir[j].op == IROp::LABEL) {
                if (ir[j].label == instr.label) {
                    skip = true;
                }
                break;
            }
        }
        if (!skip) result.push_back(instr);
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

std::optional<int> foldBinary(IROp op, int lhs, int rhs) {
    switch (op) {
        case IROp::ADD: return lhs + rhs;
        case IROp::SUB: return lhs - rhs;
        case IROp::MUL: return lhs * rhs;
        case IROp::DIV:
            if (rhs == 0) return std::nullopt;
            return lhs / rhs;
        case IROp::MOD:
            if (rhs == 0) return std::nullopt;
            return lhs % rhs;
        case IROp::LT:  return lhs <  rhs ? 1 : 0;
        case IROp::GT:  return lhs >  rhs ? 1 : 0;
        case IROp::LE:  return lhs <= rhs ? 1 : 0;
        case IROp::GE:  return lhs >= rhs ? 1 : 0;
        case IROp::EQ:  return lhs == rhs ? 1 : 0;
        case IROp::NE:  return lhs != rhs ? 1 : 0;
        case IROp::AND: return (lhs != 0 && rhs != 0) ? 1 : 0;
        case IROp::OR:  return (lhs != 0 || rhs != 0) ? 1 : 0;
        default:        return std::nullopt;
    }
}

std::optional<int> foldUnary(IROp op, int value) {
    switch (op) {
        case IROp::NEG: return -value;
        case IROp::NOT: return value == 0 ? 1 : 0;
        default:        return std::nullopt;
    }
}

std::string resolveCopy(const std::unordered_map<std::string, std::string>& copies,
                        const std::string& name) {
    std::string cur = name;
    for (int i = 0; i < 16; i++) {
        auto it = copies.find(cur);
        if (it == copies.end()) break;
        cur = it->second;
    }
    return cur;
}

void eraseCopiesTo(std::unordered_map<std::string, std::string>& copies,
                   const std::string& dst) {
    if (dst.empty()) return;
    copies.erase(dst);
    for (auto it = copies.begin(); it != copies.end(); ) {
        if (it->second == dst) it = copies.erase(it);
        else ++it;
    }
}

} // namespace

IRList Optimizer::copyPropagation(const IRList& ir) {
    IRList result;
    std::unordered_map<std::string, std::string> copies;

    for (auto instr : ir) {
        if (hasControlFlowBoundary(instr.op)) {
            copies.clear();
            result.push_back(instr);
            continue;
        }

        if (!instr.src1.empty()) instr.src1 = resolveCopy(copies, instr.src1);
        if (!instr.src2.empty()) instr.src2 = resolveCopy(copies, instr.src2);

        if (!instr.dst.empty()) eraseCopiesTo(copies, instr.dst);

        if (instr.op == IROp::ASSIGN && isTemp(instr.dst) && !instr.src1.empty()) {
            copies[instr.dst] = instr.src1;
        }

        if (instr.op == IROp::STORE || instr.op == IROp::CALL) {
            copies.clear();
        }

        result.push_back(instr);
    }

    return result;
}

IRList Optimizer::constantFolding(const IRList& ir) {
    IRList result;
    std::unordered_map<std::string, int> constValues;

    auto getConst = [&](const std::string& name) -> std::optional<int> {
        auto it = constValues.find(name);
        if (it == constValues.end()) return std::nullopt;
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
            if (lhs && rhs) {
                auto folded = foldBinary(instr.op, *lhs, *rhs);
                if (folded) {
                    constValues[instr.dst] = *folded;
                    result.push_back(IRInstr::li(instr.dst, *folded));
                } else {
                    constValues.erase(instr.dst);
                    result.push_back(instr);
                }
            } else {
                constValues.erase(instr.dst);
                if (instr.op == IROp::ADD && rhs && *rhs == 0) {
                    result.push_back(IRInstr::assign(instr.dst, instr.src1));
                } else if (instr.op == IROp::ADD && lhs && *lhs == 0) {
                    result.push_back(IRInstr::assign(instr.dst, instr.src2));
                } else if (instr.op == IROp::SUB && rhs && *rhs == 0) {
                    result.push_back(IRInstr::assign(instr.dst, instr.src1));
                } else if (instr.op == IROp::MUL && ((rhs && *rhs == 1) || (lhs && *lhs == 1))) {
                    result.push_back(IRInstr::assign(instr.dst, (rhs && *rhs == 1) ? instr.src1 : instr.src2));
                } else if (instr.op == IROp::MUL && ((rhs && *rhs == 0) || (lhs && *lhs == 0))) {
                    constValues[instr.dst] = 0;
                    result.push_back(IRInstr::li(instr.dst, 0));
                } else if (instr.op == IROp::DIV && rhs && *rhs == 1) {
                    result.push_back(IRInstr::assign(instr.dst, instr.src1));
                } else if (instr.op == IROp::MOD && rhs && *rhs == 1) {
                    constValues[instr.dst] = 0;
                    result.push_back(IRInstr::li(instr.dst, 0));
                } else if ((instr.op == IROp::EQ || instr.op == IROp::LE || instr.op == IROp::GE) &&
                           instr.src1 == instr.src2) {
                    constValues[instr.dst] = 1;
                    result.push_back(IRInstr::li(instr.dst, 1));
                } else if ((instr.op == IROp::NE || instr.op == IROp::LT || instr.op == IROp::GT) &&
                           instr.src1 == instr.src2) {
                    constValues[instr.dst] = 0;
                    result.push_back(IRInstr::li(instr.dst, 0));
                } else {
                    result.push_back(instr);
                }
            }
        } else if (instr.op == IROp::NEG || instr.op == IROp::NOT) {
            auto value = getConst(instr.src1);
            if (value) {
                auto folded = foldUnary(instr.op, *value);
                constValues[instr.dst] = *folded;
                result.push_back(IRInstr::li(instr.dst, *folded));
            } else {
                constValues.erase(instr.dst);
                result.push_back(instr);
            }
        } else if (instr.op == IROp::ASSIGN) {
            auto value = getConst(instr.src1);
            if (value) {
                constValues[instr.dst] = *value;
                result.push_back(IRInstr::li(instr.dst, *value));
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
    std::unordered_set<std::string> globals;
    std::unordered_map<std::string, int> localLoadCount;
    std::unordered_map<std::string, int> localRefCount;
    for (const auto& instr : ir) {
        if (isTemp(instr.src1))
            useCount[instr.src1]++;
        if (isTemp(instr.src2))
            useCount[instr.src2]++;
        if (instr.op == IROp::DECL && instr.src1 == "global")
            globals.insert(instr.dst);
        if (instr.op == IROp::LOAD && !isTemp(instr.src1)) {
            localLoadCount[instr.src1]++;
            localRefCount[instr.src1]++;
        }
        if (instr.op == IROp::STORE && !isTemp(instr.dst)) {
            localRefCount[instr.dst]++;
        }
    }

    IRList result;
    for (const auto& instr : ir) {
        // If dst is a temp with 0 uses, skip this instruction
        bool isDead = false;
        if (isTemp(instr.dst)) {
            if (useCount[instr.dst] == 0) {
                isDead = !hasSideEffect(instr.op);
            }
        } else if (instr.op == IROp::STORE && !globals.count(instr.dst) &&
                   localLoadCount[instr.dst] == 0) {
            isDead = true;
        } else if (instr.op == IROp::DECL && instr.src1 == "local" &&
                   localRefCount[instr.dst] == 0) {
            isDead = true;
        }
        if (!isDead) {
            result.push_back(instr);
        }
    }
    return result;
}
