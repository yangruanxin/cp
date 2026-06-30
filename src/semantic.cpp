#include "semantic.h"
#include <iostream>

IRGenerator::IRGenerator()
    : labelCounter(0), tempCounter(0), localCounter(0), inLoop(false), hasReturn(false),
      hasError(false) {}

std::string IRGenerator::newLabel() {
    return ".L" + std::to_string(labelCounter++);
}

std::string IRGenerator::newTemp() {
    return "%t" + std::to_string(tempCounter++);
}

void IRGenerator::error(const std::string& message) {
    hasError = true;
    std::cerr << "Semantic error: " << message << "\n";
}

IRList IRGenerator::generate(const std::unique_ptr<CompUnit>& unit) {
    ir.clear();
    labelCounter = 0;
    tempCounter = 0;
    localCounter = 0;
    inLoop = false;
    hasReturn = false;
    hasError = false;
    symTable = SymbolTable();
    visitCompUnit(unit);
    return ir;
}

ConstEvalResult IRGenerator::evalConstExpr(const std::unique_ptr<Expr>& expr) {
    if (!expr) return {};

    switch (expr->kind) {
        case ExprKind::INT_LITERAL:
            return {true, expr->intValue};
        case ExprKind::IDENTIFIER: {
            auto* sym = symTable.lookup(expr->name);
            if (!sym || sym->isFunction || !sym->valueKnown) return {};
            return {true, sym->value};
        }
        case ExprKind::FUNC_CALL:
            return {};
        case ExprKind::UNARY: {
            auto value = evalConstExpr(expr->operand);
            if (!value.known) return {};
            switch (expr->unaryOp) {
                case UnaryOp::PLUS:  return {true, value.value};
                case UnaryOp::MINUS: return {true, -value.value};
                case UnaryOp::NOT:   return {true, value.value == 0 ? 1 : 0};
            }
            return {};
        }
        case ExprKind::BINARY: {
            auto left = evalConstExpr(expr->lhs);
            auto right = evalConstExpr(expr->rhs);
            if (!left.known || !right.known) return {};

            switch (expr->binaryOp) {
                case BinaryOp::ADD: return {true, left.value + right.value};
                case BinaryOp::SUB: return {true, left.value - right.value};
                case BinaryOp::MUL: return {true, left.value * right.value};
                case BinaryOp::DIV:
                    if (right.value == 0) return {};
                    return {true, left.value / right.value};
                case BinaryOp::MOD:
                    if (right.value == 0) return {};
                    return {true, left.value % right.value};
                case BinaryOp::LT:  return {true, left.value <  right.value ? 1 : 0};
                case BinaryOp::GT:  return {true, left.value >  right.value ? 1 : 0};
                case BinaryOp::LE:  return {true, left.value <= right.value ? 1 : 0};
                case BinaryOp::GE:  return {true, left.value >= right.value ? 1 : 0};
                case BinaryOp::EQ:  return {true, left.value == right.value ? 1 : 0};
                case BinaryOp::NE:  return {true, left.value != right.value ? 1 : 0};
                case BinaryOp::AND: return {true, (left.value != 0 && right.value != 0) ? 1 : 0};
                case BinaryOp::OR:  return {true, (left.value != 0 || right.value != 0) ? 1 : 0};
            }
            return {};
        }
    }
    return {};
}

std::string IRGenerator::defineVariable(const std::string& name, bool isConst, bool isGlobal,
                                        int value, bool valueKnown) {
    std::string irName = isGlobal ? name : (name + "$" + std::to_string(localCounter++));
    if (!symTable.addVariable(name, irName, isConst, isGlobal, value, valueKnown)) {
        error("redefinition of '" + name + "'");
    }
    return irName;
}

std::string IRGenerator::makeBool(const std::string& value) {
    std::string result = newTemp();
    std::string zero = newTemp();
    ir.push_back(IRInstr::li(zero, 0));
    ir.push_back(IRInstr::bin(IROp::NE, result, value, zero));
    return result;
}

void IRGenerator::visitCompUnit(const std::unique_ptr<CompUnit>& unit) {
    for (const auto& decl : unit->decls) {
        bool isConst = decl->kind == StmtKind::CONST_DECL;
        auto constValue = evalConstExpr(decl->init);
        if (isConst && !constValue.known) {
            error("const '" + decl->varName + "' initializer is not a compile-time constant");
        }
        if (!constValue.known) {
            error("global variable '" + decl->varName + "' initializer must be constant in this IR backend");
        }
        int value = constValue.known ? constValue.value : 0;
        defineVariable(decl->varName, isConst, true, value, isConst && constValue.known);
        ir.push_back(IRInstr::decl(decl->varName, true, value));
    }

    for (const auto& func : unit->funcDefs) {
        std::vector<FuncType> paramTypes(func->params.size(), FuncType::INT);
        if (!symTable.addFunction(func->name, func->returnType, paramTypes)) {
            error("redefinition of function '" + func->name + "'");
        }
    }

    auto* mainSym = symTable.lookup("main");
    if (!mainSym || !mainSym->isFunction ||
        mainSym->funcType != FuncType::INT || !mainSym->paramTypes.empty()) {
        error("program must define 'int main()'");
    }

    for (const auto& func : unit->funcDefs) {
        visitFuncDef(func);
    }
}

