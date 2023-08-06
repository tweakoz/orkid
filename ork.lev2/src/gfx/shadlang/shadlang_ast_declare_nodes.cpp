////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
//  Scanner/Parser
//  this replaces CgFx for OpenGL 3.x and OpenGL ES 2.x
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/shadlang.h>
#include <ork/file/file.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/pch.h>
#include <ork/util/crc.h>
#include <regex>
#include <stdlib.h>
#include <peglib.h>
#include <ork/util/logger.h>
#include <ork/kernel/string/string.h>
#include <ork/util/parser.inl>
#include "shadlang_impl.h"

#if defined(USE_ORKSL_LANG)

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::shadlang::impl {
/////////////////////////////////////////////////////////////////////////////////////////////////

void ShadLangParser::preDeclareAstNodes() {
  ///////////////////////////////////////////////////////////
  // forced matcher declarations
  ///////////////////////////////////////////////////////////
  declare("SemaFunctionInvokation");
  declare("SemaConstructorInvokation");
  declare("SemaMemberAccess");
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void ShadLangParser::declareAstNodes() {

  DECLARE_OBJNAME_AST_NODE("fn_name");
  DECLARE_OBJNAME_AST_NODE("fn2_name");
  DECLARE_OBJNAME_AST_NODE("vtx_name");
  DECLARE_OBJNAME_AST_NODE("geo_name");
  DECLARE_OBJNAME_AST_NODE("frg_name");
  DECLARE_OBJNAME_AST_NODE("com_name");
  DECLARE_OBJNAME_AST_NODE("uniset_name");
  DECLARE_OBJNAME_AST_NODE("uniblk_name");
  DECLARE_OBJNAME_AST_NODE("vif_name");
  DECLARE_OBJNAME_AST_NODE("gif_name");
  DECLARE_OBJNAME_AST_NODE("fif_name");
  DECLARE_OBJNAME_AST_NODE("cif_name");
  DECLARE_OBJNAME_AST_NODE("lib_name");
  DECLARE_OBJNAME_AST_NODE("sb_name");

  DECLARE_OBJNAME_AST_NODE("pass_name");
  DECLARE_OBJNAME_AST_NODE("fxconfigdecl_name");
  DECLARE_OBJNAME_AST_NODE("fxconfigref_name");
  DECLARE_OBJNAME_AST_NODE("technique_name");
  ///////////////////////////////////////////////////////////
  DECLARE_STD_AST_NODE(DataType);
  DECLARE_STD_AST_NODE(DataTypeWithUserTypes);
  DECLARE_STD_AST_NODE(MemberRef);
  DECLARE_STD_AST_NODE(ArrayRef);
  DECLARE_STD_AST_NODE(RValueConstructor);
  DECLARE_STD_AST_NODE(TypedIdentifier);
  DECLARE_STD_AST_NODE(DataDeclaration);
  DECLARE_STD_AST_NODE(DataDeclarations);
  DECLARE_STD_AST_NODE(ArrayDeclaration);
  DECLARE_STD_AST_NODE(ParensExpression);
  ///////////////////////////////////////////////////////////
  DECLARE_STD_AST_NODE(Directive);
  DECLARE_STD_AST_NODE(ImportDirective);
  ///////////////////////////////////////////////////////////
  DECLARE_STD_AST_NODE(Operator);
  DECLARE_STD_AST_NODE(AssignmentOperator);
  DECLARE_STD_AST_NODE(MultiplicativeOperator);
  DECLARE_STD_AST_NODE(AdditiveOperator);
  DECLARE_STD_AST_NODE(ShiftOperator);
  DECLARE_STD_AST_NODE(RelationalOperator);
  DECLARE_STD_AST_NODE(EqualityOperator);
  DECLARE_STD_AST_NODE(ArrayIndexOperator);
  DECLARE_STD_AST_NODE(MemberAccessOperator);
  DECLARE_STD_AST_NODE(UnaryOperator);
  DECLARE_STD_AST_NODE(IncrementOperator);
  DECLARE_STD_AST_NODE(DecrementOperator);
  DECLARE_STD_AST_NODE(PrimaryIdentifier);
  ///////////////////////////////////////////////////////////
  DECLARE_STD_AST_NODE(MultiplicativeExpression);
  DECLARE_STD_AST_NODE(AdditiveExpression);
  DECLARE_STD_AST_NODE(UnaryExpression);
  DECLARE_STD_AST_NODE(PostfixExpression);
  DECLARE_STD_AST_NODE(PrimaryExpression);
  DECLARE_STD_AST_NODE(ConditionalExpression);
  DECLARE_STD_AST_NODE(AssignmentExpression);
  DECLARE_STD_AST_NODE(LogicalAndExpression);
  DECLARE_STD_AST_NODE(LogicalOrExpression);
  DECLARE_STD_AST_NODE(InclusiveOrExpression);
  DECLARE_STD_AST_NODE(ExclusiveOrExpression);
  DECLARE_STD_AST_NODE(AndExpression);
  DECLARE_STD_AST_NODE(EqualityExpression);
  DECLARE_STD_AST_NODE(RelationalExpression);
  DECLARE_STD_AST_NODE(ShiftExpression);
  DECLARE_STD_AST_NODE(Expression);
  DECLARE_STD_AST_NODE(Literal);
  DECLARE_STD_AST_NODE(NumericLiteral);
  DECLARE_STD_AST_NODE(FloatLiteral);
  DECLARE_STD_AST_NODE(IntegerLiteral);
  DECLARE_STD_AST_NODE(AssignmentExpression1);
  DECLARE_STD_AST_NODE(AssignmentExpression2);
  DECLARE_STD_AST_NODE(AssignmentExpression3);
  DECLARE_STD_AST_NODE(CastExpression);
  DECLARE_STD_AST_NODE(CastExpression1);
  DECLARE_STD_AST_NODE(ExpressionList);
  ///////////////////////////////////////////////////////////
  DECLARE_STD_AST_NODE(Statement);
  DECLARE_STD_AST_NODE(DiscardStatement);
  DECLARE_STD_AST_NODE(ExpressionStatement);
  DECLARE_STD_AST_NODE(CompoundStatement);
  DECLARE_STD_AST_NODE(IfStatement);
  DECLARE_STD_AST_NODE(WhileStatement);
  DECLARE_STD_AST_NODE(ForStatement);
  DECLARE_STD_AST_NODE(ReturnStatement);
  ///////////////////////////////////////////////////////////
  DECLARE_STD_AST_NODE(InterfaceLayout);
  DECLARE_STD_AST_NODE(InterfaceOutputs);
  DECLARE_STD_AST_NODE(InterfaceInputs);
  DECLARE_STD_AST_NODE(InterfaceInput);
  DECLARE_STD_AST_NODE(InterfaceOutput);
  DECLARE_STD_AST_NODE(InheritList);
  DECLARE_STD_AST_NODE(InheritListItem);
  DECLARE_STD_AST_NODE(Pass);
  DECLARE_STD_AST_NODE(FxConfigRef);
  ///////////////////////////////////////////////////////////
  DECLARE_STD_AST_NODE(VertexInterface);
  DECLARE_STD_AST_NODE(FragmentInterface);
  DECLARE_STD_AST_NODE(GeometryInterface);
  DECLARE_STD_AST_NODE(ComputeInterface);
  DECLARE_STD_AST_NODE(StateBlock);
  DECLARE_STD_AST_NODE(StateBlockItem);
  DECLARE_STD_AST_NODE(FxConfigDecl);
  DECLARE_STD_AST_NODE(UniformSet);
  DECLARE_STD_AST_NODE(UniformBlk);
  DECLARE_STD_AST_NODE(LibraryBlock);
  DECLARE_STD_AST_NODE(VertexShader);
  DECLARE_STD_AST_NODE(GeometryShader);
  DECLARE_STD_AST_NODE(FragmentShader);
  DECLARE_STD_AST_NODE(ComputeShader);
  DECLARE_STD_AST_NODE(FunctionDef1);
  DECLARE_STD_AST_NODE(FunctionDef2);
  DECLARE_STD_AST_NODE(StructDecl);
  DECLARE_STD_AST_NODE(Technique);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::shadlang::impl
/////////////////////////////////////////////////////////////////////////////////////////////////
#endif