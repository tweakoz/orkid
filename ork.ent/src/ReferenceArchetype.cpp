////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <pkg/ent/ReferenceArchetype.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/asset/FileAssetLoader.h>
#include <ork/asset/FileAssetNamer.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/serialize/XMLDeserializer.h>
#include <ork/stream/FileInputStream.h>
#include <ork/application/application.h>
#include <pkg/ent/scene.h>
#include <ork/asset/AssetManager.hpp>
#include <ork/kernel/string/string.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ArchetypeAsset, "ArchetypeAsset");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ReferenceArchetype, "ReferenceArchetype");

template class ork::asset::AssetManager<ork::ent::ArchetypeAsset>;

namespace ork { namespace ent {

////////////////////////////////////////////////////////////////////////////////

class ArchetypeAssetLoader : public asset::FileAssetLoader
{
public:
	ArchetypeAssetLoader();
private:
	/*virtual*/ bool LoadFileAsset(asset::Asset*, ConstString);
	/*virtual*/ void DestroyAsset(asset::Asset *asset)
	{
		ArchetypeAsset *archasset = (ArchetypeAsset *)asset;
	//	delete archasset;
	}
};

////////////////////////////////////////////////////////////////////////////////

ArchetypeAssetLoader::ArchetypeAssetLoader() 
{
	AddFileExtension(".mox");
	AddFileExtension(".mob");
}

////////////////////////////////////////////////////////////////////////////////

std::stack<asset::Asset*> gaastack;

bool ArchetypeAssetLoader::LoadFileAsset(asset::Asset* pAsset, ConstString asset_name)
{
	int isize = gaastack.size();
	if( isize )
	{
		if( gaastack.top()==pAsset )
			return true;
	}
	gaastack.push(pAsset);
	
	float ftime1 = ork::CSystem::GetRef().GetLoResRelTime();
	ArchetypeAsset* archasset = rtti::autocast(pAsset);

	stream::FileInputStream istream(asset_name.c_str());
	reflect::serialize::XMLDeserializer iser(istream);
	rtti::ICastable* pcastable = 0;
	
#if defined(_XBOX) && defined(PROFILE)
	PIXBeginNamedEvent(0, "ArchetypeAssetLoader::LoadFileAsset(%s).Deserialize", pAsset->GetName());
#endif
	bool bOK = iser.Deserialize( pcastable );
#if defined(_XBOX) && defined(PROFILE)
	PIXEndNamedEvent();
#endif
	if( bOK )
	{
		Archetype* parch = rtti::autocast(pcastable);
		if( parch )
		{
			archasset->SetArchetype(parch);
			parch->SetName( AddPooledLiteral("ReferencedArchetype") );
		}
		else
		{
			archasset->SetArchetype(0);
		}
	}

	//ork::Object* pobj = rtti::autocast(DeserializeObject(asset_name.c_str()));
	//archasset->SetArchetype(rtti::autocast(pobj));

	float ftime2 = ork::CSystem::GetRef().GetLoResRelTime();

	static float ftotaltime = 0.0f;
	static int iltotaltime = 0;

	ftotaltime += (ftime2-ftime1);

	int itotaltime = int(ftotaltime);

	//if( itotaltime > iltotaltime )
	{
		std::string outstr = ork::CreateFormattedString(
		"FILEAsset AccumTime<%f>\n", ftotaltime );
		////OutputDebugString( outstr.c_str() );
		iltotaltime = itotaltime;
	}
	gaastack.pop();

	return archasset->GetArchetype() ? true : false;
}

////////////////////////////////////////////////////////////////////////////////

static ArchetypeAssetLoader loader;
static asset::FileAssetNamer namer("archetypes/");

void ArchetypeAsset::Describe()
{
	GetClassStatic()->AddLoader(loader);
	GetClassStatic()->SetAssetNamer(&namer);
}

////////////////////////////////////////////////////////////////////////////////

ArchetypeAsset::ArchetypeAsset() : mArchetype(NULL) {}

ArchetypeAsset::~ArchetypeAsset()
{
	if(mArchetype)
	{
		delete mArchetype;
		mArchetype = NULL;
	}
}

////////////////////////////////////////////////////////////////////////////////

void ReferenceArchetype::Describe()
{
	reflect::RegisterProperty("ArchetypeAsset", &ReferenceArchetype::mArchetypeAsset);

	reflect::AnnotatePropertyForEditor<ReferenceArchetype>("ArchetypeAsset", "editor.class", "ged.factory.assetlist");
	reflect::AnnotatePropertyForEditor<ReferenceArchetype>("ArchetypeAsset", "editor.assettype", "refarch");
	reflect::AnnotatePropertyForEditor<ReferenceArchetype>("ArchetypeAsset", "editor.assetclass", "ArchetypeAsset");
	//reflect::AnnotateClassForEditor<ReferenceArchetype>( "editor.instantiable", false );
	//reflect::AnnotatePropertyForEditor<ReferenceArchetype>( "Components", "editor.visible", "false" );
}

////////////////////////////////////////////////////////////////////////////////

ReferenceArchetype::ReferenceArchetype() : mArchetypeAsset(NULL) {}

////////////////////////////////////////////////////////////////////////////////

void ReferenceArchetype::DoCompose(ork::ent::ArchComposer& composer)
{
	if(mArchetypeAsset)
		if(mArchetypeAsset->GetArchetype())
			mArchetypeAsset->GetArchetype()->Compose(composer.mSceneComposer);
}

////////////////////////////////////////////////////////////////////////////////

void ReferenceArchetype::DoComposeEntity( Entity *pent ) const
{
	/////////////////////////////////////////////////
	const ent::EntData& pentdata = pent->GetEntData();
	if( mArchetypeAsset )
	{
		//const ent::ComponentDataTable::LutType& clut = GetComponentDataTable().GetComponents();
		//for( ent::ComponentDataTable::LutType::const_iterator it = clut.begin(); it!= clut.end(); it++ )
		//{	ent::ComponentData* pcompdata = it->second;
		//	if( pcompdata )
		//	{	ent::ComponentInst* pinst = pcompdata->CreateComponent(pent);
		//		if( pinst )
		//		{	pent->GetComponents().AddComponent(pinst);
		//		}
		//	}
		//}
		if(const Archetype* parch = mArchetypeAsset->GetArchetype())
		{	parch->ComposeEntity(pent);
			//const ent::ComponentDataTable::LutType& clut = parch->GetComponentDataTable().GetComponents();
			//for( ent::ComponentDataTable::LutType::const_iterator it = clut.begin(); it!= clut.end(); it++ )
			//{	ent::ComponentData* pcompdata = it->second;
			//	if(pcompdata)
			//	{	ent::ComponentInst* pinst = pcompdata->CreateComponent(pent);
			//		if( pinst )
			//		{	pent->GetComponents().AddComponent(pinst);
			//		}
			//	}
			//}
		}
	}
	/////////////////////////////////////////////////
}

////////////////////////////////////////////////////////////////////////////////

void ReferenceArchetype::DoLinkEntity(SceneInst* inst, Entity *pent) const
{
	if(mArchetypeAsset)
		if(mArchetypeAsset->GetArchetype())
			mArchetypeAsset->GetArchetype()->LinkEntity(inst, pent);
}

////////////////////////////////////////////////////////////////////////////////

void ReferenceArchetype::DoStartEntity(SceneInst* inst, const CMatrix4 &world, Entity *pent) const
{
	if(mArchetypeAsset)
		if(mArchetypeAsset->GetArchetype())
			mArchetypeAsset->GetArchetype()->StartEntity(inst, world, pent);
}

////////////////////////////////////////////////////////////////////////////////

void ReferenceArchetype::DoStopEntity(SceneInst* psi, Entity *pent) const
{
	if(mArchetypeAsset)
		if(mArchetypeAsset->GetArchetype())
			mArchetypeAsset->GetArchetype()->StopEntity(psi, pent);
}

} } // ork::ent
