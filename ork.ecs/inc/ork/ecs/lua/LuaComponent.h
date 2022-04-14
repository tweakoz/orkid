////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/file/fileenv.h>
#include <ork/rtti/RTTIX.inl>

#include "../component.h"
#include "../system.h"

namespace ork { namespace ecs {

///////////////////////////////////////////////////////////////////////////////

struct LuaComponentData : public ecs::ComponentData {
  DeclareConcreteX(LuaComponentData, ecs::ComponentData);

public:
  LuaComponentData();

  const file::Path& GetPath() const {
    return mScriptPath;
  }
  void SetPath(const file::Path& pth) {
    mScriptPath = pth;
  }

  ecs::Component* createComponent(ecs::Entity* pent) const final;
  static object::ObjectClass* componentClass();
  void DoRegisterWithScene(ork::ecs::SceneComposer& sc) const final;

  file::Path mScriptPath;
};

///////////////////////////////////////////////////////////////////////////////

struct LuaSystemData : public ork::ecs::SystemData {
  DeclareConcreteX(LuaSystemData, ork::ecs::SystemData);

public:
  ///////////////////////////////////////////////////////
  LuaSystemData();
  ///////////////////////////////////////////////////////

private:
  ork::ecs::System* createSystem(ork::ecs::Simulation* pinst) const final;
};



}} // namespace ork::ecs
