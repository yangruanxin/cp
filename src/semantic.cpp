#include "semantic.h"
#include <iostream>
#include <cassert>

IRGenerator::IRGenerator()
    : labelCounter(0), tempCounter(0), inLoop(false), hasReturn(false) {}

std::string IRGenerator::newLabel() {
    return ".L" + std::to_string(labelCounter++);
}

std::string IRGenerator::newTemp() {
    return "%t" + std::to_string(tempCounter++);
}

IRList IRGenerator::generate(const std::unique_ptr<CompUnit>& unit) {
    ir.clear();
    visitCompUnit(unit);
    return std::move(ir);
}

void IRGenerator::visitCompUnit(const std::unique_ptr<CompUnit>& unit) {
    // First pass: register all functions
    for (const auto& func : unit->funcDefs) {
        std::vector<FuncType> paramTypes;
        for (size_t i = 0; i < func->params.size(); i++) {
            paramTypes.push_back(FuncType::INT);
        }
        symTable.addFunction(func->name, func->returnType, paramTypes);
    }

    // Second pass: handle global variable declarations
    for (const auto& decl : unit->decls) {
        if (decl->kind == StmtKind::DECL || decl->kind == StmtKind::CONST_DECL) {
            bool isConst = (decl->kind == StmtKind::CONST_DECL);
            int val = 0;
            if (decl->init) {
                val = [&]() { /* simplified const eval */ return 0; }();
            }
            symTable.addVariable(decl->varName, isConst, true, val, true);
            ir.push_back(IRInstr::decl(decl->varName, true, val));
        }
    }

    // Third pass: generate IR for each function
    for (const auto& func : unit->funcDefs) {
        visitFuncDef(func);
    }

    // Check main function exists
    auto* mainSym = symTable.lookup("main");
    if (!mainSym || !mainSym->isFunction) {
        std::cerr << "Semantic error: missing main function\n";
    }
}

void IRGenerator::visitFuncDef(const std::unique_ptr<FuncDef>& func) {
    currentFuncReturnType = func->returnType;
    hasReturn = false;

    ir.push_back(IRInstr::funcBegin(func->name));

    symTable.enterScope();
    for (const auto& p : func->params) {
        symTable.addVariable(p.name, false, false);
        ir.push_back(IRInstr::decl(p.name, false, 0));
    }
    visitStmt(func->body);
    symTable.exitScope();

    if (currentFuncReturnType == FuncType::INT && !hasReturn) {
        std::cerr << "Semantic error: function '" << func->name
                  << "' must return a value on all paths\n";
    }

    ir.push_back(IRInstr::funcEnd());
}

void IRGenerator::visitStmt(const std::unique_ptr<Stmt>& stmt) {
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
        case StmtKind::EXPR: {
            std::string tmp = visitExpr(stmt->expr);
            (void)tmp;
            break;
        }
        case StmtKind::ASSIGN: {
            auto* sym = symTable.lookup(stmt->varName);
            if (!sym) {
                std::cerr << "Semantic error: undefined variable '" << stmt->varName << "'\n";
            } else if (sym->isConst) {
                std::cerr << "Semantic error: cannot assign to const '" << stmt->varName << "'\n";
            }
            std::string val = visitExpr(stmt->expr);
            // Store to variable
            if (sym && sym->isGlobal) {
                ir.push_back(IRInstr::store(stmt->varName, 0, val));
            } else {
                ir.push_back(IRInstr::store(stmt->varName, 0, val));
            }
            break;
        }
        case StmtKind::DECL: {
            std::string val = visitExpr(stmt->init);
            symTable.addVariable(stmt->varName, false, symTable.isGlobalScope());
            ir.push_back(IRInstr::decl(stmt->varName, symTable.isGlobalScope(), 0));
            ir.push_back(IRInstr::assign(stmt->varName, val));
            break;
        }
        case StmtKind::CONST_DECL: {
            std::string val = visitExpr(stmt->init);
            symTable.addVariable(stmt->varName, true, symTable.isGlobalScope());
            ir.push_back(IRInstr::decl(stmt->varName, symTable.isGlobalScope(), 0));
            ir.push_back(IRInstr::assign(stmt->varName, val));
            break;
        }
        case StmtKind::IF: {
            std::string cond = visitExpr(stmt->cond);
            std::string elseLabel = newLabel();
            std::string endLabel = newLabel();
            ir.push_back(IRInstr::ifGoto(cond, elseLabel));
            visitStmt(stmt->thenStmt);
            ir.push_back(IRInstr::goto_(endLabel));
            ir.push_back(IRInstr::label(elseLabel));
            if (stmt->elseStmt) {
                visitStmt(stmt->elseStmt);
            }
            ir.push_back(IRInstr::label(endLabel));
            break;
        }
        case StmtKind::WHILE: {
            std::string beginLabel = newLabel();
            std::string endLabel = newLabel();
            bool outerLoop = inLoop;
            std::string outerBegin = loopBeginLabel;
            std::string outerEnd = loopEndLabel;
            inLoop = true;
            loopBeginLabel = beginLabel;
            loopEndLabel = endLabel;

            ir.push_back(IRInstr::label(beginLabel));
            std::string cond = visitExpr(stmt->cond);
            ir.push_back(IRInstr::ifGoto(cond, endLabel));
            visitStmt(stmt->body);
            ir.push_back(IRInstr::goto_(beginLabel));
            ir.push_back(IRInstr::label(endLabel));

            inLoop = outerLoop;
            loopBeginLabel = outerBegin;
            loopEndLabel = outerEnd;
            break;
        }
        case StmtKind::BREAK:
            if (!inLoop) {
                std::cerr << "Semantic error: break outside loop\n";
            } else {
                ir.push_back(IRInstr::goto_(loopEndLabel));
            }
            break;
        case StmtKind::CONTINUE:
            if (!inLoop) {
                std::cerr << "Semantic error: continue outside loop\n";
            } else {
                ir.push_back(IRInstr::goto_(loopBeginLabel));
            }
            break;
        case StmtKind::RETURN:
            hasReturn = true;
            if (currentFuncReturnType == FuncType::VOID && stmt->retExpr) {
                std::cerr << "Semantic error: void function should not return a value\n";
            } else if (currentFuncReturnType == FuncType::INT && !stmt->retExpr) {
                std::cerr << "Semantic error: int function must return a value\n";
            }
            if (stmt->retExpr) {
                std::string val = visitExpr(stmt->retExpr);
                ir.push_back(IRInstr::assign("a0", val));
            }
            ir.push_back(IRInstr::ret());
            break;
    }
}

