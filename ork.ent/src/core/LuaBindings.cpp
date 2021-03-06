////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/any.h>
#include <ork/math/cvector3.h>
#include <ork/pch.h>
#include <ork/reflect/properties/register.h>
#include <pkg/ent/ScriptComponent.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/scene.h>
#include <pkg/ent/scene.hpp>

#include <cxxabi.h>
#include <iostream>
#include <sstream>

#include <pkg/ent/LuaBindings.h>
#include "LuaIntf/LuaIntf.h"

using namespace LuaIntf;
using namespace std::literals;

///////////////////////////////////////////////////////////////////////////////

std::stringstream& operator<<(std::stringstream& str, const ork::fvec3& v) {
  ork::fxstring<256> fxs;
  fxs.format("[%f,%f,%f]", v.GetX(), v.GetY(), v.GetZ());
  str << fxs.c_str();
  return str;
}

///////////////////////////////////////////////////////////////////////////////

namespace LuaIntf {

LUA_USING_LIST_TYPE(std::vector)
LUA_USING_MAP_TYPE(std::map)

template <> struct LuaTypeMapping<ork::ent::ScriptVar> {
  static void push(lua_State* L, const ork::ent::ScriptVar& inp) {
    inp.pushToLua(L);
  }
  static ork::ent::ScriptVar get(lua_State* L, int index) {
    ork::ent::ScriptVar rval;
    rval.fromLua(L, index);
    return rval;
  }

  static ork::ent::ScriptVar opt(lua_State* L, int index, const ork::ent::ScriptVar& def) {
    return lua_isnoneornil(L, index) ? def : get(L, index);
  }
};

} // namespace LuaIntf

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

