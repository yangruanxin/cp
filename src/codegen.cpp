#include "codegen.h"
#include <sstream>

CodeGenerator::CodeGenerator() : labelCounter(0), stackOffset(0) {}

void CodeGenerator::emit(const std::string& code) {
    output += code + "\n";
}

std::string CodeGenerator::newLabel() {
    return ".L" + std::to_string(labelCounter++);
}

std::string CodeGenerator::generate(const std::unique_ptr<CompUnit>& unit) {
    emit(".text");
    genCompUnit(unit);
    return output;
}

void CodeGenerator::genCompUnit(const std::unique_ptr<CompUnit>& unit) {
    for (const auto& func : unit->funcDefs) {
        genFuncDef(func);
    }
}

void CodeGenerator::genFuncDef(const std::unique_ptr<FuncDef>& func) {
    emit(".globl " + func->name);
    emit(func->name + ":");
    genPrologue(func->name, 32); // TODO: calculate actual stack size
    genStmt(func->body);
    genEpilogue(func->name, 32);
}

void CodeGenerator::genPrologue(const std::string& name, int stackSize) {
    emit("    addi sp, sp, -" + std::to_string(stackSize));
    emit("    sw ra, " + std::to_string(stackSize - 4) + "(sp)");
    emit("    sw s0, " + std::to_string(stackSize - 8) + "(sp)");
    emit("    addi s0, sp, " + std::to_string(stackSize));
}

void CodeGenerator::genEpilogue(const std::string& name, int stackSize) {
    emit("    lw ra, " + std::to_string(stackSize - 4) + "(sp)");
    emit("    lw s0, " + std::to_string(stackSize - 8) + "(sp)");
    emit("    addi sp, sp, " + std::to_string(stackSize));
    emit("    jalr zero, ra, 0");
}

void CodeGenerator::genStmt(const std::unique_ptr<Stmt>& stmt) {
    switch (stmt->kind) {
        case StmtKind::BLOCK:
            for (const auto& s : stmt->stmts) {
                genStmt(s);
            }
            break;
        case StmtKind::EMPTY:
            break;
        case StmtKind::EXPR:
            genExpr(stmt->expr);
            break;
        case StmtKind::ASSIGN: {
            std::string val = genExpr(stmt->expr);
            // TODO: store to variable
            break;
        }
        case StmtKind::DECL:
        case StmtKind::CONST_DECL: {
            std::string val = genExpr(stmt->init);
            // TODO: allocate and store variable
            break;
        }
        case StmtKind::IF: {
            std::string elseLabel = newLabel();
            std::string endLabel = newLabel();
            std::string cond = genExpr(stmt->cond);
            emit("    beqz " + cond + ", " + elseLabel);
            genStmt(stmt->thenStmt);
            emit("    j " + endLabel);
            emit(elseLabel + ":");
            if (stmt->elseStmt) genStmt(stmt->elseStmt);
            emit(endLabel + ":");
            break;
        }
        case StmtKind::WHILE: {
            std::string loopLabel = newLabel();
            std::string endLabel = newLabel();
            emit(loopLabel + ":");
            std::string cond = genExpr(stmt->cond);
            emit("    beqz " + cond + ", " + endLabel);
            genStmt(stmt->body);
            emit("    j " + loopLabel);
            emit(endLabel + ":");
            break;
        }
        case StmtKind::BREAK:
            // TODO: needs loop end label tracking
            break;
        case StmtKind::CONTINUE:
            // TODO: needs loop start label tracking
            break;
        case StmtKind::RETURN: {
            if (stmt->retExpr) {
                std::string val = genExpr(stmt->retExpr);
                emit("    mv a0, " + val);
            }
            // Epilogue handled by caller
            break;
        }
    }
}

std::string CodeGenerator::genExpr(const std::unique_ptr<Expr>& expr) {
    switch (expr->kind) {
        case ExprKind::INT_LITERAL: {
            std::string reg = "t0"; // TODO: proper register allocation
            emit("    li " + reg + ", " + std::to_string(expr->intValue));
            return reg;
        }
        case ExprKind::IDENTIFIER: {
            // TODO: load from stack/global
            return "t0";
        }
        case ExprKind::UNARY: {
            std::string op = genExpr(expr->operand);
            switch (expr->unaryOp) {
                case UnaryOp::PLUS: return op;
                case UnaryOp::MINUS:
                    emit("    neg t0, " + op);
                    return "t0";
                case UnaryOp::NOT:
                    emit("    seqz t0, " + op);
                    return "t0";
            }
            return "t0";
        }
        case ExprKind::BINARY: {
            std::string lhs = genExpr(expr->lhs);
            std::string rhs = genExpr(expr->rhs);
            switch (expr->binaryOp) {
                case BinaryOp::ADD: emit("    add t0, " + lhs + ", " + rhs); break;
                case BinaryOp::SUB: emit("    sub t0, " + lhs + ", " + rhs); break;
                case BinaryOp::MUL: emit("    mul t0, " + lhs + ", " + rhs); break;
                case BinaryOp::DIV: emit("    div t0, " + lhs + ", " + rhs); break;
                case BinaryOp::MOD: emit("    rem t0, " + lhs + ", " + rhs); break;
                case BinaryOp::LT:  emit("    slt t0, " + lhs + ", " + rhs); break;
                case BinaryOp::GT:  emit("    sgt t0, " + lhs + ", " + rhs); break;
                case BinaryOp::LE:  emit("    sle t0, " + lhs + ", " + rhs); break;
                case BinaryOp::GE:  emit("    sge t0, " + lhs + ", " + rhs); break;
                case BinaryOp::EQ:  emit("    sub t0, " + lhs + ", " + rhs); emit("    seqz t0, t0"); break;
                case BinaryOp::NE:  emit("    sub t0, " + lhs + ", " + rhs); emit("    snez t0, t0"); break;
                case BinaryOp::AND:
                case BinaryOp::OR:
                    // TODO: short-circuit evaluation
                    break;
            }
            return "t0";
        }
        case ExprKind::FUNC_CALL: {
            for (size_t i = 0; i < expr->args.size(); i++) {
                std::string argReg = genExpr(expr->args[i]);
                if (i < 8) {
                    std::string aReg = "a" + std::to_string(i);
                    emit("    mv " + aReg + ", " + argReg);
                }
            }
            emit("    jal ra, " + expr->name);
            return "a0";
        }
    }
    return "t0";
}