std::string IRGenerator::visitExpr(const std::unique_ptr<Expr>& expr) {
    switch (expr->kind) {
        case ExprKind::INT_LITERAL: {
            std::string t = newTemp();
            ir.push_back(IRInstr::li(t, expr->intValue));
            return t;
        }
        case ExprKind::IDENTIFIER: {
            auto* sym = symTable.lookup(expr->name);
            if (!sym) {
                std::cerr << "Semantic error: undefined identifier '" << expr->name << "'\n";
                std::string t = newTemp();
                ir.push_back(IRInstr::li(t, 0));
                return t;
            }
            if (sym->valueKnown) {
                expr->isConst = true;
                expr->constValue = sym->value;
                std::string t = newTemp();
                ir.push_back(IRInstr::li(t, sym->value));
                return t;
            }
            std::string t = newTemp();
            ir.push_back(IRInstr::load(t, expr->name, 0));
            return t;
        }
        case ExprKind::UNARY: {
            std::string op = visitExpr(expr->operand);
            std::string t = newTemp();
            switch (expr->unaryOp) {
                case UnaryOp::PLUS:  ir.push_back(IRInstr::assign(t, op)); break;
                case UnaryOp::MINUS: ir.push_back(IRInstr::una(IROp::NEG, t, op)); break;
                case UnaryOp::NOT:   ir.push_back(IRInstr::una(IROp::NOT, t, op)); break;
            }
            return t;
        }
        case ExprKind::BINARY: {
            std::string lhs = visitExpr(expr->lhs);
            std::string rhs = visitExpr(expr->rhs);
            std::string t = newTemp();

            auto opToIROp = [](BinaryOp bop) -> IROp {
                switch (bop) {
                    case BinaryOp::ADD: return IROp::ADD;
                    case BinaryOp::SUB: return IROp::SUB;
                    case BinaryOp::MUL: return IROp::MUL;
                    case BinaryOp::DIV: return IROp::DIV;
                    case BinaryOp::MOD: return IROp::MOD;
                    case BinaryOp::LT:  return IROp::LT;
                    case BinaryOp::GT:  return IROp::GT;
                    case BinaryOp::LE:  return IROp::LE;
                    case BinaryOp::GE:  return IROp::GE;
                    case BinaryOp::EQ:  return IROp::EQ;
                    case BinaryOp::NE:  return IROp::NE;
                    case BinaryOp::AND: return IROp::AND;
                    case BinaryOp::OR:  return IROp::OR;
                }
                return IROp::ADD;
            };

            if (expr->binaryOp == BinaryOp::AND) {
                // Short-circuit: t = lhs && rhs
                std::string Lfalse = newLabel();
                std::string Lend = newLabel();
                ir.push_back(IRInstr::assign(t, lhs));
                ir.push_back(IRInstr::ifGoto(t, Lfalse));
                std::string r = visitExpr(expr->rhs);
                ir.push_back(IRInstr::assign(t, r));
                ir.push_back(IRInstr::goto_(Lend));
                ir.push_back(IRInstr::label(Lfalse));
                ir.push_back(IRInstr::li(t, 0));
                ir.push_back(IRInstr::label(Lend));
            } else if (expr->binaryOp == BinaryOp::OR) {
                // Short-circuit: t = lhs || rhs
                std::string Ltrue = newLabel();
                std::string Lend = newLabel();
                ir.push_back(IRInstr::assign(t, lhs));
                ir.push_back(IRInstr::ifGoto(t, Ltrue));
                std::string r = visitExpr(expr->rhs);
                ir.push_back(IRInstr::assign(t, r));
                ir.push_back(IRInstr::goto_(Lend));
                ir.push_back(IRInstr::label(Ltrue));
                ir.push_back(IRInstr::li(t, 1));
                ir.push_back(IRInstr::label(Lend));
            } else {
                ir.push_back(IRInstr::bin(opToIROp(expr->binaryOp), t, lhs, rhs));
            }
            return t;
        }
        case ExprKind::FUNC_CALL: {
            for (size_t i = 0; i < expr->args.size(); i++) {
                std::string argVal = visitExpr(expr->args[i]);
                ir.push_back(IRInstr::arg(argVal, (int)i));
            }
            std::string t = newTemp();
            ir.push_back(IRInstr::call(t, expr->name));
            return t;
        }
    }
    return newTemp();
}
