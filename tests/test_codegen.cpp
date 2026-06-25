// C 的自测 — 手写 IR 数组，验证汇编输出
#include "ir.h"
#include "codegen.h"
#include <iostream>
#include <cassert>

int main() {
    // Test 1: simple function returning 42
    IRList ir = {
        IRInstr::funcBegin("main"),
        IRInstr::li("t0", 42),
        IRInstr::assign("a0", "t0"),
        IRInstr::ret(),
        IRInstr::funcEnd(),
    };

    CodeGenerator cg;
    std::string asmOutput = cg.generate(ir);
    assert(asmOutput.find(".text") != std::string::npos);
    assert(asmOutput.find("main") != std::string::npos);
    assert(asmOutput.find("li") != std::string::npos);

    std::cout << "Test 1 PASS: generated assembly for return 42\n";
    std::cout << "--- Output ---\n" << asmOutput << "---\n";

    // Test 2: arithmetic expression
    IRList ir2 = {
        IRInstr::funcBegin("calc"),
        IRInstr::li("t0", 1),
        IRInstr::li("t1", 2),
        IRInstr::add("t2", "t0", "t1"),
        IRInstr::assign("a0", "t2"),
        IRInstr::ret(),
        IRInstr::funcEnd(),
    };
    std::string asm2 = cg.generate(ir2);
    assert(asm2.find("add") != std::string::npos);
    std::cout << "Test 2 PASS: generated assembly for 1+2\n";

    // Test 3: if-goto control flow
    IRList ir3 = {
        IRInstr::funcBegin("test_if"),
        IRInstr::li("t0", 1),
        IRInstr::ifGoto("t0", ".L_else"),
        IRInstr::li("a0", 10),
        IRInstr::goto_(".L_end"),
        IRInstr::label(".L_else"),
        IRInstr::li("a0", 20),
        IRInstr::label(".L_end"),
        IRInstr::ret(),
        IRInstr::funcEnd(),
    };
    std::string asm3 = cg.generate(ir3);
    assert(asm3.find("bnez") != std::string::npos);
    assert(asm3.find("j ") != std::string::npos);
    std::cout << "Test 3 PASS: generated assembly for if-else\n";

    std::cout << "All codegen tests passed!\n";
    return 0;
}
