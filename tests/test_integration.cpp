// D 的集成测试 — 端到端编译 ToyC → 汇编
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "codegen.h"
#include <iostream>
#include <cassert>
#include <sstream>
#include <string>

// 编译一段 ToyC 源码，返回汇编
std::string compile(const std::string& source) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    SemanticAnalyzer sema;
    sema.analyze(ast);
    CodeGenerator cg;
    return cg.generate(ast);
}

void test_end_to_end() {
    std::string source = R"(int main() { return 42; })";
    std::string asmOutput = compile(source);
    assert(!asmOutput.empty());
    // The output should be valid RISC-V assembly (at minimum has text section)
    assert(asmOutput.find(".text") != std::string::npos);
    std::cout << "PASS: test_end_to_end\n";
    std::cout << asmOutput << "\n";
}

int main() {
    test_end_to_end();
    std::cout << "All integration tests passed!\n";
    return 0;
}
