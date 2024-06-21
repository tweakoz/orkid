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
    , mCD(data)
    , mScriptObject(nullptr) {
}

///////////////////////////////////////////////////////////////////////////////

void PythonComponent::_onUninitialize(ork::ecs::Simulation* psi) {
  if (mScriptObject) {
    auto scm      = psi->findSystem<PythonSystem>();
    auto as_ctx = scm->getManager().get<pysys::PythonContext*>();
    OrkAssert(as_ctx);
    /*
    auto L                = as_ctx->mPythonState;
    PythonIntf::PythonState lua = L;
    auto ent              = this->GetEntity();
    if (mScriptObject->mOnEntUninitialize >= 0) {
      lua.getRef(mScriptObject->mOnEntUninitialize);
      assert(lua.isFunction(LUA_STACKINDEX_TOP));
      lua.push(_luaentity);
      // printf( "CALL mOnEntUninitialize\n");
      int iret = lua.pcall(1, 0, 0);
      OrkAssert(iret == 0);
    }
    */
  }
}

///////////////////////////////////////////////////////////////////////////////

bool PythonComponent::_onLink(ork::ecs::Simulation* sim) {

  auto ent = this->GetEntity();
  auto scm = sim->findSystem<PythonSystem>();

  auto path = mCD.GetPath();

  logchan_pysyscomp->log( "PythonComponent::_onLink: scm<%p> path<%s>", scm, path.toAbsolute().c_str() );
  
  if (nullptr == scm){
    logerrchannel()->log( "PythonComponent::_onLink: scm is nullptr!!!!" );
    return false;
  }

  auto as_ctx = scm->getManager().get<pysys::PythonContext*>();
  OrkAssert(as_ctx);
  /*
  auto L                = as_ctx->mPythonState;
  PythonIntf::PythonState lua = L;

  if (scm) {
    mScriptObject = scm->FlyweightScriptObject(path);

    if (mScriptObject) {
      auto ent = this->GetEntity();

      _luaentity        = PythonIntf::PythonRef::createTable(L);
      _luaentity["ent"] = ent;

      if (mScriptObject->mOnEntInitialize >= 0) {
        lua.getRef(mScriptObject->mOnEntInitialize);
        assert(lua.isFunction(LUA_STACKINDEX_TOP));
        lua.push(_luaentity);
        // printf( "CALL mOnEntInitialize\n");
        int iret = lua.pcall(1, 0, 0);
        OrkAssert(iret == 0);
      }
    }
  }

  if (mScriptObject) {
    auto as_ctx = scm->getManager().get<pysys::PythonContext*>();
    OrkAssert(as_ctx);
    auto L                = as_ctx->mPythonState;
    PythonIntf::PythonState lua = L;
    auto ent              = this->GetEntity();
    if (mScriptObject->mOnEntLink >= 0) {
      lua.getRef(mScriptObject->mOnEntLink);
      assert(lua.isFunction(LUA_STACKINDEX_TOP));
      lua.push(_luaentity);
      int iret = lua.pcall(1, 0, 0);
      if (iret != 0) {
        logchan_pysyscomp->log("PCALL-ERROR: PythonComponent::_onLink: scm<%p> path<%s>", scm, path.toAbsolute().c_str());
        // printf( "CALL mOnEntLink\n");
      }
      OrkAssert(iret == 0);
    }
  }
  */
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void PythonComponent::_onUnlink(ork::ecs::Simulation* psi) {
  if (mScriptObject) {
    auto scm      = psi->findSystem<PythonSystem>();
    auto as_ctx = scm->getManager().get<pysys::PythonContext*>();
    OrkAssert(as_ctx);
    /*
    auto L                = as_ctx->mPythonState;
    PythonIntf::PythonState lua = L;
    auto ent              = this->GetEntity();
    if (mScriptObject->mOnEntUnlink >= 0) {
      lua.getRef(mScriptObject->mOnEntUnlink);
      assert(lua.isFunction(LUA_STACKINDEX_TOP));
      lua.push(_luaentity);
      // printf( "CALL mOnEntUnlink\n");
      int iret = lua.pcall(1, 0, 0);
      OrkAssert(iret == 0);
    }
    */
  }
}

///////////////////////////////////////////////////////////////////////////////

bool PythonComponent::_onStage(ork::ecs::Simulation* sim) {
  logchan_pysyscomp->log("PythonComponent::_onStage: mScriptObject<%p>", mScriptObject);
  if (mScriptObject) {
    auto scm      = sim->findSystem<PythonSystem>();
    auto as_ctx = scm->getManager().get<pysys::PythonContext*>();
    OrkAssert(as_ctx);
    /*
    auto L                = as_ctx->mPythonState;
    PythonIntf::PythonState lua = L;
    auto ent              = this->GetEntity();
    if (mScriptObject->mOnEntStage >= 0) {
      lua.getRef(mScriptObject->mOnEntStage);
      assert(lua.isFunction(LUA_STACKINDEX_TOP));
      lua.push(_luaentity);
      // printf( "CALL mOnEntStage\n");
      int iret = lua.pcall(1, 0, 0);
      OrkAssert(iret == 0);
    }
    */
  }
  return true;
}
///////////////////////////////////////////////////////////////////////////////

void PythonComponent::_onUnstage(ork::ecs::Simulation* psi) {
  if (mScriptObject) {
    auto scm      = psi->findSystem<PythonSystem>();
    auto as_ctx = scm->getManager().get<pysys::PythonContext*>();
    OrkAssert(as_ctx);
    /*
    auto L                = as_ctx->mPythonState;
    PythonIntf::PythonState lua = L;
    auto ent              = this->GetEntity();
    if (mScriptObject->mOnEntUnstage >= 0) {
      lua.getRef(mScriptObject->mOnEntUnstage);
      assert(lua.isFunction(LUA_STACKINDEX_TOP));
      lua.push(_luaentity);
      // printf( "CALL mOnEntUnstage\n");
      int iret = lua.pcall(1, 0, 0);
      OrkAssert(iret == 0);
    }
    */
  }
}
///////////////////////////////////////////////////////////////////////////////

bool PythonComponent::_onActivate(Simulation* psi) {
  auto scm = psi->findSystem<PythonSystem>();
  if (scm && mScriptObject) {
    scm->_onActivateComponent(this);
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void PythonComponent::_onDeactivate(Simulation* psi) {

  auto scm = psi->findSystem<PythonSystem>();
  if (scm && mScriptObject) {
    scm->_onDeactivateComponent(this);
  }
}

///////////////////////////////////////////////////////////////////////////////

void PythonComponent::_onNotify(Simulation* psi, token_t evID, evdata_t data) {

  /*
  ScriptVar luaID;
  luaID._encoded = evID;

  ScriptVar luadata;
  luadata._encoded = data;

  auto scm = psi->findSystem<PythonSystem>();

  //printf("PythonComponent<%p> scm<%p> mScriptObject<%p>\n", this, scm, (void*)mScriptObject);

  if (scm && mScriptObject) {

    //printf("  _onNotify<%p>\n", mScriptObject->mOnNotify);

    if (mScriptObject->mOnNotify >= 0) {
      auto asluactx = scm->getManager().get<pysys::PythonContext*>();
      OrkAssert(asluactx);
      auto L   = asluactx->mPythonState;
      auto ent = this->GetEntity();

      PythonIntf::PythonState lua = L;
      lua.getRef(mScriptObject->mOnNotify);
      OrkAssert(lua.isFunction(LUA_STACKINDEX_TOP));
      lua.push(_luaentity);
      luaID.pushToPython(L);
      luadata.pushToPython(L);
      int iret = lua.pcall(3, 0, 0);
      OrkAssert(iret == 0);
    }
  }*/
}

}
