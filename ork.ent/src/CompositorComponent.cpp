////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.hpp>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/drawable.h>
#include <pkg/ent/Compositor.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/enum_serializer.h>
#include <pkg/ent/PerfController.h>
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::CompositingSystemData, "CompositingSystemData");
///////////////////////////////////////////////////////////////////////////////
BEGIN_ENUM_SERIALIZER(ork::ent, EOutputRes)
	DECLARE_ENUM(EOutputRes_640x480)
	DECLARE_ENUM(EOutputRes_960x640)
	DECLARE_ENUM(EOutputRes_1024x1024)
	DECLARE_ENUM(EOutputRes_1280x720)
	DECLARE_ENUM(EOutputRes_1600x1200)
	DECLARE_ENUM(EOutputRes_1920x1080)
END_ENUM_SERIALIZER()
///////////////////////////////////////////////////////////////////////////////
BEGIN_ENUM_SERIALIZER(ork::ent, EOutputResMult)
	DECLARE_ENUM(EOutputResMult_Quarter)
	DECLARE_ENUM(EOutputResMult_Half)
	DECLARE_ENUM(EOutputResMult_Full)
	DECLARE_ENUM(EOutputResMult_Double)
	DECLARE_ENUM(EOutputResMult_Quadruple)
END_ENUM_SERIALIZER()
///////////////////////////////////////////////////////////////////////////////
BEGIN_ENUM_SERIALIZER(ork::ent, EOutputTimeStep)
	DECLARE_ENUM(EOutputTimeStep_RealTime)
	DECLARE_ENUM(EOutputTimeStep_15fps)
	DECLARE_ENUM(EOutputTimeStep_24fps)
	DECLARE_ENUM(EOutputTimeStep_30fps)
	DECLARE_ENUM(EOutputTimeStep_48fps)
	DECLARE_ENUM(EOutputTimeStep_60fps)
	DECLARE_ENUM(EOutputTimeStep_72fps)
	DECLARE_ENUM(EOutputTimeStep_96fps)
	DECLARE_ENUM(EOutputTimeStep_120fps)
	DECLARE_ENUM(EOutputTimeStep_240fps)
END_ENUM_SERIALIZER()
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void CompositingSystemData::Describe()
{
    using namespace ork::reflect;

	RegisterProperty("Enable", &CompositingSystemData::mbEnable );
	RegisterProperty("OutputFrames", &CompositingSystemData::mbOutputFrames );

	RegisterMapProperty("Groups", &CompositingSystemData::mCompositingGroups);

	RegisterMapProperty("Scenes", &CompositingSystemData::mScenes);

	RegisterProperty("ActiveScene", &CompositingSystemData::mActiveScene);
	RegisterProperty("ActiveItem", &CompositingSystemData::mActiveItem);

	RegisterProperty("OutputResBase", &CompositingSystemData::mOutputBaseResolution);
	RegisterProperty("OutputResMult", &CompositingSystemData::mOutputResMult);
	RegisterProperty("OutputFrameRate", &CompositingSystemData::mOutputFrameRate);

	AnnotatePropertyForEditor<CompositingSystemData>("Groups", "editor.factorylistbase", "CompositingGroup" );
	AnnotatePropertyForEditor<CompositingSystemData>("Scenes", "editor.factorylistbase", "CompositingScene" );
	AnnotatePropertyForEditor<CompositingSystemData>( "OutputResBase", "editor.class", "ged.factory.enum" );
	AnnotatePropertyForEditor<CompositingSystemData>( "OutputResMult", "editor.class", "ged.factory.enum" );
	AnnotatePropertyForEditor<CompositingSystemData>( "OutputFrameRate", "editor.class", "ged.factory.enum" );

	static const char* EdGrpStr =
		        "grp://Main Enable ActiveScene ActiveItem "
		        "grp://Output OutputFrames OutputResBase OutputResMult OutputFrameRate "
		        "grp://Data Groups Scenes ";
	reflect::AnnotateClassForEditor<CompositingSystemData>( "editor.prop.groups", EdGrpStr );

}

///////////////////////////////////////////////////////////////////////////////

CompositingSystemData::CompositingSystemData()
: mbEnable(true)
, mToggle(true)
, mbOutputFrames(false)
, mOutputFrameRate(EOutputTimeStep_RealTime)
, mOutputBaseResolution(EOutputRes_1280x720)
, mOutputResMult(EOutputResMult_Full)
{
}

///////////////////////////////////////////////////////////////////////////////

