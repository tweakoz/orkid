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
namespace ork::lev2::shadlang {
/////////////////////////////////////////////////////////////////////////////////////////////////
using namespace SHAST;

///////////////////////////////////////////////////////////////////////////////////////////
// prune expression chain nodes
///////////////////////////////////////////////////////////////////////////////////////////

void _pruneExpressionChainNodes(impl::ShadLangParser* slp, astnode_ptr_t top) {
  std::set<astnode_ptr_t> nodes_to_reduce;
  auto collect_nodes     = std::make_shared<Visitor>();
  collect_nodes->_on_pre = [&](astnode_ptr_t node) {
    if (auto node_as_expression = std::dynamic_pointer_cast<Expression>(node)) {
      if (node_as_expression->_children.size() == 1) {
        auto parent = node_as_expression->_parent;
        if (auto parent_as_expression = std::dynamic_pointer_cast<Expression>(parent)) {
          nodes_to_reduce.insert(node_as_expression);
        }
      }
    }
  };
  slp->visitAST(top, collect_nodes);
  slp->reduceAST(nodes_to_reduce);
}
//////////////////////////////////////////////////
// prune expression base classes
//////////////////////////////////////////////////

void _pruneExpressionBaseNodes(impl::ShadLangParser* slp, astnode_ptr_t top) {
  std::set<astnode_ptr_t> nodes_to_reduce;
  auto collect_nodes     = std::make_shared<Visitor>();
  collect_nodes->_on_pre = [&](astnode_ptr_t node) {
    if (node->_name == "Expression") {
      nodes_to_reduce.insert(node);
    }
  };
  slp->visitAST(top, collect_nodes);
  slp->reduceAST(nodes_to_reduce);
}

//////////////////////////////////////////////////
// prune statement base classes
//////////////////////////////////////////////////

void _pruneStatementBaseNodes(impl::ShadLangParser* slp, astnode_ptr_t top) {
  std::set<astnode_ptr_t> nodes_to_reduce;
  auto collect_nodes     = std::make_shared<Visitor>();
  collect_nodes->_on_pre = [&](astnode_ptr_t node) {
    if (node->_name == "Statement") {
      nodes_to_reduce.insert(node);
    }
  };
  slp->visitAST(top, collect_nodes);
  slp->reduceAST(nodes_to_reduce);
}

//////////////////////////////////////////////////
// prune DataType nodes 
//  that are children of TypedIdentifier or DataTypeWithUserTypes
//////////////////////////////////////////////////

void _pruneDataTypeNodes(impl::ShadLangParser* slp, astnode_ptr_t top) {
  std::set<astnode_ptr_t> nodes_to_reduce;
  auto collect_nodes     = std::make_shared<Visitor>();
  collect_nodes->_on_pre = [&](astnode_ptr_t node) {
    if (node->_name == "DataTypeWithUserTypes") {
      if (node->_children.size() == 1) {
        auto child = node->_children[0];
        OrkAssert(child->_name == "DataType");
        nodes_to_reduce.insert(child);
      }
    }
  };
  slp->visitAST(top, collect_nodes);
  slp->reduceAST(nodes_to_reduce);
  nodes_to_reduce.clear();
  collect_nodes->_on_pre = [&](astnode_ptr_t node) {
    if (node->_name == "TypedIdentifier") {
      if (node->_children.size() == 1) {
        auto child = node->_children[0];
        OrkAssert(child->_name == "DataType");
        nodes_to_reduce.insert(child);
      }
    }
  };
  slp->visitAST(top, collect_nodes);
  slp->reduceAST(nodes_to_reduce);
}

//////////////////////////////////////////////////
// prune DataDeclaration (that have an ArrayDeclaration child)
//////////////////////////////////////////////////

void _pruneDataDeclarationNodes(impl::ShadLangParser* slp, astnode_ptr_t top) {
  std::set<astnode_ptr_t> nodes_to_reduce;
  auto collect_nodes     = std::make_shared<Visitor>();
  collect_nodes->_on_pre = [&](astnode_ptr_t node) {
    if (node->_name == "DataDeclaration") {
      if (node->_children.size() == 1) {
        auto child = node->_children[0];
        if (child->_name == "ArrayDeclaration") {
          nodes_to_reduce.insert(node);
        }
      }
    }
  };
  slp->visitAST(top, collect_nodes);
  slp->reduceAST(nodes_to_reduce);
}

void impl::ShadLangParser::reduceAST(std::set<astnode_ptr_t> nodes_to_reduce) {
  while (nodes_to_reduce.size()) {
    auto it   = nodes_to_reduce.begin();
    auto node = *it;
    OrkAssert(node);
    auto parent = node->_parent;
    ////////////////////////////////////////////
    if (node->_children.size() == 1) {
      auto new_child     = node->_children[0];
      new_child->_parent = parent;
      for (size_t index = 0; index < parent->_children.size(); index++) {
        auto prev_child = parent->_children[index];
        if (prev_child == node) {
          parent->_children[index] = new_child;
        }
      }
    }
    ////////////////////////////////////////////
    else if (node->_children.size() == 0) {
      size_t do_index = 9999999;
      for (size_t index = 0; index < parent->_children.size(); index++) {
        auto prev_child = parent->_children[index];
        if (prev_child == node) {
          do_index = index;
          break;
        }
      }
      if (do_index != 9999999) {
        parent->_children.erase(parent->_children.begin() + do_index);
      }
    }
    ////////////////////////////////////////////
    nodes_to_reduce.erase(it);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////

void impl::ShadLangParser::pruneAST(astnode_ptr_t top) {
  _pruneExpressionChainNodes(this,top);
  _pruneExpressionBaseNodes(this,top);
  _pruneStatementBaseNodes(this,top);
  _pruneDataTypeNodes(this,top);
  _pruneDataDeclarationNodes(this,top);
}


/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::shadlang
/////////////////////////////////////////////////////////////////////////////////////////////////

#endif
