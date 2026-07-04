// D 编写 — 串联 A→B→D(opt)→C
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "codegen.h"
#include "optimizer.h"
#include <iostream>
#include <sstream>

int main(int argc, char* argv[]) {
    bool optimize = false;
    if (argc > 1 && std::string(argv[1]) == "-opt") {
        optimize = true;
    }

    // 读取 stdin
    std::string source((std::istreambuf_iterator<char>(std::cin)),
                        std::istreambuf_iterator<char>());

    try {
        // A: 词法分析 + 语法分析 → AST
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto ast = parser.parse();

        // B: 语义分析 + IR 生成
        IRGenerator irGen;
        IRList ir = irGen.generate(ast);
        if (irGen.hasErrors()) {
            return 1;
        }

        // D: 优化（可选）
        if (optimize) {
            Optimizer opt;
            ir = opt.optimize(ir);
        }

        // C: 代码生成 → RISC-V 汇编
        CodeGenerator cg;
        std::string assembly = cg.generate(ir);
        std::cout << assembly;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
