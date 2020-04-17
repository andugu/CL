//////////////////////////////////////////////////////////////////////
//
//    TypeCheckVisitor - Walk the parser tree to do the semantic
//                       typecheck for the Asl programming language
//
//    Copyright (C) 2019  Universitat Politecnica de Catalunya
//
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU General Public License
//    as published by the Free Software Foundation; either version 3
//    of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Affero General Public License for more details.
//
//    You should have received a copy of the GNU Affero General Public
//    License along with this library; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
//
//    contact: José Miguel Rivero (rivero@cs.upc.edu)
//             Computer Science Department
//             Universitat Politecnica de Catalunya
//             despatx Omega.110 - Campus Nord UPC
//             08034 Barcelona.  SPAIN
//
//////////////////////////////////////////////////////////////////////


#include "TypeCheckVisitor.h"

#include "antlr4-runtime.h"

#include "../common/TypesMgr.h"
#include "../common/SymTable.h"
#include "../common/TreeDecoration.h"
#include "../common/SemErrors.h"

#include <iostream>
#include <string>

// uncomment the following line to enable debugging messages with DEBUG*
// #define DEBUG_BUILD
#include "../common/debug.h"

// using namespace std;


// Constructor
TypeCheckVisitor::TypeCheckVisitor(TypesMgr       & Types,
				   SymTable       & Symbols,
				   TreeDecoration & Decorations,
				   SemErrors      & Errors) :
  Types{Types},
  Symbols {Symbols},
  Decorations{Decorations},
  Errors{Errors} {
}

