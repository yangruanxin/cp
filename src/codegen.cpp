#include "codegen.h"
#include <algorithm>

// ------------------------------------------------------------
// 工具
// ------------------------------------------------------------
void CodeGenerator::emit(const std::string& s) {
    out += s;
    out += '\n';
}

static int align16(int n) {
    return (n + 15) / 16 * 16;
}

bool CodeGenerator::isAReg(const std::string& op) const {
    return op.size() == 2 && op[0] == 'a' && op[1] >= '0' && op[1] <= '7';
}

std::string CodeGenerator::regForTemp(const std::string& op) const {
    if (op.size() < 3 || op[0] != '%' || op[1] != 't') return "";
    int n = 0;
    try { n = std::stoi(op.substr(2)); } catch (...) { return ""; }
    if (n >= 0 && n <= 6) return "t" + std::to_string(n);     // t0-t6
    if (n >= 7 && n <= 14) return "a" + std::to_string(n - 7); // a0-a7
    if (n == 15) return "s0";
    if (n == 16) return "s1";
    return "";
}

CodeGenerator::Kind CodeGenerator::classify(const std::string& op) const {
    if (op.empty()) return Kind::ZERO;
    if (op[0] == '%') {
        if (mappingEnabled && !regForTemp(op).empty()) return Kind::REGISTER;
        return Kind::SLOT;  // %t7+ 或 mappingDisabled → 栈
    }
    // 先查寄存器已分配的局部变量，再查栈槽（局部变量/形参），最后查全局。
    // 顺序重要：局部可以遮蔽同名全局。
    if (varRegMap.count(op)) return Kind::VAREG;         // 已分配物理寄存器的局部变量
    if (slotOff.count(op)) return Kind::SLOT;            // 已分配槽的局部/形参
    if (declaredLocals.count(op)) return Kind::SLOT;     // 声明的局部（保险）
    if (globals.count(op)) return Kind::GLOBAL;          // 全局变量/常量
    if (isAReg(op)) return Kind::AREG;                   // a0..a7 伪寄存器
    return Kind::UNKNOWN;
}

// ------------------------------------------------------------
// 在偏移量可能超过 12 位有符号立即数范围时使用辅助寄存器
// ------------------------------------------------------------
void CodeGenerator::emitSW(const std::string& reg, int offset, const std::string& base) {
    if (offset < -2048 || offset > 2047) {
        emit("    li t5, " + std::to_string(offset));
        emit("    add t5, " + base + ", t5");
        emit("    sw " + reg + ", 0(t5)");
    } else {
        emit("    sw " + reg + ", " + std::to_string(offset) + "(" + base + ")");
    }
}
void CodeGenerator::emitLW(const std::string& reg, int offset, const std::string& base) {
    if (offset < -2048 || offset > 2047) {
        emit("    li t5, " + std::to_string(offset));
        emit("    add t5, " + base + ", t5");
        emit("    lw " + reg + ", 0(t5)");
    } else {
        emit("    lw " + reg + ", " + std::to_string(offset) + "(" + base + ")");
    }
}

// ------------------------------------------------------------
// 载入 / 存储一个操作数
// ------------------------------------------------------------
void CodeGenerator::loadInto(const std::string& op, const std::string& target) {
    switch (classify(op)) {
        case Kind::REGISTER: {
            std::string reg = regForTemp(op);
            if (reg != target) emit("    mv " + target + ", " + reg);
            break;
        }
        case Kind::ZERO:
            emit("    li " + target + ", 0");
            break;
        case Kind::SLOT: {
            auto it = slotOff.find(op);
            int off = (it != slotOff.end()) ? it->second : 0;
            emitLW(target, off, "sp");
            break;
        }
        case Kind::VAREG: {
            std::string reg = varRegMap.at(op);
            if (reg != target) emit("    mv " + target + ", " + reg);
            break;
        }
        case Kind::GLOBAL:
            emit("    la " + target + ", " + op);
            emit("    lw " + target + ", 0(" + target + ")");
            break;
        case Kind::AREG:
            if (op != target) emit("    mv " + target + ", " + op);
            break;
        case Kind::UNKNOWN:
            // 兜底：当作 0，保证可汇编。
            emit("    li " + target + ", 0");
            break;
    }
}

