// D 的自测 — IR 优化器
#include "ir.h"
#include "optimizer.h"
#include <iostream>
#include <cassert>

bool containsOp(const IRList& ir, IROp op) {
    for (const auto& i : ir) {
        if (i.op == op) return true;
    }
    return false;
}

bool containsLoadImm(const IRList& ir, const std::string& dst, int value) {
    for (const auto& i : ir) {
        if (i.op == IROp::LOAD_IMM && i.dst == dst && i.intValue == value) {
            return true;
        }
    }
    return false;
}

int main() {
    // Test 1: constant folding — 3 + 4 should become 7
    IRList ir = {
        IRInstr::li("%t0", 3),
        IRInstr::li("%t1", 4),
        IRInstr::add("%t2", "%t0", "%t1"),
        IRInstr::assign("a0", "%t2"),
        IRInstr::ret(),
    };

    Optimizer opt;
    IRList optimized = opt.optimize(ir);

    std::cout << "Before: " << ir.size() << " instructions\n";
    std::cout << "After:  " << optimized.size() << " instructions\n";

    // Should have folded 3+4 into a single li
    bool foundFold = false;
    for (const auto& i : optimized) {
        if (i.op == IROp::LOAD_IMM && i.intValue == 7) foundFold = true;
    }
    assert(foundFold);
    std::cout << "Test 1 PASS: constant folding (3+4 → 7)\n";

    // Test 2: dead code elimination
    IRList ir2 = {
        IRInstr::li("a0", 42),   // used (return value)
        IRInstr::li("%t0", 1),   // dead (never used)
        IRInstr::li("%t1", 2),   // dead (never used)
        IRInstr::add("%t2", "%t0", "%t1"), // dead
        IRInstr::ret(),
    };

    IRList opt2 = opt.optimize(ir2);
    std::cout << "Before DCE: " << ir2.size() << " instructions\n";
    std::cout << "After DCE:  " << opt2.size() << " instructions\n";
    assert(opt2.size() < ir2.size());
    std::cout << "Test 2 PASS: dead code elimination\n";

    // Test 3: relational and unary folding
    IRList ir3 = {
        IRInstr::li("%t0", 5),
        IRInstr::li("%t1", 3),
        IRInstr::bin(IROp::GE, "%t2", "%t0", "%t1"),
        IRInstr::una(IROp::NOT, "%t3", "%t2"),
        IRInstr::assign("a0", "%t3"),
        IRInstr::ret(),
    };
    IRList opt3 = opt.optimize(ir3);
    assert(containsLoadImm(opt3, "a0", 0));
    std::cout << "Test 3 PASS: relational/unary folding\n";

    // Test 4: division by zero must not be folded by the optimizer
    IRList ir4 = {
        IRInstr::li("%t0", 1),
        IRInstr::li("%t1", 0),
        IRInstr::bin(IROp::DIV, "%t2", "%t0", "%t1"),
        IRInstr::assign("a0", "%t2"),
        IRInstr::ret(),
    };
    IRList opt4 = opt.optimize(ir4);
    assert(containsOp(opt4, IROp::DIV));
    std::cout << "Test 4 PASS: division-by-zero folding guard\n";

    // Test 5: calls and arguments are side-effecting and must be kept
    IRList ir5 = {
        IRInstr::li("%t0", 9),
        IRInstr::arg("%t0", 0),
        IRInstr::call("%t1", "touch_global"),
        IRInstr::ret(),
    };
    IRList opt5 = opt.optimize(ir5);
    assert(containsOp(opt5, IROp::ARG));
    assert(containsOp(opt5, IROp::CALL));
    assert(containsLoadImm(opt5, "%t0", 9));
    std::cout << "Test 5 PASS: call/argument preservation\n";

    // Test 6: constants must not propagate across labels/branches
    IRList ir6 = {
        IRInstr::li("%cond", 1),
        IRInstr::ifGoto("%cond", ".L_true"),
        IRInstr::li("%result", 2),
        IRInstr::goto_(".L_end"),
        IRInstr::labelInstr(".L_true"),
        IRInstr::li("%result", 3),
        IRInstr::labelInstr(".L_end"),
        IRInstr::assign("a0", "%result"),
        IRInstr::ret(),
    };
    IRList opt6 = opt.optimize(ir6);
    assert(containsOp(opt6, IROp::ASSIGN));
    assert(!containsLoadImm(opt6, "a0", 3));
    std::cout << "Test 6 PASS: control-flow boundary guard\n";

    std::cout << "All optimizer tests passed!\n";
    return 0;
}
