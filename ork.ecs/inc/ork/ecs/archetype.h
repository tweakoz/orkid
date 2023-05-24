////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/object/Object.h>
#include <ork/kernel/string/ArrayString.h>
#include <ork/rtti/RTTIX.inl>

#include "types.h"
#include "component.h"
#include "componenttable.h"
#include "sceneobject.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {

///////////////////////////////////////////////////////////////////////////////

struct Archetype : public SceneObject {
  DeclareConcreteX(Archetype, SceneObject);

public:

  Archetype();
  ~Archetype() {
    deleteComponents();
  }

  //void initializeEntity(Simulation* psi, const DecompTransform& xf, Entity* pent) const;
  void uninitializeEntity(Simulation* psi, Entity* pent) const;
  void composeEntity(Simulation* psi, Entity* pent) const;
  void decomposeEntity(Simulation* psi, Entity* pent) const;
  void linkEntity(Simulation* psi, Entity* pent) const;
  void unlinkEntity(Simulation* psi, Entity* pent) const;
  void stageEntity(Simulation* psi, Entity* pent) const;
  void unstageEntity(Simulation* psi, Entity* pent) const;
  void activateEntity(Simulation* psi, Entity* pent) const;
  void deactivateEntity(Simulation* psi, Entity* pent) const;

  void deleteComponents();

  template <typename T> std::shared_ptr<T> typedComponent();
  template <typename T> std::shared_ptr<const T> typedComponent() const;
  template <typename T> std::shared_ptr<T> addComponent();

  const ComponentDataTable::LutType& componentdata() const {
    return mComponentDatas;
  }
  //ComponentDataTable& componentDataTable() {
    //return mComponentDataTable;
  //}

  void setSceneData(SceneData* psd) {
    _scenedata = psd;
  }
  SceneData* sceneData() const {
    return _scenedata;
  }


  //protected:
  //virtual void DoInitializeEntity(Simulation* psi, const DecompTransform& xf, Entity* pent) const;
  //virtual void DoUninitializeEntity(Simulation* psi, Entity* pent) const;
  //virtual void DoComposeEntity(Simulation* psi, Entity* pent) const;
  //virtual void DoDecomposeEntity(Simulation* psi, Entity* pent) const;
  //virtual void DoLinkEntity(Simulation* psi, Entity* pent) const;
  //virtual void DoUnlinkEntity(Simulation* psi, Entity* pent) const;
  //virtual void DoStageEntity(Simulation* psi, Entity* pent) const;
  //virtual void DoUnstageEntity(Simulation* psi, Entity* pent) const;
  //virtual void DoActivateEntity(Simulation* psi, Entity* pent) const;
  //virtual void DoDeactivateEntity(Simulation* psi, Entity* pent) const {
  //}

  //virtual void DoCompose(ArchComposer& arch_composer);

  SceneData* _scenedata = nullptr;
  ComponentDataTable::LutType mComponentDatas;
  //ComponentDataTable mComponentDataTable;

private:
  bool postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) override;
};

/*struct CompositeArchetype : public Archetype {
  DeclareConcreteX(CompositeArchetype, Archetype);
  ork::orklut<ork::object::ObjectClass*, componentdata_constptr_t> _components;
  template <typename T> std::shared_ptr<T> addComponent();
  void DoCompose(ArchComposer& arch_composer) final;
  void DoActivateEntity(Simulation* psi, Entity* pent) const final;
};

template <typename T> std::shared_ptr<T> addComponent();
*/
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs {
