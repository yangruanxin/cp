#include "codegen.h"
#include <algorithm>
#include <unordered_map>

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

static bool fitsI12(int n) {
    return n >= -2048 && n <= 2047;
}

void CodeGenerator::emitAdjustSp(int amount, const std::string& scratch) {
    if (amount == 0) return;
    if (fitsI12(amount)) {
        emit("    addi sp, sp, " + std::to_string(amount));
    } else if (amount > 0) {
        emit("    li " + scratch + ", " + std::to_string(amount));
        emit("    add sp, sp, " + scratch);
    } else {
        emit("    li " + scratch + ", " + std::to_string(-amount));
        emit("    sub sp, sp, " + scratch);
    }
}

void CodeGenerator::emitLoadStack(const std::string& target, int offset,
                                  const std::string& scratch) {
    if (fitsI12(offset)) {
        emit("    lw " + target + ", " + std::to_string(offset) + "(sp)");
    } else {
        emit("    li " + scratch + ", " + std::to_string(offset));
        emit("    add " + scratch + ", sp, " + scratch);
        emit("    lw " + target + ", 0(" + scratch + ")");
    }
}

void CodeGenerator::emitStoreStack(const std::string& reg, int offset,
                                   const std::string& scratch) {
    if (fitsI12(offset)) {
        emit("    sw " + reg + ", " + std::to_string(offset) + "(sp)");
    } else {
        emit("    li " + scratch + ", " + std::to_string(offset));
        emit("    add " + scratch + ", sp, " + scratch);
        emit("    sw " + reg + ", 0(" + scratch + ")");
    }
}

bool CodeGenerator::isAReg(const std::string& op) const {
    return op.size() == 2 && op[0] == 'a' && op[1] >= '0' && op[1] <= '7';
}

CodeGenerator::Kind CodeGenerator::classify(const std::string& op) const {
    if (op.empty()) return Kind::ZERO;
    if (regOf.count(op)) return Kind::SREG;                // 函数内缓存寄存器
    if (op[0] == '%') return Kind::SLOT;                 // 临时量
    if (globals.count(op)) return Kind::GLOBAL;          // 全局变量/常量
    if (slotOff.count(op)) return Kind::SLOT;            // 已分配槽的局部/形参
    // 已声明的局部但尚未分配槽（不应发生，保险起见）。
    if (declaredLocals.count(op)) return Kind::SLOT;
    if (isAReg(op)) return Kind::AREG;                   // a0..a7 伪寄存器
    return Kind::UNKNOWN;
}

// ------------------------------------------------------------
// 载入 / 存储一个操作数
// ------------------------------------------------------------
void CodeGenerator::loadInto(const std::string& op, const std::string& target) {
    switch (classify(op)) {
        case Kind::ZERO:
            emit("    li " + target + ", 0");
            break;
        case Kind::SLOT: {
            auto it = slotOff.find(op);
            int off = (it != slotOff.end()) ? it->second : 0;
            emitLoadStack(target, off, "t3");
            break;
        }
        case Kind::GLOBAL:
            emit("    la " + target + ", " + op);
            emit("    lw " + target + ", 0(" + target + ")");
            break;
        case Kind::AREG:
            if (op != target) emit("    mv " + target + ", " + op);
            break;
        case Kind::SREG: {
            auto it = regOf.find(op);
            if (it != regOf.end() && it->second != target) {
                emit("    mv " + target + ", " + it->second);
            }
            break;
        }
        case Kind::UNKNOWN:
            // 兜底：当作 0，保证可汇编。
            emit("    li " + target + ", 0");
            break;
    }
}

