////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <cxxabi.h>
#include "LuaIntf/LuaIntf.h"
#include <map>

#include <ork/ecs/types.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

struct LuaSystem;

struct ScriptNil {};

struct LuaContext {
  LuaContext(Simulation* psi, LuaSystem* system);
  ~LuaContext();
  lua_State* mLuaState = nullptr;
  Simulation* mSimulation = nullptr;
  LuaSystem* _luasystem = nullptr;

  std::unordered_map<uint64_t,scriptwrapper_t> _tokwrappers;
};

struct ScriptVar {
  svar64_t _encoded;

  void fromLua(lua_State* L, int index);
  void pushToLua(lua_State* L) const;
};
struct ScriptTable {
  std::map<std::string, ScriptVar> _items;
};


bool DoString(lua_State* L, const char* str);

} // namespace ork::ent