void CodeGenerator::storeFromReg(const std::string& reg, const std::string& op,
                                 const std::string& scratch) {
    switch (classify(op)) {
        case Kind::REGISTER: {
            std::string dst = regForTemp(op);
            if (dst != reg) emit("    mv " + dst + ", " + reg);
            break;
        }
        case Kind::ZERO:
            break;  // 写入 zero/丢弃，无操作
        case Kind::SLOT: {
            auto it = slotOff.find(op);
            int off = (it != slotOff.end()) ? it->second : 0;
            emitSW(reg, off, "sp");
            break;
        }
        case Kind::VAREG: {
            std::string dst = varRegMap.at(op);
            if (dst != reg) emit("    mv " + dst + ", " + reg);
            break;
        }
        case Kind::GLOBAL:
            emit("    la " + scratch + ", " + op);
            emit("    sw " + reg + ", 0(" + scratch + ")");
            break;
        case Kind::AREG:
            if (op != reg) emit("    mv " + op + ", " + reg);
            break;
        case Kind::UNKNOWN:
            break;
    }
}

// ------------------------------------------------------------
// 全局数据段
// ------------------------------------------------------------
void CodeGenerator::emitGlobals(const IRList& ir) {
    std::vector<const IRInstr*> gs;
    for (const auto& in : ir) {
        if (in.op == IROp::DECL && in.src1 == "global") {
            globals.insert(in.dst);
            gs.push_back(&in);
        }
    }
    if (gs.empty()) return;
    emit("    .data");
    for (const auto* g : gs) {
        emit("    .globl " + g->dst);
        emit("    .align 2");
        emit(g->dst + ":");
        emit("    .word " + std::to_string(g->intValue));
    }
}

// ------------------------------------------------------------
// 形参识别
//   B 为每个形参在 FUNC_BEGIN 后紧跟发出 DECL(name,"local",0)，且不会
//   紧跟一条写回同名变量的 ASSIGN；而每个局部声明都形如
//   DECL(name) + ASSIGN(name,val)。据此可无歧义地区分形参。
// ------------------------------------------------------------
void CodeGenerator::collectParams(const IRList& ir, size_t begin, size_t end) {
    params.clear();
    for (size_t i = begin + 1; i < end; i++) {
        const auto& in = ir[i];
        if (in.op != IROp::DECL || in.src1 != "local") continue;
        // 向前扫描到同名的 ASSIGN（中间可以有 LI 等指令），如果遇到另一个
        // DECL、LABEL、FUNC_END 或 FUNC_BEGIN，则此 DECL 不是局部初始化而是形参。
        bool foundInit = false;
        for (size_t j = i + 1; j < end; j++) {
            const auto& next = ir[j];
            if (next.op == IROp::ASSIGN && next.dst == in.dst) {
                foundInit = true; break;
            }
            if (next.op == IROp::STORE && next.dst == in.dst) {
                foundInit = true; break;
            }
            if (next.op == IROp::DECL || next.op == IROp::LABEL ||
                next.op == IROp::FUNC_END || next.op == IROp::FUNC_BEGIN) {
                break;
            }
        }
        if (!foundInit) {
            params.push_back(in.dst);
        }
    }
}

