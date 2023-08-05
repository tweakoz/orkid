#pragma once

#include <ork/util/parser.h>

///////////////////////////////////////////////////////////

#define DECLARE_STD_AST_CLASS(baseclass,x)\
struct x : public baseclass {\
  inline x() {\
    _name = #x;\
  }\
};

///////////////////////////////////////////////////////////

namespace ork::lev2::shadlang::SHAST {

///////////////////////////////////////////////////////////

struct AstNode {
  AstNode() {
    static int gid = 0;
    _nodeID = gid++;
    _user = std::make_shared<varmap::VarMap>();
  }
  virtual ~AstNode() {
  }
  virtual std::string desc() const {
    return _name;
  }
  template <typename T> std::shared_ptr<T> childAs(size_t index) {
    OrkAssert(index < _children.size());
    auto ch  = _children[index];
    auto ret = std::dynamic_pointer_cast<T>(ch);
    OrkAssert(ret);
    return ret;
  }
  ///////////////////////////
  static void replaceInParent( astnode_ptr_t oldnode, //
                               astnode_ptr_t newnode);
  ///////////////////////////
  std::string _name;
  bool _descend = true;
  bool _showDOT = true;
  int _nodeID = -1;
  astnode_ptr_t _parent;
  std::vector<astnode_ptr_t> _children;
  varmap::varmap_ptr_t _user;

};

///////////////////////////////////////////////////////////

DECLARE_STD_AST_CLASS(AstNode,InheritList);
DECLARE_STD_AST_CLASS(AstNode,InheritListItem);
DECLARE_STD_AST_CLASS(AstNode,LanguageElement);
DECLARE_STD_AST_CLASS(AstNode,TranslationUnit);
DECLARE_STD_AST_CLASS(AstNode,Translatable);

DECLARE_STD_AST_CLASS(AstNode,UniformSet);
DECLARE_STD_AST_CLASS(AstNode,UniformBlk);
DECLARE_STD_AST_CLASS(AstNode,DeclArgumentList);
DECLARE_STD_AST_CLASS(AstNode,InterfaceLayout);
DECLARE_STD_AST_CLASS(AstNode,InterfaceInput);
DECLARE_STD_AST_CLASS(AstNode,InterfaceOutput);
DECLARE_STD_AST_CLASS(AstNode,InterfaceInputs);
DECLARE_STD_AST_CLASS(AstNode,InterfaceOutputs);
DECLARE_STD_AST_CLASS(AstNode,Dependency);
DECLARE_STD_AST_CLASS(Dependency,Extension);
//
DECLARE_STD_AST_CLASS(AstNode,Directive);
DECLARE_STD_AST_CLASS(Directive,ImportDirective);
//
DECLARE_STD_AST_CLASS(LanguageElement,LValue);
DECLARE_STD_AST_CLASS(LanguageElement,DataType);
DECLARE_STD_AST_CLASS(LanguageElement,DataTypeWithUserTypes);
DECLARE_STD_AST_CLASS(LanguageElement,MemberRef);
DECLARE_STD_AST_CLASS(LanguageElement,ArrayRef);
DECLARE_STD_AST_CLASS(LanguageElement,ArgumentDeclaration);
DECLARE_STD_AST_CLASS(LanguageElement,DataDeclaration);
DECLARE_STD_AST_CLASS(LanguageElement,ArrayDeclaration);
DECLARE_STD_AST_CLASS(LanguageElement,TypedIdentifier);
DECLARE_STD_AST_CLASS(LanguageElement,ObjectName);
DECLARE_STD_AST_CLASS(LanguageElement,RValueConstructor);
DECLARE_STD_AST_CLASS(LanguageElement,StateBlockItem);
DECLARE_STD_AST_CLASS(LanguageElement,AssignmentStatementVarRef);
DECLARE_STD_AST_CLASS(LanguageElement,AssignmentStatementVarDecl);
DECLARE_STD_AST_CLASS(LanguageElement,Expression);
DECLARE_STD_AST_CLASS(LanguageElement,Statement);
DECLARE_STD_AST_CLASS(LanguageElement,DataDeclarations);
DECLARE_STD_AST_CLASS(LanguageElement,StructDecl);
//
DECLARE_STD_AST_CLASS(LanguageElement,Operator);
DECLARE_STD_AST_CLASS(Operator,AssignmentOperator);
DECLARE_STD_AST_CLASS(Operator,MultiplicativeOperator);
DECLARE_STD_AST_CLASS(Operator,AdditiveOperator);
DECLARE_STD_AST_CLASS(Operator,ShiftOperator);
DECLARE_STD_AST_CLASS(Operator,RelationalOperator);
DECLARE_STD_AST_CLASS(Operator,EqualityOperator);
DECLARE_STD_AST_CLASS(Operator,UnaryOperator);
DECLARE_STD_AST_CLASS(LanguageElement,ArrayIndexOperator);
DECLARE_STD_AST_CLASS(LanguageElement,MemberAccessOperator);
DECLARE_STD_AST_CLASS(LanguageElement,ParensExpression);
DECLARE_STD_AST_CLASS(LanguageElement,IncrementOperator);
DECLARE_STD_AST_CLASS(LanguageElement,DecrementOperator);
DECLARE_STD_AST_CLASS(LanguageElement,PrimaryIdentifier);
//
DECLARE_STD_AST_CLASS(Expression,AdditiveExpression);
DECLARE_STD_AST_CLASS(Expression,MultiplicativeExpression);
DECLARE_STD_AST_CLASS(Expression,UnaryExpression);
DECLARE_STD_AST_CLASS(Expression,PostfixExpression);
DECLARE_STD_AST_CLASS(Expression,PrimaryExpression);
DECLARE_STD_AST_CLASS(Expression,AssignmentExpression);
DECLARE_STD_AST_CLASS(Expression,ConditionalExpression);
DECLARE_STD_AST_CLASS(Expression,ShiftExpression);
DECLARE_STD_AST_CLASS(Expression,LogicalAndExpression);
DECLARE_STD_AST_CLASS(Expression,LogicalOrExpression);
DECLARE_STD_AST_CLASS(Expression,InclusiveOrExpression);
DECLARE_STD_AST_CLASS(Expression,ExclusiveOrExpression);
DECLARE_STD_AST_CLASS(Expression,AndExpression);
DECLARE_STD_AST_CLASS(Expression,EqualityExpression);
DECLARE_STD_AST_CLASS(Expression,RelationalExpression);
DECLARE_STD_AST_CLASS(Expression,CastExpression);
//
DECLARE_STD_AST_CLASS(LanguageElement,SemaExpression);
DECLARE_STD_AST_CLASS(SemaExpression,SemaMemberAccess);
DECLARE_STD_AST_CLASS(SemaExpression,SemaFunctionArguments);
DECLARE_STD_AST_CLASS(SemaExpression,SemaFunctionInvokation);
DECLARE_STD_AST_CLASS(SemaExpression,SemaFunctionName);
DECLARE_STD_AST_CLASS(SemaExpression,SemaConstructorType);
DECLARE_STD_AST_CLASS(SemaExpression,SemaConstructorInvokation);
DECLARE_STD_AST_CLASS(SemaExpression,SemaConstructorArguments);
//
DECLARE_STD_AST_CLASS(Expression,ExpressionList);
//
DECLARE_STD_AST_CLASS(Expression,Literal);
DECLARE_STD_AST_CLASS(Literal,NumericLiteral);
DECLARE_STD_AST_CLASS(NumericLiteral,FloatLiteral);
DECLARE_STD_AST_CLASS(NumericLiteral,IntegerLiteral);
//
DECLARE_STD_AST_CLASS(AssignmentExpression,AssignmentExpression1);
DECLARE_STD_AST_CLASS(AssignmentExpression,AssignmentExpression2);
DECLARE_STD_AST_CLASS(AssignmentExpression,AssignmentExpression3);
//
DECLARE_STD_AST_CLASS(CastExpression,CastExpression1);
//
DECLARE_STD_AST_CLASS(Statement,IfStatement);
DECLARE_STD_AST_CLASS(Statement,WhileStatement);
DECLARE_STD_AST_CLASS(Statement,ForStatement);
DECLARE_STD_AST_CLASS(Statement,ReturnStatement);
DECLARE_STD_AST_CLASS(Statement,CompoundStatement);
DECLARE_STD_AST_CLASS(Statement,ExpressionStatement);
DECLARE_STD_AST_CLASS(Statement,DiscardStatement);
DECLARE_STD_AST_CLASS(Statement,EmptyStatement);
//
DECLARE_STD_AST_CLASS(Translatable,LibraryBlock);
DECLARE_STD_AST_CLASS(Translatable,FxConfigDecl);
DECLARE_STD_AST_CLASS(Translatable,Shader);
DECLARE_STD_AST_CLASS(Translatable,PipelineInterface);
DECLARE_STD_AST_CLASS(Translatable,StateBlock);
DECLARE_STD_AST_CLASS(Translatable,Technique);
DECLARE_STD_AST_CLASS(Translatable,FunctionDef1);
DECLARE_STD_AST_CLASS(Translatable,FunctionDef2);
//
DECLARE_STD_AST_CLASS(AstNode,Pass);
DECLARE_STD_AST_CLASS(AstNode,FxConfigRef);
//
DECLARE_STD_AST_CLASS(Shader,VertexShader);
DECLARE_STD_AST_CLASS(Shader,FragmentShader);
DECLARE_STD_AST_CLASS(Shader,GeometryShader);
DECLARE_STD_AST_CLASS(Shader,ComputeShader);
//
DECLARE_STD_AST_CLASS(PipelineInterface,VertexInterface);
DECLARE_STD_AST_CLASS(PipelineInterface,GeometryInterface);
DECLARE_STD_AST_CLASS(PipelineInterface,FragmentInterface);

///////////////////////////////////////////////////////////

} // namespace ork::lev2::shadlang::SHAST
