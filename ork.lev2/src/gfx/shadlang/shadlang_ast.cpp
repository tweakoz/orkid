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

} // namespace ork::lev2::shadlang::SHAST

#endif
