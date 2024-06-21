////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/any.h>
#include <ork/math/cvector3.h>
#include <ork/pch.h>
#include <ork/reflect/properties/register.h>

#include <cxxabi.h>
#include <iostream>
#include <sstream>

#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>

#include <ork/ecs/lua/LuaComponent.h>

#include "LuaIntf/LuaIntf.h"
#include "LuaImpl.h"
#include "LuaBindings.h"

#include "../core/message_private.h"

using namespace LuaIntf;
using namespace std::literals;

///////////////////////////////////////////////////////////////////////////////

std::stringstream& operator<<(std::stringstream& str, const ork::fvec3& v) {
  ork::fxstring<256> fxs;
  fxs.format("[%f,%f,%f]", v.x, v.y, v.z);
  str << fxs.c_str();
  return str;
}

///////////////////////////////////////////////////////////////////////////////

namespace LuaIntf {

LUA_USING_LIST_TYPE(std::vector)
LUA_USING_MAP_TYPE(std::map)
LUA_USING_SHARED_PTR_TYPE(std::shared_ptr)

template <> struct LuaTypeMapping<ork::ecs::ScriptVar> {
  static void push(lua_State* L, const ork::ecs::ScriptVar& inp) {
    inp.pushToLua(L);
  }
  static ork::ecs::ScriptVar get(lua_State* L, int index) {
    ork::ecs::ScriptVar rval;
    rval.fromLua(L, index);
    return rval;
  }

  static ork::ecs::ScriptVar opt(lua_State* L, int index, const ork::ecs::ScriptVar& def) {
    return lua_isnoneornil(L, index) ? def : get(L, index);
  }
};

} // namespace LuaIntf

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ecs {
///////////////////////////////////////////////////////////////////////////////

void ScriptVar::fromLua(lua_State* L, int index) {

  int type = lua_type(L, index);

  switch (type) {
    case LUA_TNONE:
    case LUA_TNIL:
      _encoded.set<ScriptNil>(ScriptNil());
      break;
    case LUA_TNUMBER:
      _encoded.set<double>(lua_tonumber(L, index));
      break;
    case LUA_TBOOLEAN:
      _encoded.set<bool>(lua_toboolean(L, index));
      break;
    case LUA_TSTRING: {
      _encoded.set<std::string>(lua_tostring(L, index));
      break;
    }
    case LUA_TLIGHTUSERDATA: {
      //_encoded.set<void*>();

      auto wrapped = (ScriptWrapper*) lua_touserdata(L, index);

      OrkAssert(wrapped->_value.isA<token_t>());

      break;
    }
    case LUA_TUSERDATA: {
      if (auto pfvec3 = LuaIntf::CppObject::cast<fvec3>(L, index, false)){
        _encoded.set<fvec3>(*pfvec3);
      }
      else if (auto as_resp = LuaIntf::CppObject::cast<impl::_ComponentResponse>(L, index, false)){
        // hack for now until we can figure out how to bind by shared_ptr instead of by value
        auto rval = std::make_shared<impl::_ComponentResponse>();
        (*rval) = (*as_resp);
        _encoded.set<impl::comp_response_ptr_t>(rval);
      }
      else {
        OrkAssert(false);
      }
      break;
    }
    case LUA_TTABLE: {
      auto& table = _encoded.make<ScriptTable>();
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

//  auto L = luactx->mLuaState;

  lua_getglobal(L, "__ork_luacontext");
  auto ctx = (LuaContext*) lua_touserdata(L,-1);
  lua_pop(L, 1);

  assert(_encoded.isSet());
  if (auto as_str = _encoded.tryAs<std::string>()) {
    lua_pushlstring(L, as_str.value().c_str(), as_str.value().length());
  } else if (auto as_number = _encoded.tryAs<double>()) {
    lua_pushnumber(L, as_number.value());
  } else if (auto as_bool = _encoded.tryAs<bool>()) {
    lua_pushboolean(L, int(as_bool.value()));
  } else if (auto as_number = _encoded.tryAs<float>()) {
    lua_pushnumber(L, double(as_number.value()));
  } else if (auto as_number = _encoded.tryAs<double>()) {
    lua_pushnumber(L, double(as_number.value()));
  } else if (auto as_number = _encoded.tryAs<int>()) {
    lua_pushnumber(L, double(as_number.value()));
  } else if (auto as_number = _encoded.tryAs<int32_t>()) {
    lua_pushnumber(L, double(as_number.value()));
  } else if (auto as_vstar = _encoded.tryAs<void*>()) {
    lua_pushlightuserdata(L, as_vstar.value());
  } else if (auto as_token = _encoded.tryAs<token_t>()) {
    Lua::push(L,as_token.value());
  } else if (auto as_fvec3 = _encoded.tryAs<fvec3>()) {
    Lua::push(L,as_fvec3.value());
  } else if (auto as_fquat = _encoded.tryAs<fquat>()) {
    Lua::push(L,as_fquat.value());
  } else if (auto as_response = _encoded.tryAs<impl::comp_response_ptr_t>()) {
    Lua::push(L,as_response.value());
    //LuaRef::fromValue(L,as_response.value());
    //CppObjectSharedPtr::pushToStack
    //Lua::push(L,as_fquat.value());

  } else {
    OrkAssert(false);
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

LuaContext::LuaContext(Simulation* psi, LuaSystem* system)
    : mSimulation(psi)
    , _luasystem(system) {

  mLuaState = ::luaL_newstate(); // aka lua_open
  OrkAssert(mLuaState != nullptr);
  luaL_openlibs(mLuaState);
  auto L = mLuaState;

  lua_pushlightuserdata(L,(void*) this);
  lua_setglobal(L, "__ork_luacontext");

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
      .beginClass<token_t>("token_t")
      .addProperty("hash", [](const token_t* v) -> double { return double(v->hashed()); })
      .addMetaFunction("__eq", [](const token_t* a,const token_t* b) -> bool { 
        return a->hashed() == b->hashed();
      })
      .addMetaFunction(
          "__tostring",
          [](const token_t* v) -> std::string {

            auto str = detokenize(*v);
            fxstring<64> fxs;
            if(str.length()){
              fxs.format("tok(%zx:%s)", v->hashed(), str.c_str());
            }
            else{
              fxs.format("tok(%zx)", v->hashed());
            }
            return fxs.c_str();
          })
      .endClass()
      ////////////////////////////////////////
      .beginClass<fquat>("quat")
      .addConstructor(LUA_ARGS(fvec3, float))
      .addMetaFunction("__mul", [](const fquat* a, const fquat* b) -> fquat { return (*a) * (*b); })
      .addFunction("fromNormalVectors", [](fquat* _this, const fvec3* x,const fvec3* y,const fvec3* z) { //
          fmtx4 mtx;
          mtx.fromNormalVectors(*x,*y,*z);
          _this->fromMatrix(mtx); 
       })
      .endClass()
      ////////////////////////////////////////
      .beginClass<fvec3>("vec3")
      ////////////////////////////////////////
      .addConstructor(LUA_ARGS(float, float, float))
      //.def(tostring(self))
      //.property("xz", &fvec3,&SetVec3XZ)
      ////////////////////////////////////////
      .addProperty("x", [](const fvec3* v) -> float { return v->x; })
      ////////////////////////////////////////
      .addProperty("y", [](const fvec3* v) -> float { return v->y; })
      ////////////////////////////////////////
      .addProperty("z", [](const fvec3* v) -> float { return v->z; })
      ////////////////////////////////////////
      .addFunction("magnitude", [](const fvec3* v) -> float { return v->magnitude(); })
      ////////////////////////////////////////
      .addFunction("normalized", [](const fvec3* v) -> fvec3 { return v->normalized(); })
      ////////////////////////////////////////
      .addFunction("cross", [](const fvec3* v1, const fvec3* v2) -> fvec3 { return v1->crossWith(*v2); })
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
      .beginClass<Component>("component")
      .addProperty(
          "type",
          [](const Component* c) -> std::string {
            auto clazz = c->GetClass();
            auto cn    = clazz->Name();
            return cn.c_str();
          })
      ////////////////////////////////////////
      .addProperty(
          "family",
          [](const Component* c) -> std::string {
            auto ps = c->GetFamily();
            return ps.c_str();
          })
      ////////////////////////////////////////
      .addFunction(
          "notify",
          [](Component* ci, token_t evcode, ScriptVar evdata) {
            auto clazz = ci->GetClass();
            auto cn    = clazz->Name();
            auto sim = ci->sceneInst();
            ci->_notify(sim, evcode, evdata._encoded);
          })
      ////////////////////////////////////////
      .addFunction(
          "request",
          [](Component* ci, token_t evcode, ScriptVar evdata) -> ScriptVar {
            auto clazz = ci->GetClass();
            auto cn    = clazz->Name();
            auto sim = ci->sceneInst();
            auto ctrlr = sim->_controller;

            uint64_t respID = ctrlr->_objectIdCounter.fetch_add(1);

            auto rref = ResponseRef{._responseID = respID};

            auto response = std::make_shared<impl::_ComponentResponse>();
            //response->_compref = CRQ._compref;
            response->_requestID = evcode;
            response->_eventData = evdata._encoded;
            response->_respref = rref;

            ctrlr->_mutateObject([=](Controller::id2obj_map_t& unlocked) { //
              unlocked[respID].set<impl::comp_response_ptr_t>(response); //
            });

            ci->_request(sim, response, evcode, evdata._encoded);

            ScriptVar rval;
            rval._encoded = response;
            return rval;
          })
      ////////////////////////////////////////
      .addMetaFunction(
          "__tostring",
          [](const Component* c) -> std::string {
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
      .addFunction("setScale", [](Entity* pent, float scale) { pent->setScale(scale); })
      ////////////////////////////////////////
      .addFunction(
          "setRotAxisAngle",
          [](Entity* pent, fvec3 axis, float ang) {
            fvec4 aa(axis, ang);
            pent->setRotAxisAngle(aa);
          })
      ////////////////////////////////////////
      .addFunction("setRotation", [](Entity* pent, fquat rot) { pent->setRotation(rot); })
      ////////////////////////////////////////
      .addPropertyReadOnly(
          "components",
          [](const Entity* e) -> std::map<std::string, Component*> {
            //printf("ent<%p> get components\n", e );
            std::map<std::string, Component*> rval;
            const auto& comptable = e->GetComponents();
            const auto& components = comptable.GetComponents();
            for (auto item : components ) {
              auto c                = item.second;
              auto cname = c->scriptName();
              //printf("comp<%s> <%p>\n", cname, c );
              rval[cname] = c;
            }
            //printf("ent<%p> components size<%zu>\n", e, rval.size());
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
            auto scenedata   = psi->GetData();
            auto archnamestr = std::string(arch);
            auto entnamestr  = std::string(entname);
            auto archso      = scenedata->findSceneObjectByName(AddPooledString(archnamestr.c_str()));

            auto position = spdata.get<fvec3>("pos");

            // printf("SPAWN<%s:%p> ename<%s>\n", archnamestr.c_str(), archso, entnamestr.c_str());

            if (auto as_arch = std::dynamic_pointer_cast<const Archetype>(archso)) {
              auto spawner = std::make_shared<SpawnData>();
              auto named = AddPooledString(entnamestr.c_str());
              spawner->SetArchetype(as_arch);
              spawner->_dagnode->_xfnode->_transform->set(position, fquat(), 1.0f);
              auto newent = (Entity*) nullptr; //psi->_spawnNamedDynamicEntity(spawner,named);
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
            auto e           = psi->findEntity(entname_str);
            return e;
          })
      .endClass()
      ////////////////////////////////////////
      ////////////////////////////////////////
      .addFunction("scene", [=]() -> Simulation* { return psi; })
      .addFunction("tokenize", [=](const char* string_to_tokenize) -> token_t { 
        auto token = tokenize(string_to_tokenize);
        return token;
      })
      ////////////////////////////////////////
      .beginClass<impl::_ComponentResponse>("ComponentResponse")
      .endClass()
      ////////////////////////////////////////
      .endModule();

  //printf("create LuaState<%p> psi<%p>\n", (void*) mLuaState, (void*) psi);
}
LuaContext::~LuaContext() {
  //printf("destroy LuaState<%p>\n", (void*) mLuaState);
  lua_close(mLuaState);
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ecs
