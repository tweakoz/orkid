////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/rtti/RTTI.h>
#include <ork/object/Object.h>
#include <ork/math/TransformNode.h>

#include "types.h"

namespace ork::ecs {

///////////////////////////////////////////////////////////////////////////////

struct SceneObjectClass : public object::ObjectClass {

  DeclareExplicitX(SceneObjectClass, object::ObjectClass, rtti::NamePolicy, object::ObjectCategory);

public:
  SceneObjectClass(const rtti::RTTIData& data)
      : object::ObjectClass(data) {
    SetPreferredName("SceneObject");
  }
  ork::ConstString GetPreferredName() const {
    return mPreferredName;
  }
  void SetPreferredName(PieceString ps) {
    mPreferredName = ps;
  }

protected:
  ArrayString<64> mPreferredName;
};

///////////////////////////////////////////////////////////////////////////////

struct SceneObject : public ork::Object {

  DeclareExplicitX(SceneObject, ork::Object, ork::rtti::AbstractPolicy, ecs::SceneObjectClass);

public:
  void SetName(PoolString name);
  PoolString GetName() const {
    return mName;
  }
  void SetName(const char* name);

protected:
  SceneObject();

private:
  PoolString mName;
};

///////////////////////////////////////////////////////////////////////////////

struct DagNodeData : public ork::Object {

  DeclareConcreteX(DagNodeData, ork::Object);

public:
  DagNodeData(const ork::rtti::ICastable* powner = 0);

  xfnode_const_ptr_t transformNode() const {
    return _xfnode;
  }
  xfnode_ptr_t transformNode() {
    return _xfnode;
  }
  const ork::rtti::ICastable* owner() const {
    return _owner;
  }
  orkvector<dagnodedata_ptr_t>& children() {
    return _children;
  }
  ork::fmtx4 computeMatrix() const {
    return _xfnode->computeMatrix();
  }

  void addChild(dagnodedata_ptr_t pchild);
  void removeChild(dagnodedata_ptr_t pchild);

  /////////////

  const ork::rtti::ICastable* _owner;
  orkvector<dagnodedata_ptr_t> _children;
  xfnode_ptr_t _xfnode;

  /////////////
};

///////////////////////////////////////////////////////////////////////////////

struct SceneDagObject : public SceneObject {

  DeclareAbstractX(SceneDagObject, SceneObject);

public:
  SceneDagObject();
  ~SceneDagObject();

  void SetParentName(const PoolString& pname);
  const PoolString& GetParentName() const {
    return _parentName;
  }

  xfnode_ptr_t transformNode() {
    return _dagnode->_xfnode;
  }
  decompxf_ptr_t transform() {
    return _dagnode->_xfnode->_transform;
  }
  xfnode_const_ptr_t transformNode() const {
    return _dagnode->_xfnode;
  }
  decompxf_const_ptr_t transform() const {
    return _dagnode->_xfnode->_transform;
  }

  dagnodedata_ptr_t _dagnode;
  PoolString _parentName;
};

///////////////////////////////////////////////////////////////////////////////

class SceneGroup : public SceneDagObject {

  DeclareConcreteX(SceneGroup, SceneDagObject);

public:
  SceneGroup();
  ~SceneGroup();

  ////////////////////////////////////////////////////////////////

  const orkvector<scenedagobject_ptr_t>& Children() const {
    return _children;
  }

  void addChild(scenedagobject_ptr_t pchild);
  void removeChild(scenedagobject_ptr_t pchild);

  ////////////////////////////////////////////////////////////////

  void ungroupAll();

private:
  orkvector<scenedagobject_ptr_t> _children;
};

} //namespace ork::ecs {
