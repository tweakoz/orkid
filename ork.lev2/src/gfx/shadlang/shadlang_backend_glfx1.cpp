////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/shadlang.h>

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

  using base_cb_t = std::function<void(astnode_ptr_t)>;

  std::string _outstr;
  std::stack<astnode_ptr_t> _node_stack;
  std::map<std::string, base_cb_t> _precb_map;
  std::map<std::string, base_cb_t> _postcb_map;

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
    precb(node);
  }

  ///////////////////////////////////////////////
  _node_stack.push(node);
  for (auto c : node->_children) {
    _visit(c);
  }
  _node_stack.pop();
  ///////////////////////////////////////////////
  // post-visit
  ///////////////////////////////////////////////

  auto it_post = _postcb_map.find(node->_type_name);
  if( it_post != _postcb_map.end() ){
    auto postcb = it_post->second;
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
  registerAstPreCB<AssignmentExpression2>([](auto tu){
    //OrkAssert(false);
  });
}

/////////////////////////////////////////////////////////////////////////////////////////////////

std::string toGLFX1(translationunit_ptr_t top) {
  auto backend = std::make_shared<GLFX1Backend>();
  backend->generate(top);
  return backend->_outstr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::shadlang
