////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/shadlang.h>
#include "shadlang_impl.h"

namespace ork::lev2::shadlang {
using namespace SHAST;
/////////////////////////////////////////////////////////////////////////////////////////////////

struct GLFX1Backend {

  GLFX1Backend();

  void _visit(astnode_ptr_t node);
  void generate(translationunit_ptr_t top);

  template <typename T>
  std::shared_ptr<T> as(astnode_ptr_t node) {
    return std::dynamic_pointer_cast<T>(node);
  }

  template <typename T>
  void registerAstPreCB(std::function<void(std::shared_ptr<T>)> tcb) {
    auto cb = [=](astnode_ptr_t node) {
      auto tnode = as<T>(node);
      if (tnode) {
        tcb(tnode);
      }
    };
    _precb_map[T::_static_type_name] = cb;
  }
  template <typename T>
  void registerAstPostCB(std::function<void(std::shared_ptr<T>)> tcb) {
    auto cb = [=](astnode_ptr_t node) {
      auto tnode = as<T>(node);
      if (tnode) {
        tcb(tnode);
      }
    };
    _postcb_map[T::_static_type_name] = cb;
  }

  template <typename T>
  void registerAstPreChildCB(std::function<void(std::shared_ptr<T>,astnode_ptr_t)> tcb) {
    auto cb = [=](astnode_ptr_t node, astnode_ptr_t child) {
      auto tnode = as<T>(node);
      if (tnode) {
        tcb(tnode,child);
      }
    };
    _prechildcb_map[T::_static_type_name] = cb;
  }
  template <typename T>
  void registerAstPostChildCB(std::function<void(std::shared_ptr<T>,astnode_ptr_t)> tcb) {
    auto cb = [=](astnode_ptr_t node, astnode_ptr_t child) {
      auto tnode = as<T>(node);
      if (tnode) {
        tcb(tnode,child);
      }
    };
    _postchildcb_map[T::_static_type_name] = cb;
  }

  //////////////////////////////////////////////////////////////////////////////////
  void emitBeginLine(const char* formatstring, ...) {
    char formatbuffer[512];
    va_list args;
    va_start(args, formatstring);
    vsnprintf(&formatbuffer[0], sizeof(formatbuffer), formatstring, args);
    va_end(args);
    emitContinueLine( "/* %04d */ ", _lineoutindex );
    _outstr += std::string(_indent * 2, ' ');
    _outstr += std::string(formatbuffer);
  }
  //////////////////////////////////////////////////////////////////////////////////
  void emitContinueLine(const char* formatstring, ...) {
    char formatbuffer[512];
    va_list args;
    va_start(args, formatstring);
    vsnprintf(&formatbuffer[0], sizeof(formatbuffer), formatstring, args);
    va_end(args);
    _outstr += std::string(formatbuffer);
  }
  //////////////////////////////////////////////////////////////////////////////////
  void emitEndLine(const char* formatstring, ...) {
    char formatbuffer[512];
    va_list args;
    va_start(args, formatstring);
    vsnprintf(&formatbuffer[0], sizeof(formatbuffer), formatstring, args);
    va_end(args);
    _outstr += std::string(formatbuffer);
    _outstr += "\n";
    _lineoutindex++;
  }
  //////////////////////////////////////////////////////////////////////////////////
  void emitLine(const char* formatstring, ...) {
    char formatbuffer[512];
    va_list args;
    va_start(args, formatstring);
    vsnprintf(&formatbuffer[0], sizeof(formatbuffer), formatstring, args);
    va_end(args);
    emitContinueLine( "/* %04d */ ", _lineoutindex );
    _outstr += std::string(_indent * 2, ' ');
    _outstr += std::string(formatbuffer);
    _outstr += "\n";
    _lineoutindex++;
  }
  //////////////////////////////////////////////////////////////////////////////////

  void emitFunctionDef2( fndef2_ptr_t fd2 );
  void emitAssignmentExpression1( asnexp1_ptr_t ae1 );

  //////////////////////////////////////////////////////////////////////////////////


  using base_cb_t = std::function<void(astnode_ptr_t)>;
  using child_cb_t = std::function<void(astnode_ptr_t,astnode_ptr_t)>;

  std::string _outstr;
  std::stack<astnode_ptr_t> _node_stack;
  std::map<std::string, base_cb_t> _precb_map;
  std::map<std::string, base_cb_t> _postcb_map;