// ------------------------------------------------------------
// 预扫描：收集需要栈槽的名字，计算帧布局
// ------------------------------------------------------------
void CodeGenerator::prescan(const IRList& ir, size_t begin, size_t end) {
    slotOff.clear();
    declaredLocals.clear();
    pendingArgs.clear();
    varRegMap.clear();
    mappingEnabled = false;

    collectParams(ir, begin, end);
    for (const auto& p : params) declaredLocals.insert(p);

    // 局部变量声明名
    for (size_t i = begin + 1; i < end; i++) {
        const auto& in = ir[i];
        if (in.op == IROp::DECL && in.src1 == "local") {
            declaredLocals.insert(in.dst);
        }
    }
    std::vector<std::string> order;  // 保持确定性的槽分配顺序
    auto want = [&](const std::string& op) {
        if (op.empty()) return;
        if (op[0] == '%') {
            if (mappingEnabled && !regForTemp(op).empty()) return;  // 寄存器映射，不占栈
            if (!slotOff.count(op)) { slotOff[op] = -1; order.push_back(op); }
            return;
        }
        // 先分配局部（含遮蔽全局的同名局部），再跳过全局
        if (declaredLocals.count(op)) {           // 局部/形参
            if (!slotOff.count(op)) { slotOff[op] = -1; order.push_back(op); }
            return;
        }
        if (globals.count(op)) return;            // 全局：不占栈
        // a0..a7 伪寄存器与未知名（如返回值 a0）不占栈。
    };

    int maxArgCount = 0;  // 出参个数上界（用于 >8 实参的栈传递）
    for (size_t i = begin + 1; i < end; i++) {
        const auto& in = ir[i];
        want(in.dst);
        want(in.src1);
        want(in.src2);
        if (in.op == IROp::ARG) {
            maxArgCount = std::max(maxArgCount, in.intValue + 1);
        }
    }
    outArgBytes = (maxArgCount > 8) ? (maxArgCount - 8) * 4 : 0;

    // 检查函数内是否有 CALL 指令
    bool hasCall = false;
    for (size_t i = begin; i < end && !hasCall; i++) {
        if (ir[i].op == IROp::CALL) hasCall = true;
    }
    // 无 CALL 时使用寄存器映射；有 CALL 时全栈（跨调用存活值在栈上安全）
    mappingEnabled = !hasCall;

    int idx = 0;
    for (const auto& name : order) {
        slotOff[name] = outArgBytes + idx * 4;
        idx++;
    }
    int numSlots = (int)order.size();
    raOff = outArgBytes + numSlots * 4;

    // 无 CALL 时，将局部变量分配到 callee-saved 寄存器 s2-s11
    if (mappingEnabled) {
        const char* sRegs[] = {"s2","s3","s4","s5","s6","s7","s8","s9","s10","s11"};
        int maxVarRegs = 10;
        int nextVarReg = 0;
        for (const auto& name : order) {
            if (nextVarReg >= maxVarRegs) break;
            if (!declaredLocals.count(name)) continue;
            // 形参保留在栈上（跨调用安全），不分配寄存器
            bool isParam = false;
            for (const auto& p : params) if (p == name) { isParam = true; break; }
            if (isParam) continue;
            varRegMap[name] = sRegs[nextVarReg++];
        }
    }
    frameSize = align16(raOff + 4);
}