void CodeGenerator::storeFromReg(const std::string& reg, const std::string& op,
                                 const std::string& scratch) {
    switch (classify(op)) {
        case Kind::ZERO:
            break;  // 写入 zero/丢弃，无操作
        case Kind::SLOT: {
            auto it = slotOff.find(op);
            int off = (it != slotOff.end()) ? it->second : 0;
            emitStoreStack(reg, off, scratch);
            break;
        }
        case Kind::GLOBAL:
            emit("    la " + scratch + ", " + op);
            emit("    sw " + reg + ", 0(" + scratch + ")");
            break;
        case Kind::AREG:
            if (op != reg) emit("    mv " + op + ", " + reg);
            break;
        case Kind::SREG: {
            auto it = regOf.find(op);
            if (it != regOf.end() && it->second != reg) {
                emit("    mv " + it->second + ", " + reg);
            }
            break;
        }
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
//   B 为每个形参发出 DECL(name,"param",0)，普通局部为 DECL(name,"local",0)。
// ------------------------------------------------------------
void CodeGenerator::collectParams(const IRList& ir, size_t begin, size_t end) {
    params.clear();
    for (size_t i = begin + 1; i < end; i++) {
        const auto& in = ir[i];
        if (in.op == IROp::DECL && in.src1 == "param") params.push_back(in.dst);
    }
}

// ------------------------------------------------------------
// 预扫描：收集需要栈槽的名字，计算帧布局
// ------------------------------------------------------------
void CodeGenerator::prescan(const IRList& ir, size_t begin, size_t end) {
    slotOff.clear();
    regOf.clear();
    usedSRegs.clear();
    sRegOff.clear();
    declaredLocals.clear();
    pendingArgs.clear();

    collectParams(ir, begin, end);
    for (const auto& p : params) declaredLocals.insert(p);

    // 局部变量声明名
    for (size_t i = begin + 1; i < end; i++) {
        const auto& in = ir[i];
        if (in.op == IROp::DECL && (in.src1 == "local" || in.src1 == "param")) {
            declaredLocals.insert(in.dst);
        }
    }

    std::unordered_map<std::string, int> freq;
    std::vector<std::string> firstSeen;
    auto candidate = [&](const std::string& op) {
        if (op.empty() || isAReg(op) || globals.count(op)) return false;
        return op[0] == '%' || declaredLocals.count(op);
    };
    auto countUse = [&](const std::string& op) {
        if (!candidate(op)) return;
        if (freq[op]++ == 0) firstSeen.push_back(op);
    };

    int maxArgCount = 0;  // 出参个数上界（用于 >8 实参的栈传递）
    for (size_t i = begin + 1; i < end; i++) {
        const auto& in = ir[i];
        countUse(in.dst);
        countUse(in.src1);
        countUse(in.src2);
        if (in.op == IROp::ARG) {
            maxArgCount = std::max(maxArgCount, in.intValue + 1);
        }
    }

    std::vector<std::string> ranked = firstSeen;
    std::stable_sort(ranked.begin(), ranked.end(), [&](const std::string& a, const std::string& b) {
        return freq[a] > freq[b];
    });

    static const char* kSRegs[] = {"s1", "s2", "s3", "s4", "s5", "s6",
                                   "s7", "s8", "s9", "s10", "s11"};
    int regCount = 0;
    for (const auto& name : ranked) {
        if (regCount >= 11) break;
        if (freq[name] <= 2) continue;
        regOf[name] = kSRegs[regCount++];
        usedSRegs.push_back(regOf[name]);
    }

    std::vector<std::string> order;  // 保持确定性的槽分配顺序
    auto wantSlot = [&](const std::string& op) {
        if (!candidate(op) || regOf.count(op)) return;
        if (!slotOff.count(op)) { slotOff[op] = -1; order.push_back(op); }
        // a0..a7 伪寄存器与未知名（如返回值 a0）不占栈。
    };

    for (size_t i = begin + 1; i < end; i++) {
        const auto& in = ir[i];
        wantSlot(in.dst);
        wantSlot(in.src1);
        wantSlot(in.src2);
    }

    outArgBytes = (maxArgCount > 8) ? (maxArgCount - 8) * 4 : 0;

    int idx = 0;
    for (const auto& name : order) {
        slotOff[name] = outArgBytes + idx * 4;
        idx++;
    }
    int numSlots = (int)order.size();
    raOff = outArgBytes + numSlots * 4;
    int saveBase = raOff + 4;
    for (size_t i = 0; i < usedSRegs.size(); i++) {
        sRegOff[usedSRegs[i]] = saveBase + (int)i * 4;
    }
    frameSize = align16(saveBase + (int)usedSRegs.size() * 4);
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
        case IROp::IF_GOTO:
            loadInto(in.src1, "t0");
            emit("    bnez t0, " + in.label);
            break;

        case IROp::LOAD_IMM:
            emit("    li t0, " + std::to_string(in.intValue));
            storeFromReg("t0", in.dst, "t1");
            break;

        case IROp::ADD: case IROp::SUB: case IROp::MUL:
        case IROp::DIV: case IROp::MOD:
        case IROp::LT:  case IROp::GT:  case IROp::LE:
        case IROp::GE:  case IROp::EQ:  case IROp::NE:
        case IROp::AND: case IROp::OR: {
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
            break;
        }

        case IROp::NEG:
            loadInto(in.src1, "t0");
            emit("    neg t2, t0");
            storeFromReg("t2", in.dst, "t3");
            break;
        case IROp::NOT:
            loadInto(in.src1, "t0");
            emit("    seqz t2, t0");
            storeFromReg("t2", in.dst, "t3");
            break;

        case IROp::ASSIGN:
            loadInto(in.src1, "t0");
            storeFromReg("t0", in.dst, "t1");
            break;

        case IROp::DECL:
            // 全局已在 .data 处理；局部仅占栈槽，初值由其后的 ASSIGN 完成；
            // 形参由 prologue 从 a 寄存器写入。此处无需生成代码。
            break;

        case IROp::LOAD:
            // dst = 变量 src1 的值
            loadInto(in.src1, "t0");
            storeFromReg("t0", in.dst, "t1");
            break;
        case IROp::STORE:
            // 变量 dst = src1
            loadInto(in.src1, "t0");
            storeFromReg("t0", in.dst, "t1");
            break;

        case IROp::ARG:
            pendingArgs.emplace_back(in.src1, in.intValue);
            break;
        case IROp::CALL: {
            // 在调用前一刻才把实参载入 a 寄存器 / 出参栈区，
            // 从而不受实参求值顺序（可能含嵌套调用）影响。
            for (const auto& [op, idx] : pendingArgs) {
                if (idx < 8) {
                    loadInto(op, "a" + std::to_string(idx));
                } else {
                    loadInto(op, "t0");
                    emitStoreStack("t0", (idx - 8) * 4, "t1");
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
    emitAdjustSp(-frameSize, "t0");
    emitStoreStack("ra", raOff, "t0");
    for (const auto& s : usedSRegs) {
        emitStoreStack(s, sRegOff[s], "t0");
    }
    for (size_t i = 0; i < params.size(); i++) {
        if (i < 8) {
            storeFromReg("a" + std::to_string(i), params[i], "t0");
        } else {
            // 第 9 个及以后的入参在调用者栈帧：本帧建立后位于 sp+frameSize+...
            int inOff = frameSize + (int)(i - 8) * 4;
            emitLoadStack("t0", inOff, "t1");
            storeFromReg("t0", params[i], "t1");
        }
    }

    // ---- body ----
    for (size_t i = begin + 1; i < end; i++) {
        bool nextIsFuncEnd = (i + 1 == end);
        genInstr(ir[i], nextIsFuncEnd);
    }

    // ---- epilogue ----
    emit(epiLabel + ":");
    for (auto it = usedSRegs.rbegin(); it != usedSRegs.rend(); ++it) {
        emitLoadStack(*it, sRegOff[*it], "t0");
    }
    emitLoadStack("ra", raOff, "t0");
    emitAdjustSp(frameSize, "t0");
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