ork::ent::System* CompositingSystemData::createSystem(ork::ent::Simulation *pinst) const
{
	return new CompositingSystem( *this, pinst );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CompositingSystem::CompositingSystem( const CompositingSystemData& data, ork::ent::Simulation *pinst )
	: ork::ent::System( &data, pinst )
	, _compositingData(data)
	, miActiveSceneItem(0)
	, mfTimeAccum(0.0f)
{

    // on link ?
    mfTimeAccum=0.0f;
	mfLastTime = 0.0f;
	miActiveSceneItem = 0;

}

CompositingSystem::~CompositingSystem()
{
}

///////////////////////////////////////////////////////////////////////////////

const CompositingSceneItem* CompositingSystem::compositingItem(int isceneidx,int itemidx) const {
	const ent::CompositingSceneItem* rval = nullptr;
	const ent::CompositingScene* pscene = nullptr;
	const auto& CCD = systemData();
	const orklut<PoolString,ork::Object*>& Groups = CCD.GetGroups();
	const orklut<PoolString,ork::Object*>& Scenes = CCD.GetScenes();
	int inumgroups = Groups.size();
	int inumscenes = Scenes.size();
	if( inumscenes && isceneidx>= 0 )
	{
		int idx = isceneidx%inumscenes;
		auto it = Scenes.find( CCD.GetActiveScene() );
		if( it != Scenes.end() )
		{
			ork::Object* pOBJ = it->second;
			if( pOBJ )
				pscene = rtti::autocast(pOBJ);
		}
	}
	if( pscene && itemidx>= 0 )
	{
		const auto& Items = pscene->GetItems();
		auto it = Items.find( CCD.GetActiveItem() );
		if( it != Items.end() )
		{
			ork::Object* pOBJ = it->second;
			if( pOBJ )
				rval = rtti::autocast(pOBJ);
		}
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

bool CompositingSystem::IsEnabled() const {
    return _compositingData.IsEnabled();
}

EOutputTimeStep CompositingSystem::GetCurrentFrameRateEnum() const
{
	return   IsEnabled()
           ? _compositingData.OutputFrameRate()
           : EOutputTimeStep_RealTime;
}

float CompositingSystem::GetCurrentFrameRate() const
{
	EOutputTimeStep time_step = GetCurrentFrameRateEnum();
	float framerate = 0.0f;
	switch( time_step )
	{
		case EOutputTimeStep_15fps:
			framerate = 1.0f/15.0f;
			break;
		case EOutputTimeStep_24fps:
			framerate = 24.0f;
			break;
		case EOutputTimeStep_30fps:
			framerate = 30.0f;
			break;
		case EOutputTimeStep_48fps:
			framerate = 48.0f;
			break;
		case EOutputTimeStep_60fps:
			framerate = 60.0f;
			break;
		case EOutputTimeStep_72fps:
			framerate = 72.0f;
			break;
		case EOutputTimeStep_96fps:
			framerate = 96.0f;
			break;
		case EOutputTimeStep_120fps:
			framerate = 120.0f;
			break;
		case EOutputTimeStep_240fps:
			framerate = 240.0f;
			break;
		case EOutputTimeStep_RealTime:
		default:
			break;
	}

	return framerate;
}

///////////////////////////////////////////////////////////////////////////////

void CompositingSystem::Draw( CompositorSystemDrawData& drawdata ) {
	lev2::FrameRenderer& the_renderer = drawdata.mFrameRenderer;
	lev2::RenderContextFrameData& framedata = the_renderer.GetFrameData();
	lev2::GfxTarget* pTARG = framedata.GetTarget();
	orkstack<CompositingPassData>& cgSTACK = drawdata.mCompositingGroupStack;
	ent::CompositingSystem* pCMCI = this;

	SRect tgtrect = SRect( 0, 0, pTARG->GetW(), pTARG->GetH() );

	anyp PassData;
	PassData.Set<orkstack<ent::CompositingPassData>*>( & cgSTACK );
	the_renderer.GetFrameData().SetUserProperty( "nodes", PassData );

	/////////////////////////////////////////////////////////////////////////////////

	ESimulationMode emode = mpSimulation->GetSimulationMode();

	/////////////////////////////////
	// Lock Drawable Buffer
	/////////////////////////////////

	const DrawableBuffer* DB = DrawableBuffer::BeginDbRead(7);//mDbLock.Aquire(7);
	framedata.SetUserProperty( "DB", anyp(DB) );

	if( DB ) {
		_compcontext.Draw(pTARG,drawdata,this);
		DrawableBuffer::EndDbRead(DB);//mDbLock.Aquire(7);
	}

}

///////////////////////////////////////////////////////////////////////////////

void CompositingSystem::composeToScreen( lev2::GfxTarget* pT ){
	int scene_item = 0;
	if( auto pCSI = compositingItem(0,scene_item) ){
		_compcontext.SetTechnique( pCSI->GetTechnique() );
		_compcontext.CompositeToScreen(pT,this);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

const CompositingGroup* CompositingSystem::GetGroup(const PoolString& grpname) const {
	const CompositingGroup* rval = 0;
	if( auto sceneitem = compositingItem(0,miActiveSceneItem) ){
        auto itA = _compositingData.GetGroups().find(grpname);
		if( itA!=_compositingData.GetGroups().end() ){
    		ork::Object* pA = itA->second;
			rval = rtti::autocast(pA);
		}
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

void CompositingSystem::DoUpdate(Simulation *inst) {
	float fDT = inst->GetDeltaTime();

	mfLastTime = mfTimeAccum;
	mfTimeAccum += fDT;

	int i0 = int(mfLastTime*1.0f);
	int i1 = int(mfTimeAccum*1.0f);

	if( i1!=i0 )
		miActiveSceneItem++;
}


///////////////////////////////////////////////////////////////////////////////

const CompositingContext& CompositingSystem::GetCCtx() const {
    return _compcontext;
}

///////////////////////////////////////////////////////////////////////////////

CompositingContext& CompositingSystem::GetCCtx(){
    return _compcontext;
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
