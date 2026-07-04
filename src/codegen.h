#ifndef CODEGEN_H
#define CODEGEN_H

#include "ir.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

// ============================================================
// C 模块 — IR → RISC-V32 汇编 (RV32IM)
//
// 设计原则：正确性优先的「全部溢出到栈」基线代码生成器。
//   * 每个 IR 临时量 (%tN) 和每个局部变量/形参都拥有一个独立的栈槽。
//   * 任何 IR 指令在执行期间，其操作数都先从栈/内存载入 scratch
//     寄存器 (t0~t3)，计算后立即写回栈槽。
//   * 因此没有任何值跨指令存活于寄存器中，函数调用天然安全，无需
//     保存调用者保存寄存器，极大降低出错概率。
//
// 与 B 的契约（见 ir.h 与 semantic.cpp）：
//   * 临时量命名为 "%tN"。
//   * 变量按名字引用：LOAD dst,name,0 读变量；STORE name,0,val 写变量。
//   * "a0".."a7" 为返回值 / 调用结果 / 实参的伪寄存器。
//   * 形参由 FUNC_BEGIN 之后、未紧跟同名 ASSIGN 的 DECL 表示，按出现
//     顺序对应 a0,a1,...（见 collectParams）。
// ============================================================

class CodeGenerator {
public:
    std::string generate(const IRList& ir);

private:
    std::string out;

    // 全局变量/常量名集合（位于 .data，按符号名访问）。
    std::unordered_set<std::string> globals;

    // ---- 当前函数状态 ----
    std::unordered_map<std::string, int> slotOff;  // 名字 -> sp 相对偏移
    std::vector<std::string> params;               // 形参，按 a0,a1,... 顺序
    std::unordered_set<std::string> declaredLocals;// 局部变量 + 形参名
    std::vector<std::pair<std::string, int>> pendingArgs; // 待传递实参 (操作数, 序号)
    int frameSize = 0;
    int raOff = 0;
    int outArgBytes = 0;
    std::string epiLabel;

    enum class Kind { ZERO, SLOT, GLOBAL, AREG, UNKNOWN };

    void emit(const std::string& s);
    void emitSW(const std::string& reg, int offset, const std::string& base);
    void emitLW(const std::string& reg, int offset, const std::string& base);
    void emitGlobals(const IRList& ir);

    // 处理 [begin, end] 区间（含 FUNC_BEGIN..FUNC_END）的一个函数。
    void genFunction(const IRList& ir, size_t begin, size_t end);
    void prescan(const IRList& ir, size_t begin, size_t end);
    void collectParams(const IRList& ir, size_t begin, size_t end);

    Kind classify(const std::string& op) const;
    bool isAReg(const std::string& op) const;

    // 将操作数 op 的值强制载入物理寄存器 target。
    void loadInto(const std::string& op, const std::string& target);
    // 将物理寄存器 reg 的值写回操作数 op（scratch 仅用于全局变量寻址，需异于 reg）。
    void storeFromReg(const std::string& reg, const std::string& op,
                      const std::string& scratch);

    void genInstr(const IRInstr& instr, bool nextIsFuncEnd);
};

#endif