// ------------------------------------------------------------
// 单条指令
// ------------------------------------------------------------
void CodeGenerator::genInstr(const IRInstr& in, bool nextIsFuncEnd) {
    switch (in.op) {
        case IROp::LABEL:
            emit(in.label + ":");
            break;
        case IROp::GOTO:
            emit("    j " + in.label);
            break;
        case IROp::IF_GOTO: {
            if (classify(in.src1) == Kind::REGISTER) {
                emit("    bnez " + regForTemp(in.src1) + ", " + in.label);
            } else {
                loadInto(in.src1, "t0");
                emit("    bnez t0, " + in.label);
            }
            break;
        }

        case IROp::LOAD_IMM: {
            if (classify(in.dst) == Kind::REGISTER) {
                emit("    li " + regForTemp(in.dst) + ", " + std::to_string(in.intValue));
            } else {
                emit("    li t0, " + std::to_string(in.intValue));
                storeFromReg("t0", in.dst, "t1");
            }
            break;
        }

        case IROp::ADD: case IROp::SUB: case IROp::MUL:
        case IROp::DIV: case IROp::MOD:
        case IROp::LT:  case IROp::GT:  case IROp::LE:
        case IROp::GE:  case IROp::EQ:  case IROp::NE:
        case IROp::AND: case IROp::OR: {
            // 如果 src1, src2, dst 都是寄存器映射，直接使用映射寄存器
            Kind k1 = classify(in.src1), k2 = classify(in.src2), kd = classify(in.dst);
            if (k1 == Kind::REGISTER && k2 == Kind::REGISTER && kd == Kind::REGISTER) {
                std::string r1 = regForTemp(in.src1), r2 = regForTemp(in.src2), rd = regForTemp(in.dst);
                auto bin = [&](const std::string& op, auto... args) {
                    // For operations needing intermediate value, use rd as both temp and dest
                };
                switch (in.op) {
                    case IROp::ADD: emit("    add " + rd + ", " + r1 + ", " + r2); break;
                    case IROp::SUB: emit("    sub " + rd + ", " + r1 + ", " + r2); break;
                    case IROp::MUL: emit("    mul " + rd + ", " + r1 + ", " + r2); break;
                    case IROp::DIV: emit("    div " + rd + ", " + r1 + ", " + r2); break;
                    case IROp::MOD: emit("    rem " + rd + ", " + r1 + ", " + r2); break;
                    case IROp::LT:  emit("    slt " + rd + ", " + r1 + ", " + r2); break;
                    case IROp::GT:  emit("    slt " + rd + ", " + r2 + ", " + r1); break;
                    case IROp::LE:  emit("    slt " + rd + ", " + r2 + ", " + r1);
                                    emit("    xori " + rd + ", " + rd + ", 1"); break;
                    case IROp::GE:  emit("    slt " + rd + ", " + r1 + ", " + r2);
                                    emit("    xori " + rd + ", " + rd + ", 1"); break;
                    case IROp::EQ:  emit("    sub " + rd + ", " + r1 + ", " + r2);
                                    emit("    seqz " + rd + ", " + rd); break;
                    case IROp::NE:  emit("    sub " + rd + ", " + r1 + ", " + r2);
                                    emit("    snez " + rd + ", " + rd); break;
                    case IROp::AND: emit("    snez " + rd + ", " + r1);
                                    emit("    snez " + r1 + ", " + r2);
                                    emit("    and " + rd + ", " + rd + ", " + r1); break;
                    case IROp::OR:  emit("    or " + rd + ", " + r1 + ", " + r2);
                                    emit("    snez " + rd + ", " + rd); break;
                    default: break;
                }
            } else {
                loadInto(in.src1, "t0");
                loadInto(in.src2, "t1");
                switch (in.op) {
                    case IROp::ADD: emit("    add t2, t0, t1"); break;
                    case IROp::SUB: emit("    sub t2, t0, t1"); break;
                    case IROp::MUL: emit("    mul t2, t0, t1"); break;
                    case IROp::DIV: emit("    div t2, t0, t1"); break;
                    case IROp::MOD: emit("    rem t2, t0, t1"); break;
                    case IROp::LT:  emit("    slt t2, t0, t1"); break;
                    case IROp::GT:  emit("    slt t2, t1, t0"); break;
                    case IROp::LE:  emit("    slt t2, t1, t0"); emit("    xori t2, t2, 1"); break;
                    case IROp::GE:  emit("    slt t2, t0, t1"); emit("    xori t2, t2, 1"); break;
                    case IROp::EQ:  emit("    sub t2, t0, t1"); emit("    seqz t2, t2"); break;
                    case IROp::NE:  emit("    sub t2, t0, t1"); emit("    snez t2, t2"); break;
                    case IROp::AND: emit("    snez t0, t0"); emit("    snez t1, t1");
                                    emit("    and t2, t0, t1"); break;
                    case IROp::OR:  emit("    or t2, t0, t1"); emit("    snez t2, t2"); break;
                    default: break;
                }
                storeFromReg("t2", in.dst, "t3");
            }
            break;
        }

        case IROp::NEG: {
            Kind kd = classify(in.dst);
            if (kd == Kind::REGISTER) {
                std::string rd = regForTemp(in.dst);
                Kind ks = classify(in.src1);
                if (ks == Kind::REGISTER) {
                    emit("    neg " + rd + ", " + regForTemp(in.src1));
                } else {
                    loadInto(in.src1, rd);
                    emit("    neg " + rd + ", " + rd);
                }
            } else {
                loadInto(in.src1, "t0");
                emit("    neg t2, t0");
                storeFromReg("t2", in.dst, "t3");
            }
            break;
        }
        case IROp::NOT: {
            Kind kd = classify(in.dst);
            if (kd == Kind::REGISTER) {
                std::string rd = regForTemp(in.dst);
                Kind ks = classify(in.src1);
                if (ks == Kind::REGISTER) {
                    emit("    seqz " + rd + ", " + regForTemp(in.src1));
                } else {
                    loadInto(in.src1, rd);
                    emit("    seqz " + rd + ", " + rd);
                }
            } else {
                loadInto(in.src1, "t0");
                emit("    seqz t2, t0");
                storeFromReg("t2", in.dst, "t3");
            }
            break;
        }

        case IROp::ASSIGN: {
            Kind kd = classify(in.dst);
            Kind ks = classify(in.src1);
            if (kd == Kind::REGISTER) {
                std::string rd = regForTemp(in.dst);
                if (ks == Kind::REGISTER) {
                    std::string rs = regForTemp(in.src1);
                    if (rd != rs) emit("    mv " + rd + ", " + rs);
                } else {
                    loadInto(in.src1, rd);
                }
            } else if (kd == Kind::VAREG) {
                loadInto(in.src1, varRegMap.at(in.dst));
            } else if (kd == Kind::AREG && ks == Kind::REGISTER) {
                emit("    mv " + in.dst + ", " + regForTemp(in.src1));
            } else {
                loadInto(in.src1, "t0");
                storeFromReg("t0", in.dst, "t1");
            }
            break;
        }

        case IROp::DECL:
            // 全局已在 .data 处理；局部仅占栈槽，初值由其后的 ASSIGN 完成；
            // 形参由 prologue 从 a 寄存器写入。此处无需生成代码。
            break;

        case IROp::LOAD: {
            // dst = 变量 src1 的值
            if (classify(in.dst) == Kind::REGISTER) {
                std::string r = regForTemp(in.dst);
                loadInto(in.src1, r);
            } else {
                loadInto(in.src1, "t0");
                storeFromReg("t0", in.dst, "t1");
            }
            break;
        }
        case IROp::STORE: {
            // 变量 dst = src1
            if (classify(in.src1) == Kind::REGISTER) {
                std::string r = regForTemp(in.src1);
                std::string scratch = (r == "t1") ? "t0" : "t1";
                storeFromReg(r, in.dst, scratch);
            } else {
                loadInto(in.src1, "t0");
                storeFromReg("t0", in.dst, "t1");
            }
            break;
        }

        case IROp::ARG:
            pendingArgs.emplace_back(in.src1, in.intValue);
            break;
        case IROp::CALL: {
            // 在调用前一刻才把实参载入 a 寄存器 / 出参栈区，
            // 从而不受实参求值顺序（可能含嵌套调用）影响。
            for (const auto& pa : pendingArgs) {
                const std::string& op = pa.first;
                int idx = pa.second;
                if (idx < 8) {
                    loadInto(op, "a" + std::to_string(idx));
                } else {
                    loadInto(op, "t0");
                    emit("    sw t0, " + std::to_string((idx - 8) * 4) + "(sp)");
                }
            }
            pendingArgs.clear();
            emit("    call " + in.label);
            storeFromReg("a0", in.dst, "t0");
            break;
        }

        case IROp::RETURN:
            // a0 已由前面的 ASSIGN("a0",..) 设置（若有返回值）。
            if (!nextIsFuncEnd) emit("    j " + epiLabel);
            break;

        case IROp::FUNC_BEGIN:
        case IROp::FUNC_END:
            break;  // 由 genFunction 处理
    }
}

