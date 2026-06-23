#ifndef AST_H
#define AST_H

#include <memory>
#include <string>
#include <vector>

// Forward declarations
class ASTVisitor;

enum class ExprKind {
    INT_LITERAL, IDENTIFIER,
    UNARY, BINARY,
    FUNC_CALL
};

enum class UnaryOp { PLUS, MINUS, NOT };
enum class BinaryOp {
    ADD, SUB, MUL, DIV, MOD,
    LT, GT, LE, GE, EQ, NE,
    AND, OR
};
enum class StmtKind {
    BLOCK, EMPTY, EXPR, ASSIGN,
    DECL, CONST_DECL,
    IF, WHILE, BREAK, CONTINUE, RETURN
};
enum class FuncType { INT, VOID };

struct Expr {
    ExprKind kind;
    // IntLiteral
    int intValue;
    // Identifier / FuncCall
    std::string name;
    // Unary
    UnaryOp unaryOp;
    std::unique_ptr<Expr> operand;
    // Binary
    BinaryOp binaryOp;
    std::unique_ptr<Expr> lhs, rhs;
    // FuncCall
    std::vector<std::unique_ptr<Expr>> args;
    // for type checking
    bool isConst = false;
    int constValue = 0;
};

struct Stmt {
    StmtKind kind;
    // Block
    std::vector<std::unique_ptr<Stmt>> stmts;
    // Expr / Assign
    std::unique_ptr<Expr> expr;
    std::string varName;  // for Assign and Decl
    // Decl / ConstDecl
    bool hasInit = false;
    std::unique_ptr<Expr> init;
    // If
    std::unique_ptr<Expr> cond;
    std::unique_ptr<Stmt> thenStmt, elseStmt;
    // While
    std::unique_ptr<Stmt> body;
    // Return
    std::unique_ptr<Expr> retExpr;
};

struct Param {
    std::string name;
};

struct FuncDef {
    FuncType returnType;
    std::string name;
    std::vector<Param> params;
    std::unique_ptr<Stmt> body;
};

struct CompUnit {
    std::vector<std::unique_ptr<Stmt>> decls;      // global var/const decls
    std::vector<std::unique_ptr<FuncDef>> funcDefs;
};

#endif