  std::map<std::string, child_cb_t> _postchildcb_map;
  std::map<std::string, child_cb_t> _prechildcb_map;

  size_t _indent = 0;
  size_t _lineoutindex = 0;

};

////////////////////////////////////////////////////////////////

void GLFX1Backend::_visit(astnode_ptr_t node) {

  int parent_id = -1;
  if (_node_stack.size()) {
    parent_id = _node_stack.top()->_nodeID;
  }

  ///////////////////////////////////////////////
  // pre-visit
  ///////////////////////////////////////////////

  auto it_pre = _precb_map.find(node->_type_name);
  if( it_pre != _precb_map.end() ){
    auto precb = it_pre->second;
    if(node->_should_emit)
      precb(node);
  }

  ///////////////////////////////////////////////
  _node_stack.push(node);

  for (auto c : node->_children) {

    auto it_chipre = _prechildcb_map.find(node->_type_name);
    if( it_chipre != _prechildcb_map.end() ){
      auto prechicb = it_chipre->second;
      prechicb(node,c);
    }

    _visit(c);

    auto it_chipost = _postchildcb_map.find(node->_type_name);
    if( it_chipost != _postchildcb_map.end() ){
      auto postchicb = it_chipost->second;
      postchicb(node,c);
    }

  }
  _node_stack.pop();
  ///////////////////////////////////////////////
  // post-visit
  ///////////////////////////////////////////////

  auto it_post = _postcb_map.find(node->_type_name);
  if( it_post != _postcb_map.end() ){
    auto postcb = it_post->second;
    if(node->_should_emit)
      postcb(node);
  }

  ///////////////////////////////////////////////

}

////////////////////////////////////////////////////////////////

void GLFX1Backend::generate(translationunit_ptr_t top) {
  _visit(top);
}

////////////////////////////////////////////////////////////////

GLFX1Backend::GLFX1Backend(){

  registerAstPreCB<TranslationUnit>([](auto tu){
    //OrkAssert(false);
  });
  /////////////////////////////////////////////////////////////////////
  // functions
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<FunctionDef2>([=](auto fd2){
    auto fn_name = fd2-> template typedValueForKey<std::string>("function_name").value();
    auto dt_node = fd2-> template childAs<DataType>(0);
    dt_node->_should_emit = false;
    auto return_type = dt_node-> template typedValueForKey<std::string>("data_type").value();
    emitBeginLine( "%s %s", return_type.c_str(), fn_name.c_str() );
  });
  registerAstPreCB<DeclArgumentList>([=](auto da_node){
    emitContinueLine( "(" );
  });
  registerAstPostCB<DeclArgumentList>([=](auto da_node){
    emitContinueLine( ")" );
  });
  registerAstPostChildCB<DeclArgumentList>([=](auto da_node, astnode_ptr_t child){
    size_t num_children = da_node->_children.size();
    if(num_children>1){
      if( child != da_node->_children.back() ){
        emitContinueLine( ", " );
      }
    }
  });
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<SemaConstructorInvokation>([=](auto fd2){
    auto fn_name = fd2-> template typedValueForKey<std::string>("data_type").value();
    emitContinueLine( "%s", fn_name.c_str() );
  });
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<SemaConstructorArguments>([=](auto sca_node){
    emitContinueLine( "(" );
  });
  registerAstPostCB<SemaConstructorArguments>([=](auto sca_node){
    emitContinueLine( ")" );
  });
  registerAstPostChildCB<SemaConstructorArguments>([=](auto sca_node, astnode_ptr_t child){
    size_t num_children = sca_node->_children.size();
    if(num_children>1){
      if( child != sca_node->_children.back() ){
        emitContinueLine( ", " );
      }
    }
  });
  /////////////////////////////////////////////////////////////////////
  // statements
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<CompoundStatement>([=](auto expstmt){
      emitEndLine( "{" );
      _indent++;
  });
  registerAstPostCB<CompoundStatement>([=](auto expstmt){
      _indent--;
      emitLine( "}" );
  });
  registerAstPreChildCB<CompoundStatement>([=](auto compstmt, astnode_ptr_t child){
      emitBeginLine( "" );
  });
  registerAstPostChildCB<CompoundStatement>([=](auto compstmt, astnode_ptr_t child){
      emitEndLine( "" );
  });
  registerAstPreCB<ExpressionStatement>([=](auto expstmt){
      //emitBeginLine( "" );
  });
  registerAstPostCB<ExpressionStatement>([=](auto expstmt){
      emitContinueLine( "; ");
  });
  registerAstPreCB<ForStatement>([=](auto forstmt){
      emitContinueLine( "for(" );
  });
  registerAstPostChildCB<ForStatement>([=](auto forstmt, astnode_ptr_t child){
    size_t num_children = forstmt->_children.size();
    if( child == forstmt->_children[num_children-2] ){
      emitContinueLine( ") " );
    }
  });
  registerAstPreCB<ReturnStatement>([=](auto retstmt){
      emitContinueLine( "return " );
  });
  registerAstPostCB<ReturnStatement>([=](auto retstmt){
      emitContinueLine( ";");
  });
  /////////////////////////////////////////////////////////////////////
  // expressions
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<ParensExpression>([=](auto mo_node){
    emitContinueLine( "(" );
  });
  registerAstPostCB<ParensExpression>([=](auto mo_node){
    emitContinueLine( ")" );
  });
  registerAstPostChildCB<AssignmentExpression1>([=](auto ae_node, astnode_ptr_t child){
    if( child == ae_node->_children[0] ){
      emitContinueLine( " " );
    }
  });
  registerAstPostChildCB<ExpressionList>([=](auto exprlist, astnode_ptr_t child){
    size_t num_children = exprlist->_children.size();
    if(num_children>1){
      if( child != exprlist->_children.back() ){
        emitContinueLine( ", " );
      }
    }
  });
  /////////////////////////////////////////////////////////////////////
  // operators
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<AssignmentOperator>([=](auto ao_node){
    auto oper = ao_node-> template typedValueForKey<std::string>("operator").value();
    emitContinueLine( " %s ", oper.c_str() );
  });
  registerAstPreCB<AdditiveOperator>([=](auto ao_node){
    auto oper = ao_node-> template typedValueForKey<std::string>("operator").value();
    emitContinueLine( " %s ", oper.c_str() );
  });
  registerAstPreCB<MultiplicativeOperator>([=](auto mo_node){
    auto oper = mo_node-> template typedValueForKey<std::string>("operator").value();
    emitContinueLine( " %s ", oper.c_str() );
  });
  registerAstPreCB<RelationalOperator>([=](auto ro_node){
    auto oper = ro_node-> template typedValueForKey<std::string>("operator").value();
    emitContinueLine( " %s ", oper.c_str() );
  });
  registerAstPreCB<MemberAccessOperator>([=](auto mo_node){
    auto membname = mo_node-> template typedValueForKey<std::string>("member_name").value();
    emitContinueLine( ".%s", membname.c_str() );
  });
  registerAstPreCB<IncrementOperator>([=](auto inc_node){
    emitContinueLine( "++" );
  });
  registerAstPreCB<DecrementOperator>([=](auto inc_node){
    emitContinueLine( "--" );
  });
  /////////////////////////////////////////////////////////////////////
  // misc prims
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<DataType>([=](auto dt_node){
    auto dt_type = dt_node-> template typedValueForKey<std::string>("data_type").value();
    emitContinueLine( "%s", dt_type.c_str() );
  });
  registerAstPreCB<PrimaryIdentifier>([=](auto pid_node){
    auto ident = pid_node-> template typedValueForKey<std::string>("identifier_name").value();
    emitContinueLine( "%s", ident.c_str() );
  });
  registerAstPreCB<TypedIdentifier>([=](auto tid_node){
    auto dt_type = tid_node-> template typedValueForKey<std::string>("data_type").value();
    auto ident = tid_node-> template typedValueForKey<std::string>("identifier_name").value();
    emitContinueLine( "%s %s", dt_type.c_str(), ident.c_str() );
  });
  registerAstPreCB<SemaIntegerLiteral>([=](auto int_node){
    auto literal_value = int_node-> template typedValueForKey<std::string>("literal_value").value();
    emitContinueLine( "%s", literal_value.c_str() );
  });
  registerAstPreCB<SemaFloatLiteral>([=](auto float_node){
    auto literal_value = float_node-> template typedValueForKey<std::string>("literal_value").value();
    emitContinueLine( "%s", literal_value.c_str() );
  });
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void GLFX1Backend::emitFunctionDef2( fndef2_ptr_t fd2 ){
}

/////////////////////////////////////////////////////////////////////////////////////////////////

std::string toGLFX1(translationunit_ptr_t top) {
  auto backend = std::make_shared<GLFX1Backend>();
  backend->generate(top);
  return backend->_outstr;
}


/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::shadlang
