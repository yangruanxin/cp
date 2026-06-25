// D 的自测 — IR 优化器
#include "ir.h"
#include "optimizer.h"
#include <iostream>
#include <cassert>

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

    std::cout << "All optimizer tests passed!\n";
    return 0;
}
