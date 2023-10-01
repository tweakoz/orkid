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
namespace ork::lev2::shadlang::SHAST {
/////////////////////////////////////////////////////////////////////////////////////////////////

std::string toASTstring(astnode_ptr_t node) {
  std::string rval;
  _dumpAstTreeVisitor(node, 0, rval);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void dumpAstNode(astnode_ptr_t node){
  auto ast_str = toASTstring(node);
  printf( "AST<%s>\n", ast_str.c_str());
}

///////////////////////////////////////////////////////////////////////////////

AstNode::AstNode(){
  static int gid = 0;
  _nodeID = gid++;
  _uservars = std::make_shared<varmap::VarMap>();
}

///////////////////////////////////////////////////////////////////////////////

bool AstNode::hasKey(const key_t& key) const {
  return _uservars->hasKey(key);
}

///////////////////////////////////////////////////////////////////////////////

bool AstNode::walkDownAST( //
    astnode_ptr_t node,    //
    walk_visitor_fn_t visitor) {
  bool down = visitor(node);
  if (down) {
    for (auto c : node->_children) {
      bool cont = walkDownAST(c, visitor);
      if( not cont ){
        return false;
      }
    }
  }
  return down;
}

void AstNode::visitChildren( //
    astnode_ptr_t node,    //
    visitor_fn_t visitor) {
  for( auto c : node->_children ){
    visitor(c);
  }
}

///////////////////////////////////////////////////////////////////////////////

void AstNode::visitNode(      //
    astnode_ptr_t node,      //
    visitor_ptr_t visitor) { //

  if (visitor->_on_pre) {
    visitor->_on_pre(node);
  }
  visitor->_nodestack.push(node);
  for (auto c : node->_children) {
    visitNode(c, visitor);
  }
  visitor->_nodestack.pop();
  if (visitor->_on_post) {
    visitor->_on_post(node);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

InheritanceTracker::InheritanceTracker(transunit_ptr_t transu)
  : _translation_unit(transu) {

}
/////////////////////////////////////////////////////////////////////////////////////////////////
void InheritanceTracker::_processNode(astnode_ptr_t node) {

  //////////////////////////////////////////////////////////////////////
  if (auto as_lib = std::dynamic_pointer_cast<SemaInheritLibrary>(node)) {
    auto INHID = as_lib->typedValueForKey<std::string>("inherit_id").value();
    auto it = _set_inherited_libs.find(INHID);
    if( it == _set_inherited_libs.end() ){
      _set_inherited_libs.insert(INHID);
      auto LIB   = _translation_unit->find<LibraryBlock>(INHID);
      OrkAssert(LIB);
      _inherited_libs.push_back(LIB);
      if(_onInheritLibrary){
        _onInheritLibrary(INHID,LIB);
      }
    }
  }
  //////////////////////////////////////////////////////////////////////
  else if (auto as_vif = std::dynamic_pointer_cast<SemaInheritVertexInterface>(node)) {
    auto INHID = as_vif->typedValueForKey<std::string>("inherit_id").value();
    auto it = _set_inherited_interfaces.find(INHID);
    if( it == _set_inherited_interfaces.end() ){
      _set_inherited_interfaces.insert(INHID);
      auto IFACE = _translation_unit->find<VertexInterface>(INHID);
      OrkAssert(IFACE);
      _inherited_ifaces.push_back(IFACE);
      if(_onInheritInterface)
        _onInheritInterface(INHID,IFACE);
    }
  }
  //////////////////////////////////////////////////////////////////////
  else if (auto as_fif = std::dynamic_pointer_cast<SemaInheritFragmentInterface>(node)) {
    auto INHID = as_fif->typedValueForKey<std::string>("inherit_id").value();
    auto it = _set_inherited_interfaces.find(INHID);
    if( it == _set_inherited_interfaces.end() ){
      _set_inherited_interfaces.insert(INHID);
      auto IFACE = _translation_unit->find<FragmentInterface>(INHID);
      OrkAssert(IFACE);
      _inherited_ifaces.push_back(IFACE);
      if(_onInheritInterface)
        _onInheritInterface(INHID,IFACE);
    }
  }
  //////////////////////////////////////////////////////////////////////
  else if (auto as_gif = std::dynamic_pointer_cast<SemaInheritGeometryInterface>(node)) {
    auto INHID = as_gif->typedValueForKey<std::string>("inherit_id").value();
    auto it = _set_inherited_interfaces.find(INHID);
    if( it == _set_inherited_interfaces.end() ){
      _set_inherited_interfaces.insert(INHID);
      auto IFACE = _translation_unit->find<GeometryInterface>(INHID);
      OrkAssert(IFACE);
      _inherited_ifaces.push_back(IFACE);
      if(_onInheritInterface)
        _onInheritInterface(INHID,IFACE);
    }
  }
  //////////////////////////////////////////////////////////////////////
  else if (auto as_cif = std::dynamic_pointer_cast<SemaInheritComputeInterface>(node)) {
    auto INHID = as_cif->typedValueForKey<std::string>("inherit_id").value();
    auto it = _set_inherited_interfaces.find(INHID);
    if( it == _set_inherited_interfaces.end() ){
      _set_inherited_interfaces.insert(INHID);
      auto IFACE = _translation_unit->find<ComputeInterface>(INHID);
      OrkAssert(IFACE);
      _inherited_ifaces.push_back(IFACE);
      if(_onInheritInterface)
        _onInheritInterface(INHID,IFACE);
    }
  }
  //////////////////////////////////////////////////////////////////////
  else if (auto as_uset = std::dynamic_pointer_cast<SemaInheritUniformSet>(node)) {
    auto INHID    = as_uset->typedValueForKey<std::string>("inherit_id").value();
    auto ast_uset = _translation_unit->find<SHAST::UniformSet>(INHID);
    OrkAssert(ast_uset);
    auto it = _set_inherited_unisets.find(INHID);
    if( it == _set_inherited_unisets.end() ){
      _set_inherited_unisets.insert(INHID);
      _inherited_usets.push_back(ast_uset);
      if( _onInheritUniformSet ){
        _onInheritUniformSet(INHID, ast_uset);
      }
    }
  }
  //////////////////////////////////////////////////////////////////////
  else if (auto as_ublk = std::dynamic_pointer_cast<SemaInheritUniformBlk>(node)) {
    auto INHID    = as_ublk->typedValueForKey<std::string>("inherit_id").value();
    auto ast_ublk = _translation_unit->find<SHAST::UniformBlk>(INHID);
    auto it = _set_inherited_uniblks.find(INHID);
    if( it == _set_inherited_uniblks.end() ){
      _set_inherited_uniblks.insert(INHID);
      _inherited_blks.push_back(ast_ublk);
      if( _onInheritUniformBlk ){
        _onInheritUniformBlk(INHID, ast_ublk);
      }
    }
  }
  //////////////////////////////////////////////////////////////////////
  else if (auto as_sb = std::dynamic_pointer_cast<SemaInheritStateBlock>(node)) {
  }
  //////////////////////////////////////////////////////////////////////
  else if (auto as_ext = std::dynamic_pointer_cast<SemaInheritExtension>(node)) {
    OrkAssert(as_ext);
    auto ext_name = as_ext->typedValueForKey<std::string>("extension_name").value();
    auto it = _set_inherited_extensions.find(ext_name);
    if( it == _set_inherited_extensions.end() ){
      _set_inherited_extensions.insert(ext_name);
      _inherited_exts.push_back(ext_name);
      if( _onInheritExtension ){
        _onInheritExtension(ext_name,as_ext);
      }
    }
  }
  else{
    OrkAssert(false);
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void InheritanceTracker::fetchInheritances(astnode_ptr_t parent_node) {

  _stack_depth++;
  for (auto c : parent_node->_children) {
    //////////////////////////////////////////////////////////////////////
    if (auto as_lib = std::dynamic_pointer_cast<SemaInheritLibrary>(c)) {
      auto INHID = as_lib->typedValueForKey<std::string>("inherit_id").value();
      auto LIB   = _translation_unit->find<LibraryBlock>(INHID);
      OrkAssert(LIB);
      fetchInheritances(LIB);
      _processNode(as_lib);
    }
    //////////////////////////////////////////////////////////////////////
    else if (auto as_vif = std::dynamic_pointer_cast<SemaInheritVertexInterface>(c)) {
      auto INHID = as_vif->typedValueForKey<std::string>("inherit_id").value();
      auto IFACE = _translation_unit->find<VertexInterface>(INHID);
      OrkAssert(IFACE);
      fetchInheritances(IFACE);
      _processNode(as_vif);
    }
    //////////////////////////////////////////////////////////////////////
    else if (auto as_fif = std::dynamic_pointer_cast<SemaInheritFragmentInterface>(c)) {
      auto INHID = as_fif->typedValueForKey<std::string>("inherit_id").value();
      auto IFACE = _translation_unit->find<FragmentInterface>(INHID);
      OrkAssert(IFACE);
      fetchInheritances(IFACE);
      _processNode(as_fif);
    }
    //////////////////////////////////////////////////////////////////////
    else if (auto as_gif = std::dynamic_pointer_cast<SemaInheritGeometryInterface>(c)) {
      auto INHID = as_gif->typedValueForKey<std::string>("inherit_id").value();
      auto IFACE = _translation_unit->find<GeometryInterface>(INHID);
      OrkAssert(IFACE);
      fetchInheritances(IFACE);
      _processNode(as_gif);
    }
    //////////////////////////////////////////////////////////////////////
    else if (auto as_cif = std::dynamic_pointer_cast<SemaInheritComputeInterface>(c)) {
      auto INHID = as_cif->typedValueForKey<std::string>("inherit_id").value();
      auto IFACE = _translation_unit->find<ComputeInterface>(INHID);
      OrkAssert(IFACE);
      fetchInheritances(IFACE);
      _processNode(as_cif);
    }
    //////////////////////////////////////////////////////////////////////
    else if (auto as_uset = std::dynamic_pointer_cast<SemaInheritUniformSet>(c)) {
      auto INHID    = as_uset->typedValueForKey<std::string>("inherit_id").value();
      auto ast_uset = _translation_unit->find<SHAST::UniformSet>(INHID);
      OrkAssert(ast_uset);
      fetchInheritances(ast_uset);
      _processNode(as_uset);
    }
    //////////////////////////////////////////////////////////////////////
    else if (auto as_ublk = std::dynamic_pointer_cast<SemaInheritUniformBlk>(c)) {
      auto INHID    = as_ublk->typedValueForKey<std::string>("inherit_id").value();
      auto ast_ublk = _translation_unit->find<SHAST::UniformBlk>(INHID);
      OrkAssert(ast_ublk);
      fetchInheritances(ast_ublk);
      _processNode(as_ublk);
    }
    //////////////////////////////////////////////////////////////////////
    else if (auto as_sb = std::dynamic_pointer_cast<SemaInheritStateBlock>(c)) {
    }
    //////////////////////////////////////////////////////////////////////
    else if (auto as_ext = std::dynamic_pointer_cast<SemaInheritExtension>(c)) {
      OrkAssert(as_ext);
      _processNode(as_ext); // non-recursive
    }
  }
  _stack_depth--;
}


} // namespace ork::lev2::shadlang::SHAST

#endif
