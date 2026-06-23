#include "semantic.h"
#include <iostream>

SemanticAnalyzer::SemanticAnalyzer() : inLoop(false), hasReturn(false) {}

void SemanticAnalyzer::analyze(const std::unique_ptr<CompUnit>& unit) {
    visitCompUnit(unit);
}

void SemanticAnalyzer::visitCompUnit(const std::unique_ptr<CompUnit>& unit) {
    // First pass: collect all function declarations
    for (const auto& func : unit->funcDefs) {
        std::vector<FuncType> paramTypes;
        for (const auto& p : func->params) {
            paramTypes.push_back(FuncType::INT);
        }
        symTable.addFunction(func->name, func->returnType, paramTypes);
    }

    // Second pass: analyze functions
    for (const auto& func : unit->funcDefs) {
        visitFuncDef(func);
    }

    // Check main function exists
    auto* mainSym = symTable.lookup("main");
    if (!mainSym || !mainSym->isFunction) {
        std::cerr << "Semantic error: missing main function\n";
    }
}

void SemanticAnalyzer::visitFuncDef(const std::unique_ptr<FuncDef>& func) {
    currentFuncReturnType = func->returnType;
    hasReturn = false;

    symTable.enterScope();
    for (const auto& p : func->params) {
        symTable.addVariable(p.name, false, false);
    }
    visitStmt(func->body);
    symTable.exitScope();

    // Check return path for int functions
    if (currentFuncReturnType == FuncType::INT && !hasReturn) {
        std::cerr << "Semantic error: function '" << func->name
                  << "' must return a value on all paths\n";
    }
}

void SemanticAnalyzer::visitStmt(const std::unique_ptr<Stmt>& stmt) {
    switch (stmt->kind) {
        case StmtKind::BLOCK: {
            symTable.enterScope();
            for (const auto& s : stmt->stmts) {
                visitStmt(s);
            }
            symTable.exitScope();
            break;
        }
        case StmtKind::EMPTY:
            break;
        case StmtKind::EXPR:
            visitExpr(stmt->expr);
            break;
        case StmtKind::ASSIGN: {
            auto* sym = symTable.lookup(stmt->varName);
            if (!sym) {
                std::cerr << "Semantic error: undefined variable '" << stmt->varName << "'\n";
            } else if (sym->isConst) {
                std::cerr << "Semantic error: cannot assign to const '" << stmt->varName << "'\n";
            } else if (sym->isFunction) {
                std::cerr << "Semantic error: cannot assign to function '" << stmt->varName << "'\n";
            }
            visitExpr(stmt->expr);
            break;
        }
        case StmtKind::DECL: {
            int val = visitExpr(stmt->init);
            symTable.addVariable(stmt->varName, false, symTable.isGlobalScope());
            break;
        }
        case StmtKind::CONST_DECL: {
            int val = visitExpr(stmt->init);
            symTable.addVariable(stmt->varName, true, symTable.isGlobalScope(), val, true);
            break;
        }
        case StmtKind::IF: {
            visitExpr(stmt->cond);
            visitStmt(stmt->thenStmt);
            if (stmt->elseStmt) {
                visitStmt(stmt->elseStmt);
            }
            break;
        }
        case StmtKind::WHILE: {
            visitExpr(stmt->cond);
            bool outerLoop = inLoop;
            inLoop = true;
            visitStmt(stmt->body);
            inLoop = outerLoop;
            break;
        }
        case StmtKind::BREAK:
            if (!inLoop) {
                std::cerr << "Semantic error: break outside loop\n";
            }
            break;
        case StmtKind::CONTINUE:
            if (!inLoop) {
                std::cerr << "Semantic error: continue outside loop\n";
            }
            break;
        case StmtKind::RETURN:
            hasReturn = true;
            if (currentFuncReturnType == FuncType::VOID && stmt->retExpr) {
                std::cerr << "Semantic error: void function should not return a value\n";
            } else if (currentFuncReturnType == FuncType::INT && !stmt->retExpr) {
                std::cerr << "Semantic error: int function must return a value\n";
            }
            if (stmt->retExpr) visitExpr(stmt->retExpr);
            break;
    }
}

int SemanticAnalyzer::visitExpr(const std::unique_ptr<Expr>& expr) {
    switch (expr->kind) {
        case ExprKind::INT_LITERAL:
            return expr->intValue;
        case ExprKind::IDENTIFIER: {
            auto* sym = symTable.lookup(expr->name);
            if (!sym) {
                std::cerr << "Semantic error: undefined identifier '" << expr->name << "'\n";
                return 0;
            }
            if (sym->valueKnown) {
                expr->isConst = true;
                expr->constValue = sym->value;
                return sym->value;
            }
            return 0;
        }
        case ExprKind::UNARY: {
            int val = visitExpr(expr->operand);
            switch (expr->unaryOp) {
                case UnaryOp::PLUS: return val;
                case UnaryOp::MINUS: return -val;
                case UnaryOp::NOT: return !val;
            }
            return 0;
        }
        case ExprKind::BINARY: {
            int l = visitExpr(expr->lhs);
            int r = visitExpr(expr->rhs);
            if (expr->lhs->isConst && expr->rhs->isConst) {
                expr->isConst = true;
                switch (expr->binaryOp) {
                    case BinaryOp::ADD: expr->constValue = l + r; break;
                    case BinaryOp::SUB: expr->constValue = l - r; break;
                    case BinaryOp::MUL: expr->constValue = l * r; break;
                    case BinaryOp::DIV: expr->constValue = l / r; break;
                    case BinaryOp::MOD: expr->constValue = l % r; break;
                    case BinaryOp::LT:  expr->constValue = l < r; break;
                    case BinaryOp::GT:  expr->constValue = l > r; break;
                    case BinaryOp::LE:  expr->constValue = l <= r; break;
                    case BinaryOp::GE:  expr->constValue = l >= r; break;
                    case BinaryOp::EQ:  expr->constValue = l == r; break;
                    case BinaryOp::NE:  expr->constValue = l != r; break;
                    case BinaryOp::AND: expr->constValue = l && r; break;
                    case BinaryOp::OR:  expr->constValue = l || r; break;
                }
                return expr->constValue;
            }
            return 0;
        }
        case ExprKind::FUNC_CALL: {
            auto* sym = symTable.lookup(expr->name);
            if (!sym || !sym->isFunction) {
                std::cerr << "Semantic error: undefined function '" << expr->name << "'\n";
                return 0;
            }
            for (const auto& arg : expr->args) {
                visitExpr(arg);
            }
            return 0;
        }
    }
    return 0;
}
