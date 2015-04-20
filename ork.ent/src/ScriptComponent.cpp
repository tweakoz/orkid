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

///////////////////////////////////////////////////////////////////////////////

extern "C" { 
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <luabind/luabind.hpp>

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI( ork::ent::ScriptComponentData, "ScriptComponentData" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::ScriptComponentInst, "ScriptComponentInst" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::ScriptManagerComponentData, "ScriptManagerComponentData" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::ScriptManagerComponentInst, "ScriptManagerComponentInst" );

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

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

	ork::reflect::RegisterProperty( "ScriptFile", & ScriptComponentData::mScriptPath );
	ork::reflect::AnnotatePropertyForEditor<ScriptComponentData>("ScriptFile", "editor.class", "ged.factory.filelist");
	ork::reflect::AnnotatePropertyForEditor<ScriptComponentData>("ScriptFile", "editor.filetype", "lua");
	ork::reflect::AnnotatePropertyForEditor<ScriptComponentData>("ScriptFile", "editor.filebase", "src://scripts/");
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

bool ScriptComponentInst::DoLink(ork::ent::SceneInst *psi)
{
	auto scm = psi->FindTypedSceneComponent<ScriptManagerComponentInst>();

	if( scm )
	{
		auto asluasys = scm->GetLuaManager().Get<LuaSystem*>();
		OrkAssert(asluasys);

		auto ent = this->GetEntity();
		auto name = ent->GetEntData().GetName().c_str();

		printf( "LINKING SCRIPTCOMPONENT<%p> of ent<%s> into Lua exec list\n", this, name );

		/////////////////////////
		// TODO: link this script component into lua's execution list somehow
		/////////////////////////



	}
	return true;
}
void ScriptComponentInst::DoUnLink(ork::ent::SceneInst *psi)
{
	auto scm = psi->FindTypedSceneComponent<ScriptManagerComponentInst>();

	if( scm )
	{
		auto asluasys = scm->GetLuaManager().Get<LuaSystem*>();
		OrkAssert(asluasys);
	}
}

bool ScriptComponentInst::DoStart(SceneInst *psi, const CMatrix4 &world)
{
	auto scm = psi->FindTypedSceneComponent<ScriptManagerComponentInst>();

	if( scm )
	{
		auto asluasys = scm->GetLuaManager().Get<LuaSystem*>();
		OrkAssert(asluasys);

		auto ent = this->GetEntity();
		auto name = ent->GetEntData().GetName().c_str();
		auto path = mCD.GetPath();
		auto abspath = path.ToAbsolute();

		printf( "STARTING SCRIPTCOMPONENT<%p> of ent<%s> pth<%s> into Lua exec list\n", this, name, abspath.c_str() );

		if( abspath.DoesPathExist() )
		{
			CFile scriptfile(abspath,EFM_READ);
			size_t filesize = 0;
			scriptfile.GetLength(filesize);
			char* scripttext = (char*) malloc(filesize+1);
			scriptfile.Read(scripttext,filesize);
			scripttext[filesize] = 0;
			mScriptText = scripttext;
			printf( "%s\n", scripttext);
			free(scripttext);
		}

		/////////////////////////
		// TODO: link this script component into lua's execution list somehow
		/////////////////////////



	}
	return true;
}
void ScriptComponentInst::DoStop(SceneInst *psi)
{

}


void ScriptComponentInst::DoUpdate(ork::ent::SceneInst* psi)
{
	// NOP (scriptmanager will execute)
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

void ScriptManagerComponentInst::Describe()
{
}


ScriptManagerComponentInst::ScriptManagerComponentInst( const ScriptManagerComponentData& data, ork::ent::SceneInst *pinst )
	: ork::ent::SceneComponentInst( &data, pinst )
	, mScriptRef(LUA_NOREF)
{
	auto luasys = new LuaSystem(pinst);
	mLuaManager.Set<LuaSystem*>(luasys);

	auto path = file::Path("src://scripts/scene.lua");
	auto abspath = path.ToAbsolute();

	if( abspath.DoesPathExist() )
	{
		CFile scriptfile(abspath,EFM_READ);
		size_t filesize = 0;
		scriptfile.GetLength(filesize);
		char* scripttext = (char*) malloc(filesize+1);
		scriptfile.Read(scripttext,filesize);
		scripttext[filesize] = 0;
		mScriptText = scripttext;
		printf( "%s\n", scripttext);
		free(scripttext);


		auto asluasys = mLuaManager.Get<LuaSystem*>();
		OrkAssert(asluasys);

		int ret = luaL_loadstring(asluasys->mLuaState,mScriptText.c_str());

		mScriptRef = luaL_ref(asluasys->mLuaState, LUA_REGISTRYINDEX);
		printf( "mScriptRef<%d>\n", mScriptRef );
		lua_pop(asluasys->mLuaState, 1); // dont call, just reference

	}

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

	if( mScriptRef!=LUA_NOREF )
	{
		// bring up the previously parsed script
		lua_rawgeti(asluasys->mLuaState, LUA_REGISTRYINDEX, mScriptRef);

		// execute it
		int ret = lua_pcall(asluasys->mLuaState,0,0,0);
		if( ret )
		{
			printf( "LUARET<%d>\n", ret );
			printf("%s\n", lua_tostring(asluasys->mLuaState, -1));
		}
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

static SceneInst* GetSceneInst(lua_State* L)
{
	lua_getglobal(L, "orksys");
	auto udat = lua_touserdata(L,1);
	lua_pop(L,1);
	auto luasys = (LuaSystem*) udat;
	auto psi = luasys->mSceneInst;
	return psi;
}


class LuaScene
{

public:
	LuaScene()
	{
		printf( "LuaScene::LuaScene()\n");
	}
	~LuaScene()
	{
		printf( "LuaScene::~LuaScene()\n");
	}

 	int NumEntities(lua_State* L)
	{
		printf( "LuaScene::NumEntities(st:%p)\n", L);
		auto psi = GetSceneInst(L);
		auto count = psi->Entities().size();	
		//int argc = lua_gettop(L);
		//printf("argc<%d>\n",argc);
		//for ( int n=1; n<=argc; ++n )
		//	printf( "arg<%d> '%s'\n", n, lua_tostring(L, n));
		//printf( "psi<%p> NumEntities<%d>\n", psi, int(count) );
		lua_pushnumber(L, int(count)); // return value
		return int(count); // number of return values
	}

	int GetEntities(lua_State* L)
	{
		auto psi = GetSceneInst(L);
		/*LuaRef table = LuaRef::createTable(L);

		const auto& ents = psi->Entities();	


		for( const auto& item : ents )
		{
		    table[item.first.c_str()] = item.second;
		    //int value = table.get<int>("value");

		    //for (auto& e : table) {
		    //    std::string key = e.key<std::string>();
		    //    LuaRef value = e.value<LuaRef>();
		    //    ...
		   // }
		}
		LuaBinding(table);
		table.pushToStack();*/
        return 1;
    }
};

LuaSystem::LuaSystem(SceneInst*psi)
	: mSceneInst(psi)
{
	mLuaState = ::luaL_newstate(); // aka lua_open
	luaL_openlibs(mLuaState);

 	//lua_register(mLuaState, "NumEntities", LuaNumEnties);

	lua_pushlightuserdata(mLuaState,(void*)this);
	lua_setglobal(mLuaState, "orksys");

	/*LuaBinding(mLuaState)
		.beginModule("ork")
        .beginClass<LuaScene>("scene")	
		.addConstructor(LUA_ARGS())
		.addFunction("NumEntities", & LuaScene::NumEntities )
		.addFunction("GetEntities", & LuaScene::GetEntities )
		.endClass()
		.endModule()
	;*/
	
	printf( "create LuaState<%p> psi<%p>\n", mLuaState, psi );

}
LuaSystem::~LuaSystem()
{
	printf( "destroy LuaState<%p>\n", mLuaState );
	lua_close(mLuaState);
}

///////////////////////////////////////////////////////////////////////////////

} }  // ork::ent3d