// ------------------------------------------------------------
// 单个函数
// ------------------------------------------------------------
void CodeGenerator::genFunction(const IRList& ir, size_t begin, size_t end) {
    const std::string& name = ir[begin].label;
    prescan(ir, begin, end);
    epiLabel = ".Lepi_" + name;

    emit("    .text");
    emit("    .globl " + name);
    emit("    .align 2");
    emit(name + ":");

    // ---- prologue ----
    if (frameSize > 2047) {
        emit("    li t0, " + std::to_string(frameSize));
        emit("    sub sp, sp, t0");
    } else {
        emit("    addi sp, sp, -" + std::to_string(frameSize));
    }
    emitSW("ra", raOff, "sp");
    for (size_t i = 0; i < params.size(); i++) {
        int off = slotOff[params[i]];
        if (i < 8) {
            emitSW("a" + std::to_string(i), off, "sp");
        } else {
            // 第 9 个及以后的入参在调用者栈帧：本帧建立后位于 sp+frameSize+...
            int inOff = frameSize + (int)(i - 8) * 4;
            emitLW("t0", inOff, "sp");
            emitSW("t0", off, "sp");
        }
    }

    // ---- body ----
    for (size_t i = begin + 1; i < end; i++) {
        bool nextIsFuncEnd = (i + 1 == end);
        genInstr(ir[i], nextIsFuncEnd);
    }

    // ---- epilogue ----
    emit(epiLabel + ":");
    emitLW("ra", raOff, "sp");
    if (frameSize > 2047) {
        emit("    li t0, " + std::to_string(frameSize));
        emit("    add sp, sp, t0");
    } else {
        emit("    addi sp, sp, " + std::to_string(frameSize));
    }
    emit("    ret");
}

// ------------------------------------------------------------
// 顶层
// ------------------------------------------------------------
std::string CodeGenerator::generate(const IRList& ir) {
    out.clear();
    globals.clear();

    emitGlobals(ir);

    for (size_t i = 0; i < ir.size(); i++) {
        if (ir[i].op == IROp::FUNC_BEGIN) {
            size_t j = i + 1;
            int depth = 1;
            while (j < ir.size() && depth > 0) {
                if (ir[j].op == IROp::FUNC_BEGIN) depth++;
                else if (ir[j].op == IROp::FUNC_END) depth--;
                if (depth == 0) break;
                j++;
            }
            if (j >= ir.size()) j = ir.size() - 1;  // 容错：缺失 FUNC_END
            genFunction(ir, i, j);
            i = j;
        }
        // 顶层的全局 DECL 已在 emitGlobals 处理，跳过其余。
    }

    return out;
}
