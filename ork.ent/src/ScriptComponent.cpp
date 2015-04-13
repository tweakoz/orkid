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

struct LuaSystem
{
	lua_State* mLuaState;

	LuaSystem()
	{
		mLuaState = ::luaL_newstate(); // aka lua_open

		luaopen_io(mLuaState);
		luaopen_base(mLuaState);
		luaopen_table(mLuaState);
		luaopen_string(mLuaState);
		luaopen_math(mLuaState);
		//luaopen_loadlib(mLuaState);

		printf( "create LuaState<%p>\n", mLuaState );
	}
	~LuaSystem()
	{
		printf( "destroy LuaState<%p>\n", mLuaState );
		lua_close(mLuaState);
	}
};

void ScriptManagerComponentInst::Describe()
{
}


ScriptManagerComponentInst::ScriptManagerComponentInst( const ScriptManagerComponentData& data, ork::ent::SceneInst *pinst )
	: ork::ent::SceneComponentInst( &data, pinst )
{
	auto luasys = new LuaSystem;
	mLuaManager.Set<LuaSystem*>(luasys);
}

ScriptManagerComponentInst::~ScriptManagerComponentInst()
{
	auto asluasys = mLuaManager.Get<LuaSystem*>();
	OrkAssert(asluasys);
	delete asluasys;
}

void ScriptManagerComponentInst::DoUpdate(SceneInst *inst) // final
{
	auto asluasys = mLuaManager.Get<LuaSystem*>();
	OrkAssert(asluasys);

	FixedString<1024> exec_str;
	exec_str.format("print(\"Hello world, from \",_VERSION,\"!\")");

	int ret = luaL_dostring (asluasys->mLuaState, exec_str.c_str() );

}

///////////////////////////////////////////////////////////////////////////////

} }  // ork::ent3d

