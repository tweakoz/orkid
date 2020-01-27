////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <pkg/ent/entity.h>
#include <pkg/ent/scene.h>
#include <ork/lev2/gfx/camera/cameradata.h>

#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/kernel/orklut.hpp>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/lev2_asset.h>
#include <pkg/ent/LightingSystem.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <orktool/qtui/qtui_tool.h>
#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <ork/reflect/enum_serializer.inl>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <orktool/filter/gfx/collada/collada.h>
#include <orktool/filter/gfx/meshutil/meshutil_fixedgrid.h>
#include <ork/math/audiomath.h>
#include <ork/kernel/mutex.h>
#include <ork/kernel/thread.h>
#include <ork/reflect/serialize/XMLSerializer.h>
#include <ork/stream/FileOutputStream.h>

#include "SurfaceBaker.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

BakingGroup::BakingGroup()
	: meBakeMapType(EBMT_AMBOCC)
	, miResolution(128)
	, miDiceSize(1024)
	, miNumSamples(64)
	, mfShadowBias(0.0f)
	, mfMaxError( 0.25f )
	, mfAdaptive( 1.0f )
	, mfMaxPixelDist( 20.0f )
	, mfMaxHitDist( 1000.0f )
	, mfFilterWidth(2.0f)
	, mfAtlasStretching(0.9f)
	, mfAtlasUnification(2.0f)
	, mbComputeShadows(false)
	, mbComputeAmbOcc(false)
	, mbUseVertexColors(true)
	, mMatchItem()
	, mMatchLights()
	, mShadowCasters()
{
}

void BakersChoiceDelegate::Describe()
{
}

