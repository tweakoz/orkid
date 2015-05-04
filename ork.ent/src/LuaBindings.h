#pragma once

extern "C" { 
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}


#include <luabind/luabind.hpp>
#include <luabind/raw_policy.hpp>

using namespace luabind;

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

///////////////////////////////////////////////////////////////////////////////

void LuaProtectedCallByRef(lua_State* L, int script_ref);
void LuaProtectedCallByName(lua_State* L, int script_ref,const char* name);
void LuaProtectedCallByName(lua_State* L, int script_ref,const char* name,luabind::object o);
void LuaProtectedCallByName(lua_State* L, int script_ref,const char* name,luabind::object o1,luabind::object o2);

template <typename T> static void LuaProtectedCallByNameT(lua_State* L, int script_ref,const char* name, const T& the_t )
{
	if( script_ref!=LUA_NOREF )
	{
		try
		{
			luabind::object lobj(L,the_t);
			luabind::call_function<void>(L,name,lobj);
		}
		catch(const luabind::error& caught)
		{
			printf( "OnUpdate returned error<%s>\n", caught.what() );
		}
	}	
}
bool DoString(lua_State* L, const char* str);

struct SCILuaData
{
	luabind::object mFN;
	luabind::object mARG;
};

}} // namespace ork { namespace ent {
