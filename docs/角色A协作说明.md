# 角色 A 协作说明

## 目标

本说明用于帮助其他角色在协作过程中快速理解角色 A 的输出内容、调用方式和使用约定。

## 角色 A 的产出

角色 A 的主要产物是 AST，输入是 ToyC 源码字符串，输出是 `CompUnit` 根节点。

## 关键入口

- `Lexer::tokenize()`：把源码切分成 token 序列
- `Parser::parse()`：把 token 序列构造成 AST
- `dumpAst()`：输出可读的 AST 结构，便于调试

## 重要数据结构

### `CompUnit`
- `decls`：全局变量/常量声明
- `funcDefs`：函数定义

### `Stmt`
- 使用 `StmtKind` 区分不同类型
- 例如：`BLOCK`、`DECL`、`CONST_DECL`、`IF`、`WHILE`、`RETURN`

### `Expr`
- 使用 `ExprKind` 区分不同类型
- 例如：`INT_LITERAL`、`IDENTIFIER`、`BINARY`、`FUNC_CALL`

## 建议使用方式

```cpp
#include "lexer.h"
#include "parser.h"

std::string source = R"(int main() { return 0; })";
Lexer lexer(source);
auto tokens = lexer.tokenize();
Parser parser(tokens);
auto ast = parser.parse();
```

## 调试建议

如果需要观察 AST 结构，可以使用：

```cpp
std::cout << dumpAst(ast);
```

## 注意事项

- 角色 A 当前覆盖的是 ToyC 的核心前端子集
- 词法分析器会对非法字符和未闭合注释抛出异常
- 语义分析模块在使用 AST 时，应优先关注 `StmtKind` 和 `ExprKind` 的分类