void BakersChoiceDelegate::EnumerateChoices( orkmap<ork::PoolString,ValueType>& Choices )
{	if( mpARCH )
	{	const ork::orklut<PoolString,BakerSettings*>& itemmap = mpARCH->GetBakerSettingsMap();
		for( ork::orklut<PoolString,BakerSettings*>::const_iterator itp=itemmap.begin(); itp!=itemmap.end(); itp++ )
		{	const ork::PoolString& itemname = itp->first;
			any64 myany;
			myany.Set(itemname.c_str());
			Choices.insert(std::make_pair(itemname,myany));
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void JobSetChoiceDelegate::Describe()
{
}

void JobSetChoiceDelegate::EnumerateChoices( orkmap<ork::PoolString,ValueType>& Choices )
{	if( mpSETTINGS )
	{	const ork::orklut<PoolString,FarmJobSet*>& itemmap = mpSETTINGS->JobSetMap();
		for( ork::orklut<PoolString,FarmJobSet*>::const_iterator itp=itemmap.begin(); itp!=itemmap.end(); itp++ )
		{	const ork::PoolString& itemname = itp->first;
			any64 myany;
			myany.Set(itemname.c_str());
			Choices.insert(std::make_pair(itemname,myany));
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FarmGroupChoiceDelegate::Describe()
{
}

void FarmGroupChoiceDelegate::EnumerateChoices( orkmap<ork::PoolString,ValueType>& Choices )
{	if( mpARCH )
	{	const orklut<PoolString,FarmNodeGroup*>& itemmap = mpARCH->GetFarmNodeGroupMap();
		for( ork::orklut<PoolString,FarmNodeGroup*>::const_iterator itp=itemmap.begin(); itp!=itemmap.end(); itp++ )
		{	const ork::PoolString& itemname = itp->first;
			any64 myany;
			myany.Set(itemname.c_str());
			Choices.insert(std::make_pair(itemname,myany));
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void LightMapperArchetype::Describe()
{
	ork::reflect::RegisterMapProperty( "Settings", & LightMapperArchetype::mSettingsMap );
	ork::reflect::annotatePropertyForEditor< LightMapperArchetype >("Settings", "editor.factorylistbase", "EditorOnly/BakerSettings" );

	ork::reflect::RegisterMapProperty( "FarmNodeGroups", & LightMapperArchetype::mFarmNodeGroups );
	ork::reflect::annotatePropertyForEditor< LightMapperArchetype >("FarmNodeGroups", "editor.factorylistbase", "EditorOnly/FarmNodeGroup" );

	ork::reflect::RegisterProperty( "ActiveBakeSettings", & LightMapperArchetype::mCurrentSetting );
	ork::reflect::annotatePropertyForEditor< LightMapperArchetype >( "ActiveBakeSettings", "ged.userchoice.delegate", "BakersChoiceDelegate" );

	ork::reflect::RegisterProperty( "ActiveRenderFarmGroup", & LightMapperArchetype::mCurrentNodeGroup );
	ork::reflect::annotatePropertyForEditor< LightMapperArchetype >( "ActiveRenderFarmGroup", "ged.userchoice.delegate", "FarmGroupChoiceDelegate" );
}

///////////////////////////////////////////////////////////////////////////////

const BakerSettings* LightMapperArchetype::GetCurrentSetting() const
{
	const BakerSettings* psetting = 0;
	orklut<PoolString,BakerSettings*>::const_iterator it=mSettingsMap.find(mCurrentSetting);
	if( it!=mSettingsMap.end() )
	{
		psetting=it->second;
	}
	return psetting;
}

///////////////////////////////////////////////////////////////////////////////

const FarmNodeGroup* LightMapperArchetype::GetCurrentFarmNodeGroup() const
{
	const FarmNodeGroup* pgroup = 0;
	orklut<PoolString,FarmNodeGroup*>::const_iterator it=mFarmNodeGroups.find(mCurrentNodeGroup);
	if( it!=mFarmNodeGroups.end() )
	{
		pgroup=it->second;
	}
	return pgroup;
}

///////////////////////////////////////////////////////////////////////////////

void FarmNode::Describe()
{
	ork::reflect::RegisterProperty( "HostName", & FarmNode::mHostName );
	ork::reflect::RegisterProperty( "SshPort", & FarmNode::mSSHPort );
	ork::reflect::RegisterProperty( "ExclusiveConnection", & FarmNode::mbExclusiveConnection );
	ork::reflect::annotatePropertyForEditor< FarmNode >( "SshPort", "editor.range.min", "0" );
	ork::reflect::annotatePropertyForEditor< FarmNode >( "SshPort", "editor.range.max", "22200" );
}

///////////////////////////////////////////////////////////////////////////////

void FarmJob::Describe()
{
	//ork::reflect::RegisterProperty( "RgmFile", & FarmJob::mRgmInputName );
	//ork::reflect::annotatePropertyForEditor<FarmJob>("RgmFile", "editor.class", "ged.factory.filelist");
	//ork::reflect::annotatePropertyForEditor<FarmJob>("RgmFile", "editor.filetype", "rgm");
	//ork::reflect::annotatePropertyForEditor<FarmJob>("RgmFile", "editor.filebase", "src://");

	ork::reflect::RegisterProperty( "DaeFile", & FarmJob::mDaeInputName );
	ork::reflect::annotatePropertyForEditor<FarmJob>("DaeFile", "editor.class", "ged.factory.filelist");
	ork::reflect::annotatePropertyForEditor<FarmJob>("DaeFile", "editor.filetype", "dae");
	ork::reflect::annotatePropertyForEditor<FarmJob>("DaeFile", "editor.filebase", "src://environ/");

	ork::reflect::RegisterProperty( "BakeGroupMatch", & FarmJob::mBakeGroupMatch );
}

void FarmJobSet::Describe()
{
	ork::reflect::RegisterMapProperty( "Jobs", & FarmJobSet::mFarmJobs );
	reflect::annotatePropertyForEditor<FarmJobSet>( "Jobs", "editor.factorylistbase", "EditorOnly/FarmJob" );
}

///////////////////////////////////////////////////////////////////////////////

void FarmNodeGroup::Describe()
{
	ork::reflect::RegisterMapProperty( "FarmNodes", & FarmNodeGroup::mFarmNodes );
	ork::reflect::annotatePropertyForEditor< FarmNodeGroup >("FarmNodes", "editor.factorylistbase", "EditorOnly/FarmNode" );
}

///////////////////////////////////////////////////////////////////////////////

void BakingGroup::Describe()
{
	ork::reflect::RegisterProperty( "BakeType", & BakingGroup::meBakeMapType );
	ork::reflect::annotatePropertyForEditor<BakingGroup>(	"BakeType", "editor.class", "ged.factory.enum" );

	ork::reflect::RegisterProperty( "MatchItem", & BakingGroup::mMatchItem );
	ork::reflect::RegisterProperty( "MatchLights", & BakingGroup::mMatchLights );
	ork::reflect::RegisterProperty( "ShadowCasters", & BakingGroup::mShadowCasters );

	ork::reflect::RegisterProperty( "ComputeShadows", & BakingGroup::mbComputeShadows );
	ork::reflect::RegisterProperty( "ComputeAmbOcc", & BakingGroup::mbComputeAmbOcc );

	ork::reflect::RegisterProperty("UseVertexColors", &BakingGroup::mbUseVertexColors);

	ork::reflect::RegisterProperty("UserShader", &BakingGroup::mUserShaderName);
	ork::reflect::annotatePropertyForEditor<BakingGroup>("UserShader", "editor.class", "ged.factory.filelist");
	ork::reflect::annotatePropertyForEditor<BakingGroup>("UserShader", "editor.filetype", "gsl");
	ork::reflect::annotatePropertyForEditor<BakingGroup>("UserShader", "editor.filebase", "miniorkdata://");

	ork::reflect::RegisterProperty("UserSdbShader", &BakingGroup::mUserSdbShaderName);
	ork::reflect::annotatePropertyForEditor<BakingGroup>("UserSdbShader", "editor.class", "ged.factory.filelist");
	ork::reflect::annotatePropertyForEditor<BakingGroup>("UserSdbShader", "editor.filetype", "gsl");
	ork::reflect::annotatePropertyForEditor<BakingGroup>("UserSdbShader", "editor.filebase", "miniorkdata://");

	ork::reflect::RegisterProperty("BakeResolution", &BakingGroup::miResolution);
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "BakeResolution", "editor.range.min", "64" );
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "BakeResolution", "editor.range.max", "1024" );

	ork::reflect::RegisterProperty("AtlasStretching", &BakingGroup::mfAtlasStretching);
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "AtlasStretching", "editor.range.min", "0.0" );
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "AtlasStretching", "editor.range.max", "1.0" );

	ork::reflect::RegisterProperty("AtlasUnification", &BakingGroup::mfAtlasUnification);
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "AtlasUnification", "editor.range.min", "0.25" );
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "AtlasUnification", "editor.range.max", "4.0" );

	ork::reflect::RegisterProperty("DiceSize", &BakingGroup::miDiceSize);
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "DiceSize", "editor.range.min", "256" );
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "DiceSize", "editor.range.max", "2048" );

	ork::reflect::RegisterProperty("NumSamples", &BakingGroup::miNumSamples);
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "NumSamples", "editor.range.min", "8" );
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "NumSamples", "editor.range.max", "1024" );

	ork::reflect::RegisterProperty("ShadowBias", &BakingGroup::mfShadowBias);
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "ShadowBias", "editor.range.min", "0.0f" );
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "ShadowBias", "editor.range.max", "1000.0f" );
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "ShadowBias", "editor.range.log", "true" );

	ork::reflect::RegisterProperty("MaxError", &BakingGroup::mfMaxError);
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "MaxError", "editor.range.min", "0.0f" );
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "MaxError", "editor.range.max", "1.0f" );
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "MaxError", "editor.range.log", "true" );

	ork::reflect::RegisterProperty("Adaptive", &BakingGroup::mfAdaptive);
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "Adaptive", "editor.range.min", "0.0f" );
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "Adaptive", "editor.range.max", "2.0f" );
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "Adaptive", "editor.range.log", "true" );

	ork::reflect::RegisterProperty("MaxPixelDist", &BakingGroup::mfMaxPixelDist);
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "MaxPixelDist", "editor.range.min", "0.0f" );
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "MaxPixelDist", "editor.range.max", "30.0f" );
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "MaxPixelDist", "editor.range.log", "true" );

	ork::reflect::RegisterProperty("MaxHitDist", &BakingGroup::mfMaxHitDist);
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "MaxHitDist", "editor.range.min", "0.0f" );
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "MaxHitDist", "editor.range.max", "10000.0f" );
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "MaxHitDist", "editor.range.log", "true" );

	ork::reflect::RegisterProperty("FilterWidth", &BakingGroup::mfFilterWidth);
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "FilterWidth", "editor.range.min", "0.0f" );
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "FilterWidth", "editor.range.max", "8.0f" );
	ork::reflect::annotatePropertyForEditor< BakingGroup >( "FilterWidth", "editor.range.log", "true" );

	static const char* EdGrpStr =
				"sort://MatchItem MatchLights UseVertexColors "
				"grp://Atlas DiceSize AtlasStretching AtlasUnification "
				"grp://Baking BakeType BakeResolution UserShader UserSdbShader "
				"FilterWidth "
				"grp://Shadows ComputeShadows ComputeAmbOcc ShadowCasters NumSamples "
				"Adaptive MaxError MaxPixelDist MaxHitDist ShadowBias  ";

	reflect::annotateClassForEditor<BakingGroup>( "editor.prop.groups", EdGrpStr );
}

