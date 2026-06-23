#include <iostream>
#include <string>
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "codegen.h"
#include "optimizer.h"
#include <sstream>

int main(int argc, char* argv[]) {
    bool optimize = false;
    if (argc > 1 && std::string(argv[1]) == "-opt") {
        optimize = true;
    }

    std::string source((std::istreambuf_iterator<char>(std::cin)),
                        std::istreambuf_iterator<char>());

    Lexer lexer(source);
    auto tokens = lexer.tokenize();

    Parser parser(tokens);
    auto ast = parser.parse();

    SemanticAnalyzer sema;
    sema.analyze(ast);

    if (optimize) {
        Optimizer opt;
        opt.optimize(ast);
    }

    CodeGenerator cg;
    std::string assembly = cg.generate(ast);
    std::cout << assembly;

    return 0;
}
