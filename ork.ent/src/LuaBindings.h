#pragma once

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <cxxabi.h>
#include "LuaIntf/LuaIntf.h"
#include <map>

///////////////////////////////////////////////////////////////////////////////
namespace ork::ent {
///////////////////////////////////////////////////////////////////////////////

struct ScriptNil {};

struct ScriptVar
{
	svar64_t _encoded;

	void fromLua(lua_State* L, int index);
	void pushToLua(lua_State* L) const;

};
struct ScriptTable
{
  std::map<std::string,ScriptVar> _items;
};

struct LuaSystem
{
	LuaSystem(SceneInst*psi);
	~LuaSystem();
	lua_State* mLuaState;
	SceneInst* mSceneInst;
};

bool DoString(lua_State* L, const char* str);

} // namespace ork::ent {
