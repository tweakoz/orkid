////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/any.h>
#include <ork/math/cvector3.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <pkg/ent/ScriptComponent.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/scene.h>
#include <pkg/ent/scene.hpp>

#include <cxxabi.h>
#include <iostream>
#include <sstream>

#include "LuaBindings.h"
#include "LuaIntf/LuaIntf.h"

///////////////////////////////////////////////////////////////////////////////

std::stringstream& operator<<(std::stringstream& str, const ork::fvec3& v) {
  ork::fxstring<256> fxs;
  fxs.format("[%f,%f,%f]", v.GetX(), v.GetY(), v.GetZ());
  str << fxs.c_str();
  return str;
}

namespace LuaIntf {
LUA_USING_LIST_TYPE(std::vector)
LUA_USING_MAP_TYPE(std::map)
} // namespace LuaIntf

using namespace LuaIntf;

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

bool DoString(lua_State* L, const char* str) {
  if (luaL_loadbuffer(L, str, std::strlen(str), str) || lua_pcall(L, 0, 0, 0)) {
    const char* a = lua_tostring(L, -1);
    std::cout << a << "\n";
    lua_pop(L, 1);
    return true;
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////
/*
luabind::object GetEntities(lua_State* L,luabind::object o)
{
    auto psi = object_cast<SceneInst*>(o);
    luabind::object enttab = luabind::newtable(L);
    const auto& ents = psi->Entities();
    for( const auto& item : ents )
    {
        //LuaOpaque16 o(item.second);
        luabind::object o(L,item.second);
        enttab[item.first.c_str()] = o;
    }
    return enttab;
}
luabind::object SpawnEntity(lua_State* L,luabind::object scene, luabind::object archname, luabind::object entname)
{
    auto psi = object_cast<SceneInst*>(scene);

    const auto& scenedata = psi->GetData();
    auto archnamestr = object_cast<std::string>(archname);
    auto entnamestr = object_cast<std::string>(entname);
    auto archso = scenedata.FindSceneObjectByName(AddPooledString(archnamestr.c_str()));

    if( const Archetype* as_arch = rtti::autocast(archso) )
    {
        //printf( "SPAWN<%s:%p> ename<%s>\n", archnamestr.c_str(), archso, entnamestr.c_str() );
        EntData* spawner = new EntData;
        spawner->SetName(AddPooledString(entnamestr.c_str()));
        spawner->SetArchetype(as_arch);
        auto newent = psi->SpawnDynamicEntity(spawner);
        return luabind::object(L,newent);
    }
    else
    {
        assert(false);
        //printf( "SPAWN<%s:%p>\n", archnamestr.c_str(), archso );
        return luabind::object(L,"");
    }
}
void DeSpawnEntity(lua_State* L,luabind::object scene, luabind::object ento)
{
    auto psi = object_cast<SceneInst*>(scene);
    auto as_ent = object_cast<Entity*>(ento);
    if( as_ent )
        psi->QueueDeactivateEntity(as_ent);
}

luabind::object GetEntArchetype( lua_State* L, Entity* pent )
{
    const Archetype* a = pent ? pent->GetEntData().GetArchetype() : nullptr;
    return luabind::object(L,a);

}

luabind::object GetArchetypeName( lua_State* L, luabind::object o )
{
    auto pa = object_cast<const Archetype*>(o);
    const char* name =  pa ? pa->GetName().c_str() : "";
    return luabind::object(L,std::string(name));
}

luabind::object CreateVector3( lua_State* L, double x, double y, double z )
{
    auto v = new fvec3(float(x),float(y),float(z));
    return luabind::object(L,v);
}
luabind::object GetVec3XZ( lua_State* L,  luabind::object o )
{
    auto v = object_cast<fvec3*>(o);
    fxstring<256> fx;
    fx.format("[%f,%f]", v->GetX(), v->GetZ() );
    std::string rval = fx.c_str();
    return luabind::object(L,rval);
}
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
*/

LuaSystem::LuaSystem(SceneInst* psi) : mSceneInst(psi) {
  mLuaState = ::luaL_newstate(); // aka lua_open
  luaL_openlibs(mLuaState);
  // luabind::open(mLuaState);
  auto L = mLuaState;

  lua_newtable(L);
  lua_setglobal(L, "ork");

  auto setGlobTabFn = [L](const char* tbl, const char* fnnam, lua_CFunction cf) {
    lua_getglobal(L, tbl);
    lua_pushstring(L, fnnam);
    lua_pushcfunction(L, cf);
    lua_settable(L, -3);
  };
  // setGlobTabFn("ork","getEntityName",&GetEntityName);

  LuaBinding(L)
      .beginModule("ork")
      ////////////////////////////////////////
      .beginClass<fvec3>("vec3")
      .addConstructor(LUA_ARGS(float, float, float))
      //.def(tostring(self))
      //.property("xz", &fvec3,&SetVec3XZ)
      .addProperty("x", &fvec3::GetX, &fvec3::SetX)
      .addProperty("y", &fvec3::GetY, &fvec3::SetY)
      .addProperty("z", &fvec3::GetZ, &fvec3::SetZ)
      .addMetaFunction("__tostring",
                       [](const fvec3* v) -> std::string {
                         fxstring<64> fxs;
                         fxs.format("vec3(%g,%g,%g)", v->x, v->y, v->z);
                         return fxs.c_str();
                       })
      .addMetaFunction("__add", [](const fvec3* a, const fvec3* b) -> fvec3 { return (*a) + (*b); })
      .endClass()
      ////////////////////////////////////////
      .beginClass<ComponentInst>("component")
      .addProperty("type",
                   [](const ComponentInst* c) -> std::string {
                     auto clazz = c->GetClass();
                     auto cn = clazz->Name();
                     return cn.c_str();
                   })
      .addProperty("family",
                   [](const ComponentInst* c) -> std::string {
                     auto ps = c->GetFamily();
                     return ps.c_str();
                   })
      .addFunction("sendEvent",
                   [](ComponentInst* ci, const char* evcode, LuaRef evdata) {
                     auto clazz = ci->GetClass();
                     auto cn = clazz->Name();
                     printf( "sendEvent ci<%s> code<%s> ... \n", cn.c_str(), evcode);
                     //for (auto& e : evdata) {
                      //       const auto&key = e.key<std::string>();
                        //     printf( " key<%s>\n", key.c_str() );
                             //LuaRef value = e.value<LuaRef>();
                             //...
                     //}
                     event::VEvent vev;
                     vev.mCode = AddPooledString(evcode);
                     vev.mData.Set<LuaRef>(evdata);
                     ci->Notify(&vev);
                   })
      .addMetaFunction("__tostring",
                       [](const ComponentInst* c) -> std::string {
                         auto clazz = c->GetClass();
                         auto cn = clazz->Name();
                         return cn.c_str();
                       })
      .endClass()
      ////////////////////////////////////////
      .beginClass<Entity>("entity")
      .addPropertyReadOnly("name", &Entity::name)
      .addPropertyReadOnly("pos",
                           [](Entity* pent) -> fvec3 {
                             fvec3 pos = pent->GetEntityPosition();
                             return pos;
                           })
      .addPropertyReadOnly("components",
                           [](const Entity* e) -> std::map<std::string, ComponentInst*> {
                             std::map<std::string, ComponentInst*> rval;
                             for (auto item : e->GetComponents().GetComponents()) {
                               auto c = item.second;
                               rval[c->friendlyName()] = c;
                             }
                             printf("ent<%p> components size<%zu>\n", e, rval.size());
                             return rval;
                           })
      .addMetaFunction("__tostring",
                       [](const Entity* e) -> std::string {
                         ork::fxstring<256> str;
                         const char* ename = e ? e->GetEntData().GetName().c_str() : "";
                         auto a = e ? e->GetEntData().GetArchetype() : nullptr;
                         const char* aname = a ? a->GetName().c_str() : "";
                         str.format("(ent<%s> arch<%s>)", ename, aname);
                         return str.c_str();
                       })

      .endClass()
      ////////////////////////////////////////
      .beginClass<SceneInst>("scene")
      .addFunction("spawn",
                   [](SceneInst* psi, const char* arch, const char* entname, LuaRef spdata) -> Entity* {
                     const auto& scenedata = psi->GetData();
                     auto archnamestr = std::string(arch);
                     auto entnamestr = std::string(entname);
                     auto archso = scenedata.FindSceneObjectByName(AddPooledString(archnamestr.c_str()));

                     auto position = spdata.get<fvec3>("pos");

                     printf("SPAWN<%s:%p> ename<%s>\n", archnamestr.c_str(), archso, entnamestr.c_str());

                     if (const Archetype* as_arch = rtti::autocast(archso)) {
                       EntData* spawner = new EntData;
                       spawner->SetName(AddPooledString(entnamestr.c_str()));
                       spawner->SetArchetype(as_arch);
                       auto mtx = fmtx4();
                       mtx.ComposeMatrix(position, fquat(), 1.0f);
                       spawner->GetDagNode().SetTransformMatrix(mtx);
                       auto newent = psi->SpawnDynamicEntity(spawner);
                       return newent;
                     } else {
                       assert(false);
                       // printf( "SPAWN<%s:%p>\n", archnamestr.c_str(), archso );
                       return nullptr;
                     }
                     return nullptr;
                   })
      .endClass()
      ////////////////////////////////////////
      .addFunction("scene", [=]() -> SceneInst* { return psi; })
      ////////////////////////////////////////
      .endModule();

/////////////////////////////////////
#if 0
    module(mLuaState,"ork")
    [
    	def("getscene",&GetScene),

		class_<SceneInst,SceneInst*>("scene")
			.def("entities", &GetEntities )
			.def("spawn", &SpawnEntity)
			.def("despawn", &DeSpawnEntity),

		class_<Entity,Entity*>("entity")
			.def("archetype", &GetEntArchetype )
			.def("name",	&GetEntityName )
			.property("pos", &GetEntityPos, &SetEntityPos)
			//.property("pos", &GetEntityAxisAng, &SetEntityAxisAng)
			.def("__tostring",&EntToString),

		class_<Archetype,Archetype*>("archetype")
			.def("name",	&GetArchetypeName )
			.def("__tostring",&ArchToString),

		class_<LuaSystem,LuaSystem*>("LuaSystem"),

		class_<LuaOpaque64>("Opaque64"),

		class_<LuaOpaque16>("Opaque16")
			.def("type", &LuaOpaque16::GetType ),

		class_<fvec3,fvec3*>("vec3")
			.def(tostring(self))
			.property("xz", &GetVec3XZ,&SetVec3XZ)
			.property("x", &GetVec3X,&SetVec3X)
			.property("y", &GetVec3Y,&SetVec3Y)
			.property("z", &GetVec3Z,&SetVec3Z),

		/*class_<fvec4,fvec4*>("Vector4")
			.def(tostring(self))
			.property("x", &GetVec4X,&SetVec4X)
			.property("y", &GetVec4Y,&SetVec4Y)
			.property("z", &GetVec4Z,&SetVec4Z),
			.property("w", &GetVec4Z,&SetVec4Z),
            */

		def("vec3",&CreateVector3)

    ];
#endif
  /////////////////////////////////////
  // luabind::object globtab = luabind::newtable(mLuaState);
  // globtab[1] = this;
  // globals(mLuaState)["luascene"] = globtab;
  /////////////////////////////////////
  // auto exec_table =luabind::newtable(mLuaState);
  // luabind::globals(mLuaState)["entity_exec_table"] = exec_table;
  /////////////////////////////////////
  printf("create LuaState<%p> psi<%p>\n", mLuaState, psi);
}
LuaSystem::~LuaSystem() {
  printf("destroy LuaState<%p>\n", mLuaState);
  lua_close(mLuaState);
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
