# 角色 A 协作文档：词法分析与语法分析接口说明

## 1. 角色定位

角色 A 负责把 ToyC 源码文本转换为统一的 AST，作为后续语义分析、IR 生成和代码生成的输入。当前已完成的前端闭环包括：

- 词法分析：识别关键字、标识符、数字、运算符、分隔符和文件结束符
- 语法分析：构建表达式、语句、函数定义和编译单元 AST
- 调试能力：提供 AST 输出函数，方便联调和排查问题
- 自测覆盖：词法器和解析器的回归测试已加入

## 2. 代码入口与文件说明

### 2.1 主要文件

- [src/lexer.h](../src/lexer.h) / [src/lexer.cpp](../src/lexer.cpp)
  - 词法分析器实现
- [src/parser.h](../src/parser.h) / [src/parser.cpp](../src/parser.cpp)
  - 语法分析器实现
- [src/ast.h](../src/ast.h)
  - AST 的核心数据结构定义
- [tests/test_lexer.cpp](../tests/test_lexer.cpp)
  - 词法分析自测
- [tests/test_parser.cpp](../tests/test_parser.cpp)
  - 语法分析自测

### 2.2 对外接口

角色 B 和其他角色应优先使用以下接口：

- `Lexer::tokenize()`：返回 token 序列
- `Parser::parse()`：返回 `std::unique_ptr<CompUnit>`
- `dumpAst(const std::unique_ptr<CompUnit>& unit)`：输出可读的 AST 结构，便于调试

## 3. 词法分析器能力说明

### 3.1 已支持的 token 类型

- 关键字：`int`、`void`、`const`、`if`、`else`、`while`、`break`、`continue`、`return`
- 标识符：`ID`
- 数字常量：`NUMBER`
- 运算符：`+ - * / %`
- 关系运算符：`< > <= >= == !=`
- 逻辑运算符：`&& || !`
- 分隔符：`( ) { } ; , =`
- 文件结束：`END`

### 3.2 处理规则

- 空白字符会被忽略
- 单行注释 `// ...` 和多行注释 `/* ... */` 会被跳过
- 出现非法字符或未闭合注释时，会抛出异常，便于上层处理
- 每个 token 会记录行号和列号，便于报错定位

## 4. 语法分析器能力说明

### 4.1 已支持的语法结构

当前解析器已能处理以下内容：

- 全局声明和函数定义的混合编译单元
- 函数定义，包括返回类型、函数名、形参和函数体
- 语句块、空语句、表达式语句、赋值语句
- 变量声明和常量声明
- `if / else`、`while`、`break`、`continue`、`return`
- 表达式的优先级与结合性：逻辑或、逻辑与、关系运算、加减、乘除、单目运算、括号表达式、函数调用

### 4.2 语法约定

- 变量和常量声明要求带初始化表达式
- 语句块通过 `{ ... }` 表示
- 赋值语句使用 `=`
- `return` 语句支持有无返回表达式两种形式

## 5. AST 结构与契约

角色 B 及后续模块应直接基于 AST 进行消费。AST 的核心结构定义见 [src/ast.h](../src/ast.h)。

### 5.1 根节点

- `CompUnit`
  - `decls`：全局变量/常量声明
  - `funcDefs`：函数定义

### 5.2 语句节点

- `StmtKind::BLOCK`：语句块
- `StmtKind::EMPTY`：空语句
- `StmtKind::EXPR`：表达式语句
- `StmtKind::ASSIGN`：赋值语句
- `StmtKind::DECL`：普通变量声明
- `StmtKind::CONST_DECL`：常量声明
- `StmtKind::IF`：条件语句
- `StmtKind::WHILE`：循环语句
- `StmtKind::BREAK`：break
- `StmtKind::CONTINUE`：continue
- `StmtKind::RETURN`：return

### 5.3 表达式节点

- `ExprKind::INT_LITERAL`：整数常量
- `ExprKind::IDENTIFIER`：标识符
- `ExprKind::UNARY`：一元表达式
- `ExprKind::BINARY`：二元表达式
- `ExprKind::FUNC_CALL`：函数调用

## 6. 其他角色如何使用角色 A 的输出

### 6.1 推荐调用方式

其他角色可以按下面方式接入前端：

```cpp
#include "lexer.h"
#include "parser.h"

std::string source = R"(int main() { return 0; })";
Lexer lexer(source);
auto tokens = lexer.tokenize();
Parser parser(tokens);
auto ast = parser.parse();
```

### 6.2 调试 AST

如果需要查看生成的树结构，可调用：

```cpp
std::string dump = dumpAst(ast);
std::cout << dump;
```

这对于语义分析模块和后续联调非常有帮助。

## 7. 约束与注意事项

- 当前实现面向 ToyC 的核心子集，适合课程实验和模块联调
- 未实现数组、指针、复杂类型系统等高级特性
- 解析器目前更偏“能构造正确 AST 的稳定前端”，而不是面向极其健壮的错误恢复
- 语义分析阶段仍需依赖 AST 进行类型检查、作用域检查和 IR 生成

## 8. 自测与验证

可直接使用以下方式进行验证：

```bash
cl /nologo /EHsc /std:c++20 /I src tests\test_lexer.cpp src\lexer.cpp /Fe:build\test_lexer.exe
cl /nologo /EHsc /std:c++20 /I src tests\test_parser.cpp src\lexer.cpp src\parser.cpp /Fe:build\test_parser.exe
build\test_lexer.exe
build\test_parser.exe
```

## 9. 对后续角色的建议

- 角色 B 在做语义分析时，应优先遍历 `CompUnit::funcDefs` 和 `CompUnit::decls`
- 需要关注 `StmtKind` 和 `ExprKind` 的区分，避免把不同语句类型混淆
- 若发现解析结果不符合预期，可先用 `dumpAst` 检查 AST，再继续排查语义或 IR 生成问题
