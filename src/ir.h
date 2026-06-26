#ifndef IR_H
#define IR_H

#include <string>
#include <vector>

// ============================================================
// 终极契约 — IR 指令定义
// B (语义分析 → IR 生成) 和 C (IR → 汇编) 共同确认此文件
// 一旦定稿，前 5 天绝不修改
// ============================================================

enum class IROp {
    // 函数
    FUNC_BEGIN,    // label: 函数名
    FUNC_END,      // 函数体结束

    // 标签与控制流
    LABEL,         // label: 标签名
    GOTO,          // goto label
    IF_GOTO,       // if src1(非零) goto label

    // 算术 (dst = src1 op src2)
    ADD, SUB, MUL, DIV, MOD,

    // 关系 (dst = src1 op src2 → 0/1)
    LT, GT, LE, GE, EQ, NE,

    // 逻辑 (用于短路求值)
    AND, OR,

    // 一元 (dst = op src1)
    NEG, NOT,

    // 立即数 (dst = value)
    LOAD_IMM,

    // 赋值/变量
    ASSIGN,        // dst = src1
    DECL,          // 声明变量: dst = 变量名, src1 = "global"/"local", intValue = 初值
    LOAD,          // dst = MEM[src1 + intValue(offset)]
    STORE,         // MEM[dst + intValue(offset)] = src1

    // 函数调用
    CALL,          // dst = CALL label(args in a0-a7)
    RETURN,        // return

    // 参数传递 (已由 B 按序排好)
    ARG,           // ARG src1 → 第 intValue 个参数
};

struct IRInstr {
    IROp op;
    std::string dst;
    std::string src1;
    std::string src2;
    int intValue = 0;      // 立即数值或 LOAD/STORE 偏移
    std::string label;     // 标签名/函数名

    // ---- 便捷构造器 ----
    static IRInstr labelInstr(const std::string& l)  { return {IROp::LABEL, {}, {}, {}, 0, l}; }
    static IRInstr goto_(const std::string& l)       { return {IROp::GOTO, {}, {}, {}, 0, l}; }
    static IRInstr ifGoto(const std::string& c, const std::string& l) {
        return {IROp::IF_GOTO, {}, c, {}, 0, l};
    }
    static IRInstr bin(IROp op, const std::string& d, const std::string& s1, const std::string& s2) {
        return {op, d, s1, s2, 0, {}};
    }
    static IRInstr add(const std::string& d, const std::string& s1, const std::string& s2) {
        return bin(IROp::ADD, d, s1, s2);
    }
    static IRInstr una(IROp op, const std::string& d, const std::string& s) {
        return {op, d, s, {}, 0, {}};
    }
    static IRInstr li(const std::string& d, int v)  { return {IROp::LOAD_IMM, d, {}, {}, v, {}}; }
    static IRInstr assign(const std::string& d, const std::string& s) {
        return {IROp::ASSIGN, d, s, {}, 0, {}};
    }
    static IRInstr decl(const std::string& name, bool global, int initVal) {
        return {IROp::DECL, name, global ? "global" : "local", {}, initVal, {}};
    }
    static IRInstr load(const std::string& d, const std::string& base, int off) {
        return {IROp::LOAD, d, base, {}, off, {}};
    }
    static IRInstr store(const std::string& base, int off, const std::string& val) {
        return {IROp::STORE, base, val, {}, off, {}};
    }
    static IRInstr call(const std::string& d, const std::string& fn) {
        return {IROp::CALL, d, {}, {}, 0, fn};
    }
    static IRInstr ret()                            { return {IROp::RETURN, {}, {}, {}, 0, {}}; }
    static IRInstr funcBegin(const std::string& n)  { return {IROp::FUNC_BEGIN, {}, {}, {}, 0, n}; }
    static IRInstr funcEnd()                        { return {IROp::FUNC_END, {}, {}, {}, 0, {}}; }
    static IRInstr arg(const std::string& val, int idx) {
        return {IROp::ARG, {}, val, {}, idx, {}};
    }
};

using IRList = std::vector<IRInstr>;

#endif
