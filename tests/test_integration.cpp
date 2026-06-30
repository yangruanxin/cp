// D 的集成测试 — 端到端：ToyC 源码 → RISC-V 汇编
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "codegen.h"
#include "optimizer.h"
#include <iostream>
#include <cassert>
#include <sstream>
#include <vector>

std::string compile(const std::string& source, bool optimize = false) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    IRGenerator irGen;
    IRList ir = irGen.generate(ast);
    if (optimize) {
        Optimizer opt;
        ir = opt.optimize(ir);
    }
    CodeGenerator cg;
    return cg.generate(ir);
}

IRList makeIr(const std::string& source, bool optimize = false) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    IRGenerator irGen;
    IRList ir = irGen.generate(ast);
    if (optimize) {
        Optimizer opt;
        ir = opt.optimize(ir);
    }
    return ir;
}

int main() {
    // Test 1: simplest program
    std::string src1 = "int main() { return 42; }";
    std::string asm1 = compile(src1);
    assert(!asm1.empty());
    assert(asm1.find(".text") != std::string::npos);
    std::cout << "Integration Test 1 PASS: return 42\n";

    // Test 2: arithmetic expression
    std::string src2 = "int main() { return 1 + 2 * 3; }";
    std::string asm2 = compile(src2);
    assert(!asm2.empty());
    std::cout << "Integration Test 2 PASS: 1 + 2 * 3\n";

    // Test 3: if-else
    std::string src3 = R"(
        int main() {
            int a = 1;
            if (a) return 10; else return 20;
        }
    )";
    std::string asm3 = compile(src3);
    assert(!asm3.empty());
    std::cout << "Integration Test 3 PASS: if-else\n";

    // Test 4: while loop
    std::string src4 = R"(
        int main() {
            int i = 0;
            while (i < 10) {
                i = i + 1;
            }
            return i;
        }
    )";
    std::string asm4 = compile(src4);
    std::cout << "Integration Test 4 PASS: while loop\n";

    // Test 5: with optimization
    std::string asm5 = compile(src1, true);
    std::cout << "Integration Test 5 PASS: return 42 with -opt\n";

    // Test 6: functions + globals + consts
    std::string src6 = R"(
        const int base = 3;
        int g = 4;
        int add(int a, int b) {
            return a + b;
        }
        int main() {
            int x = add(base, g);
            return x * 2;
        }
    )";
    std::string asm6 = compile(src6, true);
    assert(!asm6.empty());
    assert(asm6.find(".data") != std::string::npos);
    assert(asm6.find("add:") != std::string::npos);
    std::cout << "Integration Test 6 PASS: functions/globals with -opt\n";

    // Test 7: short-circuit logic should survive optimization
    std::string src7 = R"(
        int main() {
            int a = 0;
            int b = 5;
            if ((a != 0) && (10 / a > 1)) {
                return 1;
            }
            if ((b > 0) || (10 / a > 1)) {
                return 7;
            }
            return 0;
        }
    )";
    std::string asm7 = compile(src7, true);
    assert(!asm7.empty());
    std::cout << "Integration Test 7 PASS: short-circuit logic with -opt\n";

    // Test 8: block scope shadowing should use distinct IR names
    std::string src8 = R"(
        int main() {
            int x = 1;
            {
                int x = 2;
                x = x + 1;
            }
            return x;
        }
    )";
    IRList ir8 = makeIr(src8);
    std::vector<std::string> localDecls;
    for (const auto& in : ir8) {
        if (in.op == IROp::DECL && in.src1 == "local") {
            localDecls.push_back(in.dst);
        }
    }
    assert(localDecls.size() >= 2);
    assert(localDecls[0] != localDecls[1]);
    std::cout << "Integration Test 8 PASS: scoped shadowing names\n";

    // Test 9: mutable globals must be loaded from memory after stores
    std::string src9 = R"(
        int g = 1;
        int main() {
            g = 2;
            return g;
        }
    )";
    IRList ir9 = makeIr(src9, true);
    bool hasGlobalStore = false;
    bool hasGlobalLoad = false;
    for (const auto& in : ir9) {
        if (in.op == IROp::STORE && in.dst == "g") hasGlobalStore = true;
        if (in.op == IROp::LOAD && in.src1 == "g") hasGlobalLoad = true;
    }
    assert(hasGlobalStore);
    assert(hasGlobalLoad);
    std::cout << "Integration Test 9 PASS: mutable global load/store\n";

    // Test 10: void function call as a statement
    std::string src10 = R"(
        void touch() {
            return;
        }
        int main() {
            touch();
            return 1;
        }
    )";
    std::string asm10 = compile(src10, true);
    assert(!asm10.empty());
    assert(asm10.find("call touch") != std::string::npos);
    std::cout << "Integration Test 10 PASS: void call statement\n";

    // Test 11: many locals should not emit out-of-range stack offsets directly
    std::ostringstream src11;
    src11 << "int main(){\n";
    for (int i = 0; i < 700; i++) {
        src11 << "int v" << i << " = " << i << ";\n";
    }
    src11 << "return v699;\n}\n";
    std::string asm11 = compile(src11.str(), false);
    assert(!asm11.empty());
    assert(asm11.find("add t3, sp, t3") != std::string::npos ||
           asm11.find("add t0, sp, t0") != std::string::npos ||
           asm11.find("add t1, sp, t1") != std::string::npos);
    std::cout << "Integration Test 11 PASS: many locals large stack frame\n";

    // Print a sample
    std::cout << "\n=== Sample assembly output ===\n" << asm1 << "===\n";

    std::cout << "\nAll integration tests passed!\n";
    return 0;
}
