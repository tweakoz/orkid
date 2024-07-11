////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/any.h>
#include <ork/pch.h>
#include <ork/reflect/properties/register.h>
#include <ork/util/md5.h>

#include <cxxabi.h>
#include <iostream>
#include <sstream>

#include <ork/reflect/properties/registerX.inl>


#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>
#include <ork/ecs/simulation.inl>

#include <ork/util/logger.h>

#include "PythonImpl.h"

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::ecs::PythonComponentData, "PythonComponentData");
ImplementReflectionX(ork::ecs::PythonComponent, "PythonComponent");
ImplementReflectionX(ork::ecs::PythonSystemData, "PythonSystemData");

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////
using namespace ork;
using namespace ork::object;
using namespace ork::reflect;

static logchannel_ptr_t logchan_pysyscomp = logger()->createChannel("ecs.pycomp",fvec3(0.9,0.8,0.0));

void PythonComponentData::describeX(ComponentDataClass* clazz) {
  clazz->directProperty("ScriptFile", &PythonComponentData::mScriptPath)
      ->annotate("editor.class", "ged.factory.filelist")
      ->annotate("editor.filetype", "lua")
      ->annotate("editor.filebase", "src://scripts/");
}

///////////////////////////////////////////////////////////////////////////////

PythonComponentData::PythonComponentData() {
  // printf("PythonComponentData::PythonComponentData() this: %p\n", this);
}

///////////////////////////////////////////////////////////////////////////////

Component* PythonComponentData::createComponent(ecs::Entity* pent) const {
  return new PythonComponent(*this, pent);
}

///////////////////////////////////////////////////////////////////////////////

void PythonComponentData::DoRegisterWithScene(ork::ecs::SceneComposer& sc) const {
  sc.Register<ork::ecs::PythonSystemData>();
}

object::ObjectClass* PythonComponentData::componentClass() {
  return PythonComponent::GetClassStatic();
}

///////////////////////////////////////////////////////////////////////////////

void PythonComponent::describeX(ObjectClass* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

PythonComponent::PythonComponent(const PythonComponentData& data, ecs::Entity* pent)
    : ork::ecs::Component(&data, pent)
    , mCD(data){
}

///////////////////////////////////////////////////////////////////////////////

void PythonComponent::_onUninitialize(ork::ecs::Simulation* psi) {
}

///////////////////////////////////////////////////////////////////////////////

bool PythonComponent::_onLink(ork::ecs::Simulation* sim) {

  auto ent = this->GetEntity();
  auto scm = sim->findSystem<PythonSystem>();

  auto path = mCD.GetPath();

  //logchan_pysyscomp->log( "PythonComponent::_onLink: scm<%p> path<%s>", scm, path.toAbsolute().c_str() );
  
  if (nullptr == scm){
    logerrchannel()->log( "PythonComponent::_onLink: scm is nullptr!!!!" );
    return false;
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////

void PythonComponent::_onUnlink(ork::ecs::Simulation* psi) {
}

///////////////////////////////////////////////////////////////////////////////

bool PythonComponent::_onStage(ork::ecs::Simulation* sim) {
  return true;
}
///////////////////////////////////////////////////////////////////////////////

void PythonComponent::_onUnstage(ork::ecs::Simulation* psi) {
}
///////////////////////////////////////////////////////////////////////////////

bool PythonComponent::_onActivate(Simulation* psi) {
  auto scm = psi->findSystem<PythonSystem>();
  if (scm) {
    scm->_onActivateComponent(this);
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void PythonComponent::_onDeactivate(Simulation* psi) {
  auto scm = psi->findSystem<PythonSystem>();
  if (scm ) {
    scm->_onDeactivateComponent(this);
  }
}

///////////////////////////////////////////////////////////////////////////////

void PythonComponent::_onNotify(Simulation* psi, token_t evID, evdata_t data) {


}

}
