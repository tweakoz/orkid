////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/application/application.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/drawable.h>
#include <pkg/ent/entity.h>
#include <ork/kernel/string/string.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/DirectObjectMapPropertyType.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/lev2/gfx/renderer.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/audiobank.h>
#include <ork/asset/AssetManager.h>
#include <pkg/ent/ReferenceArchetype.h>

#include <ork/stream/StringInputStream.h>
#include <ork/stream/ResizableStringOutputStream.h>
#include <ork/reflect/serialize/XMLDeserializer.h>
#include <ork/reflect/serialize/XMLSerializer.h>

#include <ork/lev2/lev2_asset.h>
#include <ork/kernel/orklut.hpp>
#include <ork/math/basicfilters.h>
#include <ork/kernel/opq.h>

template class ork::orklut<const ork::object::ObjectClass *, ork::ent::SceneComponentData *>;

using namespace ork::reflect;

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SceneData,"Ent3dSceneData");

#define VERBOSE 0
#define PRINT_CONDITION (ork::PieceString(pent->GetEntData().GetName()).find("rocket") != ork::PieceString::npos)

#if VERBOSE
#	define DEBUG_PRINT	if(PRINT_CONDITION) orkprintf
#else
#	define DEBUG_PRINT(...)
#endif

#if defined(_DARWIN)
static dispatch_queue_t TheEditorQueue()
{
	static dispatch_queue_t EQ = dispatch_queue_create("com.TweakoZ.OrkTool.EditorQueue", NULL);
	return EQ;
}

dispatch_queue_t EditOnlyQueue()
{
	return TheEditorQueue();
}
dispatch_queue_t MainQueue()
{
	return dispatch_get_main_queue();
}
#endif

namespace ork {
	

///////////////////////////////////////////////////////////
// EDITORQUEUE
//   Queue which processes only when scene not running,
//     and ALWAYS from the main thread
///////////////////////////////////////////////////////////

static bool gbQRUNMODE = false;

void EnterRunMode()
{
	OrkAssert(false==gbQRUNMODE);
	gbQRUNMODE = true;
#if defined(_DARWIN) // TODO: need to replace GCD on platforms other than DARWIN
	dispatch_sync( EditOnlyQueue(), 
	^{
		//printf( "EDITORQ ENTERING RUNMODE, SUSPENDING EDITORQUEUE\n" );
	});
	dispatch_suspend( EditOnlyQueue() );
#endif
	//printf( "EDITORQ ENTERING RUNMODE, EDITORQUEUE SUSPENDED\n" );
}
void LeaveRunMode()
{
	////////////////////////////////////////
	bool bRESUME = false;
	if( gbQRUNMODE ) bRESUME=true;
	gbQRUNMODE = false;
	////////////////////////////////////////
	//printf( "EDITORQ LEAVING RUNMODE, STARTING EDITORQUEUE...\n" );
#if defined(_DARWIN) // TODO: need to replace GCD on platforms other than DARWIN
	if( bRESUME )
		dispatch_resume( EditOnlyQueue() );
	////////////////////////////////////////
	dispatch_sync( EditOnlyQueue(), 
	^{
		//printf( "EDITORQ LEAVING RUNMODE, EDITORQUEUE STARTED.\n" );
	});
#endif
	////////////////////////////////////////
}
	

///////////////////////////////////////////////////////////

}


