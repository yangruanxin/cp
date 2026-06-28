#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"
#include <memory>
#include <vector>

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);
    std::unique_ptr<CompUnit> parse();

private:
    const std::vector<Token>& tokens;
    size_t pos;

    const Token& peek() const;
    const Token& advance();
    bool match(TokenType type);
    bool check(TokenType type) const;
    const Token& expect(TokenType type);
    const Token& previous() const;

    // Grammar rules
    std::unique_ptr<CompUnit> parseCompUnit();
    std::unique_ptr<Stmt> parseDecl();
    std::unique_ptr<Stmt> parseConstDecl();
    std::unique_ptr<Stmt> parseVarDecl();
    std::unique_ptr<FuncDef> parseFuncDef();
    std::unique_ptr<FuncDef> parseFuncDefWithType(FuncType type);
    std::unique_ptr<Stmt> parseStmt();
    std::unique_ptr<Stmt> parseBlock();
    std::vector<Param> parseParams();
    std::unique_ptr<Expr> parseExpr();
    std::unique_ptr<Expr> parseLOrExpr();
    std::unique_ptr<Expr> parseLAndExpr();
    std::unique_ptr<Expr> parseRelExpr();
    std::unique_ptr<Expr> parseAddExpr();
    std::unique_ptr<Expr> parseMulExpr();
    std::unique_ptr<Expr> parseUnaryExpr();
    std::unique_ptr<Expr> parsePrimaryExpr();
};

std::string dumpAst(const std::unique_ptr<CompUnit>& unit);

#endif
