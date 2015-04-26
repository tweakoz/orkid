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
#include <ork/kernel/any.h>

///////////////////////////////////////////////////////////////////////////////

extern "C" { 
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <sstream>
#include <iostream>

#include <luabind/luabind.hpp>
#include <luabind/raw_policy.hpp>

#include <cxxabi.h>

using namespace luabind;

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI( ork::ent::ScriptComponentData, "ScriptComponentData" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::ScriptComponentInst, "ScriptComponentInst" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::ScriptManagerComponentData, "ScriptManagerComponentData" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::ScriptManagerComponentInst, "ScriptManagerComponentInst" );

std::stringstream& operator<<(std::stringstream& str, const ork::CVector3& v)
{
	ork::fxstring<256> fxs;
	fxs.format("[%f,%f,%f]", v.GetX(), v.GetY(), v.GetZ() );
	str<<fxs.c_str();
	return str;	
}

bool DoString(lua_State* L, const char* str)
{
	if (luaL_loadbuffer(L, str, std::strlen(str), str) || lua_pcall(L, 0, 0, 0))
	{
		const char* a = lua_tostring(L, -1);
		std::cout << a << "\n";
		lua_pop(L, 1);
		return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////

#include <luabind/operator.hpp>

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

static void LuaProtectedCallByRef(lua_State* L, int script_ref)
{
	if( script_ref!=LUA_NOREF )
	{
		// bring up the previously parsed script
		lua_rawgeti(L, LUA_REGISTRYINDEX, script_ref);

		// execute it
		int ret = lua_pcall(L,0,0,0);
		if( ret )
		{
			printf( "LUARET<%d>\n", ret );
			printf("%s\n", lua_tostring(L, -1));
		}
	}	
}

static void LuaProtectedCallByName(lua_State* L, int script_ref,const char* name)
{
	if( script_ref!=LUA_NOREF )
	{
		try
		{
			luabind::call_function<void>(L,name);
		}
		catch(const luabind::error& caught)
		{
			assert(false);
			//printf( "OnUpdate returned error<%s>\n", caught.what() );
		}
	}	
}
static void LuaProtectedCallByName(lua_State* L, int script_ref,const char* name,luabind::object o)
{
	if( script_ref!=LUA_NOREF )
	{
		try
		{
			luabind::call_function<void>(L,name,o);
		}
		catch(const luabind::error& caught)
		{
			printf( "OnUpdate returned error<%s>\n", caught.what() );
		}
	}	
}
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
	printf( "ScriptComponentData::ScriptComponentData() this: %p\n", this );
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
	, mScriptRef(LUA_NOREF)
{
	auto psi = pent->GetSceneInst();

	auto scm = psi->FindTypedSceneComponent<ScriptManagerComponentInst>();

	if( scm )
	{
		auto asluasys = scm->GetLuaManager().Get<LuaSystem*>();
		OrkAssert(asluasys);

		auto name = pent->GetEntData().GetName().c_str();
		auto path = mCD.GetPath();
		auto abspath = path.ToAbsolute();

		//printf( "STARTING SCRIPTCOMPONENT<%p> of ent<%s> pth<%s> into Lua exec list\n", this, name, abspath.c_str() );

		if( abspath.DoesPathExist() )
		{
			mPrefix = path.GetName().c_str();

			CFile scriptfile(abspath,EFM_READ);
			size_t filesize = 0;
			scriptfile.GetLength(filesize);
			char* scripttext = (char*) malloc(filesize+1);
			scriptfile.Read(scripttext,filesize);
			scripttext[filesize] = 0;
			mScriptText = scripttext;
			//printf( "%s\n", scripttext);
			free(scripttext);

			auto luast = asluasys->mLuaState;

		    auto wrap_table =luabind::newtable(luast);
		    wrap_table["x"]=0.0;
		    wrap_table["y"]=6.0;

		    //lua_getfenv(luast,-1);


			luabind::globals(luast)["arch"] = wrap_table;
			//wrap_table.push(luast);


    		//assert(lua_setfenv(luast, -1) != 0);

			int ret = luaL_loadstring(luast,mScriptText.c_str());
			mScriptRef = luaL_ref(luast, LUA_REGISTRYINDEX);
			//printf( "mScriptRef<%d>\n", mScriptRef );
			//lua_pop(asluasys->mLuaState, 1); // dont call, just reference

			LuaProtectedCallByRef( luast, mScriptRef );
		}

		/////////////////////////
		// TODO: link this script component into lua's execution list somehow
		/////////////////////////



	}



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

		//printf( "LINKING SCRIPTCOMPONENT<%p> of ent<%s> into Lua exec list\n", this, name );

		luabind::object o( asluasys->mLuaState, ent );

		LuaProtectedCallByName(asluasys->mLuaState,mScriptRef,"OnEntityLink",o);

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
		auto L = asluasys->mLuaState;

		auto ent = this->GetEntity();
		auto name = ent->GetEntData().GetName().c_str();

		printf( "Starting SCRIPTCOMPONENT<%p> of ent<%p:%s> into Lua exec list\n", this, ent, name );

		//DoString( L, "printf(\"oeu: %s\",inspect(OnEntityUpdate))");
		luabind::object osu_fn = luabind::globals(L)["OnEntityUpdate"];
		assert(osu_fn.is_valid());
		luabind::object osu_arg(L,ent);
		auto wrap_table =luabind::newtable(L);
	    wrap_table["fn"]=osu_fn;
	    wrap_table["ent"]=osu_arg;

		//luabind::call_function<void>(osu_fn,osu_arg);

		//assert(false);

		//fxstring<256> autoreg;
		//autoreg.format("entity_exec_table[e:name()]= function() OnScriptUpdate(e) end")
		//luaL_dostring(lua, "return 'derp'");
		luabind::object exectab = luabind::globals(L)["entity_exec_table"];// = exec_table;
		exectab[name]=wrap_table;

		//LuaProtectedCallByNameT<Entity*>(L,mScriptRef,"OnEntityStart",ent);

		/////////////////////////
		// TODO: link this script component into lua's execution list somehow
		/////////////////////////



	}
	return true;
}
void ScriptComponentInst::DoStop(SceneInst *psi)
{
	auto scm = psi->FindTypedSceneComponent<ScriptManagerComponentInst>();

	if( scm )
	{
		auto asluasys = scm->GetLuaManager().Get<LuaSystem*>();
		OrkAssert(asluasys);
		auto L = asluasys->mLuaState;

		auto ent = this->GetEntity();
		auto name = ent->GetEntData().GetName().c_str();

		//printf( "LINKING SCRIPTCOMPONENT<%p> of ent<%s> into Lua exec list\n", this, name );

		luabind::object exectab = luabind::globals(L)["entity_exec_table"];// = exec_table;
		exectab[name]=luabind::nil;

		LuaProtectedCallByNameT<Entity*>(asluasys->mLuaState,mScriptRef,"OnEntityStop",ent);

		/////////////////////////
		// TODO: link this script component into lua's execution list somehow
		/////////////////////////



	}
}


void ScriptComponentInst::DoUpdate(ork::ent::SceneInst* psi)
{
	//printf( "NOP\n");
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

	///////////////////////////////////////////////

	auto AppendPath = [&]( const char* pth )
	{
	    lua_getglobal( luasys->mLuaState, "package" );
	    lua_getfield( luasys->mLuaState, -1, "path" );

	    fxstring<256> lua_path;
	    lua_path.format(	"%s;%s", 
	    					lua_tostring( luasys->mLuaState, -1 ),
	    					pth );

	    lua_pop( luasys->mLuaState, 1 );
	    lua_pushstring( luasys->mLuaState, lua_path.c_str() );
	    lua_setfield( luasys->mLuaState, -2, "path" );
	    lua_pop( luasys->mLuaState, 1 );
	};

	///////////////////////////////////////////////
	// Set Lua Search Path
	///////////////////////////////////////////////

	auto scrpath = file::Path("src://scripts/");
	auto absscrpath = scrpath.ToAbsolute();


	if( absscrpath.DoesPathExist() )
	{
	    fxstring<256> lua_path;
	    lua_path.format( "%s?.lua", absscrpath.c_str() );
	    AppendPath( lua_path.c_str() );
	}

	///////////////////////////////////////////////
	// find & init scene file
	///////////////////////////////////////////////

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
		//printf( "%s\n", scripttext);
		free(scripttext);


		auto asluasys = mLuaManager.Get<LuaSystem*>();
		OrkAssert(asluasys);

		int ret = luaL_loadstring(asluasys->mLuaState,mScriptText.c_str());

		mScriptRef = luaL_ref(asluasys->mLuaState, LUA_REGISTRYINDEX);
		//printf( "mScriptRef<%d>\n", mScriptRef );
		//lua_pop(asluasys->mLuaState, 1); // dont call, just reference


		LuaProtectedCallByRef( asluasys->mLuaState, mScriptRef );

	}

}

ScriptManagerComponentInst::~ScriptManagerComponentInst()
{
	auto asluasys = mLuaManager.Get<LuaSystem*>();
	OrkAssert(asluasys);
	delete asluasys;
}

bool ScriptManagerComponentInst::DoLink(SceneInst* psi) // final
{
	printf( "ScriptManagerComponentInst::DoLink()\n" );
	auto asluasys = mLuaManager.Get<LuaSystem*>();
	OrkAssert(asluasys);
	LuaProtectedCallByName( asluasys->mLuaState, mScriptRef, "OnSceneLink");

	return true;
}
void ScriptManagerComponentInst::DoUnLink(SceneInst* psi) // final
{
	printf( "ScriptManagerComponentInst::DoUnLink()\n" );
	auto asluasys = mLuaManager.Get<LuaSystem*>();
	OrkAssert(asluasys);
	LuaProtectedCallByName( asluasys->mLuaState, mScriptRef, "OnSceneUnLink");
}
void ScriptManagerComponentInst::DoStart(SceneInst *psi) // final
{
	printf( "ScriptManagerComponentInst::DoStart()\n" );
	auto asluasys = mLuaManager.Get<LuaSystem*>();
	OrkAssert(asluasys);
	LuaProtectedCallByName( asluasys->mLuaState, mScriptRef, "OnSceneStart");
}
void ScriptManagerComponentInst::DoStop(SceneInst *inst) // final
{
	printf( "ScriptManagerComponentInst::DoStop()\n" );
	auto asluasys = mLuaManager.Get<LuaSystem*>();
	OrkAssert(asluasys);
	LuaProtectedCallByName( asluasys->mLuaState, mScriptRef, "OnSceneStop");
}

void ScriptManagerComponentInst::DoUpdate(SceneInst* psi) // final
{
	/////////////
	// call entity script components for 5 ms


	/////////////


	auto asluasys = mLuaManager.Get<LuaSystem*>();
	OrkAssert(asluasys);
	LuaProtectedCallByName( asluasys->mLuaState, mScriptRef, "OnSceneUpdate");
}

///////////////////////////////////////////////////////////////////////////////

SceneInst* GetSceneInst(lua_State* L)
{
	luabind::object globtab = globals(L)["luascene"];
	auto luasys = object_cast<LuaSystem*>(globtab[1]);
	auto psi = luasys->mSceneInst;
	return psi;
}

luabind::object GetScene(lua_State* L)
{
	auto psi = GetSceneInst(L);
	return luabind::object(L,psi);
}

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
		printf( "SPAWN<%s:%p> ename<%s>\n", archnamestr.c_str(), archso, entnamestr.c_str() );
		EntData* spawner = new EntData;
		spawner->SetName(AddPooledString(entnamestr.c_str()));
		spawner->SetArchetype(as_arch);
		auto newent = psi->SpawnDynamicEntity(spawner);
		return luabind::object(L,newent);
	}
	else
	{
		printf( "SPAWN<%s:%p>\n", archnamestr.c_str(), archso );
		return luabind::object(L,"");
	}
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
	auto v = new CVector3(float(x),float(y),float(z));
	return luabind::object(L,v);
}
luabind::object GetVec3XZ( lua_State* L,  luabind::object o )
{
	auto v = object_cast<CVector3*>(o);
	fxstring<256> fx;
	fx.format("[%f,%f]", v->GetX(), v->GetZ() );
	std::string rval = fx.c_str();
	return luabind::object(L,rval);
}
luabind::object GetVec3X( lua_State* L,  luabind::object o )
{
	auto v = object_cast<CVector3*>(o);
	return luabind::object(L,double(v->GetX()));
}
luabind::object GetVec3Y( lua_State* L,  luabind::object o )
{
	auto v = object_cast<CVector3*>(o);
	return luabind::object(L,double(v->GetY()));
}
luabind::object GetVec3Z( lua_State* L,  luabind::object o )
{
	auto v = object_cast<CVector3*>(o);
	return luabind::object(L,double(v->GetZ()));
}
void SetVec3XZ( lua_State* L,  luabind::object o, double x, double z )
{
	auto v = object_cast<CVector3*>(o);
	v->SetX(float(x));
	v->SetZ(float(z));
}
void SetVec3X( lua_State* L,  luabind::object o, double x )
{
	auto v = object_cast<CVector3*>(o);
	v->SetX(float(x));
}
void SetVec3Y( lua_State* L,  luabind::object o, double y )
{
	auto v = object_cast<CVector3*>(o);
	v->SetY(float(y));
}
void SetVec3Z( lua_State* L,  luabind::object o, double z )
{
	auto v = object_cast<CVector3*>(o);
	v->SetZ(float(z));
}
luabind::object GetEntityName( lua_State* L, luabind::object o )
{
	auto e = object_cast<const Entity*>(o);
	const char* ename = e ? e->GetEntData().GetName().c_str() : "";
	return luabind::object(L,std::string(ename));
}
luabind::object GetEntityPos( lua_State* L, luabind::object o )
{
	auto pos = new CVector3;
	auto ent = object_cast<Entity*>(o);
	if( ent )
	{
		auto& dn = ent->GetDagNode();
		auto& xn = dn.GetTransformNode();
		auto& xf = xn.GetTransform();

		*pos = xf.GetPosition();
	}
	return luabind::object(L,pos);
}
void SetEntityPos( lua_State* L, luabind::object e, luabind::object p )
{
	auto ent = object_cast<Entity*>(e);
	auto pos = object_cast<CVector3*>(p);
	if( ent && pos )
	{
		ent->GetDagNode().GetTransformNode().GetTransform().SetPosition(*pos);
	}
}
std::string EntToString( const Entity* e )
{
	ork::fxstring<256> str;
	const char* ename = e ? e->GetEntData().GetName().c_str() : "";
	auto a = e ? e->GetEntData().GetArchetype() : nullptr;
	const char* aname =  a ? a->GetName().c_str() : "";
	str.format("(%s:%s)", ename, aname);
	return str.c_str();
}
std::string ArchToString( const Archetype* a )
{
	ork::fxstring<256> rval;
	if( a )
	{
		ork::fxstring<256> str;
		auto aname = a->GetName().c_str();
		auto cname = a->GetClass()->Name().c_str();
		str.format( "(%s:%s)\n", aname, cname );
		rval += str;

		const auto& ct = a->GetComponentDataTable().GetComponents();
		for( const auto& i : ct )
		{
			auto c = i.second;
			auto f = i.first.c_str();
			auto ctype = c->GetClass()->Name();
			str.format( "	(%s:%s)\n", f, ctype );
			rval += str;
		}
	}
	return rval.c_str();
}

