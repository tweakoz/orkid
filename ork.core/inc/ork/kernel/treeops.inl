#pragma once

#include <memory>
#include <ork/util/logger.h>

namespace ork::tree {

///////////////////////////////////////////////////////////////////////////////////////////////
// tree operations for mutable node types
///////////////////////////////////////////////////////////////////////////////////////////////

template <typename treenode_type> struct Ops {

  using node_t        = treenode_type;
  using node_rawptr_t = node_t*;
  using node_ptr_t    = std::shared_ptr<node_t>;

  inline Ops(node_rawptr_t root)
      : _root(root) {
  }

  /////////////////////////////////////////////////////////////////////////////

  template <typename T> //
  std::shared_ptr<T> childAs(size_t index) {
    OrkAssert(index < _root->_children.size());
    auto ch  = _root->_children[index];
    auto ret = std::dynamic_pointer_cast<T>(ch);
    return ret;
  }

  /////////////////////////////////////////////////////////////////////////////

  static inline void replaceInParent(
      node_ptr_t oldnode,   //
      node_ptr_t newnode) { //

    auto parent = oldnode->_parent;
    if (parent) {
      auto& children = parent->_children;
      auto it        = std::find(children.begin(), children.end(), oldnode);
      if (it != children.end()) {
        *it              = newnode;
        newnode->_parent = parent;
      } else {
        logerrchannel()->log("replaceInParent failed to find child to replace");
      }
    }
  }

  /////////////////////////////////////////////////////////////////////////////

  static inline void removeFromParent(node_ptr_t oldnode) {
    auto parent = oldnode->_parent;
    if (parent) {
      auto& children = parent->_children;
      auto it        = std::find(children.begin(), children.end(), oldnode);
      if (it != children.end()) {
        children.erase(it);
      } else {
        logerrchannel()->log("removeFromParent failed to find child to remove");
      }
    }
  }


  node_rawptr_t _root;
};

///////////////////////////////////////////////////////////////////////////////////////////////
// tree operations for immutable node types
///////////////////////////////////////////////////////////////////////////////////////////////

template <typename treenode_type> struct ConstOps {

  using node_t          = treenode_type;
  using rootref_t       = const node_t*;
  using node_ptr_t      = std::shared_ptr<node_t>;
  using node_constptr_t = std::shared_ptr<const node_t>;

  /////////////////////////////////////////////////////////////////////////////

  inline ConstOps(rootref_t root)
      : _root(root) {
  }

  using walk_fn_t = std::function<bool(rootref_t)>;
  using visit_fn_t = std::function<void(int, rootref_t)>;

  /////////////////////////////////////////////////////////////////////////////

  inline bool walkDown(walk_fn_t walk_fn) const {
    bool bret = walk_fn(_root);
    if (bret) {
        for (auto c : _root->_children) {
        bret = c->walkDown(walk_fn);
        if (bret == false)
            break;
        }
    }
    return bret;
  }

  /////////////////////////////////////////////////////////////////////////////

  inline void visit(int level, visit_fn_t vfn) const {
    vfn(level, _root);
    for (auto c : _root->_children) {
        c->visit(level + 1, vfn);
    }
  }

  /////////////////////////////////////////////////////////////////////////////

  template <typename child_t> //
  node_ptr_t findFirstChildOfType() const {
    for (auto ch : _root->_children) {
      auto typed = std::dynamic_pointer_cast<child_t>(ch);
      if (typed)
        return typed;
      else {
        auto ret = ConstOps(ch.get()).findFirstChildOfType<child_t>();
        if (ret)
          return ret;
      }
    }
    return nullptr;
  }

  /////////////////////////////////////////////////////////////////////////////

  static inline bool related(node_ptr_t start, node_ptr_t end) {

    using check_t = std::function<bool(node_ptr_t start, node_ptr_t end)>;

    check_t chk = [&](node_ptr_t start, node_ptr_t end) -> bool {
      if (start == end) {
        return true;
      } else if (start->_parent) {
        return chk(start->_parent, end);
      }
      return false;
    };

    bool rval = chk(start, end);
    if (not rval) {
      rval = chk(end, start);
    }
    return rval;
  }

  //////////////////////////////////////////////////////////////////////
  // traverse up from start to end and return the distance
  //////////////////////////////////////////////////////////////////////

  static inline size_t branchDistance(node_ptr_t start, node_ptr_t end) {

    if (related(start, end)) {
      size_t rval = 0;

      using check_t = std::function<bool(node_ptr_t start, node_ptr_t end)>;

      check_t chk = [&](node_ptr_t start, node_ptr_t end) -> bool {
        if (start == end) {
          return true;
        } else if (start->_parent) {
          rval++;
          return chk(start->_parent, end);
        }
        return false;
      };

      bool check_ok = chk(start, end);
      OrkAssert(check_ok);
      return rval;
    } else
      return -1;
  }
  /////////////////////////////////////////////////////////////////////////////

  rootref_t _root;
};
} // namespace ork::tree