void ScriptVar::fromLua(lua_State* L, int index) {

  int type = lua_type(L, index);

  switch (type) {
    case LUA_TNONE:
    case LUA_TNIL:
      _encoded.Set<ScriptNil>(ScriptNil());
      break;
    case LUA_TNUMBER:
      _encoded.Set<double>(lua_tonumber(L, index));
      break;
    case LUA_TBOOLEAN:
      _encoded.Set<bool>(lua_toboolean(L, index));
      break;
    case LUA_TSTRING: {
      _encoded.Set<std::string>(lua_tostring(L, index));
      break;
    }
    case LUA_TLIGHTUSERDATA: {
      _encoded.Set<void*>(lua_touserdata(L, index));
      break;
    }
    case LUA_TUSERDATA: {
      if (auto pfvec3 = LuaIntf::CppObject::cast<fvec3>(L, index, false))
        _encoded.Set<fvec3>(*pfvec3);
      else {
        assert(false);
      }
      break;
    }
    case LUA_TTABLE: {
      auto& table = _encoded.Make<ScriptTable>();
      ScriptVar key, val;
      int j  = index;
      auto m = Lua::getMap<std::map<std::string, ScriptVar>>(L, index);
      for (auto item : m) {
        // printf( "GOTKEY<%s>\n", item.first.c_str());
        table._items[item.first] = item.second;
      }
      break;
    }
    default:
      assert(false);
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////

void ScriptVar::pushToLua(lua_State* L) const {
  assert(_encoded.IsSet());
  if (auto as_str = _encoded.TryAs<std::string>()) {
    lua_pushlstring(L, as_str.value().c_str(), as_str.value().length());
  } else if (auto as_number = _encoded.TryAs<double>()) {
    lua_pushnumber(L, as_number.value());
  } else if (auto as_bool = _encoded.TryAs<bool>()) {
    lua_pushboolean(L, int(as_bool.value()));
  } else if (auto as_number = _encoded.TryAs<float>()) {
    lua_pushnumber(L, double(as_number.value()));
  } else if (auto as_number = _encoded.TryAs<int>()) {
    lua_pushnumber(L, double(as_number.value()));
  } else if (auto as_number = _encoded.TryAs<int32_t>()) {
    lua_pushnumber(L, double(as_number.value()));
  } else if (auto as_vstar = _encoded.TryAs<void*>()) {
    lua_pushlightuserdata(L, as_vstar.value());
  } else {
    assert(false);
  }
}

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

LuaSystem::LuaSystem(Simulation* psi)
    : mSimulation(psi) {
  mLuaState = ::luaL_newstate(); // aka lua_open
  OrkAssert(mLuaState != nullptr);
  luaL_openlibs(mLuaState);
  auto L = mLuaState;

  lua_newtable(L);
  lua_setglobal(L, "ork");

  auto setGlobTabFn = [L](const char* tbl, const char* fnnam, lua_CFunction cf) {
    lua_getglobal(L, tbl);
    lua_pushstring(L, fnnam);
    lua_pushcfunction(L, cf);
    lua_settable(L, -3);
  };

  LuaBinding(L)
      .beginModule("ork")
      ////////////////////////////////////////
      ////////////////////////////////////////
      .beginClass<fvec3>("vec3")
      ////////////////////////////////////////
      .addConstructor(LUA_ARGS(float, float, float))
      //.def(tostring(self))
      //.property("xz", &fvec3,&SetVec3XZ)
      ////////////////////////////////////////
      .addProperty("x", &fvec3::GetX, &fvec3::SetX)
      ////////////////////////////////////////
      .addProperty("y", &fvec3::GetY, &fvec3::SetY)
      ////////////////////////////////////////
      .addProperty("z", &fvec3::GetZ, &fvec3::SetZ)
      ////////////////////////////////////////
      .addFunction("mag", [](const fvec3* v) -> float { return v->Mag(); })
      ////////////////////////////////////////
      .addFunction("normal", [](const fvec3* v) -> fvec3 { return v->Normal(); })
      ////////////////////////////////////////
      .addMetaFunction(
          "__tostring",
          [](const fvec3* v) -> std::string {
            fxstring<64> fxs;
            fxs.format("vec3(%g,%g,%g)", v->x, v->y, v->z);
            return fxs.c_str();
          })
      ////////////////////////////////////////
      .addMetaFunction("__add", [](const fvec3* a, const fvec3* b) -> fvec3 { return (*a) + (*b); })
      .addMetaFunction(
          "__sub",
          [](const fvec3* a, const fvec3* b) -> fvec3 {
            // printf("vec3.sub a<%p> b<%p>\n", a, b);
            return (*a) - (*b);
          })
      .addMetaFunction("__mul", [](const fvec3* a, float b) -> fvec3 { return (*a) * b; })
      .endClass()
      ////////////////////////////////////////
      ////////////////////////////////////////
      .beginClass<ComponentInst>("component")
      .addProperty(
          "type",
          [](const ComponentInst* c) -> std::string {
            auto clazz = c->GetClass();
            auto cn    = clazz->Name();
            return cn.c_str();
          })
      ////////////////////////////////////////
      .addProperty(
          "family",
          [](const ComponentInst* c) -> std::string {
            auto ps = c->GetFamily();
            return ps.c_str();
          })
      ////////////////////////////////////////
      .addFunction(
          "notify",
          [](ComponentInst* ci, const char* evcode, ScriptVar evdata) {
            auto clazz = ci->GetClass();
            auto cn    = clazz->Name();
            // printf("notify ci<%s> code<%s> ... \n", cn.c_str(), evcode);
            ComponentEvent ev;
            ev._eventID   = evcode;
            ev._eventData = evdata._encoded;
            ci->notify(ev);
          })
      ////////////////////////////////////////
      .addFunction(
          "query",
          [](ComponentInst* ci, const char* evcode, ScriptVar evdata) -> ScriptVar {
            auto clazz = ci->GetClass();
            auto cn    = clazz->Name();
            // printf("query\n");
            ComponentQuery q;
            q._eventID   = evcode;
            q._eventData = evdata._encoded;
            ScriptVar rval;
            rval._encoded = ci->query(q);
            return rval;
          })
      ////////////////////////////////////////
      .addMetaFunction(
          "__tostring",
          [](const ComponentInst* c) -> std::string {
            auto clazz = c->GetClass();
            auto cn    = clazz->Name();
            return cn.c_str();
          })
      .endClass()
      ////////////////////////////////////////
      ////////////////////////////////////////
      .beginClass<Entity>("entity")
      .addPropertyReadOnly("name", &Entity::name)
      ////////////////////////////////////////
      .addFunction(
          "pos",
          [](Entity* pent) {
            fvec3 pos = pent->GetEntityPosition();
            return pos;
          })
      ////////////////////////////////////////
      .addFunction("setPos", [](Entity* pent, fvec3 pos) { pent->setPos(pos); })
      ////////////////////////////////////////
      .addFunction(
          "setRotAxisAngle",
          [](Entity* pent, fvec3 axis, float ang) {
            fvec4 aa(axis, ang);
            pent->setRotAxisAngle(aa);
          })
      ////////////////////////////////////////
      .addPropertyReadOnly(
          "components",
          [](const Entity* e) -> std::map<std::string, ComponentInst*> {
            std::map<std::string, ComponentInst*> rval;
            for (auto item : e->GetComponents().GetComponents()) {
              auto c                = item.second;
              rval[c->scriptName()] = c;
            }
            // printf("ent<%p> components size<%zu>\n", e, rval.size());
            return rval;
          })
      ////////////////////////////////////////
      .addMetaFunction(
          "__tostring",
          [](const Entity* e) -> std::string {
            ork::fxstring<256> str;
            const char* ename = e->name().c_str();
            auto a            = e ? e->data()->GetArchetype() : nullptr;
            const char* aname = a ? a->GetName().c_str() : "";
            str.format("(ent<%s> arch<%s>)", ename, aname);
            return str.c_str();
          })

      .endClass()
      ////////////////////////////////////////
      ////////////////////////////////////////
      .beginClass<Simulation>("scene")
      .addFunction(
          "spawn",
          [](Simulation* psi, const char* arch, const char* entname, LuaRef spdata) -> Entity* {
            const auto& scenedata = psi->GetData();
            auto archnamestr      = std::string(arch);
            auto entnamestr       = std::string(entname);
            auto archso           = scenedata.FindSceneObjectByName(AddPooledString(archnamestr.c_str()));

            auto position = spdata.get<fvec3>("pos");

            // printf("SPAWN<%s:%p> ename<%s>\n", archnamestr.c_str(), archso, entnamestr.c_str());

            if (const Archetype* as_arch = rtti::autocast(archso)) {
              EntData* spawner = new EntData;
              spawner->SetName(AddPooledString(entnamestr.c_str()));
              spawner->SetArchetype(as_arch);
              auto mtx = fmtx4();
              mtx.compose(position, fquat(), 1.0f);
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
      .addFunction(
          "findEntity",
          [](Simulation* psi, const char* entname) -> Entity* {
            auto entname_str = AddPooledString(entname);
            auto e           = psi->FindEntity(entname_str);
            return e;
          })
      .endClass()
      ////////////////////////////////////////
      ////////////////////////////////////////
      .addFunction("scene", [=]() -> Simulation* { return psi; })
      ////////////////////////////////////////
      ////////////////////////////////////////
      .endModule();

  printf("create LuaState<%p> psi<%p>\n", mLuaState, psi);
}
LuaSystem::~LuaSystem() {
  printf("destroy LuaState<%p>\n", mLuaState);
  lua_close(mLuaState);
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