///////////////////////////////////////////////////////////////////////////////

void BakerSettings::Describe()
{
	ork::reflect::RegisterProperty("DebugPyg", &BakerSettings::mDebugPyg);
	ork::reflect::RegisterProperty("DebugPygEmbedGeom", &BakerSettings::mDebugPygEmbedGeom);
	ork::reflect::annotateClassForEditor< LightMapperArchetype >("editor.object.ops", ConstString("Atlas:AtlasMapperOps Bake:BakeOps ") );
	ork::reflect::RegisterMapProperty( "BakingGroups", & BakerSettings::mBakingGroupMap );
	reflect::annotatePropertyForEditor<BakerSettings>( "BakingGroups", "editor.factorylistbase", "EditorOnly/BakingGroup" );
	ork::reflect::RegisterProperty("DaeInput", &BakerSettings::mDaeInputName);
	ork::reflect::annotatePropertyForEditor<BakerSettings>("DaeInput", "editor.class", "ged.factory.filelist");
	ork::reflect::annotatePropertyForEditor<BakerSettings>("DaeInput", "editor.filetype", "dae");
	ork::reflect::annotatePropertyForEditor<BakerSettings>("DaeInput", "editor.filebase", "src://");
	ork::reflect::RegisterMapProperty( "JobSets", & BakerSettings::mFarmJobSets );

	ork::reflect::RegisterProperty("ActiveJobSet", &BakerSettings::mCurrentJobSet);

	ork::reflect::annotatePropertyForEditor< BakerSettings >( "ActiveJobSet", "ged.userchoice.delegate", "JobSetChoiceDelegate" );

	reflect::annotatePropertyForEditor<BakerSettings>( "JobSets", "editor.factorylistbase", "EditorOnly/FarmJobSet" );


	static const char* EdGrpStr =
				"sort://DaeInput BakingGroups ActiveJobSet JobSets DebugPyg DebugPygEmbedGeom";

	reflect::annotateClassForEditor<BakerSettings>( "editor.prop.groups", EdGrpStr );

	AtlasMapperOps::LinkMe();
	ImtMapperOps::LinkMe();

	FarmNode::LinkMe();
	FarmNodeGroup::LinkMe();
	FarmJob::LinkMe();
}

