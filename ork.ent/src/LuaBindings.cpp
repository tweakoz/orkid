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

#include <sstream>
#include <iostream>
#include <cxxabi.h>

#include "LuaBindings.h"

///////////////////////////////////////////////////////////////////////////////

std::stringstream& operator<<(std::stringstream& str, const ork::CVector3& v)
{
	ork::fxstring<256> fxs;
	fxs.format("[%f,%f,%f]", v.GetX(), v.GetY(), v.GetZ() );
	str<<fxs.c_str();
	return str;	
}

#include <luabind/operator.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

void LuaProtectedCallByRef(lua_State* L, int script_ref)
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
			lua_pop(L, 1);
		}
	}	
}

void LuaProtectedCallByName(lua_State* L, int script_ref,const char* name)
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
void LuaProtectedCallByName(lua_State* L, int script_ref,const char* name,luabind::object o)
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
void LuaProtectedCallByName(lua_State* L, int script_ref,const char* name,luabind::object o1,luabind::object o2)
{
	if( script_ref!=LUA_NOREF )
	{
		try
		{
			luabind::call_function<void>(L,name,o1,o2);
		}
		catch(const luabind::error& caught)
		{
			printf( "OnUpdate returned error<%s>\n", caught.what() );
		}
	}	
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
		//printf( "SPAWN<%s:%p> ename<%s>\n", archnamestr.c_str(), archso, entnamestr.c_str() );
		EntData* spawner = new EntData;
		spawner->SetName(AddPooledString(entnamestr.c_str()));
		spawner->SetArchetype(as_arch);
		auto newent = psi->SpawnDynamicEntity(spawner);
		return luabind::object(L,newent);
	}
	else
	{
		assert(false);
		//printf( "SPAWN<%s:%p>\n", archnamestr.c_str(), archso );
		return luabind::object(L,"");
	}
}
void DeSpawnEntity(lua_State* L,luabind::object scene, luabind::object ento)
{
	auto psi = object_cast<SceneInst*>(scene);
	auto as_ent = object_cast<Entity*>(ento);
	if( as_ent )
		psi->QueueDeactivateEntity(as_ent);
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
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
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
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
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
			.def("spawn", &SpawnEntity)
			.def("despawn", &DeSpawnEntity),

		class_<Entity,Entity*>("entity")
			.def("archetype", &GetEntArchetype )
			.def("name",	&GetEntityName )
			.property("pos", &GetEntityPos, &SetEntityPos)
			//.property("pos", &GetEntityAxisAng, &SetEntityAxisAng)
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

		/*class_<CVector4,CVector4*>("Vector4")
			.def(tostring(self))
			.property("x", &GetVec4X,&SetVec4X)
			.property("y", &GetVec4Y,&SetVec4Y)
			.property("z", &GetVec4Z,&SetVec4Z),
			.property("w", &GetVec4Z,&SetVec4Z),*/

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
}} // namespace ork { namespace ent {