// Methods to visit each kind of node:
//
antlrcpp::Any TypeCheckVisitor::visitProgram(AslParser::ProgramContext *ctx) {
  DEBUG_ENTER();
  SymTable::ScopeId sc = getScopeDecor(ctx);
  Symbols.pushThisScope(sc);
  for (auto ctxFunc : ctx->function()) {
    visit(ctxFunc);
  }
  if (Symbols.noMainProperlyDeclared())
    Errors.noMainProperlyDeclared(ctx);
  Symbols.popScope();
  Errors.print();
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitFunction(AslParser::FunctionContext *ctx) {
  DEBUG_ENTER();
  SymTable::ScopeId sc = getScopeDecor(ctx);
  Symbols.pushThisScope(sc);
  //Symbols.print();

  TypesMgr::TypeId function_type = getTypeDecor(ctx);
  Symbols.setCurrentFunctionTy(function_type);

  visit(ctx->statements());

  Symbols.popScope();
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitStatements(AslParser::StatementsContext *ctx) {
  DEBUG_ENTER();
  visitChildren(ctx);
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitProcCall(AslParser::ProcCallContext *ctx) {
    DEBUG_ENTER();

    visit(ctx->ident());
    TypesMgr::TypeId t = getTypeDecor(ctx->ident());

    if (not Types.isFunctionTy(t) and not Types.isErrorTy(t)) {
        Errors.isNotCallable(ctx->ident());
    }

    if ((not Types.isErrorTy(t)) and Types.isFunctionTy(t)){

        // Check parameters types
        std::vector<TypesMgr::TypeId> t_param_orig = Types.getFuncParamsTypes(t);
        unsigned int num_params = Types.getNumOfParameters(t);

        if (num_params != ctx->expr().size())
            Errors.numberOfParameters(ctx->ident());

        for (unsigned int i = 0; i < ctx->expr().size(); ++i){

            visit(ctx->expr(i));
            TypesMgr::TypeId t_param_caller = getTypeDecor(ctx->expr(i));

            if (i < num_params){
                if ((not Types.isErrorTy(t_param_caller)) and (not Types.isErrorTy(t_param_orig[i]))
                    and (not Types.copyableTypes(t_param_orig[i], t_param_caller)))
                    Errors.incompatibleParameter(ctx->expr(i), i + 1, ctx);
            }
        }

        // Set return type
        putTypeDecor(ctx, t);
        putIsLValueDecor(ctx, false);
    }

    DEBUG_EXIT();
    return 0;
}

antlrcpp::Any TypeCheckVisitor::visitArrayAccess(AslParser::ArrayAccessContext *ctx){
    DEBUG_ENTER();

    visit(ctx->ident());
    TypesMgr::TypeId t_ident = getTypeDecor(ctx->ident());
    visit(ctx->expr());
    TypesMgr::TypeId t_index = getTypeDecor(ctx->expr());

    // ident isn't a array var
    if ((not Types.isErrorTy(t_ident)) and (not Types.isArrayTy(t_ident))) {
        Errors.nonArrayInArrayAccess(ctx);
        putTypeDecor(ctx, Types.createErrorTy());
        putIsLValueDecor(ctx, false);
    }

    // expr isn't a int value
    if ((not Types.isErrorTy(t_index)) and (not Types.isIntegerTy(t_index))){
        Errors.nonIntegerIndexInArrayAccess(ctx->expr());
        putTypeDecor(ctx, Types.createErrorTy());
        putIsLValueDecor(ctx, false);
    }

    // Success case
    if (Types.isArrayTy(t_ident)){
        putTypeDecor(ctx, Types.getArrayElemType(t_ident));
        putIsLValueDecor(ctx, true);
    }

    DEBUG_EXIT();
    return 0;
}

antlrcpp::Any TypeCheckVisitor::visitControlExpr(AslParser::ControlExprContext *ctx){
  DEBUG_ENTER();

  visit(ctx->expr());
  TypesMgr::TypeId t = getTypeDecor(ctx->expr());
  putTypeDecor(ctx, t);

  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitAssignStmt(AslParser::AssignStmtContext *ctx) {
  DEBUG_ENTER();

  visit(ctx->left_expr());
  visit(ctx->expr());
  TypesMgr::TypeId t1 = getTypeDecor(ctx->left_expr());
  TypesMgr::TypeId t2 = getTypeDecor(ctx->expr());

  if ((not Types.isErrorTy(t1)) and (not Types.isErrorTy(t2)) and (not Types.copyableTypes(t1, t2)))
    Errors.incompatibleAssignment(ctx->ASSIGN());

  if ((not Types.isErrorTy(t1)) and (not getIsLValueDecor(ctx->left_expr())))
    Errors.nonReferenceableLeftExpr(ctx->left_expr());

  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitIfStmt(AslParser::IfStmtContext *ctx) {
  DEBUG_ENTER();

  visit(ctx->expr());
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr());

  if ((not Types.isErrorTy(t1)) and (not Types.isBooleanTy(t1)))
    Errors.booleanRequired(ctx);

  // visit if and else
  for (auto i : ctx->statements())
      visit(i);

  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitWhileStmt(AslParser::WhileStmtContext *ctx){
    DEBUG_ENTER();

    visit(ctx->expr());
    TypesMgr::TypeId t = getTypeDecor(ctx->expr());

    if ((not Types.isErrorTy(t)) and (not Types.isBooleanTy(t)))
        Errors.booleanRequired(ctx);

    visit(ctx->statements());

    DEBUG_EXIT();
    return 0;
}

antlrcpp::Any TypeCheckVisitor::visitForStmt(AslParser::ForStmtContext *ctx){
  DEBUG_ENTER();

  visit(ctx->control());
  for (auto i : ctx->expr()){
    visit(i);
  }
  visit(ctx->statements());

  if (ctx->expr().size() < 1 or ctx->expr().size() > 3)
    Errors.numberOfRangeExpressions(ctx);

  TypesMgr::TypeId t_control = getTypeDecor(ctx->control());
  if (not Types.isErrorTy(t_control) and not Types.isIntegerTy(t_control))
    Errors.forRequireIntegerVar(ctx->control());

  for (auto i : ctx->expr()){
    TypesMgr::TypeId t_param = getTypeDecor(i);
    if (not Types.isErrorTy(t_param) and not Types.isIntegerTy(t_param))
      Errors.forRequireIntegerExpr(i);
  }

  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitReadStmt(AslParser::ReadStmtContext *ctx) {
  DEBUG_ENTER();

  visit(ctx->left_expr());
  TypesMgr::TypeId t1 = getTypeDecor(ctx->left_expr());

  if ((not Types.isErrorTy(t1)) and (not Types.isPrimitiveTy(t1)) and
      (not Types.isFunctionTy(t1)))
    Errors.readWriteRequireBasic(ctx);

  if ((not Types.isErrorTy(t1)) and (not getIsLValueDecor(ctx->left_expr())))
    Errors.nonReferenceableExpression(ctx);

  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitWriteExpr(AslParser::WriteExprContext *ctx) {
  DEBUG_ENTER();

  visit(ctx->expr());
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr());

  if ((not Types.isErrorTy(t1)) and (not Types.isPrimitiveTy(t1)))
    Errors.readWriteRequireBasic(ctx);

  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitReturnStmt(AslParser::ReturnStmtContext *ctx){
    DEBUG_ENTER();

    TypesMgr::TypeId t_func = Symbols.getCurrentFunctionTy();

    if (Types.isFunctionTy(t_func)){

        TypesMgr::TypeId t_ret  = Types.getFuncReturnType(t_func);

        // return ? and non-void
        if (ctx->expr() == nullptr and (not Types.isErrorTy(t_ret)) and (not Types.isVoidTy(t_ret))) {
            Errors.incompatibleReturn(ctx->RETURN());
        }

        if (ctx->expr() != nullptr) {
            visit(ctx->expr());
            TypesMgr::TypeId t_expr = getTypeDecor(ctx->expr());

            // return x and non compatible types or return x and void
            if ((not Types.isErrorTy(t_expr)) and (not Types.isErrorTy(t_ret))
                and (not Types.copyableTypes(t_ret, t_expr)))
                Errors.incompatibleReturn(ctx->RETURN());
        }
    }

    DEBUG_EXIT();
    return 0;
}

antlrcpp::Any TypeCheckVisitor::visitArrayAccessLeftValue(AslParser::ArrayAccessLeftValueContext *ctx){
    DEBUG_ENTER();

    visit(ctx->array_access());
    TypesMgr::TypeId t = getTypeDecor(ctx->array_access());

    putTypeDecor(ctx, t);
    putIsLValueDecor(ctx, true);

    DEBUG_EXIT();
    return 0;
}

antlrcpp::Any TypeCheckVisitor::visitIdentifier(AslParser::IdentifierContext *ctx){
    DEBUG_ENTER();

    visit(ctx->ident());
    TypesMgr::TypeId t1 = getTypeDecor(ctx->ident());

    putTypeDecor(ctx, t1);

    bool b = getIsLValueDecor(ctx->ident());
    putIsLValueDecor(ctx, b);

    DEBUG_EXIT();
    return 0;
}


/**************************
		VISIT EXPRESSIONS
***************************/


antlrcpp::Any TypeCheckVisitor::visitParenthesis(AslParser::ParenthesisContext *ctx) {
    DEBUG_ENTER();

    visit(ctx->expr());
    TypesMgr::TypeId t = getTypeDecor(ctx->expr());
    putTypeDecor(ctx, t);

    DEBUG_EXIT();
    return 0;
}

antlrcpp::Any TypeCheckVisitor::visitArrayAccessExpr(AslParser::ArrayAccessExprContext *ctx) {
    DEBUG_ENTER();

    visit(ctx->array_access());
    TypesMgr::TypeId t = getTypeDecor(ctx->array_access());
    putTypeDecor(ctx, t);

    DEBUG_EXIT();
    return 0;
}

antlrcpp::Any TypeCheckVisitor::visitFunctionExpr(AslParser::FunctionExprContext *ctx){
    DEBUG_ENTER();

    visit(ctx->function_call());
    TypesMgr::TypeId t = getTypeDecor(ctx->function_call());

    if ((not Types.isErrorTy(t)) and Types.isFunctionTy(t)){

        TypesMgr::TypeId t_ret = Types.getFuncReturnType(t);

        if (Types.isVoidTy(t_ret))
            Errors.isNotFunction(ctx->function_call());
        else {
            putTypeDecor(ctx, t_ret);
            putIsLValueDecor(ctx, false);
        }
    }

    DEBUG_EXIT();
    return 0;
}

antlrcpp::Any TypeCheckVisitor::visitUnary(AslParser::UnaryContext *ctx){
    DEBUG_ENTER();

    visit(ctx->expr());
    TypesMgr::TypeId t = getTypeDecor(ctx->expr());
    std::string operacio = ctx->op->getText();

    if (operacio == "not") {
        if ((not Types.isErrorTy(t)) and (not Types.isBooleanTy(t)))
            Errors.incompatibleOperator(ctx->op);

        t = Types.createBooleanTy();
    }

    else
        if ((not Types.isErrorTy(t)) and (not Types.isNumericTy(t)))
            Errors.incompatibleOperator(ctx->op);

    putTypeDecor(ctx, t);
    putIsLValueDecor(ctx, false);

    DEBUG_EXIT();
    return 0;
}

antlrcpp::Any TypeCheckVisitor::visitArithmetic(AslParser::ArithmeticContext *ctx) {
    DEBUG_ENTER();

    visit(ctx->expr(0));
    TypesMgr::TypeId t1 = getTypeDecor(ctx->expr(0));
    visit(ctx->expr(1));
    TypesMgr::TypeId t2 = getTypeDecor(ctx->expr(1));
    std::string op = ctx->op->getText();

    // Mod only works with ints
    if (op == "%") {
        if (((not Types.isErrorTy(t1)) and (not Types.isIntegerTy(t1))) or
            ((not Types.isErrorTy(t2)) and (not Types.isIntegerTy(t2))))
            Errors.incompatibleOperator(ctx->op);
    }
    else {
        if (((not Types.isErrorTy(t1)) and (not Types.isNumericTy(t1))) or
        ((not Types.isErrorTy(t2)) and (not Types.isNumericTy(t2))))
            Errors.incompatibleOperator(ctx->op);
    }

    TypesMgr::TypeId t;

    if (((not Types.isErrorTy(t1)) and (not Types.isErrorTy(t2))) and (Types.isFloatTy(t1) or Types.isFloatTy(t2))){
        t = Types.createFloatTy();
    }
    else {
        t = Types.createIntegerTy();
    }

    putTypeDecor(ctx, t);
    putIsLValueDecor(ctx, false);

    DEBUG_EXIT();
    return 0;
}

antlrcpp::Any TypeCheckVisitor::visitRelational(AslParser::RelationalContext *ctx) {
    DEBUG_ENTER();

    visit(ctx->expr(0));
    TypesMgr::TypeId t1 = getTypeDecor(ctx->expr(0));
    visit(ctx->expr(1));
    TypesMgr::TypeId t2 = getTypeDecor(ctx->expr(1));
    std::string oper = ctx->op->getText();

    if ((not Types.isErrorTy(t1)) and (not Types.isErrorTy(t2)) and
        (not Types.comparableTypes(t1, t2, oper)))
        Errors.incompatibleOperator(ctx->op);

    TypesMgr::TypeId t = Types.createBooleanTy();
    putTypeDecor(ctx, t);
    putIsLValueDecor(ctx, false);

    DEBUG_EXIT();
    return 0;
}

antlrcpp::Any TypeCheckVisitor::visitLogical(AslParser::LogicalContext *ctx){
    DEBUG_ENTER();

    visit(ctx->expr(0));
    TypesMgr::TypeId t1 = getTypeDecor(ctx->expr(0));
    visit(ctx->expr(1));
    TypesMgr::TypeId t2 = getTypeDecor(ctx->expr(1));

    if ((((not Types.isErrorTy(t1)) and (not Types.isBooleanTy(t1))) or
         (((not Types.isErrorTy(t2)) and (not Types.isBooleanTy(t2)))))) {
        Errors.incompatibleOperator(ctx->op);
    }

    TypesMgr::TypeId t = Types.createBooleanTy();
    putTypeDecor(ctx, t);
    putIsLValueDecor(ctx, false);

    DEBUG_EXIT();
    return 0;
}

antlrcpp::Any TypeCheckVisitor::visitMaximal(AslParser::MaximalContext *ctx) {
    DEBUG_ENTER();

    if (ctx->expr().size() < 2){
      for (auto i : ctx->expr())
        visit(i);
      Errors.numberOfMaxArguments(ctx);
    }

    else {
      TypesMgr::TypeId t = 0;
      visit(ctx->expr(0));
      TypesMgr::TypeId t_param_one = getTypeDecor(ctx->expr(0));
      bool exists_float = false;
      bool errors_exist = false;

      for (auto i : ctx->expr()){
          visit(i);
          TypesMgr::TypeId t_param = getTypeDecor(i);

          if (Types.isFloatTy(t_param))
            exists_float = true;

          if (not Types.isErrorTy(t_param_one) and not Types.isErrorTy(t_param)){
            if (Types.isNumericTy(t_param_one) and not Types.isNumericTy(t_param)){
              Errors.incompatibleMaxArguments(ctx);
              errors_exist = true;
            }
            if (Types.isCharacterTy(t_param_one) and not Types.isCharacterTy(t_param)){
              Errors.incompatibleMaxArguments(ctx);
              errors_exist = true;
            }
          }
      }

      if (not errors_exist){
        if (Types.isNumericTy(t_param_one)) {
          if (exists_float)
            t = Types.createFloatTy();
          else
            t = Types.createIntegerTy();
        }
        else if (Types.isCharacterTy(t_param_one))
          t = Types.createCharacterTy();
      }
      else
        t = Types.createErrorTy();

      putTypeDecor(ctx, t);
      putIsLValueDecor(ctx, false);

    }

    DEBUG_EXIT();
    return 0;
}

antlrcpp::Any TypeCheckVisitor::visitValue(AslParser::ValueContext *ctx) {
    DEBUG_ENTER();

    if (ctx->INTVAL() != nullptr){
        TypesMgr::TypeId t = Types.createIntegerTy();
        putTypeDecor(ctx, t);
        putIsLValueDecor(ctx, false);
    }

    else if (ctx->BOOLVAL() != nullptr){
        TypesMgr::TypeId t = Types.createBooleanTy();
        putTypeDecor(ctx, t);
        putIsLValueDecor(ctx, false);
    }

    else if (ctx->FLOATVAL() != nullptr){
        TypesMgr::TypeId t = Types.createFloatTy();
        putTypeDecor(ctx, t);
        putIsLValueDecor(ctx, false);
    }

    else if (ctx->CHARVAL() != nullptr){
        TypesMgr::TypeId t = Types.createCharacterTy();
        putTypeDecor(ctx, t);
        putIsLValueDecor(ctx, false);
    }

    else {
        TypesMgr::TypeId  t = Types.createErrorTy();
        putTypeDecor(ctx, t);
    }

    DEBUG_EXIT();
    return 0;
}

antlrcpp::Any TypeCheckVisitor::visitExprIdent(AslParser::ExprIdentContext *ctx) {
    DEBUG_ENTER();

    visit(ctx->ident());
    TypesMgr::TypeId t1 = getTypeDecor(ctx->ident());
    putTypeDecor(ctx, t1);

    bool b = getIsLValueDecor(ctx->ident());
    putIsLValueDecor(ctx, b);

    DEBUG_EXIT();
    return 0;
}

antlrcpp::Any TypeCheckVisitor::visitIdent(AslParser::IdentContext *ctx) {
  DEBUG_ENTER();

  std::string ident = ctx->getText();

  if (Symbols.findInStack(ident) == -1) {
    Errors.undeclaredIdent(ctx->ID());
    TypesMgr::TypeId te = Types.createErrorTy();
    putTypeDecor(ctx, te);
    putIsLValueDecor(ctx, true);
  }

  else {
    TypesMgr::TypeId t1 = Symbols.getType(ident);
    putTypeDecor(ctx, t1);

    if (Symbols.isFunctionClass(ident))
      putIsLValueDecor(ctx, false);

    else
      putIsLValueDecor(ctx, true);
  }

  DEBUG_EXIT();
  return 0;
}


/**************************
	END	VISIT EXPRESSIONS
***************************/


// Getters for the necessary tree node atributes:
//   Scope, Type ans IsLValue
SymTable::ScopeId TypeCheckVisitor::getScopeDecor(antlr4::ParserRuleContext *ctx) {
  return Decorations.getScope(ctx);
}
TypesMgr::TypeId TypeCheckVisitor::getTypeDecor(antlr4::ParserRuleContext *ctx) {
  return Decorations.getType(ctx);
}
bool TypeCheckVisitor::getIsLValueDecor(antlr4::ParserRuleContext *ctx) {
  return Decorations.getIsLValue(ctx);
}

// Setters for the necessary tree node attributes:
//   Scope, Type ans IsLValue
void TypeCheckVisitor::putScopeDecor(antlr4::ParserRuleContext *ctx, SymTable::ScopeId s) {
  Decorations.putScope(ctx, s);
}
void TypeCheckVisitor::putTypeDecor(antlr4::ParserRuleContext *ctx, TypesMgr::TypeId t) {
  Decorations.putType(ctx, t);
}
void TypeCheckVisitor::putIsLValueDecor(antlr4::ParserRuleContext *ctx, bool b) {
  Decorations.putIsLValue(ctx, b);
}
