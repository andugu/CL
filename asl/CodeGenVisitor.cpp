//////////////////////////////////////////////////////////////////////
//
//    CodeGenVisitor - Walk the parser tree to do
//                     the generation of code
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

#include "CodeGenVisitor.h"

#include "antlr4-runtime.h"

#include "../common/TypesMgr.h"
#include "../common/SymTable.h"
#include "../common/TreeDecoration.h"
#include "../common/code.h"

#include <string>
#include <cstddef>    // std::size_t

// uncomment the following line to enable debugging messages with DEBUG*
#define DEBUG_BUILD
#include "../common/debug.h"

// using namespace std;


// Constructor
CodeGenVisitor::CodeGenVisitor(TypesMgr       & Types,
                               SymTable       & Symbols,
                               TreeDecoration & Decorations) :
  Types{Types},
  Symbols{Symbols},
  Decorations{Decorations} {
}

// Methods to visit each kind of node:
//
antlrcpp::Any CodeGenVisitor::visitProgram(AslParser::ProgramContext *ctx) {
  DEBUG_ENTER();

  code my_code;
  SymTable::ScopeId sc = getScopeDecor(ctx);
  Symbols.pushThisScope(sc);

  for (auto ctxFunc : ctx->function()) {
    subroutine subr = visit(ctxFunc);
    my_code.add_subroutine(subr);
  }

  Symbols.popScope();

  DEBUG_EXIT();
  return my_code;
}

antlrcpp::Any CodeGenVisitor::visitFunction(AslParser::FunctionContext *ctx) {
  DEBUG_ENTER();

  SymTable::ScopeId sc = getScopeDecor(ctx);
  Symbols.pushThisScope(sc);
  subroutine subr(ctx->ID()->getText());
  codeCounters.reset();
  std::vector<var> && lvars = visit(ctx->declarations());

  for (auto & onevar : lvars) {
    subr.add_var(onevar);
  }

  instructionList && code = visit(ctx->statements());

  if (Types.isVoidFunction(Symbols.getCurrentFunctionTy()))
    code = code || instruction(instruction::RETURN());

  subr.set_instructions(code);
  Symbols.popScope();
  DEBUG_EXIT();
  return subr;
}

// parameters
// ret

antlrcpp::Any CodeGenVisitor::visitDeclarations(AslParser::DeclarationsContext *ctx) {
  DEBUG_ENTER();
  std::vector<var> lvars;
  for (auto & varDeclCtx : ctx->variable_decl()) {
    var onevar = visit(varDeclCtx);
    lvars.push_back(onevar);
  }
  DEBUG_EXIT();
  return lvars;
}

antlrcpp::Any CodeGenVisitor::visitVariable_decl(AslParser::Variable_declContext *ctx) {
  DEBUG_ENTER();
  TypesMgr::TypeId   t1 = getTypeDecor(ctx->type());
  std::size_t      size = Types.getSizeOfType(t1);
  DEBUG_EXIT();
  return var{ctx->ID(0)->getText(), size}; // TODO => for
}

// type2
// BasicType
// ArrayType