///////////////////////////////////////////////////////////////////////////////

const FarmJobSet* BakerSettings::GetCurrentJobSet() const
{
	const FarmJobSet* psetting = 0;
	orklut<PoolString,FarmJobSet*>::const_iterator it=mFarmJobSets.find(mCurrentJobSet);
	if( it!=mFarmJobSets.end() )
	{
		psetting=it->second;
	}
	return psetting;
}

///////////////////////////////////////////////////////////////////////////////

BakingGroupMatchItem BakerSettings::Match( const std::string& TestName ) const
{	for(	orklut<PoolString,BakingGroup*>::const_iterator
			it=mBakingGroupMap.begin();
			it!=mBakingGroupMap.end();
			it++ )
	{
		BakingGroup* pgroup = it->second;
		const ork::PoolString& groupmatch = pgroup->MatchItem();
		const char* pitem = groupmatch.c_str();
		std::string matchitem = pitem ? pitem : "";
		if( matchitem.length() )
		{	tokenlist match_toklist = CreateTokenList( matchitem.c_str(), " " );
			for(	tokenlist::const_iterator
					itm=match_toklist.begin();
					itm!=match_toklist.end();
					itm++ )
			{	const std::string& str = (*itm);
				size_t itfind = TestName.find(str);
				if( itfind != std::string::npos )
				{
					BakingGroupMatchItem retitem;
					retitem.mMatchName = it->first;
					retitem.mMatchGroup = pgroup;
					return retitem;
				}
			}
		}
	}
	return BakingGroupMatchItem();
}

