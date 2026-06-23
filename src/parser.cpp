#include "parser.h"
#include <iostream>

Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens), pos(0) {}

const Token& Parser::peek() const { return tokens[pos]; }
const Token& Parser::advance() { return tokens[pos++]; }
bool Parser::match(TokenType type) {
    if (check(type)) { advance(); return true; }
    return false;
}
bool Parser::check(TokenType type) const { return peek().type == type; }
const Token& Parser::expect(TokenType type) {
    if (check(type)) return advance();
    std::cerr << "Syntax error at line " << peek().line
              << ": expected token " << static_cast<int>(type)
              << ", got '" << peek().value << "'\n";
    return advance();
}
const Token& Parser::previous() const { return tokens[pos - 1]; }

std::unique_ptr<CompUnit> Parser::parse() { return parseCompUnit(); }

std::unique_ptr<CompUnit> Parser::parseCompUnit() {
    auto unit = std::make_unique<CompUnit>();
    while (!check(TokenType::END)) {
        // Try to parse a function definition or a global declaration
        // FuncDef starts with "int" or "void" followed by ID and '('
        // VarDecl starts with "int" followed by ID and '='
        // ConstDecl starts with "const"
        if (check(TokenType::CONST)) {
            unit->decls.push_back(parseConstDecl());
        } else if (check(TokenType::INT)) {
            // Look ahead: if next token is ID and then '(' it's FuncDef
            if (pos + 2 < tokens.size() &&
                tokens[pos + 1].type == TokenType::ID &&
                tokens[pos + 2].type == TokenType::LPAREN) {
                unit->funcDefs.push_back(parseFuncDef());
            } else {
                unit->decls.push_back(parseVarDecl());
            }
        } else if (check(TokenType::VOID)) {
            unit->funcDefs.push_back(parseFuncDef());
        } else {
            break;
        }
    }
    return unit;
}

std::unique_ptr<FuncDef> Parser::parseFuncDef() {
    FuncType type;
    if (check(TokenType::INT)) {
        advance();
        type = FuncType::INT;
    } else {
        advance(); // void
        type = FuncType::VOID;
    }
    return parseFuncDefWithType(type);
}

std::unique_ptr<FuncDef> Parser::parseFuncDefWithType(FuncType type) {
    auto func = std::make_unique<FuncDef>();
    func->returnType = type;
    func->name = expect(TokenType::ID).value;
    expect(TokenType::LPAREN);
    func->params = parseParams();
    expect(TokenType::RPAREN);
    func->body = parseBlock();
    return func;
}

std::vector<Param> Parser::parseParams() {
    std::vector<Param> params;
    if (check(TokenType::INT)) {
        advance();
        params.push_back({expect(TokenType::ID).value});
        while (match(TokenType::COMMA)) {
            expect(TokenType::INT);
            params.push_back({expect(TokenType::ID).value});
        }
    }
    return params;
}