void IRGenerator::visitFuncDef(const std::unique_ptr<FuncDef>& func) {
    currentFuncReturnType = func->returnType;
    hasReturn = false;

    ir.push_back(IRInstr::funcBegin(func->name));

    symTable.enterScope();
    for (const auto& p : func->params) {
        std::string paramName = defineVariable(p.name, false, false);
        ir.push_back(IRInstr::param(paramName));
    }

    bool allPathsReturn = visitStmt(func->body);
    symTable.exitScope();

    if (currentFuncReturnType == FuncType::INT && !allPathsReturn) {
        error("function '" + func->name + "' must return a value on all paths");
    }

    ir.push_back(IRInstr::funcEnd());
}

bool IRGenerator::visitStmt(const std::unique_ptr<Stmt>& stmt) {
    if (!stmt) return false;

    switch (stmt->kind) {
        case StmtKind::BLOCK: {
            symTable.enterScope();
            bool returned = false;
            for (const auto& s : stmt->stmts) {
                bool stmtReturns = visitStmt(s);
                returned = returned || stmtReturns;
            }
            symTable.exitScope();
            return returned;
        }
        case StmtKind::EMPTY:
            return false;
        case StmtKind::EXPR:
            visitExpr(stmt->expr, true);
            return false;
        case StmtKind::ASSIGN: {
            auto* sym = symTable.lookup(stmt->varName);
            if (!sym) {
                error("undefined variable '" + stmt->varName + "'");
            } else if (sym->isFunction) {
                error("cannot assign to function '" + stmt->varName + "'");
            } else if (sym->isConst) {
                error("cannot assign to const '" + stmt->varName + "'");
            }
            std::string val = visitExpr(stmt->expr);
            if (sym && !sym->isFunction) {
                ir.push_back(IRInstr::store(sym->irName, 0, val));
            }
            return false;
        }
        case StmtKind::DECL:
        case StmtKind::CONST_DECL: {
            bool isConst = stmt->kind == StmtKind::CONST_DECL;
            auto constValue = evalConstExpr(stmt->init);
            if (isConst && !constValue.known) {
                error("const '" + stmt->varName + "' initializer is not a compile-time constant");
            }
            bool isGlobal = symTable.isGlobalScope();
            std::string irName = defineVariable(stmt->varName, isConst, isGlobal,
                                                constValue.known ? constValue.value : 0,
                                                isConst && constValue.known);
            ir.push_back(IRInstr::decl(irName, isGlobal,
                                       constValue.known ? constValue.value : 0));
            std::string val = constValue.known ? newTemp() : visitExpr(stmt->init);
            if (constValue.known) {
                ir.push_back(IRInstr::li(val, constValue.value));
            }
            ir.push_back(IRInstr::store(irName, 0, val));
            return false;
        }
        case StmtKind::IF: {
            std::string cond = visitExpr(stmt->cond);
            std::string notCond = newTemp();
            std::string elseLabel = newLabel();
            std::string endLabel = newLabel();
            ir.push_back(IRInstr::una(IROp::NOT, notCond, cond));
            ir.push_back(IRInstr::ifGoto(notCond, elseLabel));
            bool thenReturns = visitStmt(stmt->thenStmt);
            ir.push_back(IRInstr::goto_(endLabel));
            ir.push_back(IRInstr::labelInstr(elseLabel));
            bool elseReturns = false;
            if (stmt->elseStmt) {
                elseReturns = visitStmt(stmt->elseStmt);
            }
            ir.push_back(IRInstr::labelInstr(endLabel));
            return stmt->elseStmt != nullptr && thenReturns && elseReturns;
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

            ir.push_back(IRInstr::labelInstr(beginLabel));
            std::string cond = visitExpr(stmt->cond);
            std::string notCond = newTemp();
            ir.push_back(IRInstr::una(IROp::NOT, notCond, cond));
            ir.push_back(IRInstr::ifGoto(notCond, endLabel));
            visitStmt(stmt->body);
            ir.push_back(IRInstr::goto_(beginLabel));
            ir.push_back(IRInstr::labelInstr(endLabel));

            inLoop = outerLoop;
            loopBeginLabel = outerBegin;
            loopEndLabel = outerEnd;
            return false;
        }
        case StmtKind::BREAK:
            if (!inLoop) {
                error("break outside loop");
            } else {
                ir.push_back(IRInstr::goto_(loopEndLabel));
            }
            return false;
        case StmtKind::CONTINUE:
            if (!inLoop) {
                error("continue outside loop");
            } else {
                ir.push_back(IRInstr::goto_(loopBeginLabel));
            }
            return false;
        case StmtKind::RETURN:
            hasReturn = true;
            if (currentFuncReturnType == FuncType::VOID && stmt->retExpr) {
                error("void function should not return a value");
            } else if (currentFuncReturnType == FuncType::INT && !stmt->retExpr) {
                error("int function must return a value");
            }
            if (stmt->retExpr) {
                std::string val = visitExpr(stmt->retExpr);
                ir.push_back(IRInstr::assign("a0", val));
            }
            ir.push_back(IRInstr::ret());
            return true;
    }
    return false;
}

