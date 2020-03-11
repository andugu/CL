//////////////////////////////////////////////////////////////////////
//
//    SymbolsVisitor - Walk the parser tree to register symbols
//                     for the Asl programming language
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

#include "SymbolsVisitor.h"

#include "antlr4-runtime.h"

#include "../common/TypesMgr.h"
#include "../common/SymTable.h"
#include "../common/TreeDecoration.h"
#include "../common/SemErrors.h"

#include <iostream>
#include <string>
#include <vector>

#include <cstddef>    // std::size_t

// uncomment the following line to enable debugging messages with DEBUG*
// #define DEBUG_BUILD
#include "../common/debug.h"

// using namespace std;

// Constructor
SymbolsVisitor::SymbolsVisitor(TypesMgr       & Types,
			       SymTable       & Symbols,
			       TreeDecoration & Decorations,
			       SemErrors      & Errors) :
  Types{Types},
  Symbols{Symbols},
  Decorations{Decorations},
  Errors{Errors} {
}

// Methods to visit each kind of node:
//
antlrcpp::Any SymbolsVisitor::visitProgram(AslParser::ProgramContext *ctx) {
  DEBUG_ENTER();
  SymTable::ScopeId sc = Symbols.pushNewScope("$global$");
  putScopeDecor(ctx, sc);
  for (auto ctxFunc : ctx->function()) {
    visit(ctxFunc);
  }
  // Symbols.print();
  Symbols.popScope();
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any SymbolsVisitor::visitFunction(AslParser::FunctionContext *ctx) {
  DEBUG_ENTER();
  std::string funcName = ctx->ID()->getText();
  SymTable::ScopeId sc = Symbols.pushNewScope(funcName);
  putScopeDecor(ctx, sc);
  visit(ctx->declarations());
  // Symbols.print();
  Symbols.popScope();
  std::string ident = ctx->ID()->getText();
  if (Symbols.findInCurrentScope(ident)) {
    Errors.declaredIdent(ctx->ID());
  }
  else {
    // visit and save param types
    std::vector<TypesMgr::TypeId> lParamsTy;
    std::vector<std::pair<std::string, TypesMgr::TypeId> > v = visit(ctx->parameters());

    for (unsigned int i = 0; i < v.size(); ++i){
      lParamsTy.push_back(v[i].second);
      Symbols.addParameter(v[i].first, v[i].second);
    }

    // visit and save return type
    visit(ctx->ret());
    TypesMgr::TypeId tRet = getTypeDecor(ctx->ret());
    
    TypesMgr::TypeId tFunc = Types.createFunctionTy(lParamsTy, tRet);
    Symbols.addFunction(ident, tFunc);
  }
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any SymbolsVisitor::visitParameters(AslParser::ParametersContext *ctx){
  DEBUG_ENTER();

  std::vector<std::pair<std::string, TypesMgr::TypeId> > params;

  for (unsigned int i = 0; i < ctx->ID().size(); ++i){

    std::string name = ctx->ID(i)->getText();
    visit(ctx->type(i));

    TypesMgr::TypeId t = getTypeDecor(ctx->type(i));

    params.push_back(std::make_pair(name, t));
  }

  DEBUG_EXIT();
  return params;
}

antlrcpp::Any SymbolsVisitor::visitRet(AslParser::RetContext *ctx){
  DEBUG_ENTER();

  TypesMgr::TypeId t;

  if (ctx->type() != nullptr){
    visit(ctx->type());
    t = getTypeDecor(ctx->type());
  }
  else
    t = Types.createVoidTy();

  putTypeDecor(ctx, t);

  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any SymbolsVisitor::visitDeclarations(AslParser::DeclarationsContext *ctx) {
  DEBUG_ENTER();
  visitChildren(ctx);
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any SymbolsVisitor::visitVariable_decl(AslParser::Variable_declContext *ctx) {
  DEBUG_ENTER();
  visit(ctx->type());
  for(auto const& id : ctx->ID()){
    std::string ident = id->getText();
    if (Symbols.findInCurrentScope(ident)) {
      Errors.declaredIdent(id);
    }
    else {
      TypesMgr::TypeId t1 = getTypeDecor(ctx->type());
			Symbols.addLocalVar(ident, t1);
    }
  }
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any SymbolsVisitor::visitArrayType(AslParser::ArrayTypeContext *ctx) {
  DEBUG_ENTER();
	visit(ctx->type2());
	TypesMgr::TypeId t1 = getTypeDecor(ctx->type2());
  TypesMgr::TypeId t = Types.createArrayTy(std::stoi(ctx->INTVAL()->getText()), t1);
  putTypeDecor(ctx, t);
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any SymbolsVisitor::visitBasicType(AslParser::BasicTypeContext *ctx) {
  DEBUG_ENTER();
  visit(ctx->type2());
	auto t1 = getTypeDecor(ctx->type2());
	putTypeDecor(ctx, t1);
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any SymbolsVisitor::visitType2(AslParser::Type2Context *ctx) {
  DEBUG_ENTER();
  if (ctx->INT()) {
    TypesMgr::TypeId t = Types.createIntegerTy();
    putTypeDecor(ctx, t);
  }
  else if(ctx->FLOAT()){
    TypesMgr::TypeId t = Types.createFloatTy();
    putTypeDecor(ctx, t);
  }
  else if(ctx->BOOL()){
    TypesMgr::TypeId t = Types.createBooleanTy();
    putTypeDecor(ctx, t);
  }
  else if(ctx->CHAR()){
    TypesMgr::TypeId t = Types.createCharacterTy();
    putTypeDecor(ctx, t);
  }
  DEBUG_EXIT();
  return 0;
}

// Getters for the necessary tree node atributes:
//   Scope and Type
SymTable::ScopeId SymbolsVisitor::getScopeDecor(antlr4::ParserRuleContext *ctx) {
  return Decorations.getScope(ctx);
}

TypesMgr::TypeId SymbolsVisitor::getTypeDecor(antlr4::ParserRuleContext *ctx) {
  return Decorations.getType(ctx);
}

// Setters for the necessary tree node attributes:
//   Scope and Type
void SymbolsVisitor::putScopeDecor(antlr4::ParserRuleContext *ctx, SymTable::ScopeId s) {
  Decorations.putScope(ctx, s);
}

void SymbolsVisitor::putTypeDecor(antlr4::ParserRuleContext *ctx, TypesMgr::TypeId t) {
  Decorations.putType(ctx, t);
}