std::unique_ptr<Stmt> Parser::parseStmt() {
    if (check(TokenType::LBRACE)) return parseBlock();
    if (check(TokenType::SEMICOLON)) { advance(); return std::make_unique<Stmt>(Stmt{StmtKind::EMPTY}); }
    if (check(TokenType::CONST)) return parseDecl();
    if (check(TokenType::INT)) {
        // Variable declaration in statement
        return parseVarDecl();
    }
    if (check(TokenType::IF)) {
        advance();
        auto stmt = std::make_unique<Stmt>();
        stmt->kind = StmtKind::IF;
        expect(TokenType::LPAREN);
        stmt->cond = parseExpr();
        expect(TokenType::RPAREN);
        stmt->thenStmt = parseStmt();
        if (match(TokenType::ELSE)) {
            stmt->elseStmt = parseStmt();
        }
        return stmt;
    }
    if (check(TokenType::WHILE)) {
        advance();
        auto stmt = std::make_unique<Stmt>();
        stmt->kind = StmtKind::WHILE;
        expect(TokenType::LPAREN);
        stmt->cond = parseExpr();
        expect(TokenType::RPAREN);
        stmt->body = parseStmt();
        return stmt;
    }
    if (check(TokenType::BREAK)) {
        advance();
        expect(TokenType::SEMICOLON);
        auto stmt = std::make_unique<Stmt>();
        stmt->kind = StmtKind::BREAK;
        return stmt;
    }
    if (check(TokenType::CONTINUE)) {
        advance();
        expect(TokenType::SEMICOLON);
        auto stmt = std::make_unique<Stmt>();
        stmt->kind = StmtKind::CONTINUE;
        return stmt;
    }
    if (check(TokenType::RETURN)) {
        advance();
        auto stmt = std::make_unique<Stmt>();
        stmt->kind = StmtKind::RETURN;
        if (!check(TokenType::SEMICOLON)) {
            stmt->retExpr = parseExpr();
        }
        expect(TokenType::SEMICOLON);
        return stmt;
    }
    // Expression statement or assignment
    auto expr = parseExpr();
    if (check(TokenType::ASSIGN) && previous().type == TokenType::ID) {
        auto stmt = std::make_unique<Stmt>();
        stmt->kind = StmtKind::ASSIGN;
        stmt->varName = previous().value;
        advance(); // '='
        stmt->expr = parseExpr();
        expect(TokenType::SEMICOLON);
        return stmt;
    }
    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::EXPR;
    stmt->expr = std::move(expr);
    expect(TokenType::SEMICOLON);
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseBlock() {
    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::BLOCK;
    expect(TokenType::LBRACE);
    while (!check(TokenType::RBRACE) && !check(TokenType::END)) {
        stmt->stmts.push_back(parseStmt());
    }
    expect(TokenType::RBRACE);
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseDecl() { return parseConstDecl(); }

std::unique_ptr<Stmt> Parser::parseConstDecl() {
    expect(TokenType::CONST);
    auto stmt = parseVarDecl();
    stmt->kind = StmtKind::CONST_DECL;
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseVarDecl() {
    expect(TokenType::INT);
    auto stmt = std::make_unique<Stmt>();
    stmt->kind = StmtKind::DECL;
    stmt->varName = expect(TokenType::ID).value;
    expect(TokenType::ASSIGN);
    stmt->init = parseExpr();
    expect(TokenType::SEMICOLON);
    return stmt;
}

std::unique_ptr<Expr> Parser::parseExpr() { return parseLOrExpr(); }

std::unique_ptr<Expr> Parser::parseLOrExpr() {
    auto expr = parseLAndExpr();
    while (match(TokenType::OR)) {
        auto newExpr = std::make_unique<Expr>();
        newExpr->kind = ExprKind::BINARY;
        newExpr->binaryOp = BinaryOp::OR;
        newExpr->lhs = std::move(expr);
        newExpr->rhs = parseLAndExpr();
        expr = std::move(newExpr);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseLAndExpr() {
    auto expr = parseRelExpr();
    while (match(TokenType::AND)) {
        auto newExpr = std::make_unique<Expr>();
        newExpr->kind = ExprKind::BINARY;
        newExpr->binaryOp = BinaryOp::AND;
        newExpr->lhs = std::move(expr);
        newExpr->rhs = parseRelExpr();
        expr = std::move(newExpr);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseRelExpr() {
    auto expr = parseAddExpr();
    while (true) {
        if (match(TokenType::LT)) { auto e = std::make_unique<Expr>(); e->kind = ExprKind::BINARY; e->binaryOp = BinaryOp::LT; e->lhs = std::move(expr); e->rhs = parseAddExpr(); expr = std::move(e); }
        else if (match(TokenType::GT)) { auto e = std::make_unique<Expr>(); e->kind = ExprKind::BINARY; e->binaryOp = BinaryOp::GT; e->lhs = std::move(expr); e->rhs = parseAddExpr(); expr = std::move(e); }
        else if (match(TokenType::LE)) { auto e = std::make_unique<Expr>(); e->kind = ExprKind::BINARY; e->binaryOp = BinaryOp::LE; e->lhs = std::move(expr); e->rhs = parseAddExpr(); expr = std::move(e); }
        else if (match(TokenType::GE)) { auto e = std::make_unique<Expr>(); e->kind = ExprKind::BINARY; e->binaryOp = BinaryOp::GE; e->lhs = std::move(expr); e->rhs = parseAddExpr(); expr = std::move(e); }
        else if (match(TokenType::EQ)) { auto e = std::make_unique<Expr>(); e->kind = ExprKind::BINARY; e->binaryOp = BinaryOp::EQ; e->lhs = std::move(expr); e->rhs = parseAddExpr(); expr = std::move(e); }
        else if (match(TokenType::NE)) { auto e = std::make_unique<Expr>(); e->kind = ExprKind::BINARY; e->binaryOp = BinaryOp::NE; e->lhs = std::move(expr); e->rhs = parseAddExpr(); expr = std::move(e); }
        else break;
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseAddExpr() {
    auto expr = parseMulExpr();
    while (true) {
        if (match(TokenType::PLUS)) { auto e = std::make_unique<Expr>(); e->kind = ExprKind::BINARY; e->binaryOp = BinaryOp::ADD; e->lhs = std::move(expr); e->rhs = parseMulExpr(); expr = std::move(e); }
        else if (match(TokenType::MINUS)) { auto e = std::make_unique<Expr>(); e->kind = ExprKind::BINARY; e->binaryOp = BinaryOp::SUB; e->lhs = std::move(expr); e->rhs = parseMulExpr(); expr = std::move(e); }
        else break;
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseMulExpr() {
    auto expr = parseUnaryExpr();
    while (true) {
        if (match(TokenType::STAR)) { auto e = std::make_unique<Expr>(); e->kind = ExprKind::BINARY; e->binaryOp = BinaryOp::MUL; e->lhs = std::move(expr); e->rhs = parseUnaryExpr(); expr = std::move(e); }
        else if (match(TokenType::SLASH)) { auto e = std::make_unique<Expr>(); e->kind = ExprKind::BINARY; e->binaryOp = BinaryOp::DIV; e->lhs = std::move(expr); e->rhs = parseUnaryExpr(); expr = std::move(e); }
        else if (match(TokenType::PERCENT)) { auto e = std::make_unique<Expr>(); e->kind = ExprKind::BINARY; e->binaryOp = BinaryOp::MOD; e->lhs = std::move(expr); e->rhs = parseUnaryExpr(); expr = std::move(e); }
        else break;
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseUnaryExpr() {
    if (match(TokenType::PLUS)) {
        auto expr = std::make_unique<Expr>();
        expr->kind = ExprKind::UNARY;
        expr->unaryOp = UnaryOp::PLUS;
        expr->operand = parseUnaryExpr();
        return expr;
    }
    if (match(TokenType::MINUS)) {
        auto expr = std::make_unique<Expr>();
        expr->kind = ExprKind::UNARY;
        expr->unaryOp = UnaryOp::MINUS;
        expr->operand = parseUnaryExpr();
        return expr;
    }
    if (match(TokenType::NOT)) {
        auto expr = std::make_unique<Expr>();
        expr->kind = ExprKind::UNARY;
        expr->unaryOp = UnaryOp::NOT;
        expr->operand = parseUnaryExpr();
        return expr;
    }
    return parsePrimaryExpr();
}

std::unique_ptr<Expr> Parser::parsePrimaryExpr() {
    if (match(TokenType::NUMBER)) {
        auto expr = std::make_unique<Expr>();
        expr->kind = ExprKind::INT_LITERAL;
        expr->intValue = std::stoi(previous().value);
        return expr;
    }
    if (match(TokenType::ID)) {
        std::string idName = previous().value;
        if (match(TokenType::LPAREN)) {
            // Function call
            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::FUNC_CALL;
            expr->name = idName;
            if (!check(TokenType::RPAREN)) {
                expr->args.push_back(parseExpr());
                while (match(TokenType::COMMA)) {
                    expr->args.push_back(parseExpr());
                }
            }
            expect(TokenType::RPAREN);
            return expr;
        }
        auto expr = std::make_unique<Expr>();
        expr->kind = ExprKind::IDENTIFIER;
        expr->name = idName;
        return expr;
    }
    if (match(TokenType::LPAREN)) {
        auto expr = parseExpr();
        expect(TokenType::RPAREN);
        return expr;
    }
    std::cerr << "Syntax error at line " << peek().line << ": unexpected token '" << peek().value << "'\n";
    advance();
    return std::make_unique<Expr>();
}