namespace ork { namespace ent {

UpdateStatus gUpdateStatus;

void UpdateStatus::SetState(EUpdateState est)
{
	meStatus = est;
}

void SceneData::Describe()
{
	orkmap<PoolString, SceneObject*> ork::ent::SceneData::* item = & SceneData::mSceneObjects;
	RegisterMapProperty( "SceneObjects", & SceneData::mSceneObjects );
	
	RegisterProperty( "ScriptFile", & SceneData::mScriptPath );
	AnnotatePropertyForEditor<SceneData>("ScriptFile", "editor.class", "ged.factory.filelist");
	AnnotatePropertyForEditor<SceneData>("ScriptFile", "editor.filetype", "lua");
	AnnotatePropertyForEditor<SceneData>("ScriptFile", "editor.filebase", "src://scripts/");
	AnnotateClassForEditor<SceneData>( "editor.object.props", ConstString("ScriptFile") );

}
///////////////////////////////////////////////////////////////////////////////
SceneData::SceneData()
	: meSceneDataMode( ESCENEDATAMODE_NEW )
{
}
///////////////////////////////////////////////////////////////////////////////
SceneData::~SceneData()
{
	for( orkmap<PoolString, SceneObject*>::const_iterator it=mSceneObjects.begin(); it!=mSceneObjects.end(); it++ )
	{
		SceneObject* pobj = it->second;
		delete pobj;
	}

	for( SceneComponentLut::const_iterator it=mSceneComponents.begin(); it!=mSceneComponents.end(); it++ )
	{
		SceneComponentData* pobj = it->second;
		delete pobj;
	}
	
}
///////////////////////////////////////////////////////////////////////////////
SceneObject* SceneData::FindSceneObjectByName(const PoolString& name)
{	orkmap<PoolString, SceneObject*>::iterator it = mSceneObjects.find( name );
	return (it==mSceneObjects.end()) ? 0 : it->second;
}
///////////////////////////////////////////////////////////////////////////////
const SceneObject* SceneData::FindSceneObjectByName(const PoolString& name) const
{	orkmap<PoolString, SceneObject*>::const_iterator it = mSceneObjects.find( name );
	const SceneObject* o = (it==mSceneObjects.end()) ? 0 : it->second;

	//printf( "FindSceneObject<%s:%p>\n", name.c_str(), o );
	return o;
}




///////////////////////////////////////////////////////////////////////////////
void SceneData::AddSceneObject(SceneObject* object)
{
	ArrayString<512> basebuffer;
	ArrayString<512> buffer;
	MutableString basestr(basebuffer);
	MutableString name_attempt(buffer);
	name_attempt = object->GetName();

	int counter = 0;

	int i = int(name_attempt.size()) - 1;
	for(; i >= 0; i--)
		if(!isdigit(name_attempt.c_str()[i]))
			break;
	basestr = name_attempt.substr(0, i + 1);

	name_attempt = basestr;
	name_attempt += CreateFormattedString("%d", ++counter).c_str();
	PoolString pooled_name = AddPooledString(name_attempt);
	while(mSceneObjects.find(pooled_name) != mSceneObjects.end())
	{
		name_attempt = basestr;
		name_attempt += CreateFormattedString("%d", ++counter).c_str();
		pooled_name = AddPooledString(name_attempt);
	}
	object->SetName(pooled_name);

	//printf( "AddSceneObject<%s:%p>\n", object->GetName().c_str(), object );
	mSceneObjects.insert( std::make_pair( object->GetName(), object ) );
}
///////////////////////////////////////////////////////////////////////////////
void SceneData::RemoveSceneObject(SceneObject* object)
{	const PoolString& Name = object->GetName();
	orkmap<PoolString, SceneObject*>::iterator it = mSceneObjects.find( Name );
	OrkAssert( it != mSceneObjects.end() );
	OrkAssert( it->second == object );
	mSceneObjects.erase( it );
}
///////////////////////////////////////////////////////////////////////////
bool SceneData::IsSceneObjectPresent(SceneObject*ptest) const
{
	for( orkmap<PoolString, SceneObject*>::const_iterator it = mSceneObjects.begin(); it!=mSceneObjects.end(); it++ )
	{
		SceneObject* pso = it->second;
		if( ptest==pso ) return true;
	}
	return false;
}
///////////////////////////////////////////////////////////////////////////////
PoolString SceneData::NewObjectName() const
{	PoolString rv;
	bool bdone = false;
	while( bdone == false )
	{	static int ict = 0;
		std::string objname = CreateFormattedString( "SceneObj%d", ict);
		PoolString test = FindPooledString( objname.c_str() );
		if( test.c_str() )
		{	ict++;
		}
		else
		{	rv = AddPooledString( objname.c_str() );
			bdone = true;
		}
	}
	return rv;
}
///////////////////////////////////////////////////////////////////////////////
bool SceneData::RenameSceneObject(SceneObject* pobj, const char* pname )
{
	mSceneObjects.erase(pobj->GetName());

	PoolString pooled_name = AddPooledString(pname);
	if(mSceneObjects.find(ork::AddPooledString(pname)) != mSceneObjects.end())
	{
		ArrayString<512> basebuffer;
		ArrayString<512> buffer;
		MutableString basestr(basebuffer);
		MutableString name_attempt(buffer);
		name_attempt = pname;

		int counter = 0;

		if( name_attempt.size() )
		{
			int i = int(name_attempt.size()) - 1;
			for(; i >= 0; i--)
				if(!isdigit(name_attempt.c_str()[i]))
					break;
			basestr = name_attempt.substr(0, i + 1);
		}

		name_attempt = basestr;
		name_attempt += CreateFormattedString("%d", ++counter).c_str();
		pooled_name = AddPooledString(name_attempt);
		while(mSceneObjects.find(pooled_name) != mSceneObjects.end())
		{
			name_attempt = basestr;
			name_attempt += CreateFormattedString("%d", ++counter).c_str();
			pooled_name = AddPooledString(name_attempt);
		}
	}

	pobj->SetName(pooled_name);
	mSceneObjects.insert(std::make_pair(pooled_name, pobj));

	return true;
}
///////////////////////////////////////////////////////////////////////////////
void SceneData::OnSceneDataMode( ESceneDataMode emode )
{	switch( emode )
	{
		case ESCENEDATAMODE_NEW:
			break;
		case ESCENEDATAMODE_INIT:
			break;
		case ESCENEDATAMODE_EDIT:
			PrepareForEdit();
			break;
		case ESCENEDATAMODE_RUN:
			break;
	}
	meSceneDataMode = emode;
}
///////////////////////////////////////////////////////////////////////////////

struct MatchNullSceneObject //: public std::unary_function< std::pair<PoolString, SceneObject* >, bool >
{
	bool operator()( const orkmap<PoolString, SceneObject*>::value_type& item )
	{
		return item.second == 0;
	}
};

///////////////////////////////////////////////////////////////////////////
void SceneData::PrepareForEdit()
{
	AutoLoadAssets();

	//////////////////////////////
	// delete null objects
	//////////////////////////////

	for(orkmap<PoolString, SceneObject*>::iterator cur = mSceneObjects.begin(); cur!=mSceneObjects.end(); cur++)
	{
		if( cur->second == 0 )
		{
			mSceneObjects.erase(cur);
			cur = mSceneObjects.begin();
		}
	}

	//////////////////////////////
	// set object names
	//////////////////////////////
	for( orkmap<PoolString, SceneObject*>::iterator it = mSceneObjects.begin(); it!=mSceneObjects.end(); it++ )
	{	SceneObject* sobj = it->second;
		sobj->SetName( it->first );
	}
	//////////////////////////////
	// fixup dagnode hierarchy
	//////////////////////////////
	for( orkmap<PoolString, SceneObject*>::iterator it = mSceneObjects.begin(); it!=mSceneObjects.end(); it++ )
	{	SceneObject* sobj = it->second;
		SceneDagObject* pdag = rtti::downcast<SceneDagObject*>( sobj );
		if( pdag )
		{	const PoolString& parname = pdag->GetParentName();
			DagNode& dnode = pdag->GetDagNode();
			if( 0 != strcmp( parname.c_str(), "scene" ) )
			{
				SceneObject* spobj = this->FindSceneObjectByName( parname );
				//OrkAssert( spobj != 0 );
				SceneDagObject* pardag = rtti::autocast(spobj);
				if( pardag )
				{
					SceneGroup* pgrp = rtti::autocast( pardag );
					if( pgrp )
					{
						pgrp->AddChild( pdag );
					}
				}
				else
				{
					//pgrp->AddChild( pdag );
				}
			}
		}
	}
	///////////////////////////////
	// 1 time fixup
	///////////////////////////////
	for( orkmap<PoolString, SceneObject*>::iterator it = mSceneObjects.begin(); it!=mSceneObjects.end(); it++ )
	{	SceneObject* sobj = it->second;
		EntData* pent = rtti::downcast<EntData*>( sobj );
		if( pent )
		{	Archetype* parch = const_cast<Archetype*>(pent->GetArchetype());
			if( parch )
			{
				if( false == IsSceneObjectPresent(parch) )
				{
					parch->SetName( NewObjectName() );
					AddSceneObject(parch);
				}
			}
		}
	}
	//////////////////////////////
	// Recompose Archetypes's
	//////////////////////////////

	SceneComposer scene_composer(this);

	// TODO: Fix this...Archetypes are not in mSceneObjects
	for(orkmap<PoolString, SceneObject*>::iterator it = mSceneObjects.begin(); it!=mSceneObjects.end(); it++)
	{
		SceneObject* sobj = it->second;
		if(Archetype *archetype = rtti::autocast(sobj))
		{
			archetype->Compose(scene_composer);
		}
	}
}

void SceneData::AutoLoadAssets() const
{
	bool loaded;
	do
	{	loaded = false;
		loaded = asset::AssetManager<ArchetypeAsset>::AutoLoad() || loaded;
		loaded = asset::AssetManager<lev2::XgmAnimAsset>::AutoLoad() || loaded;
		loaded = asset::AssetManager<lev2::AudioStream>::AutoLoad() || loaded;
		loaded = asset::AssetManager<lev2::AudioBank>::AutoLoad() || loaded;
		loaded = asset::AssetManager<lev2::FxShaderAsset>::AutoLoad() || loaded;
		loaded = asset::AssetManager<lev2::XgmModelAsset>::AutoLoad() || loaded;
		loaded = asset::AssetManager<lev2::TextureAsset>::AutoLoad() || loaded;
	}
	while(loaded);
}
///////////////////////////////////////////////////////////////////////////////
void SceneData::EnterEditState()
{	OnSceneDataMode(ESCENEDATAMODE_EDIT);
}
///////////////////////////////////////////////////////////////////////////////
void SceneData::EnterInitState()
{	OnSceneDataMode(ESCENEDATAMODE_INIT);
}
///////////////////////////////////////////////////////////////////////////////
void SceneData::EnterRunState()
{	OnSceneDataMode(ESCENEDATAMODE_RUN);
}
///////////////////////////////////////////////////////////////////////////////
bool SceneData::PostDeserialize(reflect::IDeserializer &)
{	EnterEditState();
	return true;
}
///////////////////////////////////////////////////////////////////////////////
void SceneData::AddSceneComponent( SceneComponentData* pcomp )
{
	OrkAssert( mSceneComponents.find( pcomp->GetClass() ) == mSceneComponents.end() );
	mSceneComponents.AddSorted( pcomp->GetClass(), pcomp );
}
///////////////////////////////////////////////////////////////////////////////
void SceneData::ClearSceneComponents()
{
	mSceneComponents.clear();
}
///////////////////////////////////////////////////////////////////////////////
SceneComposer::SceneComposer(SceneData* psd)
	: mComponents( ork::EKEYPOLICY_LUT )
	, mpSceneData(psd)
{
	const SceneData::SceneComponentLut& scomps = psd->GetSceneComponents();
	for( SceneData::SceneComponentLut::const_iterator it=scomps.begin(); it!=scomps.end(); it++ )
	{
		mComponents.AddSorted( it->first, it->second );
	}
}
///////////////////////////////////////////////////////////////////////////////
SceneComposer::~SceneComposer()
{
	mpSceneData->ClearSceneComponents();
	for( orklut<const ork::object::ObjectClass*,SceneComponentData*>::const_iterator it=mComponents.begin(); it!=mComponents.end(); it++ )
	{	const ork::object::ObjectClass* pclass = it->first;
		SceneComponentData* psc = ork::rtti::autocast(it->second);
		if( 0 == psc )
		{
			OrkAssert( pclass->IsSubclassOf( SceneComponentData::GetClassStatic() ) );
			psc = ork::rtti::autocast(pclass->CreateObject());
		}
		mpSceneData->AddSceneComponent(psc);
	}
}
///////////////////////////////////////////////////////////////////////////////
}

template class orklut<PoolString,const CCameraData*>;

}
