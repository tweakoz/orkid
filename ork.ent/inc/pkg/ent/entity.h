////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/object/Object.h>

#include <ork/math/TransformNode.h>
#include <pkg/ent/component.h>
#include <pkg/ent/componenttable.h>

#include <ork/kernel/string/ArrayString.h>
#include <ork/lev2/gfx/renderer/drawable.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
class Drawable;
class Layer;
} // namespace ork::lev2

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace ent {

class DagComponent;
class Archetype;
class Simulation;
class SceneData;

///////////////////////////////////////////////////////////////////////////////

class SceneObjectClass : public object::ObjectClass {
  RttiDeclareExplicit(SceneObjectClass, object::ObjectClass, rtti::NamePolicy, object::ObjectCategory);

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

class SceneObject : public ork::Object {
  RttiDeclareExplicit(SceneObject, ork::Object, ork::rtti::AbstractPolicy, ent::SceneObjectClass);

protected:
  SceneObject();

public:
  void SetName(PoolString name);
  PoolString GetName() const {
    return mName;
  }
  void SetName(const char* name);

private:
  PoolString mName;
};

///////////////////////////////////////////////////////////////////////////////

class DagRenderableContextData {
  U32 mRenderFlags;

public:
  DagRenderableContextData()
      : mRenderFlags(0) {
  }
  void SetRenderFlags(U32 uval) {
    mRenderFlags = uval;
  }
  U32 GetRenderFlags(void) const {
    return mRenderFlags;
  }
};

///////////////////////////////////////////////////////////////////////////////

class DagNode : public ork::Object {

  DeclareConcreteX(DagNode, ork::Object);

  const ork::rtti::ICastable* mpOwner;
  orkvector<DagNode*> mChildren;

protected:
  TransformNode _xfnode;
  DagRenderableContextData mRenderableContextData;

  static const int knumtimedmtx = 3;
  fmtx4 mPrevMtx[knumtimedmtx];
  float mTimeStamps[knumtimedmtx];

public:
  DagNode(const ork::rtti::ICastable* powner = 0);
  const TransformNode& GetTransformNode() const {
    return _xfnode;
  }
  TransformNode& GetTransformNode() {
    return _xfnode;
  }
  const DagRenderableContextData& GetRenderableContextData() const {
    return mRenderableContextData;
  }
  DagRenderableContextData& GetRenderableContextData() {
    return mRenderableContextData;
  }
  const ork::rtti::ICastable* GetOwner() const {
    return mpOwner;
  }
  orkvector<DagNode*>& GetChildren() {
    return mChildren;
  }
  void GetMatrix(ork::fmtx4& mtx) const {
    _xfnode.GetMatrix(mtx);
  }
  const fmtx4& GetTimedMatrix(int idx) const {
    OrkAssert(idx < knumtimedmtx);
    return mPrevMtx[idx];
  }
  void StepTimedMatrices(float ftime);
  void AddChild(DagNode* pchild);
  void RemoveChild(DagNode* pchild);

  void CopyTransformMatrixFrom(const DagNode& other);
  void SetTransformMatrix(const fmtx4& mtx);
};

///////////////////////////////////////////////////////////////////////////////

class SceneDagObject : public SceneObject {
  RttiDeclareConcrete(SceneDagObject, SceneObject);
  DagNode mDagNode;
  PoolString mParentName;

public:
  SceneDagObject();
  ~SceneDagObject();

  void SetParentName(const PoolString& pname);
  const PoolString& GetParentName() const {
    return mParentName;
  }

  DagNode& GetDagNode() {
    return mDagNode;
  }
  const DagNode& GetDagNode() const {
    return mDagNode;
  }

  ork::Object* AccessDagNode() {
    return &mDagNode;
  }
};

///////////////////////////////////////////////////////////////////////////////

class SceneGroup : public SceneDagObject {
  RttiDeclareConcrete(SceneGroup, SceneDagObject);

public:
  SceneGroup();
  ~SceneGroup();

  ////////////////////////////////////////////////////////////////

  const orkvector<SceneDagObject*>& Children() const {
    return mChildren;
  }

  void AddChild(SceneDagObject* pchild);
  void RemoveChild(SceneDagObject* pchild);

  ////////////////////////////////////////////////////////////////

  void UnGroupAll();

private:
  orkvector<SceneDagObject*> mChildren;
};

///////////////////////////////////////////////////////////////////////////////

class EntData : public SceneDagObject {
  RttiDeclareConcrete(EntData, SceneDagObject);

public:
  EntData();
  ~EntData() final;

  bool PostDeserialize(reflect::IDeserializer&) final;

  const Archetype* GetArchetype() const {
    return mArchetype;
  }
  void SetArchetype(const Archetype* parch);

  void ArchetypeGetter(ork::rtti::ICastable*& val) const; // { val=mArchetype; }
  void ArchetypeSetter(ork::rtti::ICastable* const& val); // { mArchetype=rtti::downcast<Archetype*>(val); }

  template <typename T> const T* GetTypedComponent() const;

  ConstString GetUserProperty(const ConstString& key) const;

private:
  void SlotArchetypeDeleted(const ork::ent::Archetype* parch);

  const Archetype* mArchetype;
  orklut<ConstString, ConstString> mUserProperties;
};

///////////////////////////////////////////////////////////////////////////////
// an INSTANCE of an EntData is an Entity
///////////////////////////////////////////////////////////////////////////////
class Entity : public lev2::DrawableOwner {
  RttiDeclareAbstract(Entity, lev2::DrawableOwner);

public:
  typedef std::function<fmtx4()> _rendermtxprovider_t;

  ////////////////////////////////////////////////////////////////
  // Component Interface

  const ComponentTable& GetComponents() const;
  ComponentTable& GetComponents();
  Entity* Self() {
    return this;
  }
  void PrintName();
  fvec3 GetEntityPosition() const; // e this should eb gone BUT some entities have NO componenets

  template <typename T> T* GetTypedComponent(bool bsubclass = false);
  template <typename T> const T* GetTypedComponent(bool bsubclass = false) const;

  ComponentInst* GetComponentByClass(rtti::Class* clazz);
  ComponentInst* GetComponentByClassName(ork::PoolString classname);

  const DagNode& GetDagNode() const {
    return mDagNode;
  }
  DagNode& GetDagNode() {
    return mDagNode;
  }

  fmtx4 GetEffectiveMatrix() const;    // get Entity matrix if scene is running, EntData matrix if scene is stopped
  void SetDynMatrix(const fmtx4& mtx); // set this (Entity) matrix

  const EntData* data() const {
    return _entdata;
  }
  void addDrawableToDefaultLayer(lev2::Drawable* pdrw);
  void addDrawableToLayer(lev2::Drawable* pdrw, const PoolString& layername);

  ////////////////////////////////////////////////////////////////

  void EntDataGetter(ork::rtti::ICastable*& val) const;

  ////////////////////////////////////////////////////////////////

  Entity(const EntData* edata, Simulation* inst);
  ~Entity();

  ////////////////////////////////////////////////////////////////

  Simulation* simulation() const {
    return mSimulation;
  }

  PoolString name() const;

  _rendermtxprovider_t _renderMtxProvider = nullptr;

private:
  bool DoNotify(const ork::event::Event* event) final;

  Simulation* mSimulation;

  const EntData* _entdata;
  mutable bool mComposed;
  ComponentTable mComponentTable;
  ComponentTable::LutType _components;
  DagNode mDagNode;
};

///////////////////////////////////////////////////////////////////////////////

class ReferenceArchetype;
class SceneComposer;

struct ArchComposer {
  ork::orklut<ork::object::ObjectClass*, ork::Object*> _components;
  ork::ent::Archetype* mpArchetype;
  SceneComposer& mSceneComposer;

  template <typename T> T* Register();

  void Register(ork::ent::ComponentData* pdata);
  ArchComposer(ork::ent::Archetype* parch, SceneComposer& scene_composer);
  ~ArchComposer();
};

///////////////////////////////////////////////////////////////////////////////

class Archetype : public SceneObject {
  RttiDeclareAbstract(Archetype, SceneObject);

public:
  Archetype();
  ~Archetype() {
    DeleteComponents();
  }

  void LinkEntity(Simulation* psi, Entity* pent) const;
  void UnLinkEntity(Simulation* psi, Entity* pent) const;
  void StartEntity(Simulation* psi, const fmtx4& world, Entity* pent) const;
  void StopEntity(Simulation* psi, Entity* pent) const;

  void ComposeEntity(Entity* pent) const;
  void Compose(SceneComposer& scene_composer);
  void DeCompose();

  void DeleteComponents();

  template <typename T> T* GetTypedComponent();
  template <typename T> const T* GetTypedComponent() const;

  const ComponentDataTable& GetComponentDataTable() const {
    return mComponentDataTable;
  }
  ComponentDataTable& GetComponentDataTable() {
    return mComponentDataTable;
  }

  void SetSceneData(SceneData* psd) {
    mpSceneData = psd;
  }
  SceneData* GetSceneData() const {
    return mpSceneData;
  }

protected:
  virtual void DoComposeEntity(Entity* pent) const;
  virtual void DoDeComposeEntity(Entity* pent) const;
  virtual void DoLinkEntity(Simulation* psi, Entity* pent) const;
  virtual void DoUnLinkEntity(Simulation* psi, Entity* pent) const;
  virtual void DoStartEntity(Simulation* psi, const fmtx4& world, Entity* pent) const = 0;
  virtual void DoStopEntity(Simulation* psi, Entity* pent) const {
  }

  virtual void DoCompose(ArchComposer& arch_composer) = 0;

  ComponentDataTable mComponentDataTable;
  ComponentDataTable::LutType mComponentDatas;

private:
  bool PostDeserialize(reflect::IDeserializer&) override;

  SceneData* mpSceneData;
};

///////////////////////////////////////////////////////////////////////////////

class IEntController {
public:
  virtual void ComposeEntData(EntData* pentdata) const         = 0;
  virtual void DeComposeEntData(EntData* pentdata) const       = 0;
  virtual Entity* ComposeEntity(const EntData* pentdata) const = 0;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::ent
