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
  void generate(astnode_ptr_t top);

  template <typename T> std::shared_ptr<T> as(astnode_ptr_t node) {
    return std::dynamic_pointer_cast<T>(node);
  }

  template <typename T> void registerAstPreCB(std::function<void(std::shared_ptr<T>)> tcb) {
    auto cb = [=](astnode_ptr_t node) {
      auto tnode = as<T>(node);
      if (tnode) {
        tcb(tnode);
      }
    };
    _precb_map[T::_static_type_name] = cb;
  }
  template <typename T> void registerAstPostCB(std::function<void(std::shared_ptr<T>)> tcb) {
    auto cb = [=](astnode_ptr_t node) {
      auto tnode = as<T>(node);
      if (tnode) {
        tcb(tnode);
      }
    };
    _postcb_map[T::_static_type_name] = cb;
  }

  template <typename T> void registerAstPreChildCB(std::function<void(std::shared_ptr<T>, astnode_ptr_t)> tcb) {
    auto cb = [=](astnode_ptr_t node, astnode_ptr_t child) {
      auto tnode = as<T>(node);
      if (tnode) {
        tcb(tnode, child);
      }
    };
    _prechildcb_map[T::_static_type_name] = cb;
  }
  template <typename T> void registerAstPostChildCB(std::function<void(std::shared_ptr<T>, astnode_ptr_t)> tcb) {
    auto cb = [=](astnode_ptr_t node, astnode_ptr_t child) {
      auto tnode = as<T>(node);
      if (tnode) {
        tcb(tnode, child);
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
    emitContinueLine("/* %04d */ ", _lineoutindex+1);
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
    emitContinueLine("/* %04d */ ", _lineoutindex+1);
    _outstr += std::string(_indent * 2, ' ');
    _outstr += std::string(formatbuffer);
    _outstr += "\n";
    _lineoutindex++;
  }

  //////////////////////////////////////////////////////////////////////////////////

  using base_cb_t  = std::function<void(astnode_ptr_t)>;
  using child_cb_t = std::function<void(astnode_ptr_t, astnode_ptr_t)>;

  std::string _outstr;
  std::stack<astnode_ptr_t> _node_stack;
  std::map<std::string, base_cb_t> _precb_map;
  std::map<std::string, base_cb_t> _postcb_map;

  std::map<std::string, child_cb_t> _postchildcb_map;
  std::map<std::string, child_cb_t> _prechildcb_map;
  size_t _indent       = 0;
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
  if (it_pre != _precb_map.end()) {
    auto precb = it_pre->second;
    if (node->_should_emit)
      precb(node);
  }

  ///////////////////////////////////////////////
  _node_stack.push(node);

  for (auto c : node->_children) {

    auto it_chipre = _prechildcb_map.find(node->_type_name);
    if (it_chipre != _prechildcb_map.end()) {
      auto prechicb = it_chipre->second;
      prechicb(node, c);
    }

    _visit(c);

    auto it_chipost = _postchildcb_map.find(node->_type_name);
    if (it_chipost != _postchildcb_map.end()) {
      auto postchicb = it_chipost->second;
      postchicb(node, c);
    }
  }
  _node_stack.pop();
  ///////////////////////////////////////////////
  // post-visit
  ///////////////////////////////////////////////

  auto it_post = _postcb_map.find(node->_type_name);
  if (it_post != _postcb_map.end()) {
    auto postcb = it_post->second;
    if (node->_should_emit)
      postcb(node);
  }

  ///////////////////////////////////////////////
}

////////////////////////////////////////////////////////////////

void GLFX1Backend::generate(astnode_ptr_t top) {
  _visit(top);
}

////////////////////////////////////////////////////////////////

GLFX1Backend::GLFX1Backend() {

  registerAstPreCB<TranslationUnit>([](auto tu) {
    // OrkAssert(false);
  });
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<InheritListItem>([=](auto inhlistitem) {
    emitBeginLine(": " );
  });
  registerAstPostCB<InheritListItem>([=](auto inhlistitem) {
    emitEndLine("");
  });
  /////////////////////////////////////////////////////////////////////
  auto named_item_pre_child_cb = [=](auto node, astnode_ptr_t child) {
    auto as_inhi = std::dynamic_pointer_cast<InheritListItem>(child);
    if( not as_inhi ){
      if( not node->_xxx ){
        emitLine("{");
        _indent++;
        node->_indented = true;
        node->_xxx = true;
      }
    }
    emitEndLine("");
  };
  /////////////////////////////////////////////////////////////////////
  auto named_precb = [=](auto node, std::string ID) {
    auto the_name = node->template typedValueForKey<std::string>("object_name").value();
    emitLine("%s %s", ID.c_str(), the_name.c_str() );
  };
  /////////////////////////////////////////////////////////////////////
  auto named_postcb = [=](auto node) {
    if( node->_indented ){
      _indent--;
    }
    else{
      emitLine("{");
    }
    auto the_name = node->template typedValueForKey<std::string>("object_name").value();
    emitLine("");
    emitLine("} // %s", the_name.c_str() );
    emitLine("");
  };
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<LibraryBlock>([=](auto libblock) { 
    named_precb( libblock, "libblock" );
  });
  registerAstPostCB<LibraryBlock>([=](auto libblock) { named_postcb(libblock); });
  registerAstPreChildCB<LibraryBlock>(named_item_pre_child_cb);
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<UniformSet>([=](auto uni_set) { named_precb( uni_set, "uniform_set" ); });
  registerAstPostCB<UniformSet>([=](auto uni_set) { named_postcb(uni_set); });
  registerAstPreChildCB<UniformSet>(named_item_pre_child_cb);
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<UniformBlk>([=](auto uni_blk) { named_precb( uni_blk, "uniform_blk" ); });
  registerAstPostCB<UniformBlk>([=](auto uni_blk) { named_postcb(uni_blk); });
  registerAstPreChildCB<UniformBlk>(named_item_pre_child_cb);
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<DataDeclaration>([=](auto ddecl) {
    emitBeginLine("");
  });
  registerAstPostCB<DataDeclaration>([=](auto ddecl) {
    emitEndLine(";");
  });
  registerAstPreCB<ArrayDeclaration>([=](auto adecl) {
    emitBeginLine("");
  });
  registerAstPostCB<ArrayDeclaration>([=](auto adecl) {
    emitEndLine(";");
  });
  registerAstPreChildCB<ArrayDeclaration>([=](auto adecl, astnode_ptr_t child) {
    OrkAssert(adecl->_children.size()==2);
    if (child == adecl->_children[1]) {
      emitContinueLine("[");
    }
  });
  registerAstPostChildCB<ArrayDeclaration>([=](auto adecl, astnode_ptr_t child) {
    OrkAssert(adecl->_children.size()==2);
    if (child == adecl->_children[1]) {
      emitContinueLine("]");
    }
  });
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<StructDecl>([=](auto struct_node){
    emitBeginLine("struct ");
  });
  registerAstPostCB<StructDecl>([=](auto struct_node){
    _indent--;
    emitLine("};");
  });
  registerAstPostChildCB<StructDecl>([=](auto struct_node, astnode_ptr_t child){
    if( child == struct_node->_children[0] ) { // struct_name
        emitEndLine("{");
        _indent++;
    }
  });
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<StateBlock>([=](auto stateblock) { named_precb( stateblock, "stateblock" ); });
  registerAstPostCB<StateBlock>([=](auto stateblock) { named_postcb(stateblock); });
  registerAstPreChildCB<StateBlock>(named_item_pre_child_cb);
  registerAstPreCB<StateBlockItem>([=](auto stateblockitem) { 
    emitBeginLine("StateBlockItem : ");
  });
  registerAstPostCB<StateBlockItem>([=](auto stateblockitem) {
    emitEndLine(";");
  });
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<InterfaceInputs>([=](auto ii_node){
    emitLine("inputs {" );
    _indent++;
  });
  registerAstPostCB<InterfaceInputs>([=](auto ii_node){
    _indent--;
    emitLine("}" );
  });
  registerAstPreCB<InterfaceInput>([=](auto ii_node){
    emitBeginLine("");
  });
  registerAstPostCB<InterfaceInput>([=](auto ii_node){
    emitEndLine(";");
  });
  registerAstPreCB<InterfaceOutputs>([=](auto ii_node){
    emitLine("outputs {" );
    _indent++;
  });
  registerAstPostCB<InterfaceOutputs>([=](auto ii_node){
    _indent--;
    emitLine("}" );
  });
  registerAstPreCB<InterfaceOutput>([=](auto ii_node){
    emitBeginLine("");
  });
  registerAstPostCB<InterfaceOutput>([=](auto ii_node){
    emitEndLine(";");
  });
  registerAstPreCB<InterfaceLayout>([=](auto il_node){
    emitContinueLine("layout(");
  });
  registerAstPostCB<InterfaceLayout>([=](auto il_node){
    emitContinueLine(") ");
  });
  registerAstPostChildCB<InterfaceLayout>([=](auto il_node, astnode_ptr_t child){
    size_t num_children = il_node->_children.size();
    size_t index = 0;
    for( auto item : il_node->_children ){
      if( item == child ){
        break;
      }
      index++;
    }
    if(index&1){
      if( index != (num_children-1) )
        emitContinueLine(", ");
    }
    else{
      emitContinueLine("=");
    }
  });
  registerAstPreCB<InterfaceStorages>([=](auto is_node){
    emitLine("storage {" );
    _indent++;
  });
  registerAstPostCB<InterfaceStorages>([=](auto is_node){
    _indent--;
    emitLine("}" );
  });
  registerAstPreCB<InterfaceStorage>([=](auto is_node){
    emitBeginLine("" );
    _indent++;
  });
  registerAstPostCB<InterfaceStorage>([=](auto is_node){
    emitEndLine("" );
  });
  registerAstPostChildCB<InterfaceStorage>([=](auto is_node, astnode_ptr_t child){
    OrkAssert(is_node->_children.size()==4);
    if (child == is_node->_children[1]) { // storage_class
      emitEndLine(" {");
    }
    else if (child == is_node->_children[2]) { // storage_decls
      _indent--;
      emitBeginLine("} ");
    }
    else if (child == is_node->_children[3]) { // storage_name
      emitContinueLine(";");
    }
  });
  
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<FragmentInterface>([=](auto frg_if) { named_precb( frg_if, "fragment_interface" ); });
  registerAstPostCB<FragmentInterface>([=](auto frg_if) { named_postcb(frg_if); });
  registerAstPreChildCB<FragmentInterface>(named_item_pre_child_cb);
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<VertexInterface>([=](auto vtx_if) { named_precb( vtx_if, "vertex_interface" ); });
  registerAstPostCB<VertexInterface>([=](auto vtx_if) { named_postcb(vtx_if); });
  registerAstPreChildCB<VertexInterface>(named_item_pre_child_cb);
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<GeometryInterface>([=](auto geo_if) { named_precb( geo_if, "geometry_interface" ); });
  registerAstPostCB<GeometryInterface>([=](auto geo_if) { named_postcb(geo_if); });
  registerAstPreChildCB<GeometryInterface>(named_item_pre_child_cb);
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<ComputeInterface>([=](auto com_if) { named_precb( com_if, "compute_interface" ); });
  registerAstPostCB<ComputeInterface>([=](auto com_if) { named_postcb(com_if); });
  registerAstPreChildCB<ComputeInterface>(named_item_pre_child_cb);
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<VertexShader>([=](auto vtx_sh) { named_precb( vtx_sh, "vertex_shader" ); });
  registerAstPreCB<FragmentShader>([=](auto frg_sh) { named_precb( frg_sh, "fragment_shader" ); });
  registerAstPreCB<GeometryShader>([=](auto geo_sh) { named_precb( geo_sh, "geometry_shader" ); });
  registerAstPreCB<ComputeShader>([=](auto com_sh) { named_precb( com_sh, "compute_shader" ); });
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<Technique>([=](auto tek) { 
    auto the_name = tek->template typedValueForKey<std::string>("object_name").value();
    emitLine("technique %s {", the_name.c_str() );
    _indent++;
  });
  registerAstPostCB<Technique>([=](auto tek) { 
    _indent--;
    emitLine( "}");
  });
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<Pass>([=](auto pass) { 
    auto the_name = pass->template typedValueForKey<std::string>("object_name").value();
    emitLine("pass %s {", the_name.c_str() );
    _indent++;
  });
  registerAstPostCB<Pass>([=](auto pass) { 
    _indent--;
    emitLine( "}");
  });
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<FxConfigRef>([=](auto ref) { 
    emitLine( "FxConfigRef : " );
  });
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<VertexShaderRef>([=](auto ref) { 
    emitBeginLine( "VertexShaderRef : " );
  });
  registerAstPostCB<VertexShaderRef>([=](auto ref) { 
    emitEndLine( ";" );
  });
  registerAstPreCB<FragmentShaderRef>([=](auto ref) { 
    emitBeginLine( "FragmentShaderRef : " );
  });
  registerAstPostCB<FragmentShaderRef>([=](auto ref) { 
    emitEndLine( ";" );
  });
  registerAstPreCB<GeometryShaderRef>([=](auto ref) { 
    emitBeginLine( "GeometryShaderRef : " );
  });
  registerAstPostCB<GeometryShaderRef>([=](auto ref) { 
    emitEndLine( ";" );
  });
  registerAstPreCB<StateBlockRef>([=](auto ref) { 
    emitBeginLine( "StateBlockRef : " );
  });
  registerAstPostCB<StateBlockRef>([=](auto ref) { 
    emitEndLine( ";" );
  });
  /////////////////////////////////////////////////////////////////////
  // functions
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<FunctionDef2>([=](auto fd2) {
    auto fn_name          = fd2->template typedValueForKey<std::string>("function_name").value();
    auto dt_node          = fd2->template childAs<DataType>(0);
    dt_node->_should_emit = false;
    auto return_type      = dt_node->template typedValueForKey<std::string>("data_type").value();
    emitBeginLine("%s %s", return_type.c_str(), fn_name.c_str());
  });
  registerAstPreCB<DeclArgumentList>([=](auto da_node) { emitContinueLine("("); });
  registerAstPostCB<DeclArgumentList>([=](auto da_node) { emitContinueLine(")"); });
  registerAstPostChildCB<DeclArgumentList>([=](auto da_node, astnode_ptr_t child) { //
    size_t num_children = da_node->_children.size();
    if (child != da_node->_children.back()) {
      emitContinueLine(", ");
    }
  });
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<SemaConstructorInvokation>([=](auto fd2) {
    auto fn_name = fd2->template typedValueForKey<std::string>("data_type").value();
    emitContinueLine("%s", fn_name.c_str());
  });
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<SemaConstructorArguments>([=](auto sca_node) { emitContinueLine("("); });
  registerAstPostCB<SemaConstructorArguments>([=](auto sca_node) { emitContinueLine(")"); });
  registerAstPostChildCB<SemaConstructorArguments>([=](auto sca_node, astnode_ptr_t child) {
    size_t num_children = sca_node->_children.size();
    if (num_children > 1) {
      if (child != sca_node->_children.back()) {
        emitContinueLine(", ");
      }
    }
  });
  /////////////////////////////////////////////////////////////////////
  // statements
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<CompoundStatement>([=](auto expstmt) {
    emitEndLine("{");
    _indent++;
  });
  registerAstPostCB<CompoundStatement>([=](auto expstmt) {
    _indent--;
    emitLine("}");
  });
  registerAstPreChildCB<CompoundStatement>([=](auto compstmt, astnode_ptr_t child) { emitBeginLine(""); });
  registerAstPostChildCB<CompoundStatement>([=](auto compstmt, astnode_ptr_t child) { emitEndLine(""); });
  registerAstPreCB<ExpressionStatement>([=](auto expstmt) {
    // emitBeginLine( "" );
  });
  registerAstPostCB<ExpressionStatement>([=](auto expstmt) { emitContinueLine("; "); });
  registerAstPreCB<ForStatement>([=](auto forstmt) { emitContinueLine("for("); });
  registerAstPostChildCB<ForStatement>([=](auto forstmt, astnode_ptr_t child) {
    size_t num_children = forstmt->_children.size();
    if (child == forstmt->_children[num_children - 2]) {
      emitContinueLine(") ");
    }
  });
  registerAstPreCB<IfStatement>([=](auto ifstmt) { emitContinueLine("if("); });
  registerAstPostChildCB<IfStatement>([=](auto ifstmt, astnode_ptr_t child) {
    OrkAssert(ifstmt->_children.size() >= 2);
    if (child == ifstmt->_children[0]) {
      emitContinueLine(") ");
    }
  });
  registerAstPreCB<ElseStatementBody>([=](auto ifstmt) { emitBeginLine("else "); });
  registerAstPreCB<ReturnStatement>([=](auto retstmt) { emitContinueLine("return "); });
  registerAstPostCB<ReturnStatement>([=](auto retstmt) { emitContinueLine(";"); });
  registerAstPreCB<DiscardStatement>([=](auto disc) { emitContinueLine("discard "); });
  registerAstPostCB<DiscardStatement>([=](auto disc) { emitContinueLine(";"); });
  
  /////////////////////////////////////////////////////////////////////
  // expressions
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<ParensExpression>([=](auto mo_node) { emitContinueLine("("); });
  registerAstPostCB<ParensExpression>([=](auto mo_node) { emitContinueLine(")"); });
  registerAstPostChildCB<AssignmentExpression1>([=](auto ae_node, astnode_ptr_t child) {
    if (child == ae_node->_children[0]) {
      emitContinueLine(" ");
    }
  });
  registerAstPostChildCB<ExpressionList>([=](auto exprlist, astnode_ptr_t child) {
    size_t num_children = exprlist->_children.size();
    if (num_children > 1) {
      if (child != exprlist->_children.back()) {
        emitContinueLine(", ");
      }
    }
  });
  registerAstPreChildCB<IdentifierCall>([=](auto idcall, astnode_ptr_t child) {
    size_t num_children = idcall->_children.size();
    OrkAssert(num_children == 2);
    auto ident  = idcall->template childAs<SemaIdentifier>(0);
    auto parens = idcall->template childAs<ParensExpression>(1);
    auto ident_name = ident->template typedValueForKey<std::string>("identifier_name").value();
    //emitContinueLine("%s", ident_name.c_str());
  });
  registerAstPreCB<SemaIdentifier>([=](auto ident) {
    auto ident_name = ident->template typedValueForKey<std::string>("identifier_name").value();
    emitContinueLine("%s", ident_name.c_str());
  });
  registerAstPreCB<TernaryExpression>([=](auto ternary) {
    emitContinueLine(" ? ");
  });
  registerAstPostChildCB<TernaryExpression>([=](auto ternary, astnode_ptr_t child) {
    size_t num_children = ternary->_children.size();
    OrkAssert(num_children == 3);
    if( child == ternary->_children[0] ){
      emitContinueLine(" : ");
    }
  });
  /////////////////////////////////////////////////////////////////////
  // operators
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<AssignmentOperator>([=](auto ao_node) {
    auto oper = ao_node->template typedValueForKey<std::string>("operator").value();
    emitContinueLine(" %s ", oper.c_str());
  });
  registerAstPreCB<AdditiveOperator>([=](auto ao_node) {
    auto oper = ao_node->template typedValueForKey<std::string>("operator").value();
    emitContinueLine(" %s ", oper.c_str());
  });
  registerAstPreCB<MultiplicativeOperator>([=](auto mo_node) {
    auto oper = mo_node->template typedValueForKey<std::string>("operator").value();
    emitContinueLine(" %s ", oper.c_str());
  });
  registerAstPreCB<RelationalOperator>([=](auto ro_node) {
    auto oper = ro_node->template typedValueForKey<std::string>("operator").value();
    emitContinueLine(" %s ", oper.c_str());
  });
  registerAstPreCB<EqualityOperator>([=](auto ro_node) {
    auto oper = ro_node->template typedValueForKey<std::string>("operator").value();
    emitContinueLine(" %s ", oper.c_str());
  });
  registerAstPreCB<ShiftOperator>([=](auto ro_node) {
    auto oper = ro_node->template typedValueForKey<std::string>("operator").value();
    emitContinueLine(" %s ", oper.c_str());
  });
  registerAstPreCB<MemberAccessOperator>([=](auto mo_node) {
    emitContinueLine(".");
  });
  registerAstPreCB<ArrayIndexOperator>([=](auto mo_node) {
    emitContinueLine("[");
  });
  registerAstPostCB<ArrayIndexOperator>([=](auto mo_node) {
    emitContinueLine("]");
  });
  registerAstPreCB<IncrementOperator>([=](auto inc_node) { emitContinueLine("++"); });
  registerAstPreCB<DecrementOperator>([=](auto inc_node) { emitContinueLine("--"); });
  registerAstPostChildCB<AndExpression>([=](auto andexp, astnode_ptr_t child) {
    size_t num_children = andexp->_children.size();
    if (num_children > 1) {
      if (child != andexp->_children.back()) {
        emitContinueLine(" & ");
      }
    }
  });
  registerAstPostChildCB<InclusiveOrExpression>([=](auto or_node, astnode_ptr_t child) {
    size_t num_children = or_node->_children.size();
    if (num_children > 1) {
      if (child != or_node->_children.back()) {
        emitContinueLine(" | ");
      }
    }
  });
  registerAstPostChildCB<ExclusiveOrExpression>([=](auto xor_node, astnode_ptr_t child) {
    size_t num_children = xor_node->_children.size();
    if (num_children > 1) {
      if (child != xor_node->_children.back()) {
        emitContinueLine(" ^ ");
      }
    }
  });
  registerAstPostChildCB<LogicalAndExpression>([=](auto and_node, astnode_ptr_t child) {
    size_t num_children = and_node->_children.size();
    if (num_children > 1) {
      if (child != and_node->_children.back()) {
        emitContinueLine(" && ");
      }
    }
  });
  registerAstPostChildCB<LogicalOrExpression>([=](auto or_node, astnode_ptr_t child) {
    size_t num_children = or_node->_children.size();
    if (num_children > 1) {
      if (child != or_node->_children.back()) {
        emitContinueLine(" || ");
      }
    }
  });
  /////////////////////////////////////////////////////////////////////
  // misc prims
  /////////////////////////////////////////////////////////////////////
  registerAstPreCB<DataType>([=](auto dt_node) {
    auto dt_type = dt_node->template typedValueForKey<std::string>("data_type").value();
    emitContinueLine("%s", dt_type.c_str());
  });
  registerAstPreCB<PrimaryIdentifier>([=](auto pid_node) {
    auto ident = pid_node->template typedValueForKey<std::string>("identifier_name").value();
    emitContinueLine("%s", ident.c_str());
  });
  registerAstPostChildCB<TypedIdentifier>([=](auto tid_node, astnode_ptr_t child) {
    emitContinueLine(" ");
  });
  registerAstPreCB<SemaIntegerLiteral>([=](auto int_node) {
    auto literal_value = int_node->template typedValueForKey<std::string>("literal_value").value();
    emitContinueLine("%s", literal_value.c_str());
  });
  registerAstPreCB<SemaFloatLiteral>([=](auto float_node) {
    auto literal_value = float_node->template typedValueForKey<std::string>("literal_value").value();
    emitContinueLine("%s", literal_value.c_str());
  });
  registerAstPreCB<InsertLine>([=](auto line_node) {
    auto line_text = line_node->template typedValueForKey<std::string>("line_text").value();
     emitLine(line_text.c_str());
  });
}

/////////////////////////////////////////////////////////////////////////////////////////////////

std::string toGLFX1(astnode_ptr_t top) {
  auto backend = std::make_shared<GLFX1Backend>();
  backend->generate(top);
  return backend->_outstr;
}


/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::shadlang
