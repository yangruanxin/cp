// C 的自测 — 手写 IR（遵循 B 的契约：%t 临时量、按名引用变量），验证汇编输出。
#include "ir.h"
#include "codegen.h"
#include <iostream>
#include <cassert>
#include <string>

static bool has(const std::string& s, const std::string& sub) {
    return s.find(sub) != std::string::npos;
}

int main() {
    int passed = 0;

    // -------- Test 1: int main() { return 42; } --------
    {
        IRList ir = {
            IRInstr::funcBegin("main"),
            IRInstr::li("%t0", 42),
            IRInstr::assign("a0", "%t0"),
            IRInstr::ret(),
            IRInstr::funcEnd(),
        };
        CodeGenerator cg;
        std::string s = cg.generate(ir);
        assert(has(s, ".text"));
        assert(has(s, ".globl main"));
        assert(has(s, "main:"));
        assert(has(s, "li t0, 42"));          // 立即数载入临时槽
        assert(has(s, "addi sp, sp, -"));     // prologue
        assert(has(s, "sw ra,"));
        assert(has(s, ".Lepi_main:"));        // epilogue 标签
        assert(has(s, "ret"));
        std::cout << "Test 1 PASS: return 42\n";
        passed++;
    }

    // -------- Test 2: 1 + 2 --------
    {
        IRList ir = {
            IRInstr::funcBegin("main"),
            IRInstr::li("%t0", 1),
            IRInstr::li("%t1", 2),
            IRInstr::bin(IROp::ADD, "%t2", "%t0", "%t1"),
            IRInstr::assign("a0", "%t2"),
            IRInstr::ret(),
            IRInstr::funcEnd(),
        };
        CodeGenerator cg;
        std::string s = cg.generate(ir);
        assert(has(s, "add t2, t0, t1"));
        std::cout << "Test 2 PASS: 1 + 2\n";
        passed++;
    }

    // -------- Test 3: if-goto 控制流 --------
    {
        IRList ir = {
            IRInstr::funcBegin("f"),
            IRInstr::li("%t0", 1),
            IRInstr::ifGoto("%t0", ".L_else"),
            IRInstr::li("a0", 10),
            IRInstr::goto_(".L_end"),
            IRInstr::labelInstr(".L_else"),
            IRInstr::li("a0", 20),
            IRInstr::labelInstr(".L_end"),
            IRInstr::ret(),
            IRInstr::funcEnd(),
        };
        CodeGenerator cg;
        std::string s = cg.generate(ir);
        assert(has(s, "bnez t0, .L_else"));
        assert(has(s, "j .L_end"));
        assert(has(s, ".L_else:"));
        std::cout << "Test 3 PASS: if-else\n";
        passed++;
    }

    // -------- Test 4: 形参识别 + 调用 --------
    // int add(int a, int b){ return a + b; }  add 有 2 个形参 → a0,a1
    {
        IRList ir = {
            IRInstr::funcBegin("add"),
            IRInstr::decl("a", false, 0),     // 形参（其后无同名 ASSIGN）
            IRInstr::decl("b", false, 0),     // 形参
            IRInstr::load("%t0", "a", 0),
            IRInstr::load("%t1", "b", 0),
            IRInstr::bin(IROp::ADD, "%t2", "%t0", "%t1"),
            IRInstr::assign("a0", "%t2"),
            IRInstr::ret(),
            IRInstr::funcEnd(),
            // caller
            IRInstr::funcBegin("main"),
            IRInstr::li("%t0", 3),
            IRInstr::arg("%t0", 0),
            IRInstr::li("%t1", 4),
            IRInstr::arg("%t1", 1),
            IRInstr::call("%t2", "add"),
            IRInstr::assign("a0", "%t2"),
            IRInstr::ret(),
            IRInstr::funcEnd(),
        };
        CodeGenerator cg;
        std::string s = cg.generate(ir);
        // 形参 a/b 在 prologue 从 a0/a1 写回栈槽
        assert(has(s, "sw a0,"));
        assert(has(s, "sw a1,"));
        assert(has(s, "call add"));
        // 调用结果从 a0 写回临时槽
        std::cout << "Test 4 PASS: params + call\n";
        passed++;
    }

    // -------- Test 5: 局部变量声明（DECL+ASSIGN）不被误判为形参 --------
    // int main(){ int x = 5; return x; }
    {
        IRList ir = {
            IRInstr::funcBegin("main"),
            IRInstr::li("%t0", 5),
            IRInstr::decl("x", false, 0),
            IRInstr::assign("x", "%t0"),       // 紧跟同名 ASSIGN → x 是局部，非形参
            IRInstr::load("%t1", "x", 0),
            IRInstr::assign("a0", "%t1"),
            IRInstr::ret(),
            IRInstr::funcEnd(),
        };
        CodeGenerator cg;
        std::string s = cg.generate(ir);
        // 不应把 x 当成入参 → prologue 里不应出现把 a0 写到栈的形参搬运
        // （main 无形参，prologue 只有 addi/sw ra）
        assert(has(s, "addi sp, sp, -"));
        assert(has(s, "sw ra,"));
        std::cout << "Test 5 PASS: local decl not mistaken for param\n";
        passed++;
    }

    // -------- Test 6: 全局变量进入 .data --------
    {
        IRList ir = {
            IRInstr::decl("g", true, 7),
            IRInstr::funcBegin("main"),
            IRInstr::load("%t0", "g", 0),
            IRInstr::assign("a0", "%t0"),
            IRInstr::ret(),
            IRInstr::funcEnd(),
        };
        CodeGenerator cg;
        std::string s = cg.generate(ir);
        assert(has(s, ".data"));
        assert(has(s, "g:"));
        assert(has(s, ".word 7"));
        assert(has(s, "la t0, g"));            // 全局通过 la 寻址
        std::cout << "Test 6 PASS: global in .data\n";
        passed++;
    }

    // -------- Test 7: 关系运算 / 取模 --------
    {
        IRList ir = {
            IRInstr::funcBegin("main"),
            IRInstr::li("%t0", 7),
            IRInstr::li("%t1", 3),
            IRInstr::bin(IROp::MOD, "%t2", "%t0", "%t1"),
            IRInstr::bin(IROp::LE, "%t3", "%t0", "%t1"),
            IRInstr::assign("a0", "%t2"),
            IRInstr::ret(),
            IRInstr::funcEnd(),
        };
        CodeGenerator cg;
        std::string s = cg.generate(ir);
        assert(has(s, "rem t2, t0, t1"));
        assert(has(s, "slt t2, t1, t0"));       // LE 用 !(s2<s1)
        assert(has(s, "xori t2, t2, 1"));
        std::cout << "Test 7 PASS: relational / mod\n";
        passed++;
    }

    std::cout << "\nAll " << passed << " codegen tests passed!\n";
    return 0;
}
