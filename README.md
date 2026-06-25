# ToyC 编译器

编译原理课程实践项目 — 将 ToyC 语言编译为 RISC-V32 汇编代码。

## 架构

```
源码 → [A: Lexer+Parser] → AST → [B: Semantic+IRGen] → IR → [C: CodeGen] → 汇编
                                                                ↑
                                                           [D: Optimizer]
```

三阶段完全解耦，四人可并行开发。

## 快速开始

```bash
mkdir build && cd build
cmake ..
cmake --build .

./compiler < input.tc > output.s
./compiler -opt < input.tc > output.s   # 开启优化
```

## 四人分工

| 成员 | 模块 | 输入 | 输出 |
|------|------|------|------|
| **A** | 词法分析 + 语法分析 | 源码字符串 | AST |
| **B** | 语义分析 + IR 生成 | AST | 线性 IR |
| **C** | RISC-V32 代码生成 | 线性 IR | 汇编 |
| **D** | 优化 + 集成 + 测试 + 报告 | IR | 优化后 IR + 构建 |

每人可独立自测，互不阻塞：

```bash
cmake --build . --target test_lexer       # A
cmake --build . --target test_parser      # A
cmake --build . --target test_semantic    # B
cmake --build . --target test_codegen     # C
cmake --build . --target test_optimizer   # D
cmake --build . --target test_integration # D 端到端
```

## 项目结构

```
├── CMakeLists.txt
├── src/
│   ├── main.cpp           # D: 入口串联
│   ├── ast.h              # A: AST 定义
│   ├── ir.h               # B+C: IR 指令定义（核心契约）
│   ├── lexer.h/cpp        # A: 词法分析
│   ├── parser.h/cpp       # A: 语法分析
│   ├── symbol_table.h/cpp # B: 符号表
│   ├── semantic.h/cpp     # B: 语义检查 + IR 生成
│   ├── codegen.h/cpp      # C: IR → RISC-V 汇编
│   ├── optimizer.h/cpp    # D: IR 级优化
│   └── utils.h/cpp        # 工具函数
├── tests/                 # 每人独立测试
└── docs/                  # 实践报告
```
