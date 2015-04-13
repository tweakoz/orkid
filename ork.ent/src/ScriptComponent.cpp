////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/scene.h>
#include <pkg/ent/scene.hpp>
#include <pkg/ent/ScriptComponent.h>

extern "C" { 
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

INSTANTIATE_TRANSPARENT_RTTI( ork::ent::ScriptComponentData, "ScriptComponentData" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::ScriptComponentInst, "ScriptComponentInst" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::ScriptManagerComponentData, "ScriptManagerComponentData" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::ScriptManagerComponentInst, "ScriptManagerComponentInst" );

namespace ork { namespace ent {

struct LuaSystem
{
	LuaSystem(SceneInst*psi);
	~LuaSystem();
	lua_State* mLuaState;
	SceneInst* mSceneInst;
};

///////////////////////////////////////////////////////////////////////////////

void ScriptComponentData::Describe()
{
	ork::ent::RegisterFamily<ScriptComponentData>(ork::AddPooledLiteral("control"));
}
ScriptComponentData::ScriptComponentData()
{

}

ent::ComponentInst* ScriptComponentData::CreateComponent(ent::Entity* pent) const
{
	return new ScriptComponentInst( *this, pent );
}
void ScriptComponentData::DoRegisterWithScene( ork::ent::SceneComposer& sc )
{
	sc.Register<ork::ent::ScriptManagerComponentData>();
}

///////////////////////////////////////////////////////////////////////////////

void ScriptComponentInst::Describe()
{
}

ScriptComponentInst::ScriptComponentInst( const ScriptComponentData& data, ent::Entity* pent )
	: ork::ent::ComponentInst( & data, pent )
	, mCD( data )
{


}

void ScriptComponentInst::DoUpdate(ork::ent::SceneInst* psi)
{
	
}

///////////////////////////////////////////////////////////////////////////////

void ScriptManagerComponentData::Describe()
{
}
ScriptManagerComponentData::ScriptManagerComponentData()
{

}

SceneComponentInst* ScriptManagerComponentData::CreateComponentInst(ork::ent::SceneInst *pinst) const
{
	return new ScriptManagerComponentInst( *this, pinst );
}

///////////////////////////////////////////////////////////////////////////////

int LuaNumEnties(lua_State* L)
{
	lua_getglobal(L, "orksys");
	auto udat = lua_touserdata(L,1);
	lua_pop(L,1);
	auto luasys = (LuaSystem*) udat;
	auto psi = luasys->mSceneInst;
	auto count = psi->Entities().size();	
	//int argc = lua_gettop(L);
	//printf("argc<%d>\n",argc);
	//for ( int n=1; n<=argc; ++n )
	//	printf( "arg<%d> '%s'\n", n, lua_tostring(L, n));
	//printf( "psi<%p> NumEntities<%d>\n", psi, int(count) );
	lua_pushnumber(L, int(count)); // return value
	return 1; // number of return values
}

///////////////////////////////////////////////////////////////////////////////

void ScriptManagerComponentInst::Describe()
{
}


ScriptManagerComponentInst::ScriptManagerComponentInst( const ScriptManagerComponentData& data, ork::ent::SceneInst *pinst )
	: ork::ent::SceneComponentInst( &data, pinst )
{
	auto luasys = new LuaSystem(pinst);
	mLuaManager.Set<LuaSystem*>(luasys);
}

ScriptManagerComponentInst::~ScriptManagerComponentInst()
{
	auto asluasys = mLuaManager.Get<LuaSystem*>();
	OrkAssert(asluasys);
	delete asluasys;
}

void ScriptManagerComponentInst::DoUpdate(SceneInst* psi) // final
{
	auto asluasys = mLuaManager.Get<LuaSystem*>();
	OrkAssert(asluasys);

	FixedString<1024> exec_str;
	exec_str.format("print(\"Hello world, from \",_VERSION,\"!\")\n");
	exec_str += "print(\"NumENT<\",NumEntities(),\">\")\n";
	int ret = luaL_dostring (asluasys->mLuaState, exec_str.c_str() );
	if( ret )
	{
		printf( "LUARET<%d>\n", ret );
		printf("%s\n", lua_tostring(asluasys->mLuaState, -1));
	}
}

/*
static int l_printf (lua_State *L) {
  lua_pushvalue(L, lua_upvalueindex(2));
  lua_insert(L, 1);
  lua_call(L, lua_gettop(L) - 1, 1);
  lua_pushvalue(L, lua_upvalueindex(1));
  lua_pushvalue(L, -2);
  lua_call(L, 1, 0);
  return 0;
}

int luaopen_printf (lua_State* L) {
  lua_getglobal(L, "io");
  lua_getglobal(L, "string");
  lua_pushliteral(L, "write");
  lua_gettable(L, -3);
  lua_pushliteral(L, "format");
  lua_gettable(L, -3);
  lua_pushcclosure(L, l_printf, 2);
  // With 5.1, I'd probably just return 1 at this point 
  lua_setglobal(L, "printf");
  return 0;
}

*/

LuaSystem::LuaSystem(SceneInst*psi)
	: mSceneInst(psi)
{
	mLuaState = ::luaL_newstate(); // aka lua_open

	luaopen_io(mLuaState);
	luaopen_base(mLuaState);
	luaopen_table(mLuaState);
	luaopen_string(mLuaState);
	luaopen_math(mLuaState);
	//luaopen_loadlib(mLuaState);

 	lua_register(mLuaState, "NumEntities", LuaNumEnties);

	lua_pushlightuserdata(mLuaState,(void*)this);
	lua_setglobal(mLuaState, "orksys");


	printf( "create LuaState<%p> psi<%p>\n", mLuaState, psi );

}
LuaSystem::~LuaSystem()
{
	printf( "destroy LuaState<%p>\n", mLuaState );
	lua_close(mLuaState);
}

///////////////////////////////////////////////////////////////////////////////

} }  // ork::ent3d

