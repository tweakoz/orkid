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
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::CompositingComponentData, "CompositingComponent");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::CompositingComponentInst, "CompositingComponentInst");
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

void CompositingManagerComponentData::Describe()
{

}

///////////////////////////////////////////////////////////////////////////////

CompositingManagerComponentData::CompositingManagerComponentData()
{
}

///////////////////////////////////////////////////////////////////////////////

ork::ent::SceneComponentInst* CompositingManagerComponentData::CreateComponentInst(ork::ent::SceneInst *pinst) const
{
	return new CompositingManagerComponentInst( *this, pinst );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void CompositingManagerComponentInst::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

CompositingManagerComponentInst::CompositingManagerComponentInst( const CompositingManagerComponentData& data, ork::ent::SceneInst *pinst )
	: ork::ent::SceneComponentInst( &data, pinst )
	, mCMCD(data)
{
}

///////////////////////////////////////////////////////////////////////////////

ent::CompositingComponentInst* CompositingManagerComponentInst::GetCompositingComponentInst( int icidx ) const
{
	ent::CompositingComponentInst* rval = 0;
	int inumsc = mCCIs.size();
	if( inumsc )
	{
		int idx = icidx%inumsc;
		rval = mCCIs[idx];
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

void CompositingManagerComponentInst::AddCCI( CompositingComponentInst* cci )
{
	mCCIs.push_back(cci);
}

///////////////////////////////////////////////////////////////////////////////

const CompositingSceneItem* CompositingComponentInst::GetCompositingItem(int isceneidx,int itemidx) const
{
	const ent::CompositingSceneItem* rval = 0;
	const ent::CompositingScene* pscene = 0;
	const CompositingComponentData& CCD = GetCompositingData();
	const orklut<PoolString,ork::Object*>& Groups = CCD.GetGroups();
	const orklut<PoolString,ork::Object*>& Scenes = CCD.GetScenes();
	int inumgroups = Groups.size();
	int inumscenes = Scenes.size();
	if( inumscenes && isceneidx>= 0 )
	{
		int idx = isceneidx%inumscenes;
		orklut<PoolString,ork::Object*>::const_iterator it = Scenes.find( CCD.GetActiveScene() );
		if( it != Scenes.end() )
		{
			ork::Object* pOBJ = it->second;
			if( pOBJ )
				pscene = rtti::autocast(pOBJ);		
		}
	}
	if( pscene && itemidx>= 0 )
	{
		const orklut<PoolString,ork::Object*>& Items = pscene->GetItems();
		orklut<PoolString,ork::Object*>::const_iterator it = Items.find( CCD.GetActiveItem() );
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

bool CompositingManagerComponentInst::IsEnabled() const
{	bool brval = false;
	CompositingComponentInst* pCCI = GetCompositingComponentInst(0);
	if( pCCI )
	{
		brval = pCCI->GetCompositingData().IsEnabled();
	}
	return brval;
}
EOutputTimeStep CompositingManagerComponentInst::GetCurrentFrameRateEnum() const
{
	EOutputTimeStep time_step = EOutputTimeStep_RealTime;
	auto cci = IsEnabled() ? GetCompositingComponentInst(0) : nullptr;
	if( cci )
	{
		time_step = cci->GetCompositingData().OutputFrameRate();
	}
	return time_step;	
}
float CompositingManagerComponentInst::GetCurrentFrameRate() const
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

void CompositingManagerComponentInst::Draw( CMCIdrawdata& drawdata )
{
	lev2::FrameRenderer& the_renderer = drawdata.mFrameRenderer;
	lev2::RenderContextFrameData& framedata = the_renderer.GetFrameData();
	lev2::GfxTarget* pTARG = framedata.GetTarget();
	orkstack<CompositingPassData>& cgSTACK = drawdata.mCompositingGroupStack;
	ent::CompositingManagerComponentInst* pCMCI = this;
	
	SRect tgtrect = SRect( 0, 0, pTARG->GetW(), pTARG->GetH() );

	anyp PassData;
	PassData.Set<orkstack<ent::CompositingPassData>*>( & cgSTACK );
	the_renderer.GetFrameData().SetUserProperty( "nodes", PassData );
	
	/////////////////////////////////////////////////////////////////////////////////
	
	ESceneInstMode emode = mpSceneInst->GetSceneInstMode();

	/////////////////////////////////
	// Lock Drawable Buffer
	/////////////////////////////////

	const DrawableBuffer* DB = DrawableBuffer::BeginDbRead(7);//mDbLock.Aquire(7);
	framedata.SetUserProperty( "DB", anyp(DB) );

	if( DB )
	{
		/////////////////////////////////
		// get compositor info
		/////////////////////////////////

		CompositingComponentInst* pCCI = GetCompositingComponentInst(0);
		
		if( pCCI )
		{
			CompositingContext& CTX = pCCI->GetCCtx();
			CTX.Draw(pTARG,drawdata,pCCI);
		}	
		DrawableBuffer::EndDbRead(DB);//mDbLock.Aquire(7);
	}

}

///////////////////////////////////////////////////////////////////////////////

void CompositingManagerComponentInst::ComposeToScreen( lev2::GfxTarget* pT )
{
	CompositingContext& cctx = mCMCD.GetCompositingContext();
	CompositingComponentInst* pCCI = GetCompositingComponentInst(0);
	const ent::CompositingSceneItem* pCSI = 0;
	int iCSitem = 0;
	if( pCCI )
	{
		pCSI = pCCI->GetCompositingItem(0,iCSitem);
	}
	/////////////////////////////////////////////////////////////////////
	if( pCSI )
	{
		cctx.SetTechnique( pCSI->GetTechnique() );
		cctx.CompositeToScreen(pT,pCCI);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void CompositingComponentData::Describe()
{
	using namespace ork::reflect;

	ork::ent::RegisterFamily<CompositingComponentData>(ork::AddPooledLiteral("control"));

	RegisterProperty("Enable", &CompositingComponentData::mbEnable );
	RegisterProperty("OutputFrames", &CompositingComponentData::mbOutputFrames );

	RegisterMapProperty("Groups", &CompositingComponentData::mCompositingGroups);

	RegisterMapProperty("Scenes", &CompositingComponentData::mScenes);

	RegisterProperty("ActiveScene", &CompositingComponentData::mActiveScene);
	RegisterProperty("ActiveItem", &CompositingComponentData::mActiveItem);

	RegisterProperty("OutputResBase", &CompositingComponentData::mOutputBaseResolution);
	RegisterProperty("OutputResMult", &CompositingComponentData::mOutputResMult);
	RegisterProperty("OutputFrameRate", &CompositingComponentData::mOutputFrameRate);

	AnnotatePropertyForEditor<CompositingComponentData>("Groups", "editor.factorylistbase", "CompositingGroup" );
	AnnotatePropertyForEditor<CompositingComponentData>("Scenes", "editor.factorylistbase", "CompositingScene" );
	AnnotatePropertyForEditor<CompositingComponentData>( "OutputResBase", "editor.class", "ged.factory.enum" );
	AnnotatePropertyForEditor<CompositingComponentData>( "OutputResMult", "editor.class", "ged.factory.enum" );
	AnnotatePropertyForEditor<CompositingComponentData>( "OutputFrameRate", "editor.class", "ged.factory.enum" );

	static const char* EdGrpStr =
		        "grp://Main Enable ActiveScene ActiveItem "
		        "grp://Output OutputFrames OutputResBase OutputResMult OutputFrameRate "
		        "grp://Data Groups Scenes ";
	reflect::AnnotateClassForEditor<CompositingComponentData>( "editor.prop.groups", EdGrpStr );

}

///////////////////////////////////////////////////////////////////////////////

CompositingComponentData::CompositingComponentData()
	: mbEnable(true)
	, mToggle(true)
	, mbOutputFrames(false)
	, mOutputFrameRate(EOutputTimeStep_RealTime)
	, mOutputBaseResolution(EOutputRes_1280x720)
	, mOutputResMult(EOutputResMult_Full)
{
}

///////////////////////////////////////////////////////////////////////////////

ork::ent::ComponentInst* CompositingComponentData::CreateComponent(ork::ent::Entity *pent) const
{
	return new CompositingComponentInst( *this, pent );
}

///////////////////////////////////////////////////////////////////////////////

void CompositingComponentData::DoRegisterWithScene( ork::ent::SceneComposer& sc )
{
	sc.Register<ork::ent::CompositingManagerComponentData>();
	//sc.Register<ork::ent::FullscreenEffectComponentData>();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void CompositingComponentInst::Describe()
{

}

///////////////////////////////////////////////////////////////////////////////

CompositingComponentInst::CompositingComponentInst( const CompositingComponentData& data, ork::ent::Entity *pent )
	: ork::ent::ComponentInst( &data, pent )
	, mCompositingData(data)
	, mpCMCI(0)
	, miActiveSceneItem(0)
	, mfTimeAccum(0.0f)
{
	SceneInst* psi = pent->GetSceneInst();
}

///////////////////////////////////////////////////////////////////////////////

CompositingComponentInst::~CompositingComponentInst()
{
}

///////////////////////////////////////////////////////////////////////////////

const CompositingGroup* CompositingComponentInst::GetGroup(const PoolString& grpname) const
{
	const CompositingGroup* rval = 0;
	const CompositingSceneItem* pCSI = GetCompositingItem(0,miActiveSceneItem);
	if( pCSI )
	{
		const CompositingComponentData& CCD = GetCompositingData();
		orklut<PoolString,ork::Object*>::const_iterator itA = CCD.GetGroups().find(grpname);
		if( itA!=CCD.GetGroups().end() )
		{	ork::Object* pA = itA->second;
			rval = rtti::autocast(pA);
		}
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

bool CompositingComponentInst::DoLink(ork::ent::SceneInst *psi)
{
	mpCMCI = psi->FindTypedSceneComponent<CompositingManagerComponentInst>();

	ork::ent::CompositingManagerComponentInst* cmi = GetEntity()->GetSceneInst()->FindTypedSceneComponent<ent::CompositingManagerComponentInst>();

	if(cmi)
		cmi->AddCCI(this);
		
	mfTimeAccum=0.0f;
	mfLastTime = 0.0f;
	miActiveSceneItem = 0;
	
	return true;
}

///////////////////////////////////////////////////////////////////////////////

void CompositingComponentInst::DoUnLink(SceneInst *psi)
{
}

///////////////////////////////////////////////////////////////////////////////

void CompositingComponentInst::DoUpdate(SceneInst *inst)
{
	float fDT = inst->GetDeltaTime();
	
	mfLastTime = mfTimeAccum;
	mfTimeAccum += fDT;

	int i0 = int(mfLastTime*1.0f);
	int i1 = int(mfTimeAccum*1.0f);

	if( i1!=i0 )
		miActiveSceneItem++;
}


///////////////////////////////////////////////////////////////////////////////

const CompositingContext& CompositingComponentInst::GetCCtx() const
{
	assert(mpCMCI!=nullptr);
	const CompositingManagerComponentData& CMCD = mpCMCI->GetCMCD();
	return CMCD.GetCompositingContext();
}

///////////////////////////////////////////////////////////////////////////////

CompositingContext& CompositingComponentInst::GetCCtx()
{
	const CompositingManagerComponentData& CMCD = mpCMCI->GetCMCD();
	return CMCD.GetCompositingContext();
}

///////////////////////////////////////////////////////////////////////////////
bool CompositingComponentInst::DoNotify(const ork::event::Event *event)
{
	if( const dataflow::morph_event* pme = rtti::autocast(event) )
	{
		mMorphable.HandleMorphEvent( pme );
	}
	else if( const ork::ent::PerfControlEvent* pce = rtti::autocast(event) )
	{
		const char* keyname = pce->mTarget.c_str();
		printf( "CompositingComponentInst<%p> PerfControlEvent<%p> key<%s>\n", this, pce, keyname );
		PoolString v = AddPooledString(pce->mValue.c_str());

		if( 0 == strcmp(keyname,"ActiveScene") )
		{	mCompositingData.GetActiveScene() = v;
			printf( "Apply Value<%s> to CompositingComponentInst<%p>.ActiveScene\n", v.c_str(), this );
		}
		else if( 0 == strcmp(keyname,"ActiveItem") )
		{	mCompositingData.GetActiveItem() = v;
			printf( "Apply Value<%s> to CompositingComponentInst<%p>.ActiveItem\n", v.c_str(), this );
		}
		
		return true;

	}
	else if( const ork::ent::PerfSnapShotEvent* psse = rtti::autocast(event) )
	{
		const PoolString& ActiveScene = mCompositingData.GetActiveScene();
		const PoolString& ActiveItem = mCompositingData.GetActiveItem();
		
		psse->PushNode( "ActiveScene" );
		{	
			ent::PerfSnapShotEvent::str_type NodeName = psse->GenNodeName();
			PerfProgramTarget* target = new PerfProgramTarget( NodeName.c_str(), ActiveScene.c_str() );
			psse->GetProgram()->AddTarget( NodeName.c_str(), target );
		}
		psse->PopNode();
		
		psse->PushNode( "ActiveItem" );
		{	
			ent::PerfSnapShotEvent::str_type NodeName = psse->GenNodeName();
			PerfProgramTarget* target = new PerfProgramTarget( NodeName.c_str(), ActiveItem.c_str() );
			psse->GetProgram()->AddTarget( NodeName.c_str(), target );
		}
		psse->PopNode();
		
	
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////

void CompositorArchetype::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

CompositorArchetype::CompositorArchetype()
{
}

///////////////////////////////////////////////////////////////////////////////

void CompositorArchetype::DoCompose(ork::ent::ArchComposer& composer)
{
	composer.Register<ork::ent::CompositingComponentData>();
}
  
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
