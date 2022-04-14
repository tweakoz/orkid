////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include <ork/math/cmatrix4.h>
#include <ork/object/Object.h>
#include <ork/rtti/RTTIX.inl>

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

struct ComponentFragmentDataClass : public object::ObjectClass {
  RttiDeclareExplicit(ComponentFragmentDataClass, object::ObjectClass, rtti::NamePolicy, object::ObjectCategory);

public:
  ComponentFragmentDataClass(const rtti::RTTIData&);
};

///////////////////////////////////////////////////////////////////////////////

struct ComponentDataClass : public object::ObjectClass {
  RttiDeclareExplicit(ComponentDataClass, object::ObjectClass, rtti::NamePolicy, object::ObjectCategory);

public:
  ComponentDataClass(const rtti::RTTIData&);

  PoolString GetFamily() const;
  void SetFamily(PoolString family);

private:
  PoolString mFamily;
};

///////////////////////////////////////////////////////////////////////////////

struct ComponentFragmentData : public Object {
  DeclareExplicitX(ComponentFragmentData, Object, rtti::AbstractPolicy, ComponentFragmentDataClass);

public:
  ComponentFragmentData();

  virtual ComponentFragment* createFragment(Component* c) const = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct ComponentData : public Object {
  DeclareExplicitX(ComponentData, Object, rtti::AbstractPolicy, ComponentDataClass);

public:
  ComponentData();

  virtual Component* createComponent(Entity* pent) const = 0;

  PoolString GetFamily() const;

  void RegisterWithScene(SceneComposer& sc) const;

  virtual const char* GetShortSelector() const;

private:
  virtual void DoRegisterWithScene(SceneComposer& sc) const;
};

///////////////////////////////////////////////////////////////////////////////

struct ComponentFragment : public Object {
  RttiDeclareAbstract(ComponentFragment, Object);
protected:
  ComponentFragment(const ComponentFragmentData* data, Entity* entity);
};

///////////////////////////////////////////////////////////////////////////////

struct Component : public Object {
  RttiDeclareAbstract(Component, Object);

public:
  void SetEntity(Entity* entity);
  Entity* GetEntity();
  const Entity* GetEntity() const;
  Simulation* sceneInst() const;
  const char* GetEntityName() const;

  virtual const char* scriptName();

  PoolString GetFamily() const;

  const char* GetShortSelector() const;

  //void _update(Simulation* inst);
  void _uninitialize(Simulation* inst);
  bool _link(Simulation* psi);
  void _unlink(Simulation* psi);
  bool _stage(Simulation* psi);
  void _unstage(Simulation* psi);
  bool _activate(Simulation* psi);
  void _deactivate(Simulation* psi);

  void _notify(Simulation* psi, token_t evID, svar64_t data);
  void _request(Simulation* psi, impl::comp_response_ptr_t response, token_t evID, svar64_t data);

protected:
  Component(const ComponentData* data, Entity* entity);

  //virtual void _onUpdate(Simulation* inst);
  virtual void _onUninitialize(Simulation* psi);
  virtual bool _onLink(Simulation* psi);
  virtual void _onUnlink(Simulation* psi);
  virtual bool _onStage(Simulation* psi);
  virtual void _onUnstage(Simulation* psi);
  virtual bool _onActivate(Simulation* psi);
  virtual void _onDeactivate(Simulation* psi);

  virtual void _onNotify(Simulation* psi, token_t evID, svar64_t data );
  virtual void _onRequest(Simulation* psi, impl::comp_response_ptr_t response, token_t evID, svar64_t data);

  const ComponentData* mComponentData = nullptr;
  Entity* _entity                     = nullptr;
  bool mbStarted                      = false;
  bool mbValid                        = false;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
