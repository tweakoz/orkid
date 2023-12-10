////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/application/application.h>
#include <ork/reflect/properties/AccessorTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.h>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/DirectTypedVector.h>
#include <ork/reflect/properties/DirectTypedVector.hpp>
#include <ork/reflect/properties/register.h>
#include <ork/rtti/downcast.h>
#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.h>

#include <ork/ecs/ReferenceArchetype.h>

#include <ork/kernel/orklut.hpp>
#include <ork/rtti/Class.h>
#include <ork/reflect/properties/registerX.inl>

#include <ork/ecs/lua/LuaComponent.h>
#include <ork/ecs/SceneGraphComponent.h>
#include <ork/ecs/component.inl>

#include <ork/ecs/physics/bullet.h>
#include "../physics/bullet_impl.h"

#include "InterpComponent_impl.h"
#include "../scripting/LuaImpl.h"


//#define ENABLE_REFL_REGISTRATION

#if defined(ENABLE_REFL_REGISTRATION)
#include <ork/ecs/AudioComponent.h>
#include <ork/ecs/AudioAnalyzer.h>
#include <ork/ecs/CompositingSystem.h>
#include <ork/ecs/LightingSystem.h>
#include <ork/ecs/ModelArchetype.h>
#include <ork/ecs/ModelComponent.h>
#include <ork/ecs/ParticleControllable.h>
#include <ork/ecs/SimpleAnimatable.h>
#include <ork/ecs/input.h>
#include <ork/ecs/EditorCamera.h>
#include <ork/ecs/bullet.h>

#include "../camera/ObserverCamera.h"
#include "../camera/SpinnyCamera.h"
#include "../camera/TetherCamera.h"
#include "../character/CharacterLocoComponent.h"
#include "../character/SimpleCharacterArchetype.h"
#include "../core/PerformanceAnalyzer.h"
#include "../misc/GridComponent.h"
#include "../misc/ProcTex.h"
#include "../misc/QuartzComposerTest.h"
#include "../misc/Skybox.h"
#include "../misc/VrSystem.h"
#endif

namespace ork::ecs {
using namespace ::ork;
using namespace ::ork::object;
using namespace ::ork::reflect;
using namespace ::ork::rtti;

static LockedResource<TokMap> __tokmaps;


token_t tokenize(const char* name){
  return tokenize(std::string(name));
}
token_t tokenize(const ECSTOK& tok){
  return tokenize(std::string(tok._str));
}

token_t tokenize(const std::string& str){
  auto token = CrcString(str.c_str());
  auto hashed = token.hashed();
  __tokmaps.atomicOp([str, hashed](TokMap& unlocked) {
    auto it = unlocked._str2tok_map.find(str);
    if (it == unlocked._str2tok_map.end()) {
      unlocked._str2tok_map[str] = hashed;
      unlocked._tok2str_map[hashed]  = str;
    }
  });
  //printf( "TOKENIZE<%s> -> %zx\n", str.c_str(), token.hashed());
  return token;
}

std::string detokenize(token_t token){
  std::string rval;
  auto hashed = token.hashed();
  __tokmaps.atomicOp([hashed,&rval](const TokMap& unlocked) {
    auto it = unlocked._tok2str_map.find(hashed);
    if (it != unlocked._tok2str_map.end()) {
      rval=it->second;
    }
  });
  //printf( "DETOKENIZE<%zx> -> %s\n", token.hashed(), rval.c_str());
  return rval;
}

void FnBallArchetypeTouch();

void ClassInit() {

  RegisterClassX(Archetype);
  RegisterClassX(DagNodeData);
  RegisterClassX(SpawnData);
  RegisterClassX(SceneObject);
  RegisterClassX(SceneData);
  RegisterClassX(ComponentData);
  RegisterClassX(ComponentFragmentData);

  RegisterClassX(Component);
  RegisterClassX(ComponentFragment);
  //RegisterClassX(SystemFragment);

  RegisterClassX(InterpComponentData);
  RegisterClassX(InterpComponent);
  RegisterClassX(InterpSystemData);

  RegisterClassX(LuaComponentData);
  RegisterClassX(LuaComponent);
  RegisterClassX(LuaSystemData);

  RegisterClassX(SceneGraphComponentData);
  RegisterClassX(SceneGraphSystemData);
  RegisterClassX(SceneGraphComponent);
  RegisterClassX(SceneGraphSystem);
  RegisterClassX(SceneGraphNodeItemData);

  RegisterClassX(BulletSystemData);
  RegisterClassX(BulletShapeCapsuleData);
  RegisterClassX(BulletShapePlaneData);
  RegisterClassX(BulletShapeSphereData);
  RegisterClassX(BulletShapeModelData);
  RegisterClassX(BulletShapeCapsuleData);
  RegisterClassX(BulletShapeTerrainData);
  RegisterClassX(BulletObjectComponentData);
  RegisterClassX(BulletObjectForceControllerData);
  RegisterClassX(DirectionalForceData);

  RegisterClassX(BulletObjectComponent);
  RegisterClassX(BulletSystem);

  RegisterFamily<LuaComponentData>(ork::AddPooledLiteral("control"));
  RegisterFamily<InterpComponentData>(ork::AddPooledLiteral("control"));
  RegisterFamily<SceneGraphComponentData>(ork::AddPooledLiteral("render"));
  RegisterFamily<BulletObjectComponentData>(ork::AddPooledLiteral("")); // no update

}

void initModule(ork::appinitdata_ptr_t init_data){
  auto it = init_data->_miscvars.find("ecs_init");
  if(it == init_data->_miscvars.end() ){
    init_data->enqueuePreInitOp([]{
      ClassInit();
    });
    init_data->_miscvars["ecs_init"] = nullptr;
  }
}

void Init2() {
}

} //namespace ork::ecs {