///////////////////////////////////////////////////////////////////////////////

LightingGroupMatchItem BakerSettings::LightMatch( const std::string& TestName ) const
{
	/*for(	orklut<PoolString,BakingGroup*>::const_iterator
			it=mBakingGroupMap.begin();
			it!=mBakingGroupMap.end();
			it++ )
	{
		BakingGroup* pgroup = it->second;
		const ork::PoolString& groupmatch = pgroup->MatchItem();
		const char* pitem = groupmatch.c_str();
		std::string matchitem = pitem ? pitem : "";
		if( matchitem.length() )
		{	tokenlist match_toklist = CreateTokenList( matchitem.c_str(), " " );
			for(	tokenlist::const_iterator
					itm=match_toklist.begin();
					itm!=match_toklist.end();
					itm++ )
			{	const std::string& str = (*itm);
				u32 itfind = TestName.find(str);
				if( itfind != std::string::npos )
				{
					BakingGroupMatchItem retitem;
					retitem.mMatchName = it->first;
					retitem.mMatchGroup = pgroup;
					return retitem;
				}
			}
		}
	}*/
	return LightingGroupMatchItem();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AtlasMapperOps::Describe(){}

class AtlasProcessThread : public ork::Thread
{
	void run() final
	{
		static ork::recursive_mutex gproc_mutex("atlaser");
		gproc_mutex.Lock();
		{
			AtlasMapperOps* pOPS = _userdata.Get<AtlasMapperOps*>();
			LightMapperArchetype* parch = pOPS->mpARCH;
			pOPS->SetProgress( 0.01f );
			const BakerSettings* psetting = parch->GetCurrentSetting();
			if( psetting )
			{
				pOPS->mbIMT = false;
				//PerformAtlas(pOPS,psetting);
			}
			pOPS->SetProgress( 1.0f );
			tool::ged::IOpsDelegate::RemoveTask( AtlasMapperOps::GetClassStatic(), parch );
		}
		gproc_mutex.UnLock();
		//return 0;
	}
};

///////////////////////////////////////////////////////////////////////////////

void AtlasMapperOps::Execute( ork::Object* ptarget )
{
	AtlasProcessThread* pthread = new AtlasProcessThread;
	pthread->_userdata.Set<AtlasMapperOps*>(this);
	mpARCH = rtti::autocast(ptarget);
	pthread->start();
}

///////////////////////////////////////////////////////////////////////////////

void BakeOps::Describe(){}

///////////////////////////////////////////////////////////////////////////////

class BakeProcessThread : public ork::Thread
{
	void run() final
	{	static ork::mutex gproc_mutex("lightmapper");
		gproc_mutex.Lock();
		{
			BakeOps* pOPS = _userdata.Get<BakeOps*>();
			LightMapperArchetype* parch = pOPS->mpARCH;
			pOPS->SetProgress(0.0f);
			const BakerSettings* psetting = parch->GetCurrentSetting();
			const FarmNodeGroup* pfarmgroup = parch->GetCurrentFarmNodeGroup();
			if( psetting && pfarmgroup )
			{
				PerformBake(pOPS,psetting,pfarmgroup);
			}
			pOPS->SetProgress(1.0f);
			tool::ged::IOpsDelegate::RemoveTask( BakeOps::GetClassStatic(), parch );
		}
		gproc_mutex.UnLock();
		//return 0;
	}
};

///////////////////////////////////////////////////////////////////////////////

void BakeOps::Execute( ork::Object* ptarget )
{	mpARCH = rtti::autocast(ptarget);
	BakeProcessThread bthread;
	bthread._userdata.Set<BakeOps*>(this);
	bthread.runSynchronous();

	//DWORD ThreadId;
	//HANDLE thread_h = CreateThread(
	//						NULL,
	//						0,
	//						& BakeProcessThread,
	//						(void*)this,
	//						0,
	//						& ThreadId );
}

///////////////////////////////////////////////////////////////////////////////

void ImtMapperOps::Describe(){}

///////////////////////////////////////////////////////////////////////////////

class ImtProcessThread : public ork::Thread
{
	void run() final
	{	static ork::mutex gproc_mutex("lightmapper");
		gproc_mutex.Lock();
		{
			ImtMapperOps* pOPS = _userdata.Get<ImtMapperOps*>();
			LightMapperArchetype* parch = pOPS->mpARCH;
			pOPS->SetProgress(0.0f);
			const BakerSettings* psetting = parch->GetCurrentSetting();
			if( psetting )
			{
		/*		AtlasMapperOps amopts;
				lmopts.mpARCH = parch;
				amopts.mpARCH = parch;

				amopts.mbIMT = false;
				PerformAtlas(&amopts,psetting);

				amopts.mbIMT = true;
				PerformAtlas(&amopts,psetting);
	*/
			}
			pOPS->SetProgress(1.0f);
			tool::ged::IOpsDelegate::RemoveTask( ImtMapperOps::GetClassStatic(), parch );
		}
		gproc_mutex.UnLock();
		//return 0;
	}
};

///////////////////////////////////////////////////////////////////////////////

void ImtMapperOps::Execute( ork::Object* ptarget )
{	mpARCH = rtti::autocast(ptarget);
	ImtProcessThread* pthread = new ImtProcessThread;
	pthread->_userdata.Set<ImtMapperOps*>(this);
	pthread->start();
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////

BEGIN_ENUM_SERIALIZER(ork::ent,EBAKEMAP_TYPE)
	DECLARE_ENUM(EBMT_AMBOCC)
	DECLARE_ENUM(EBMT_DIFFUSE)
	DECLARE_ENUM(EBMT_USER)
END_ENUM_SERIALIZER()

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::LightMapperArchetype, "EditorOnly/LightMapperArchetype");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BakerSettings, "EditorOnly/BakerSettings");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BakingGroup, "EditorOnly/BakingGroup");

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::AtlasMapperOps, "AtlasMapperOps");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ImtMapperOps, "ImtMapperOps");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BakeOps, "BakeOps");


INSTANTIATE_TRANSPARENT_RTTI(ork::ent::FarmNode, "EditorOnly/FarmNode");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::FarmNodeGroup, "EditorOnly/FarmNodeGroup");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::FarmJob, "EditorOnly/FarmJob");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::FarmJobSet, "EditorOnly/FarmJobSet");

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BakersChoiceDelegate, "BakersChoiceDelegate");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::FarmGroupChoiceDelegate, "FarmGroupChoiceDelegate");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::JobSetChoiceDelegate, "JobSetChoiceDelegate");