antlrcpp::Any CodeGenVisitor::visitStatements(AslParser::StatementsContext *ctx) {
  DEBUG_ENTER();
  instructionList code;
  for (auto stCtx : ctx->statement()) {
    instructionList && codeS = visit(stCtx);
    code = code || codeS;
  }
  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitProcCall(AslParser::ProcCallContext *ctx) {
  DEBUG_ENTER();

  instructionList code;
  // std::string name = ctx->ident()->ID()->getSymbol()->getText();
  std::string name = ctx->ident()->getText();
  code = instruction::CALL(name);

  DEBUG_EXIT();
  return code;
}

// arrayAccess
// controlExpr

antlrcpp::Any CodeGenVisitor::visitAssignStmt(AslParser::AssignStmtContext *ctx) {
  DEBUG_ENTER();
  instructionList code;
  CodeAttribs     && codAtsE1 = visit(ctx->left_expr());
  std::string           addr1 = codAtsE1.addr;
  // std::string           offs1 = codAtsE1.offs;
  instructionList &     code1 = codAtsE1.code;
  // TypesMgr::TypeId tid1 = getTypeDecor(ctx->left_expr());
  CodeAttribs     && codAtsE2 = visit(ctx->expr());
  std::string           addr2 = codAtsE2.addr;
  // std::string           offs2 = codAtsE2.offs;
  instructionList &     code2 = codAtsE2.code;
  // TypesMgr::TypeId tid2 = getTypeDecor(ctx->expr());
  code = code1 || code2 || instruction::LOAD(addr1, addr2);
  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitIfStmt(AslParser::IfStmtContext *ctx) {
  DEBUG_ENTER();

  instructionList code;
  CodeAttribs     && codAtsE = visit(ctx->expr());
  std::string          addr1 = codAtsE.addr;
  instructionList &    code1 = codAtsE.code;
  instructionList &&   code2 = visit(ctx->statements(0));

  std::string label = codeCounters.newLabelIF();
  std::string labelEndIf = "endif"+label;

  code = code1 || instruction::FJUMP(addr1, labelEndIf) ||
         code2 || instruction::LABEL(labelEndIf);

  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitProcStmt(AslParser::ProcStmtContext *ctx) {
  DEBUG_ENTER();

  CodeAttribs && codAts = visit(ctx->function_call());

  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitWhileStmt(AslParser::WhileStmtContext *ctx) {
  DEBUG_ENTER();

  instructionList code;

  CodeAttribs && codAts1 = visit(ctx->expr());
  std::string      addr1 = codAts1.addr;
  instructionList  code1 = codAts1.code;

  CodeAttribs && codAts2 = visit(ctx->statements());
  std::string      addr2 = codAts2.addr;
  instructionList  code2 = codAts2.code;

  std::string      label = codeCounters.newLabelWHILE();
  std::string labelStart = "WhileStmt"+label;
  std::string   labelEnd = "endWhileStmt"+label;

  code = instruction::LABEL(labelStart) || code1 || instruction::FJUMP(addr1, labelEnd) ||
         code2 || instruction::UJUMP(labelStart) || instruction::LABEL(labelEnd);

  DEBUG_EXIT();
  return code;
} 

antlrcpp::Any CodeGenVisitor::visitReadStmt(AslParser::ReadStmtContext *ctx) {
  DEBUG_ENTER();

  CodeAttribs     && codAts1 = visit(ctx->left_expr());
  std::string          addr1 = codAts1.addr;
  std::string          offs1 = codAts1.offs;
  instructionList       code = codAts1.code;
  TypesMgr::TypeId      type = getTypeDecor(ctx->left_expr());

  if (Types.isIntegerTy(type) or Types.isBooleanTy(type)){
    // Identifier
    if (offs1 == "")
      code = code || instruction::READI(addr1);

    // Array
    else {
      std::string temp = "%"+codeCounters.newTEMP();
      code = code || instruction::READI(temp) || instruction::CLOAD(offs1, temp);
    }
  }

  else if (Types.isFloatTy(type)){
    // Identifier
    if (offs1 == "")
      code = code || instruction::READF(addr1);

    // Array
    else {
      std::string temp = "%"+codeCounters.newTEMP();
      code = code || instruction::READF(temp) || instruction::CLOAD(offs1, temp);
    }
  }

  else if (Types.isCharacterTy(type)){
    // Identifier
    if (offs1 == "")
      code = code || instruction::READC(addr1);

    // Array
    else {
      std::string temp = "%"+codeCounters.newTEMP();
      code = code || instruction::READC(temp) || instruction::CLOAD(offs1, temp);
    }
  }

  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitWriteExpr(AslParser::WriteExprContext *ctx) {
  DEBUG_ENTER();

  CodeAttribs  && codAt = visit(ctx->expr());
  std::string      addr = codAt.addr;
  instructionList  code = codAt.code;
  TypesMgr::TypeId type = getTypeDecor(ctx->expr());

  if (Types.isIntegerTy(type) or Types.isBooleanTy(type))
    code = code || instruction::WRITEI(addr);

  else if (Types.isFloatTy(type))
    code = code || instruction::WRITEF(addr);

  else if (Types.isCharacterTy(type))
    code = code || instruction::WRITEC(addr);

  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitWriteString(AslParser::WriteStringContext *ctx) {
  DEBUG_ENTER();

  instructionList code;
  std::string s = ctx->STRING()->getText();
  std::string temp = "%"+codeCounters.newTEMP();

  int i = 1;
  while (i < int(s.size())-1) {
    if (s[i] != '\\') {
      code = code ||
	      instruction::CHLOAD(temp, s.substr(i,1)) ||
	      instruction::WRITEC(temp);
      i += 1;
    }
    else {
      assert(i < int(s.size())-2);
      if (s[i+1] == 'n') {
        code = code || instruction::WRITELN();
        i += 2;
      }
      else if (s[i+1] == 't' or s[i+1] == '"' or s[i+1] == '\\') {
        code = code ||
          instruction::CHLOAD(temp, s.substr(i,2)) ||
	        instruction::WRITEC(temp);
        i += 2;
      }
      else {
        code = code ||
          instruction::CHLOAD(temp, s.substr(i,1)) ||
	        instruction::WRITEC(temp);
        i += 1;
      }
    }
  }

  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitReturnStmt(AslParser::ReturnStmtContext *ctx) {
  DEBUG_ENTER();

  instructionList code;

  // Non-Void Function
  if (ctx->expr() != nullptr){
    CodeAttribs codAts = visit(ctx->expr());
    std::string addr = codAts.addr;
    code = codAts.code;
    code = code || instruction::LOAD("_result", addr) || instruction::RETURN();
  }

  // Void Function
  else
    code = instruction::RETURN();

  DEBUG_EXIT();
  return code;
}

antlrcpp::Any CodeGenVisitor::visitArrayAccessLeftValue(AslParser::ArrayAccessLeftValueContext *ctx) {
  DEBUG_ENTER();

  CodeAttribs && codAts = visit(ctx->array_access());

  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitIdentifier(AslParser::IdentifierContext *ctx) {
  DEBUG_ENTER();

  CodeAttribs && codAts = visit(ctx->ident());

  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitParenthesis(AslParser::ParenthesisContext *ctx) {
  DEBUG_ENTER();

  CodeAttribs && codAts = visit(ctx->expr());

  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitArrayAccessExpr(AslParser::ArrayAccessExprContext *ctx) {
  DEBUG_ENTER();

  CodeAttribs && codAts = visit(ctx->array_access());

  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitFunctionExpr(AslParser::FunctionExprContext *ctx) {
  DEBUG_ENTER();

  CodeAttribs && codAts = visit(ctx->function_call());

  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitUnary(AslParser::UnaryContext *ctx) {
  DEBUG_ENTER();

  CodeAttribs    &&  codAt = visit(ctx->expr());
  std::string         addr = codAt.addr;
  instructionList &   code = codAt.code;
  TypesMgr::TypeId    type = getTypeDecor(ctx->expr());

  std::string temp;

  if (ctx->op->getText() == "NOT") {
    temp = "%"+codeCounters.newTEMP();
    code = code || instruction::NOT(temp, addr);
  }

  else if (ctx->op->getText() == "PLUS")
    temp = addr;

  else if (ctx->op->getText() == "SUB") {
    temp = "%"+codeCounters.newTEMP();
    if (Types.isFloatTy(type))
      code = code || instruction::FNEG(temp, addr);
    else
      code = code || instruction::NEG(temp, addr);
  }

  CodeAttribs codAts(temp, "", code);

  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitArithmetic(AslParser::ArithmeticContext *ctx) {
  DEBUG_ENTER();

  CodeAttribs     && codAt1 = visit(ctx->expr(0));
  std::string         addr1 = codAt1.addr;
  instructionList &   code1 = codAt1.code;
  TypesMgr::TypeId    type1 = getTypeDecor(ctx->expr(0));

  CodeAttribs     && codAt2 = visit(ctx->expr(1));
  std::string         addr2 = codAt2.addr;
  instructionList &   code2 = codAt2.code;
  TypesMgr::TypeId    type2 = getTypeDecor(ctx->expr(1));

  instructionList &&   code = code1 || code2;

  std::string temp = "%"+codeCounters.newTEMP();

  // TODO: can we delete temp1 and temp2?
  if (Types.isIntegerTy(type1) and Types.isFloatTy(type2)){
    std::string temp1 = "%"+codeCounters.newTEMP();
    code = code || instruction::FLOAT(temp1, addr1);
    addr1 = temp1;
  }

  else if (Types.isFloatTy(type1) and Types.isIntegerTy(type2)){
    std::string temp2 = "%"+codeCounters.newTEMP();
    code = code || instruction::FLOAT(temp2, addr2);
    addr2 = temp2;
  }

  if (Types.isFloatTy(type1) or Types.isFloatTy(type2)){
    if (ctx->op->getText() == "MUL")
      code = code || instruction::FMUL(temp, addr1, addr2);

    else if (ctx->op->getText() == "DIV")
      code = code || instruction::FDIV(temp, addr1, addr2);

    else if (ctx->op->getText() == "PLUS")
      code = code || instruction::FADD(temp, addr1, addr2);

    else if (ctx->op->getText() == "SUB")
      code = code || instruction::FSUB(temp, addr1, addr2);
  }

  else {
    if (ctx->op->getText() == "MUL")
      code = code || instruction::MUL(temp, addr1, addr2);

    else if (ctx->op->getText() == "DIV")
      code = code || instruction::DIV(temp, addr1, addr2);

    else if (ctx->op->getText() == "MOD"){
      std::string temp1 = "%"+codeCounters.newTEMP();
      std::string temp2 = "%"+codeCounters.newTEMP();
      code = code || instruction::DIV(temp1, addr1, addr2);
      code = code || instruction::MUL(temp2, temp1, addr2);
      code = code || instruction::SUB(temp, addr1, temp2);
    }

    else if (ctx->op->getText() == "PLUS")
      code = code || instruction::ADD(temp, addr1, addr2);

    else if (ctx->op->getText() == "SUB")
      code = code || instruction::SUB(temp, addr1, addr2);
  }

  CodeAttribs codAts(temp, "", code);

  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitRelational(AslParser::RelationalContext *ctx) {
  DEBUG_ENTER();

  CodeAttribs     && codAt1 = visit(ctx->expr(0));
  std::string         addr1 = codAt1.addr;
  instructionList &   code1 = codAt1.code;
  TypesMgr::TypeId    type1 = getTypeDecor(ctx->expr(0));

  CodeAttribs     && codAt2 = visit(ctx->expr(1));
  std::string         addr2 = codAt2.addr;
  instructionList &   code2 = codAt2.code;
  TypesMgr::TypeId    type2 = getTypeDecor(ctx->expr(1));

  instructionList &&   code = code1 || code2;

  std::string temp = "%"+codeCounters.newTEMP();

  // TODO: can we delete temp1 and temp2?
  if (not Types.isFloatTy(type1) and Types.isFloatTy(type2)){
    std::string temp1 = "%"+codeCounters.newTEMP();
    code = code || instruction::FLOAT(temp1, addr1);
    addr1 = temp1;
  }

  else if (Types.isFloatTy(type1) and not Types.isFloatTy(type2)){
    std::string temp2 = "%"+codeCounters.newTEMP();
    code = code || instruction::FLOAT(temp2, addr2);
    addr2 = temp2;
  }

  if (Types.isFloatTy(type1) or Types.isFloatTy(type2)){
    if (ctx->op->getText() == "EQUAL")
      code = code || instruction::FEQ(temp, addr1, addr2);

    else if (ctx->op->getText() == "NOTEQUAL")
      code = code || instruction::FEQ(temp, addr1, addr2) || instruction::NOT(temp, temp);

    else if (ctx->op->getText() == "LESS")
      code = code || instruction::FLT(temp, addr1, addr2);

    else if (ctx->op->getText() == "LESSEQUAL")
      code = code || instruction::FLE(temp, addr1, addr2);

    else if (ctx->op->getText() == "GREATER")
      code = code || instruction::FLE(temp, addr1, addr2) || instruction::NOT(temp, temp);

    else if (ctx->op->getText() == "GREATEREQUAL")
      code = code || instruction::FLT(temp, addr1, addr2) || instruction::NOT(temp, temp);
  }

  else if (Types.isBooleanTy(type1) and Types.isBooleanTy(type2)){
    if (ctx->op->getText() == "EQUAL")
      code = code || instruction::EQ(temp, addr1, addr2);

    else if (ctx->op->getText() == "NOTEQUAL")
      code = code || instruction::EQ(temp, addr1, addr2) || instruction::NOT(temp, temp);
  }

  else {
    if (ctx->op->getText() == "EQUAL")
      code = code || instruction::EQ(temp, addr1, addr2);

    else if (ctx->op->getText() == "NOTEQUAL")
      code = code || instruction::EQ(temp, addr1, addr2) || instruction::NOT(temp, temp);

    else if (ctx->op->getText() == "LESS")
      code = code || instruction::LT(temp, addr1, addr2);

    else if (ctx->op->getText() == "LESSEQUAL")
      code = code || instruction::LE(temp, addr1, addr2);

    else if (ctx->op->getText() == "GREATER")
      code = code || instruction::LE(temp, addr1, addr2) || instruction::NOT(temp, temp);

    else if (ctx->op->getText() == "GREATEREQUAL")
      code = code || instruction::LT(temp, addr1, addr2) || instruction::NOT(temp, temp);
  }

  CodeAttribs codAts(temp, "", code);

  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitLogical(AslParser::LogicalContext *ctx) {
  DEBUG_ENTER();

  CodeAttribs     && codAt1 = visit(ctx->expr(0));
  std::string         addr1 = codAt1.addr;
  instructionList &   code1 = codAt1.code;

  CodeAttribs     && codAt2 = visit(ctx->expr(1));
  std::string         addr2 = codAt2.addr;
  instructionList &   code2 = codAt2.code;

  instructionList &&   code = code1 || code2;

  std::string temp = "%"+codeCounters.newTEMP();

  if (ctx->op->getText() == "AND")
    code = code || instruction::AND(temp, addr1, addr2);

  else if (ctx->op->getText() == "OR")
    code = code || instruction::OR(temp, addr1, addr2);

  CodeAttribs codAts(temp, "", code);

  DEBUG_EXIT();
  return codAts;
}

// maximal

antlrcpp::Any CodeGenVisitor::visitValue(AslParser::ValueContext *ctx) {
  DEBUG_ENTER();

  instructionList code;
  std::string temp = "%"+codeCounters.newTEMP();

  if (ctx->INTVAL() != nullptr)
    code = instruction::ILOAD(temp, ctx->getText());

  else if (ctx->BOOLVAL() != nullptr){

    std::string value = ctx->getText();

    if (value == "true" or value == "1")
      code = instruction::ILOAD(temp, "1");

    else
      code = instruction::ILOAD(temp, "0");
  }

  else if (ctx->FLOATVAL() != nullptr)
    code = instruction::FLOAD(temp, ctx->getText());

  else if (ctx->CHARVAL() != nullptr)
    code = instruction::CHLOAD(temp, ctx->getText().substr(1, ctx->getText().length()-2));

  CodeAttribs codAts(temp, "", code);

  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitExprIdent(AslParser::ExprIdentContext *ctx) {
  DEBUG_ENTER();

  CodeAttribs && codAts = visit(ctx->ident());

  DEBUG_EXIT();
  return codAts;
}

antlrcpp::Any CodeGenVisitor::visitIdent(AslParser::IdentContext *ctx) {
  DEBUG_ENTER();

  CodeAttribs codAts(ctx->ID()->getText(), "", instructionList());

  DEBUG_EXIT();
  return codAts;
}


// Getters for the necessary tree node atributes:
//   Scope and Type
SymTable::ScopeId CodeGenVisitor::getScopeDecor(antlr4::ParserRuleContext *ctx) const {
  return Decorations.getScope(ctx);
}
TypesMgr::TypeId CodeGenVisitor::getTypeDecor(antlr4::ParserRuleContext *ctx) const {
  return Decorations.getType(ctx);
}


// Constructors of the class CodeAttribs:
//
CodeGenVisitor::CodeAttribs::CodeAttribs(const std::string & addr,
					 const std::string & offs,
					 instructionList & code) :
  addr{addr}, offs{offs}, code{code} {
}

CodeGenVisitor::CodeAttribs::CodeAttribs(const std::string & addr,
					 const std::string & offs,
					 instructionList && code) :
  addr{addr}, offs{offs}, code{code} {
}
