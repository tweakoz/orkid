#pragma once

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <cxxabi.h>
#include "LuaIntf/LuaIntf.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

struct LuaOpaque64
{
	any64 mValue;
};
struct LuaOpaque16
{
	any16 mValue;
	LuaOpaque16(){}
	template <typename T> LuaOpaque16(const T& the_t)
	{
		mValue.Set<T>(the_t);
	}
	std::string GetType() const
	{
		int status;
		auto mangled = mValue.GetTypeName();
		auto demangled = abi::__cxa_demangle( mangled, 0, 0, & status );
		return demangled;
	}
};

struct LuaSystem
{
	LuaSystem(SceneInst*psi);
	~LuaSystem();
	lua_State* mLuaState;
	SceneInst* mSceneInst;
};

bool DoString(lua_State* L, const char* str);

struct SCILuaData
{
};

}} // namespace ork { namespace ent {
