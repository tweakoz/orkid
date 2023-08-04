#pragma once

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
    svar64_t _impl;
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
  std::string _name;
  bool _descend = true;
  bool _showDOT = true;
  int _nodeID = -1;
  astnode_ptr_t _parent;
  std::vector<astnode_ptr_t> _children;
  svar64_t _impl;

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
DECLARE_STD_AST_CLASS(AstNode,StatementList);
DECLARE_STD_AST_CLASS(AstNode,InterfaceLayout);
DECLARE_STD_AST_CLASS(AstNode,InterfaceInput);
DECLARE_STD_AST_CLASS(AstNode,InterfaceOutput);
DECLARE_STD_AST_CLASS(AstNode,InterfaceInputs);
DECLARE_STD_AST_CLASS(AstNode,InterfaceOutputs);
DECLARE_STD_AST_CLASS(AstNode,ImportDirective);
DECLARE_STD_AST_CLASS(AstNode,Dependency);
DECLARE_STD_AST_CLASS(Dependency,Extension);
//
DECLARE_STD_AST_CLASS(LanguageElement,LValue);
DECLARE_STD_AST_CLASS(LanguageElement,DataType);
DECLARE_STD_AST_CLASS(LanguageElement,MemberRef);
DECLARE_STD_AST_CLASS(LanguageElement,ArrayRef);
DECLARE_STD_AST_CLASS(LanguageElement,Literal);
DECLARE_STD_AST_CLASS(LanguageElement,ArgumentDeclaration);
DECLARE_STD_AST_CLASS(LanguageElement,DataDeclaration);
DECLARE_STD_AST_CLASS(LanguageElement,TypedIdentifier);
DECLARE_STD_AST_CLASS(LanguageElement,ObjectName);
DECLARE_STD_AST_CLASS(LanguageElement,RValueConstructor);
DECLARE_STD_AST_CLASS(LanguageElement,StateBlockItem);
DECLARE_STD_AST_CLASS(LanguageElement,AssignmentStatementVarRef);
DECLARE_STD_AST_CLASS(LanguageElement,AssignmentStatementVarDecl);
DECLARE_STD_AST_CLASS(LanguageElement,Expression);
DECLARE_STD_AST_CLASS(LanguageElement,Statement);
DECLARE_STD_AST_CLASS(LanguageElement,ArgumentExpressionList);
DECLARE_STD_AST_CLASS(LanguageElement,FunctionInvokationArgument);
DECLARE_STD_AST_CLASS(LanguageElement,FunctionInvokationArguments);
DECLARE_STD_AST_CLASS(LanguageElement,FunctionInvokation);
DECLARE_STD_AST_CLASS(LanguageElement,DataDeclarations);
//
DECLARE_STD_AST_CLASS(Literal,NumericLiteral);
DECLARE_STD_AST_CLASS(NumericLiteral,FloatLiteral);
DECLARE_STD_AST_CLASS(NumericLiteral,IntegerLiteral);
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
//
DECLARE_STD_AST_CLASS(Statement,IfStatement);
DECLARE_STD_AST_CLASS(Statement,WhileStatement);
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
DECLARE_STD_AST_CLASS(Translatable,FunctionDef);
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
