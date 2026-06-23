# ToyC 编译器

编译原理课程实践项目 — 将 ToyC 语言编译为 RISC-V32 汇编代码。

## 快速开始

```bash
# 构建
mkdir build && cd build
cmake ..
cmake --build .

# 编译 ToyC 源文件
./compiler < input.tc > output.s

# 开启优化
./compiler -opt < input.tc > output.s
```

## 构建

要求：CMake 3.20+，C++20 编译器

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

测试编译：

```bash
cmake --build . --target test_lexer
cmake --build . --target test_parser
cmake --build . --target test_semantic
cmake --build . --target test_codegen
cmake --build . --target test_integration
```

## 项目结构

```
├── CMakeLists.txt         # 构建配置
├── src/
│   ├── main.cpp           # 入口
│   ├── ast.h              # AST 节点定义
│   ├── lexer.h/cpp        # 词法分析
│   ├── parser.h/cpp       # 语法分析
│   ├── symbol_table.h/cpp # 符号表
│   ├── semantic.h/cpp     # 语义分析
│   ├── codegen.h/cpp      # 代码生成 (RISC-V32)
│   ├── optimizer.h/cpp    # 优化器
│   └── utils.h/cpp        # 工具函数
├── tests/                 # 模块测试 & 集成测试
└── docs/                  # 文档
```

## 小组分工

| 成员 | 模块 |
|------|------|
| A | 词法分析 + 语法分析 |
| B | 语义分析 + 符号表 |
| C | RISC-V32 代码生成 |
| D | 集成 + 测试 + 优化 + 报告 |

## ToyC 语言

ToyC 是 C 语言的子集，支持 int/void 函数、全局/局部变量、const 常量、if/while 语句、break/continue、逻辑/关系/算术运算，具有嵌套作用域和短路求值语义。详见 `任务要求.md`。
