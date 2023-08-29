#pragma once

#include <ork/util/parser.h>
#include <ork/kernel/treeops.inl>

///////////////////////////////////////////////////////////

#define DECLARE_STD_AST_CLASS(baseclass,x)\
struct x : public baseclass {\
  inline x() {\
    _name = #x;\
    _type_name = #x;\
  }\
  static constexpr const char* _static_type_name = #x;\
};

///////////////////////////////////////////////////////////

#define DECLARE_STD_AST_CLASS_WPTR(baseclass,x,ptr_t)\
DECLARE_STD_AST_CLASS(baseclass,x)\
using ptr_t = std::shared_ptr<x>;

///////////////////////////////////////////////////////////

namespace ork::lev2::shadlang::SHAST {

///////////////////////////////////////////////////////////

struct AstNode {
  using treeops = tree::Ops<AstNode>;
  using tree_constops = tree::ConstOps<AstNode>;
  using key_t = varmap::key_t;

  AstNode();
  virtual ~AstNode() {
  }
  virtual std::string desc() const {
    return _name;
  }
  ///////////////////////////
  bool hasKey(const key_t& key) const;
  ///////////////////////////
  template <typename T> std::shared_ptr<T> childAs(size_t index) {
    return treeops(this).childAs<T>(index);
  }
  ///////////////////////////
  template <typename child_t> //
  astnode_ptr_t findFirstChildOfType() const {
    return tree_constops(this).findFirstChildOfType<child_t>();
  }
  ////////////////////////////////////////////
  template <typename T> //
  std::vector<std::shared_ptr<T>> //
  static collectNodesOfType(astnode_ptr_t top, //
                            bool recurse=true){ //
    std::vector<std::shared_ptr<T>> nodes;
    if(recurse){
      auto collect_nodes     = std::make_shared<Visitor>();
      collect_nodes->_on_pre = [&](astnode_ptr_t node) {
        if (auto as_typed = std::dynamic_pointer_cast<T>(node)) {
          nodes.push_back(as_typed);
        }
      };
      visitNode(top, collect_nodes);

    }
    else{
      for( auto c : top->_children ){
        if (auto as_typed = std::dynamic_pointer_cast<T>(c)) {
          nodes.push_back(as_typed);
        }
      }
    }
    return nodes;
  }
  ///////////////////////////
  template <typename user_t> attempt_cast<user_t> typedValueForKey(key_t named);
  template <typename user_t> void setValueForKey(key_t named, user_t value );
  template <typename user_t> std::shared_ptr<user_t> sharedForKey(key_t named);
  template <typename user_t> std::shared_ptr<user_t> makeSharedForKey(key_t named);
  template <typename user_t> void setSharedForKey(key_t named, std::shared_ptr<user_t> ptr);
  ///////////////////////////
  static bool walkDownAST(                 //
    astnode_ptr_t node,   //
    walk_visitor_fn_t visitor);
  ///////////////////////////
  static void visitChildren(                 //
    astnode_ptr_t node,   //
    visitor_fn_t visitor);
  ///////////////////////////
  static void visitNode(                 //
    astnode_ptr_t node,   //
    visitor_ptr_t visitor);
  ///////////////////////////
  void appendChild(astnode_ptr_t child) {
    _children.push_back(child);
  }
  template <typename T, typename... A> std::shared_ptr<T> appendTypedChild(A&&... args) {
    auto ptr = std::make_shared<T>(std::forward<A>(args)...);
    _children.push_back(ptr);
    return ptr;
  }
  template <typename T, typename... A> std::shared_ptr<T> insertTypedChildAt(size_t index, A&&... args) {
    auto ptr = std::make_shared<T>(std::forward<A>(args)...);
    _children.insert(_children.begin()+index,ptr);
    return ptr;
  }
  void appendChildrenFrom(astnode_ptr_t other){
    _children.insert( _children.end(), other->_children.begin(), other->_children.end() );
  }
  ///////////////////////////
  std::string _name;
  std::string _type_name;
  bool _descend = true;
  bool _showDOT = true;
  int _nodeID = -1;
  bool _should_indent = false;
  bool _should_emit = true;
  astnode_ptr_t _parent;
  std::vector<astnode_ptr_t> _children;
  varmap::varmap_ptr_t _uservars;
  bool _xxx = false;
  bool _indented = false;
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

struct TranslationUnit : public AstNode {
  static constexpr const char* _static_type_name = "TranslationUnit";
  inline TranslationUnit() {
    _name = _static_type_name;
    _type_name = _static_type_name;
  }
  template <typename T>
  std::shared_ptr<T> find(std::string named){
    astnode_ptr_t rval;
    auto it = _translatables_by_name.find(named);
    if( it != _translatables_by_name.end() ){
      rval = it->second;
    }
    return std::dynamic_pointer_cast<T>(rval);
  }
  astnode_map_t _translatables_by_name;
  astnode_map_t _imported_translatables_by_name;
};
using transunit_ptr_t = std::shared_ptr<TranslationUnit>;

///////////////////////////////////////////////////////////

struct InsertLine : public AstNode {
  static constexpr const char* _static_type_name = "InsertLine";
  inline InsertLine(std::string text) {
    _name = _static_type_name;
    _type_name = _static_type_name;
    setValueForKey<std::string>("line_text", text);
  }
};

///////////////////////////////////////////////////////////

struct MiscGroupNode : public AstNode {
  static constexpr const char* _static_type_name = "MiscGroupNode";
  inline MiscGroupNode() {
    _name = _static_type_name;
    _type_name = _static_type_name;
  }
};
using miscgroupnode_ptr_t = std::shared_ptr<MiscGroupNode>;

///////////////////////////////////////////////////////////

DECLARE_STD_AST_CLASS(AstNode,IDENTIFIER);
DECLARE_STD_AST_CLASS_WPTR(AstNode,SemaIdentifier, semaid_ptr_t);

DECLARE_STD_AST_CLASS(AstNode,InheritList);
DECLARE_STD_AST_CLASS_WPTR(AstNode,InheritListItem,inhitem_ptr_t);
DECLARE_STD_AST_CLASS(AstNode,LanguageElement);
DECLARE_STD_AST_CLASS_WPTR(AstNode,Translatable,translatable_ptr_t);

DECLARE_STD_AST_CLASS(AstNode,DeclArgumentList);
DECLARE_STD_AST_CLASS(AstNode,InterfaceLayout);
DECLARE_STD_AST_CLASS(AstNode,InterfaceInput);
DECLARE_STD_AST_CLASS(AstNode,InterfaceOutput);
DECLARE_STD_AST_CLASS(AstNode,InterfaceInputs);
DECLARE_STD_AST_CLASS(AstNode,InterfaceOutputs);
DECLARE_STD_AST_CLASS(AstNode,InterfaceStorage);
DECLARE_STD_AST_CLASS(AstNode,InterfaceStorages);

DECLARE_STD_AST_CLASS(AstNode,Dependency);
DECLARE_STD_AST_CLASS(Dependency,Extension);
//
DECLARE_STD_AST_CLASS(AstNode,Directive);
DECLARE_STD_AST_CLASS_WPTR(Directive,ImportDirective, importdirective_ptr_t);
//
DECLARE_STD_AST_CLASS(LanguageElement,LValue);
DECLARE_STD_AST_CLASS_WPTR(LanguageElement,DataType, dt_ptr_t );
DECLARE_STD_AST_CLASS(LanguageElement,DataTypeWithUserTypes);
DECLARE_STD_AST_CLASS(LanguageElement,MemberRef);
DECLARE_STD_AST_CLASS(LanguageElement,ArrayRef);
DECLARE_STD_AST_CLASS(LanguageElement,ArgumentDeclaration);

//
DECLARE_STD_AST_CLASS(LanguageElement,DataDeclarationBase);
DECLARE_STD_AST_CLASS(DataDeclarationBase,DataDeclaration);
DECLARE_STD_AST_CLASS(DataDeclarationBase,ArrayDeclaration);
//
DECLARE_STD_AST_CLASS_WPTR(LanguageElement,TypedIdentifier, tid_ptr_t);
DECLARE_STD_AST_CLASS(LanguageElement,ObjectName);
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
DECLARE_STD_AST_CLASS(Expression,AndExpressionTail);
DECLARE_STD_AST_CLASS(Expression,EqualityExpression);
DECLARE_STD_AST_CLASS(Expression,RelationalExpression);
DECLARE_STD_AST_CLASS(Expression,CastExpression);
DECLARE_STD_AST_CLASS(Expression,TernaryExpression);
DECLARE_STD_AST_CLASS(Expression,IdentifierCall);

DECLARE_STD_AST_CLASS(Expression,WTFExp);

//
DECLARE_STD_AST_CLASS(LanguageElement,SemaExpression);
DECLARE_STD_AST_CLASS(SemaExpression,SemaMemberAccess);
DECLARE_STD_AST_CLASS(SemaExpression,SemaFunctionArguments);
DECLARE_STD_AST_CLASS(SemaExpression,SemaFunctionInvokation);
DECLARE_STD_AST_CLASS(SemaExpression,SemaFunctionName);
DECLARE_STD_AST_CLASS(SemaExpression,SemaConstructorType);
DECLARE_STD_AST_CLASS(SemaExpression,SemaConstructorInvokation);
DECLARE_STD_AST_CLASS(SemaExpression,SemaConstructorArguments);

DECLARE_STD_AST_CLASS_WPTR(SemaExpression,SemaInherit,semainh_ptr_t);
DECLARE_STD_AST_CLASS_WPTR(SemaInherit,SemaInheritLibrary, semainhlib_ptr_t);
DECLARE_STD_AST_CLASS_WPTR(SemaInherit,SemaInheritUniformSet, semainhuniset_ptr_t);
DECLARE_STD_AST_CLASS_WPTR(SemaInherit,SemaInheritUniformBlk, semainhuniblk_ptr_t);
DECLARE_STD_AST_CLASS_WPTR(SemaInherit,SemaInheritStateBlock, semainhstblk_ptr_t);
DECLARE_STD_AST_CLASS_WPTR(SemaInherit,SemaInheritExtension, semainhext_ptr_t);
DECLARE_STD_AST_CLASS_WPTR(SemaInherit,SemaInheritInterface, semainhif_ptr_t);
DECLARE_STD_AST_CLASS_WPTR(SemaInheritInterface,SemaInheritVertexInterface, semainhvif_ptr_t);
DECLARE_STD_AST_CLASS_WPTR(SemaInheritInterface,SemaInheritFragmentInterface, semainhfif_ptr_t);
DECLARE_STD_AST_CLASS_WPTR(SemaInheritInterface,SemaInheritGeometryInterface, semainhgif_ptr_t);
DECLARE_STD_AST_CLASS_WPTR(SemaInheritInterface,SemaInheritComputeInterface, semainhcif_ptr_t);
//
DECLARE_STD_AST_CLASS(Expression,ExpressionList);
//
DECLARE_STD_AST_CLASS(Expression,Literal);
DECLARE_STD_AST_CLASS(Literal,NumericLiteral);
DECLARE_STD_AST_CLASS(NumericLiteral,FloatLiteral);
DECLARE_STD_AST_CLASS(NumericLiteral,IntegerLiteral);
//
DECLARE_STD_AST_CLASS_WPTR(AssignmentExpression,AssignmentExpression1, asnexp1_ptr_t );
DECLARE_STD_AST_CLASS(AssignmentExpression,AssignmentExpression2);
DECLARE_STD_AST_CLASS(AssignmentExpression,AssignmentExpression3);
//
DECLARE_STD_AST_CLASS(CastExpression,CastExpression1);
//
DECLARE_STD_AST_CLASS(Statement,IfStatement);
DECLARE_STD_AST_CLASS(Statement,IfStatementBody);
DECLARE_STD_AST_CLASS(Statement,ElseStatement);
DECLARE_STD_AST_CLASS(Statement,ElseStatementBody);
DECLARE_STD_AST_CLASS(Statement,WhileStatement);
DECLARE_STD_AST_CLASS(Statement,ForStatement);
DECLARE_STD_AST_CLASS(Statement,ReturnStatement);
DECLARE_STD_AST_CLASS(Statement,CompoundStatement);
DECLARE_STD_AST_CLASS(Statement,ExpressionStatement);
DECLARE_STD_AST_CLASS(Statement,DiscardStatement);
DECLARE_STD_AST_CLASS(Statement,EmptyStatement);
//
DECLARE_STD_AST_CLASS_WPTR(Translatable,LibraryBlock, libblock_ptr_t);
DECLARE_STD_AST_CLASS(Translatable,FxConfigDecl);
DECLARE_STD_AST_CLASS_WPTR(Translatable,Shader, shader_ptr_t);
DECLARE_STD_AST_CLASS(Translatable,PipelineInterface);
DECLARE_STD_AST_CLASS(Translatable,StateBlock);
DECLARE_STD_AST_CLASS(Translatable,Technique);
DECLARE_STD_AST_CLASS(Translatable,FunctionDef1);
DECLARE_STD_AST_CLASS_WPTR(Translatable,FunctionDef2, fndef2_ptr_t);
DECLARE_STD_AST_CLASS_WPTR(Translatable,UniformSet,uniset_ptr_t);
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

DECLARE_STD_AST_CLASS(SemaExpression,SemaFloatLiteral);
DECLARE_STD_AST_CLASS(SemaExpression,SemaIntegerLiteral);

///////////////////////////////////////////////////////////

} // namespace ork::lev2::shadlang::SHAST
