#pragma once

#include <ork/util/parser.h>
#include <ork/kernel/treeops.inl>

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
  using treeops = tree::Ops<AstNode>;
  using tree_constops = tree::ConstOps<AstNode>;
  using key_t = varmap::key_t;

  AstNode() {
    static int gid = 0;
    _nodeID = gid++;
    _uservars = std::make_shared<varmap::VarMap>();
  }
  virtual ~AstNode() {
  }
  virtual std::string desc() const {
    return _name;
  }
  ///////////////////////////
  template <typename T> std::shared_ptr<T> childAs(size_t index) {
    return treeops(this).childAs<T>(index);
  }
  ///////////////////////////
  template <typename child_t> //
  astnode_ptr_t findFirstChildOfType() const {
    return tree_constops(this).findFirstChildOfType<child_t>();
  }
  ///////////////////////////
  static void replaceInParent( astnode_ptr_t oldnode, //
                               astnode_ptr_t newnode);
  ///////////////////////////
  static void removeFromParent( astnode_ptr_t oldnode );
  ///////////////////////////
  template <typename user_t> attempt_cast<user_t> typedValueForKey(key_t named);
  template <typename user_t> void setValueForKey(key_t named, user_t value );
  template <typename user_t> std::shared_ptr<user_t> sharedForKey(key_t named);
  template <typename user_t> std::shared_ptr<user_t> makeSharedForKey(key_t named);
  template <typename user_t> void setSharedForKey(key_t named, std::shared_ptr<user_t> ptr);


  bool hasKey(const key_t& key) const;

  ///////////////////////////
  std::string _name;
  bool _descend = true;
  bool _showDOT = true;
  int _nodeID = -1;
  astnode_ptr_t _parent;
  std::vector<astnode_ptr_t> _children;
  varmap::varmap_ptr_t _uservars;

};

///////////////////////////////////////////////////////////

template <typename user_t> attempt_cast<user_t> AstNode::typedValueForKey(key_t named) {
  return _uservars->typedValueForKey<user_t>(named);
}
template <typename user_t> void AstNode::setValueForKey(key_t named, user_t value ) {
  return _uservars->set<user_t>(named,value);
}
template <typename user_t> std::shared_ptr<user_t> AstNode::sharedForKey(key_t named) {
  using ptr_t = std::shared_ptr<user_t>;
  return _uservars->typedValueForKey<ptr_t>(named).value();
}
template <typename user_t> std::shared_ptr<user_t> AstNode::makeSharedForKey(key_t named) {
  return _uservars->makeSharedForKey<user_t>(named);
}
template <typename user_t> void AstNode::setSharedForKey(key_t named, std::shared_ptr<user_t> ptr) {
  return _uservars->set<std::shared_ptr<user_t>>(named,ptr);
}

///////////////////////////////////////////////////////////

DECLARE_STD_AST_CLASS(AstNode,InheritList);
DECLARE_STD_AST_CLASS(AstNode,InheritListItem);
DECLARE_STD_AST_CLASS(AstNode,LanguageElement);
DECLARE_STD_AST_CLASS(AstNode,TranslationUnit);
DECLARE_STD_AST_CLASS(AstNode,Translatable);

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
DECLARE_STD_AST_CLASS(SemaExpression,SemaInheritLibrary);
DECLARE_STD_AST_CLASS(SemaExpression,SemaInheritVertexInterface);
DECLARE_STD_AST_CLASS(SemaExpression,SemaInheritFragmentInterface);
DECLARE_STD_AST_CLASS(SemaExpression,SemaInheritGeometryInterface);
DECLARE_STD_AST_CLASS(SemaExpression,SemaInheritComputeInterface);
DECLARE_STD_AST_CLASS(SemaExpression,SemaInheritUniformSet);
DECLARE_STD_AST_CLASS(SemaExpression,SemaInheritUniformBlock);
DECLARE_STD_AST_CLASS(SemaExpression,SemaInheritStateBlock);
DECLARE_STD_AST_CLASS(SemaExpression,SemaInheritExtension);
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
DECLARE_STD_AST_CLASS(Translatable,UniformSet);
DECLARE_STD_AST_CLASS(Translatable,UniformBlk);
//
DECLARE_STD_AST_CLASS(AstNode,Pass);
DECLARE_STD_AST_CLASS(AstNode,FxConfigRef);
//
DECLARE_STD_AST_CLASS(Shader,VertexShader);
DECLARE_STD_AST_CLASS(Shader,FragmentShader);
DECLARE_STD_AST_CLASS(Shader,GeometryShader);
DECLARE_STD_AST_CLASS(Shader,ComputeShader);
//
DECLARE_STD_AST_CLASS(AstNode,VertexShaderRef);
DECLARE_STD_AST_CLASS(AstNode,FragmentShaderRef);
DECLARE_STD_AST_CLASS(AstNode,GeometryShaderRef);
DECLARE_STD_AST_CLASS(AstNode,StateBlockRef);
//
DECLARE_STD_AST_CLASS(PipelineInterface,VertexInterface);
DECLARE_STD_AST_CLASS(PipelineInterface,GeometryInterface);
DECLARE_STD_AST_CLASS(PipelineInterface,FragmentInterface);
DECLARE_STD_AST_CLASS(PipelineInterface,ComputeInterface);

///////////////////////////////////////////////////////////

} // namespace ork::lev2::shadlang::SHAST