///////////////////////////////////////////////////////////////////////////////

LuaSystem::LuaSystem(SceneInst*psi)
	: mSceneInst(psi)
{
	mLuaState = ::luaL_newstate(); // aka lua_open
	luaL_openlibs(mLuaState);
	luabind::open(mLuaState);
	/////////////////////////////////////
	module(mLuaState,"ork")
    [
    	def("getscene",&GetScene),

		class_<SceneInst,SceneInst*>("scene")
			.def("entities", &GetEntities )
			.def("spawn", &SpawnEntity),

		class_<Entity,Entity*>("entity")
			.def("archetype", &GetEntArchetype )
			.def("name",	&GetEntityName )
			.property("pos", &GetEntityPos, &SetEntityPos)
			.def("__tostring",&EntToString),

		class_<Archetype,Archetype*>("archetype")
			.def("name",	&GetArchetypeName )
			.def("__tostring",&ArchToString),

		class_<LuaSystem,LuaSystem*>("LuaSystem"),

		class_<LuaOpaque64>("Opaque64"),

		class_<LuaOpaque16>("Opaque16")
			.def("type", &LuaOpaque16::GetType ),

		class_<CVector3,CVector3*>("Vector3")
			.def(tostring(self))
			.property("xz", &GetVec3XZ,&SetVec3XZ)
			.property("x", &GetVec3X,&SetVec3X)
			.property("y", &GetVec3Y,&SetVec3Y)
			.property("z", &GetVec3Z,&SetVec3Z),

		def("vec3",&CreateVector3)

    ];
	/////////////////////////////////////
	luabind::object globtab = luabind::newtable(mLuaState);
	globtab[1] = this;
	globals(mLuaState)["luascene"] = globtab;
	/////////////////////////////////////
	auto exec_table =luabind::newtable(mLuaState);
	luabind::globals(mLuaState)["entity_exec_table"] = exec_table;
	/////////////////////////////////////
	printf( "create LuaState<%p> psi<%p>\n", mLuaState, psi );

}
LuaSystem::~LuaSystem()
{
	printf( "destroy LuaState<%p>\n", mLuaState );
	lua_close(mLuaState);
}

///////////////////////////////////////////////////////////////////////////////

} }  // ork::ent3d