std::string IRGenerator::visitExpr(const std::unique_ptr<Expr>& expr, bool allowVoid) {
    if (!expr) {
        std::string t = newTemp();
        ir.push_back(IRInstr::li(t, 0));
        return t;
    }

    switch (expr->kind) {
        case ExprKind::INT_LITERAL: {
            std::string t = newTemp();
            ir.push_back(IRInstr::li(t, expr->intValue));
            return t;
        }
        case ExprKind::IDENTIFIER: {
            auto* sym = symTable.lookup(expr->name);
            if (!sym) {
                error("undefined identifier '" + expr->name + "'");
                std::string t = newTemp();
                ir.push_back(IRInstr::li(t, 0));
                return t;
            }
            if (sym->isFunction) {
                error("function '" + expr->name + "' used as a value");
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
            ir.push_back(IRInstr::load(t, sym->irName, 0));
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
            if (expr->binaryOp == BinaryOp::AND) {
                std::string result = newTemp();
                std::string falseLabel = newLabel();
                std::string endLabel = newLabel();
                std::string lhs = visitExpr(expr->lhs);
                std::string lhsFalse = newTemp();
                ir.push_back(IRInstr::una(IROp::NOT, lhsFalse, lhs));
                ir.push_back(IRInstr::ifGoto(lhsFalse, falseLabel));
                std::string rhs = visitExpr(expr->rhs);
                std::string rhsBool = makeBool(rhs);
                ir.push_back(IRInstr::assign(result, rhsBool));
                ir.push_back(IRInstr::goto_(endLabel));
                ir.push_back(IRInstr::labelInstr(falseLabel));
                ir.push_back(IRInstr::li(result, 0));
                ir.push_back(IRInstr::labelInstr(endLabel));
                return result;
            }
            if (expr->binaryOp == BinaryOp::OR) {
                std::string result = newTemp();
                std::string trueLabel = newLabel();
                std::string endLabel = newLabel();
                std::string lhs = visitExpr(expr->lhs);
                ir.push_back(IRInstr::ifGoto(lhs, trueLabel));
                std::string rhs = visitExpr(expr->rhs);
                std::string rhsBool = makeBool(rhs);
                ir.push_back(IRInstr::assign(result, rhsBool));
                ir.push_back(IRInstr::goto_(endLabel));
                ir.push_back(IRInstr::labelInstr(trueLabel));
                ir.push_back(IRInstr::li(result, 1));
                ir.push_back(IRInstr::labelInstr(endLabel));
                return result;
            }

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
            ir.push_back(IRInstr::bin(opToIROp(expr->binaryOp), t, lhs, rhs));
            return t;
        }
        case ExprKind::FUNC_CALL: {
            auto* sym = symTable.lookup(expr->name);
            if (!sym || !sym->isFunction) {
                error("undefined function '" + expr->name + "'");
            } else {
                if (sym->paramTypes.size() != expr->args.size()) {
                    error("function '" + expr->name + "' called with wrong number of arguments");
                }
                if (sym->funcType == FuncType::VOID && !allowVoid) {
                    error("void function '" + expr->name + "' used as a value");
                }
            }
            for (size_t i = 0; i < expr->args.size(); i++) {
                std::string argVal = visitExpr(expr->args[i]);
                ir.push_back(IRInstr::arg(argVal, static_cast<int>(i)));
            }
            bool isVoidCall = sym && sym->isFunction && sym->funcType == FuncType::VOID;
            if (isVoidCall && allowVoid) {
                ir.push_back(IRInstr::call("", expr->name));
                return "";
            }
            std::string t = newTemp();
            ir.push_back(IRInstr::call(t, expr->name));
            return t;
        }
    }

    std::string t = newTemp();
    ir.push_back(IRInstr::li(t, 0));
    return t;
}
