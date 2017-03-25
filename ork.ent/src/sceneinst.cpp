////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/event/EventListener.h>
#include <ork/application/application.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/drawable.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/Compositor.h>
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
#include <ork/lev2/input/input.h>
#include <ork/kernel/debug.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork
{
	void EnterRunMode();
	void LeaveRunMode();
};

///////////////////////////////////////////////////////////////////////////////

#define VERBOSE 1
#define PRINT_CONDITION (ork::PieceString(pent->GetEntData().GetName()).find("missile") != ork::PieceString::npos)

#if VERBOSE
#	define DEBUG_PRINT	orkprintf
#else
#	define DEBUG_PRINT(...)
#endif

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SceneInst,"Ent3dSceneInst");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SceneInstEvent,"SceneInstEvent");

///////////////////////////////////////////////////////////////////////////////

template class ork::orklut<const ork::object::ObjectClass *, ork::ent::SceneComponentInst *>;

namespace ork { namespace ent {

static ork::PoolString sSceneInstEvChanName;
static ork::PoolString sAudioFamily;
static ork::PoolString sCameraFamily;
static ork::PoolString sControlFamily;
static ork::PoolString sMotionFamily;
static ork::PoolString sPhysicsFamily;
static ork::PoolString sPositionFamily;
static ork::PoolString sFrustumFamily;
static ork::PoolString sAnimateFamily;
static ork::PoolString sParticleFamily;
static ork::PoolString sLightFamily;
static ork::PoolString sInputFamily;

void SceneInstEvent::Describe()
{
	sSceneInstEvChanName = ork::AddPooledLiteral("SceneInstEvChannel");
}
const ork::PoolString& SceneInst::EventChannel()
{
	return sSceneInstEvChanName;
}

///////////////////////////////////////////////////////////////////////////////
void SceneInst::Describe()
{
	reflect::RegisterFunctor("SlotSceneTopoChanged", &SceneInst::SlotSceneTopoChanged);

	//reflect::RegisterFunctor("QueueActivateEntity", &SceneInst::QueueActivateEntity);
	//reflect::RegisterFunctor("QueueDeactivateEntity", &SceneInst::QueueDeactivateEntity);

	sInputFamily = ork::AddPooledLiteral("input");
	sAudioFamily = ork::AddPooledLiteral("audio");
	sCameraFamily = ork::AddPooledLiteral("camera");
	sControlFamily = ork::AddPooledLiteral("control");
	sPhysicsFamily = ork::AddPooledLiteral("physics");
	sFrustumFamily = ork::AddPooledLiteral("frustum");
	sAnimateFamily = ork::AddPooledLiteral("animate");
	sParticleFamily = ork::AddPooledLiteral("particle");
	sLightFamily = ork::AddPooledLiteral("lighting");

}
///////////////////////////////////////////////////////////////////////////////
SceneInst::SceneInst( const SceneData* sdata, Application *application )
	: mSceneData(sdata)
	, meSceneInstMode(ESCENEMODE_ATTACHED)
	, mApplication(application)
	, mUpTime(0.0f)
	, mDeltaTime(0.0f)
	, mPrevDeltaTime(0.0f)
	, mLastGameTime(0.0f)
	, mStartTime(0.0f)
	, mUpDeltaTime(0.0f)
	, mGameTime(0.0f)
	, mDeltaTimeAccum(0.0f)
	, mfAvgDtAcc(0.0f)
	, mfAvgDtCtr(0.0f)
	, mEntityUpdateCount(0)
{
	AssertOnOpQ2( UpdateSerialOpQ() );
	OrkAssertI(mApplication, "SceneInst must be constructed with a non-NULL Application!");
	
	Layer* player = new Layer;
	AddLayer( AddPooledLiteral("Default"), player );

	////////////////////////////
	// create one token
	////////////////////////////
	RenderSyncToken rentok;
	while(DrawableBuffer::mOfflineRenderSynchro.try_pop(rentok)){}
	while(DrawableBuffer::mOfflineUpdateSynchro.try_pop(rentok)){}
	rentok.mFrameIndex = 0;
	DrawableBuffer::mOfflineUpdateSynchro.push(rentok); // push 1 token

}
///////////////////////////////////////////////////////////////////////////////
SceneInst::~SceneInst()
{
	AssertOnOpQ2( UpdateSerialOpQ() );
	DrawableBuffer::BeginClearAndSyncReaders();
	for( orkmap<PoolString,Entity*>::iterator it=mEntities.begin(); it!=mEntities.end(); it++ )
	{
		Entity* pent = it->second;

		if( pent )
		{
			delete pent;
		}
	}
	for( SceneComponentLut::iterator it=mSceneComponents.begin(); it!=mSceneComponents.end(); it++ )
	{
		SceneComponentInst* pSCI = it->second;

		if( pSCI )
		{
			delete pSCI;
		}
	}
	DrawableBuffer::EndClearAndSyncReaders();

	////////////////////////////
	// steal all RenderSyncToken's
	////////////////////////////
	RenderSyncToken rentok;
	while(DrawableBuffer::mOfflineRenderSynchro.try_pop(rentok)){}
	while(DrawableBuffer::mOfflineUpdateSynchro.try_pop(rentok)){}
	////////////////////////////
}
///////////////////////////////////////////////////////////////////////////

CompositingManagerComponentInst* SceneInst::GetCMCI()
{
	auto cmci = FindTypedSceneComponent<CompositingManagerComponentInst>();
	return cmci;
}

///////////////////////////////////////////////////////////////////////////////
float SceneInst::ComputeDeltaTime()
{
	auto cmci = GetCMCI();
	float frame_rate = cmci ? cmci->GetCurrentFrameRate() : 0.0f;

	AssertOnOpQ2( UpdateSerialOpQ() );
	float systime = float(CSystem::GetRef().GetLoResTime());
	float fdelta = (frame_rate!=0.0f)
					? (1.0f/frame_rate) 
					: (systime - mUpTime);

	static float fbasetime = systime;

	if( fdelta == 0.0f ) return 0.0f;

	mUpTime = systime;
	mUpDeltaTime = fdelta;

	////////////////////////////////////////////
	// allowed FPS range is 1000hz to .5 hz
	////////////////////////////////////////////
	if(fdelta < 0.00001f )
	{
		//orkprintf( "FPS is over 10000HZ!!!! you need to reset valid fps range\n" );
		//fdelta=0.001f;
		//ork::msleep(1);
		systime = float(CSystem::GetRef().GetLoResTime());
		fdelta = 0.00001f;
		mUpTime = systime;
		mUpDeltaTime = fdelta;
	}
	else if(fdelta > 0.1f)
	{
		//orkprintf( "FPS is less than 10HZ!!!! you need to reset valid fps range\n" );
		fdelta = 0.1f;
	}

	////////////////////////////////////////////

	mUpDeltaTime = fdelta;

	switch( this->GetSceneInstMode() )
	{
        case ork::ent::ESCENEMODE_ATTACHED:
        case ork::ent::ESCENEMODE_EDIT:
        case ork::ent::ESCENEMODE_SINGLESTEP:
            break;
		case ork::ent::ESCENEMODE_PAUSE:
		{
			mDeltaTime = 0.0f;
//			mDeltaTimeAccum = 1.0f/240.0f;
			break;
		}
		case ork::ent::ESCENEMODE_RUN:
		{
			///////////////////////////////
			// update clock
			///////////////////////////////
			
			mDeltaTime = (mPrevDeltaTime + fdelta)/2;
			mPrevDeltaTime = fdelta;
			mLastGameTime = mGameTime;
			mGameTime += mDeltaTime;
		}
	}

//	printf( "mGameTime<%f>\n", mGameTime );
	
	return fdelta;

}
///////////////////////////////////////////////////////////////////////////////
void SceneInst::SetCameraData(const PoolString& name, const CCameraData*camdat)
{
	CameraLut::iterator it = mCameraLut.find(name);

	if( it == mCameraLut.end() )
	{
		if( camdat != 0 )
		{
			mCameraLut.AddSorted( name, camdat );
		}
	}
	else
	{
		if( camdat == 0 )
		{
			mCameraLut.erase(it);
		}
		else
		{
			it->second = camdat;
		}
	}

	lev2::CCamera* pcam = (camdat!=0) ? camdat->GetLev2Camera() : 0;
	
	//orkprintf( "SceneInst::SetCameraData() name<%s> camdat<%p> l2cam<%p>\n", name.c_str(), camdat, pcam );
}

///////////////////////////////////////////////////////////////////////////////
const CCameraData* SceneInst::GetCameraData(const PoolString& name ) const
{
	CameraLut::const_iterator it = mCameraLut.find(name);
	return (it==mCameraLut.end()) ? 0 : it->second;
}
///////////////////////////////////////////////////////////////////////////////
void SceneInst::SlotSceneTopoChanged()
{	
	auto topo_op = [=]()
	{
		this->GetData().AutoLoadAssets();
		this->EnterEditState();
	};
	UpdateSerialOpQ().push(Op(topo_op));

}
///////////////////////////////////////////////////////////////////////////////
void SceneInst::UpdateEntityComponents(const SceneInst::ComponentList& components)
{
	AssertOnOpQ2( UpdateSerialOpQ() );
	for(SceneInst::ComponentList::const_iterator it = components.begin(); it != components.end(); ++it)
	{
		ComponentInst* pci = (*it);
		OrkAssert( pci != 0 );
		pci->Update(this);
	}
}
///////////////////////////////////////////////////////////////////////////
ent::Entity* SceneInst::GetEntity( const ent::EntData* pdata ) const
{
	const PoolString& name = pdata->GetName();
	ent::Entity* pent = FindEntity( name );
	return pent;
}
///////////////////////////////////////////////////////////////////////////
void SceneInst::SetEntity( const ent::EntData* pentdata, ent::Entity* pent )
{
	AssertOnOpQ2( UpdateSerialOpQ() );
	assert(pent!=nullptr);
	mEntities[pentdata->GetName()] = pent;
}
///////////////////////////////////////////////////////////////////////////
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET     "\x1b[30m"
#define ANSI_COLOR_GREEN     "\x1b[32m"

void SceneInst::EnterEditState()
{
	printf( "%s", ANSI_COLOR_RED );
	printf( "////////////////////////\n");
	printf( "SceneInst<%p> EnterEditState\n", this );
	printf( "////////////////////////\n");
	printf( "%s", ANSI_COLOR_GREEN );
	DrawableBuffer::BeginClearAndSyncReaders();
	AssertOnOpQ2( UpdateSerialOpQ() );

	SceneInstEvent bindev( this, SceneInstEvent::ESIEV_BIND );

	ork::event::Broadcaster::GetRef().BroadcastNotifyOnChannel( & bindev, EventChannel() );


	LeaveRunMode();
	ork::lev2::AudioDevice::GetDevice()->StopAllVoices();
	StopEntities();
	mActiveEntityComponents.clear();
	mActiveEntities.clear();
	mEntityDeactivateQueue.clear();

	//StartSceneComponents();

	ComposeEntities();
	//LinkSceneComponents();
	LinkEntities();

	ServiceDeactivateQueue();// HACK TO REMOVE ENTITIES QUEUED FOR DEACTIVATION WHILE LINKING
	StartEntities();
	mStartTime = float(CSystem::GetRef().GetLoResTime());
	mGameTime = 0.0f;
	mUpDeltaTime = 0.0f;
	mPrevDeltaTime = 1.0f/30.0f;
	mDeltaTime = 1.0f/30.0f;
	mDeltaTimeAccum = 0.0f;
	mUpTime = mStartTime;
	mLastGameTime = 0.0f;
	ServiceDeactivateQueue();
	SceneInstEvent outev( this, SceneInstEvent::ESIEV_START );
	ork::Application::GetContext()->Notify( & outev );
	DrawableBuffer::EndClearAndSyncReaders();
}
///////////////////////////////////////////////////////////////////////////
void SceneInst::EnterPauseState()
{
	ork::lev2::AudioDevice::GetDevice()->StopAllVoices();
}
///////////////////////////////////////////////////////////////////////////////
void SceneInst::EnterRunState()
{
	printf( "%s", ANSI_COLOR_RED );
	printf( "////////////////////////\n");
	printf( "SceneInst<%p> EnterRunState\n", this );
	printf( "////////////////////////\n");
	printf( "%s", ANSI_COLOR_GREEN );

	DrawableBuffer::BeginClearAndSyncReaders();
	AssertOnOpQ2( UpdateSerialOpQ() );
	EnterRunMode();

	AllocationLabel label("SceneInst::EnterRunState::255");

	SceneInstEvent bindev( this, SceneInstEvent::ESIEV_BIND );
	ork::Application::GetContext()->Notify( & bindev );

	ork::lev2::AudioDevice::GetDevice()->StopAllVoices();

	mActiveEntityComponents.clear();
	mActiveEntities.clear();
	mEntityDeactivateQueue.clear();

	//DoEnterRunState();

	AllocationLabel label268("SceneInst::EnterRunState::268");

	ComposeEntities();
	ComposeSceneComponents();

	for( const auto& item : mEntities )
	{
		auto pent = item.second;
		ActivateEntity( pent );
	}

	AllocationLabel label281("SceneInst::EnterRunState::281");

	LinkEntities();
	LinkSceneComponents();

	// HACK TO REMOVE ENTITIES QUEUED FOR DEACTIVATION WHILE LINKING
	ServiceDeactivateQueue();

	AllocationLabel label288("SceneInst::EnterRunState:288");

	StartEntities();
	StartSceneComponents();

	mStartTime = float(CSystem::GetRef().GetLoResTime());
	mGameTime = 0.0f;
	mUpDeltaTime = 0.0f;
	mPrevDeltaTime = 1.0f/30.0f;
	mDeltaTime = 1.0f/30.0f;
	mDeltaTimeAccum = 0.0f;
	mUpTime = mStartTime;
	mLastGameTime = 0.0f;

	ServiceDeactivateQueue();

	AllocationLabel label303("SceneInst::EnterRunState::303");

	SceneInstEvent outev( this, SceneInstEvent::ESIEV_START );
	ork::Application::GetContext()->Notify( & outev );
	DrawableBuffer::EndClearAndSyncReaders();

	RenderSyncToken rentok;
	while(DrawableBuffer::mOfflineRenderSynchro.try_pop(rentok)){}
	while(DrawableBuffer::mOfflineUpdateSynchro.try_pop(rentok)){}
	rentok.mFrameIndex = 0;
	DrawableBuffer::mOfflineUpdateSynchro.push(rentok);

}
///////////////////////////////////////////////////////////////////////////////
void SceneInst::OnSceneInstMode( ESceneInstMode emode )
{	
	AssertOnOpQ2( UpdateSerialOpQ() );

	switch( meSceneInstMode )
	{
        case ork::ent::ESCENEMODE_ATTACHED:
        case ork::ent::ESCENEMODE_EDIT:
        case ork::ent::ESCENEMODE_SINGLESTEP:
        case ork::ent::ESCENEMODE_PAUSE:
            break;
		case ESCENEMODE_RUN: // leaving runstate
			//SetCameraData( ork::AddPooledLiteral("game1"), 0 );
			break;
	}
	
	switch( emode )
	{	case ESCENEMODE_EDIT:
			EnterEditState();
			break;
		case ESCENEMODE_RUN:
			EnterRunState();
			break;
		case ESCENEMODE_SINGLESTEP:
			break;
		case ESCENEMODE_PAUSE:
			EnterPauseState();
			break;
        case ork::ent::ESCENEMODE_ATTACHED:
            break;
	}
	this->meSceneInstMode = emode;
}

///////////////////////////////////////////////////////////////////////////
void SceneInst::DecomposeEntities()
{
	//printf( "SceneInst<%p> BEGIN DecomposeEntities()\n", this );
	//printf( "/////////////////////////////////////\n");
	//std::string bt = get_backtrace();
	//printf( "%s", bt.c_str() );
	AssertOnOpQ2( UpdateSerialOpQ() );
	for( auto item : mEntities )
	{
		const ork::PoolString& name = item.first;
		ork::ent::Entity* pent = item.second;

		//ork::ent::Archetype* arch = pent->GetArchetype();

		//printf( "deleting ent<%p:%s>\n", pent, name.c_str() );
		delete pent;
	}
	mEntities.clear();

	//printf( "/////////////////////////////////////\n");
	//printf( "SceneInst<%p> END DecomposeEntities()\n", this );
}
///////////////////////////////////////////////////////////////////////////
void SceneInst::ComposeEntities()
{
	AssertOnOpQ2( UpdateSerialOpQ() );
	///////////////////////////////////
	// clear runtime containers
	///////////////////////////////////

	UnLinkEntities();
	DecomposeEntities();
	mEntities.clear();
	mCameraLut.clear();

	///////////////////////////////////
	// Compose Entities
	///////////////////////////////////

	//orkprintf( "beg si<%p> Compose Entities..\n", this );

	for( orkmap<ork::PoolString, ork::ent::SceneObject*>::const_iterator it=mSceneData->GetSceneObjects().begin(); it!=mSceneData->GetSceneObjects().end(); it++ )
	{
		ork::ent::SceneObject* sobj = (*it).second;
		if(ork::ent::EntData* pentdata = ork::rtti::autocast(sobj))
		{
			const ork::ent::Archetype* arch = pentdata->GetArchetype();

			ork::ent::Entity* pent = new ork::ent::Entity( *pentdata, this );

			PoolString actualLayerName = AddPooledLiteral("Default");
	
			ConstString layer_name = pentdata->GetUserProperty("DrawLayer");
			if( strlen( layer_name.c_str() ) != 0 )
			{
				actualLayerName = AddPooledString(layer_name.c_str());
			}

			Layer* player = GetLayer( actualLayerName );
			if( 0 == player )
			{
				player = new Layer;
				AddLayer( actualLayerName, player );
			}
			////////////////////////////////////////////////////////////////

			//printf( "Compose Entity<%p> arch<%p> layer<%s>\n", pent, arch, layer_name.c_str() );
			if( arch )
			{
				arch->ComposeEntity( pent );
			}
			assert(pent!=nullptr);
			mEntities[pentdata->GetName()] = pent;
		}
	}
	
	GetData().AutoLoadAssets();	

	//orkprintf( "end si<%p> Compose Entities..\n", this );
}
///////////////////////////////////////////////////////////////////////////
void SceneInst::LinkEntities()
{
	//orkprintf( "beg si<%p> Link Entities..\n", this );
	AssertOnOpQ2( UpdateSerialOpQ() );
	///////////////////////////////////
	// Link Entities
	///////////////////////////////////

	//orkprintf( "Link Entities..\n" );
	for( auto item : mEntities )
	{
		ork::ent::Entity* pent = item.second;
		const ork::ent::EntData& edata = pent->GetEntData();

		OrkAssert( pent );

		if( edata.GetArchetype() )
		{
			edata.GetArchetype()->LinkEntity( this, pent );
		}
	}

	//orkprintf( "end si<%p> Link Entities..\n", this );
}

///////////////////////////////////////////////////////////////////////////

void SceneInst::UnLinkEntities()
{
	//orkprintf( "beg si<%p> Link Entities..\n", this );
	AssertOnOpQ2( UpdateSerialOpQ() );

	///////////////////////////////////
	// Link Entities
	///////////////////////////////////

	//orkprintf( "Link Entities..\n" );
	for( auto item : mEntities )
	{
		ork::ent::Entity* pent = item.second;
		const ork::ent::EntData& edata = pent->GetEntData();

		OrkAssert( pent );

		if( edata.GetArchetype() )
		{
			edata.GetArchetype()->UnLinkEntity( this, pent );
		}
	}
	//orkprintf( "end si<%p> Link Entities..\n", this );
}

///////////////////////////////////////////////////////////////////////////

void SceneInst::ComposeSceneComponents()
{
	AssertOnOpQ2( UpdateSerialOpQ() );

	///////////////////////////////////
	// SceneComponents
	///////////////////////////////////

	const SceneData::SceneComponentLut& SceneCompLut = mSceneData->GetSceneComponents();

	for( SceneData::SceneComponentLut::const_iterator it=SceneCompLut.begin(); it!=SceneCompLut.end(); it++ )
	{
		const SceneComponentData* pscd = it->second;
		AddSceneComponent( pscd->CreateComponentInst( this ) );
	}

}

void SceneInst::DecomposeSceneComponents()
{
	AssertOnOpQ2( UpdateSerialOpQ() );

	for( auto item : mSceneComponents )
	{
		auto comp = item.second;
		delete comp;
	}
	mSceneComponents.clear();

}
///////////////////////////////////////////////////////////////////////////

void SceneInst::LinkSceneComponents()
{
	for( auto it : mSceneComponents )
	{
		SceneComponentInst* ci = it.second;
		ci->Link(this);
	}
}

///////////////////////////////////////////////////////////////////////////

void SceneInst::UnLinkSceneComponents()
{
	AssertOnOpQ2( UpdateSerialOpQ() );

	///////////////////////////////////

	for( auto it : mSceneComponents )
	{
		SceneComponentInst* ci = it.second;
		ci->UnLink(this);
	}
	
}

///////////////////////////////////////////////////////////////////////////

void SceneInst::StartSceneComponents()
{
	AssertOnOpQ2( UpdateSerialOpQ() );
	for( auto it : mSceneComponents )
	{
		SceneComponentInst* ci = it.second;
		ci->Start(this);
	}
}
///////////////////////////////////////////////////////////////////////////
void SceneInst::StopSceneComponents()
{
	AssertOnOpQ2( UpdateSerialOpQ() );
	for( auto it : mSceneComponents )
	{
		SceneComponentInst* ci = it.second;
		ci->Stop(this);
	}
	mSceneComponents.clear();
}

///////////////////////////////////////////////////////////////////////////
void SceneInst::StartEntities()
{
	AssertOnOpQ2( UpdateSerialOpQ() );

	///////////////////////////////////
	// Start Entities
	///////////////////////////////////

	//orkprintf( "Start Entities..\n" );
	for( orkmap<ork::PoolString, ork::ent::Entity*>::const_iterator it=mEntities.begin(); it!=mEntities.end(); it++ )
	{
		ork::ent::Entity* pent = it->second;
		const ork::ent::EntData& edata = pent->GetEntData();

		OrkAssert( pent );

		if( edata.GetArchetype() )
		{
			CMatrix4 world = pent->GetDagNode().GetTransformNode().GetTransform().GetMatrix();
			edata.GetArchetype()->StartEntity( this, world, pent );
		}
	}
}
///////////////////////////////////////////////////////////////////////////
void SceneInst::StopEntities()
{
	AssertOnOpQ2( UpdateSerialOpQ() );
	///////////////////////////////////
	// Start Entities
	///////////////////////////////////

	for( orkmap<ork::PoolString, ork::ent::Entity*>::const_iterator it=mEntities.begin(); it!=mEntities.end(); it++ )
	{
		ork::ent::Entity* pent = it->second;
		const ork::ent::EntData& edata = pent->GetEntData();

		OrkAssert( pent );

		if( edata.GetArchetype() )
		{
			edata.GetArchetype()->StopEntity( this, pent );
		}
	}
}

///////////////////////////////////////////////////////////////////////////
void SceneInst::QueueActivateEntity(const EntityActivationQueueItem& item) 
{ 
	//DEBUG_PRINT( "QueueActivateEntity<%p:%s>\n",  item.mpEntity, item.mpEntity->GetEntData().GetName().c_str() );
	mEntityActivateQueue.push_back(item); 
}
///////////////////////////////////////////////////////////////////////////
void SceneInst::QueueDeactivateEntity(Entity *pent) 
{ 
	//printf( "QueueDeActivateEntity<%p:%s>\n",  pent, pent->GetEntData().GetName().c_str() );
	mEntityDeactivateQueue.push_back(pent); 
}
///////////////////////////////////////////////////////////////////////////
void SceneInst::ActivateEntity(ent::Entity* pent)
{
	AssertOnOpQ2( UpdateSerialOpQ() );
	//DEBUG_PRINT( "ActivateEntity<%p:%s>\n",  pent, pent->GetEntData().GetName().c_str()  );
	EntitySet::iterator it = mActiveEntities.find( pent );
	if(it == mActiveEntities.end())
	{
		mActiveEntities.insert(pent);

		/////////////////////////////////////////
		// activate components
		/////////////////////////////////////////

		ent::ComponentTable::LutType &EntComps = pent->GetComponents().GetComponents();
		for(ent::ComponentTable::LutType::const_iterator itc = EntComps.begin(); itc != EntComps.end(); itc++)
		{
			ent::ComponentInst *cinst = (*itc).second;

			PoolString fam = cinst->GetFamily();

			// Don't add components that don't do anything to the active components list
			if(fam.empty())
				continue;

			ActiveComponentType::iterator itl = mActiveEntityComponents.find(fam);
			if(itl == mActiveEntityComponents.end())
				mActiveEntityComponents.insert(std::make_pair(fam, orklist<ork::ent::ComponentInst *>()));
			itl = mActiveEntityComponents.find(fam);

			(itl->second).push_back(cinst);
		}
	}
	else
	{
		orkprintf( "WARNING, activating an already active entity <%p>\n", pent );
	}
}
///////////////////////////////////////////////////////////////////////////
void SceneInst::DeActivateEntity(ent::Entity* pent)
{
	AssertOnOpQ2( UpdateSerialOpQ() );
	//printf( "DeActivateEntity<%p:%s>\n",  pent, pent->GetEntData().GetName().c_str() );

	EntitySet::iterator listit = mActiveEntities.find(pent);

	const Archetype *parch = pent->GetEntData().GetArchetype();
	if(listit == mActiveEntities.end())
	{
		PoolString parchname = (parch != 0) ? parch->GetName() : AddPooledLiteral("none");
		PoolString pentname = pent->GetEntData().GetName();

		orkprintf( "uhoh, someone is deactivating an entity<%p:%s> of arch<%s> that is not active!!!\n", pent, pentname.c_str(), parchname.c_str() );
		return;
	}
	OrkAssert(listit != mActiveEntities.end());

	mActiveEntities.erase(listit);

	if(parch)
		parch->StopEntity(this, pent);

	/////////////////////////////////////////
	// deactivate components
	/////////////////////////////////////////

	ent::ComponentTable::LutType& EntComps = pent->GetComponents().GetComponents();
	for( ent::ComponentTable::LutType::const_iterator itc=EntComps.begin(); itc!=EntComps.end(); itc++ )
	{
		ent::ComponentInst* cinst = (*itc).second;

		const PoolString &fam = cinst->GetFamily();

		if(fam.empty())
			continue;

		ActiveComponentType::iterator itl = mActiveEntityComponents.find(fam);
		if(itl != mActiveEntityComponents.end())
		{
			orklist<ent::ComponentInst*>& thelist = (*itl).second;

			orklist<ent::ComponentInst*>::iterator itc2 = std::find( thelist.begin(), thelist.end(), cinst );

			// Might not be active if we didn't add it in the first place
			if(itc2 != thelist.end())
				thelist.erase(itc2);
		}
	}
}

bool SceneInst::IsEntityActive(Entity* pent) const
{
	auto listit = mActiveEntities.find( pent );
	return ( listit != mActiveEntities.end() );
}
///////////////////////////////////////////////////////////////////////////
void SceneInst::ServiceDeactivateQueue()
{
	AssertOnOpQ2( UpdateSerialOpQ() );
	// Copy queue so we can queue more inside Stop
	orkvector<ent::Entity*> deactivate_queue = mEntityDeactivateQueue;
	mEntityDeactivateQueue.clear();

	for(orkvector<ent::Entity*>::const_iterator it = deactivate_queue.begin(); it != deactivate_queue.end(); it++)
	{
		ent::Entity *pent = (*it);
		OrkAssert(pent);

		DeActivateEntity(pent);
	}
}
///////////////////////////////////////////////////////////////////////////
void SceneInst::ServiceActivateQueue()
{
	AssertOnOpQ2( UpdateSerialOpQ() );
	// Copy queue so we can queue more inside Start
	orkvector<EntityActivationQueueItem> activate_queue = mEntityActivateQueue;
	mEntityActivateQueue.clear();

	for(orkvector<EntityActivationQueueItem>::const_iterator it = activate_queue.begin(); it != activate_queue.end(); it++)
	{
		const EntityActivationQueueItem &item = (*it);
		ent::Entity *pent = item.mpEntity;
		const CMatrix4 &mtx = item.mMatrix;
		OrkAssert(pent);

		//printf( "Activating Entity (Q) : ent<%p>\n", pent );
		if(const Archetype *parch = pent->GetEntData().GetArchetype())
		{
			//printf( "Activating Entity (QQ) : ent<%p> arch<%p>\n", pent, parch );
			parch->StartEntity(this, mtx, pent);
		}
		//printf( "Activating Entity (U) : %p\n", pent );

		ActivateEntity(pent);
		//printf( "Activated Entity (Q) : %p\n", pent );
	}
}

///////////////////////////////////////////////////////////////////////////

Entity* SceneInst::SpawnDynamicEntity( const ent::EntData* spawn_rec )
{
	//printf( "SpawnDynamicEntity ed<%p>\n", spawn_rec );
	auto newent = new Entity(*spawn_rec,this);
	auto arch = spawn_rec->GetArchetype();
	arch->ComposeEntity(newent);
	arch->LinkEntity(this,newent);
	EntityActivationQueueItem qi( CMatrix4::Identity, newent );
	this->QueueActivateEntity(qi);
	mEntities[spawn_rec->GetName()]=newent;
	return newent;
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

static void CopyCameraData( const SceneInst::CameraLut& srclut, CameraLut& dstlut )
{
	dstlut.clear();
	int idx = 0;
	//printf( "Copying CameraData\n" );
	for( SceneInst::CameraLut::const_iterator itCAM=srclut.begin(); itCAM!=srclut.end(); itCAM++ )
	{
		const PoolString& CameraName = itCAM->first;
		const CCameraData* pcameradata = itCAM->second;
		const lev2::CCamera* pcam = pcameradata ? pcameradata->GetLev2Camera() : 0;
		//printf( "CopyCameraData Idx<%d> CamName<%s> pcamdata<%p> pcam<%p>\n", idx, CameraName.c_str(), pcameradata, pcam );
		if( pcameradata )
		{
			dstlut.AddSorted( CameraName, *pcameradata );
		}
		idx++;
	}

}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void SceneInst::QueueAllDrawablesToBuffer(ork::ent::DrawableBuffer& buffer) const
{
	//orkprintf( "beg si<%p> qad2b..\n", this );
	AssertOnOpQ2( UpdateSerialOpQ() );

	buffer.Reset();

	////////////////////////////////////////////////////////////////
	// copy camera data from sceneinst to dbuffer
	////////////////////////////////////////////////////////////////

	CopyCameraData( mCameraLut, buffer.mCameraDataLUT );

	////////////////////////////////////////////////////////////////

	float t0 = ork::CSystem::GetRef().GetLoResTime();

	for( const auto& it : mEntities )
	{
		const ork::ent::Entity* pent = it.second;

		const Entity::LayerMap& entlayers = pent->GetLayers();
		//const ork::TransformNode3D& node3d = pent->GetDagNode().GetTransformNode();

		//NOTE: No culling! May need "was visible last frame" hack to be fast

		DrawQueueXfData xfdata;
		xfdata.mWorldMatrix = pent->GetEffectiveMatrix();
		
		//node3d.GetMatrix(xfdata.mWorldMatrix);

		for( Entity::LayerMap::const_iterator itL=entlayers.begin(); itL!=entlayers.end(); itL++ )
		{
			const PoolString& layer_name = itL->first;
			const ent::Entity::DrawableVector* dv = itL->second;
			DrawableBufLayer* buflayer = buffer.MergeLayer(layer_name);
			if( dv && buflayer )
			{
				size_t inumdv = dv->size();
				for(size_t i = 0; i < inumdv; i++ )
				{	Drawable* pdrw = dv->operator[](i);
					if( pdrw && pdrw->IsEnabled() )
					{
						//printf( "queue drw<%p>\n", pdrw );
						pdrw->QueueToLayer(xfdata,*buflayer);
					}
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void SceneInst::RenderDrawableBuffer(lev2::Renderer *renderer, const ork::ent::DrawableBuffer& dbuffer, const PoolString& LayerName ) const
{
	const ork::lev2::RenderContextFrameData* pfdata = renderer->GetTarget()->GetRenderContextFrameData();
	ork::lev2::RenderContextFrameData framedata = *pfdata;
	lev2::GfxTarget* pTARG = renderer->GetTarget();
	/////////////////////////////////
	// push temporary mutable framedata
	/////////////////////////////////
	pTARG->SetRenderContextFrameData(&framedata);
	{	
		if( framedata.GetCameraData() )
		{
			bool DoAll = (0==strcmp(LayerName.c_str(),"All"));
		
			for( const auto& layer_item : dbuffer.mLayerLut )
			{
				const PoolString& TestLayerName = layer_item.first;
				const DrawableBufLayer* player = layer_item.second;

				bool Match = (LayerName==TestLayerName);
				
				if( DoAll || (Match && pfdata->HasLayer( TestLayerName ) ) )
				{
					for( int id=0; id<=player->miItemIndex; id++ )
					{
						const DrawableBufItem& item = player->mDrawBufItems[id];
						const ork::ent::Drawable* pdrw = item.GetDrawable();
						if(pdrw)
							pdrw->QueueToRenderer( item, renderer );
					}
				}
			}
		}
	}
	/////////////////////////////////
	// pop previous framedata
	/////////////////////////////////
	pTARG->SetRenderContextFrameData(pfdata);

	////////////////////////////////////////////////
	static int ictr = 0;
	float fcurtime = ork::CSystem::GetRef().GetLoResTime();
	static float lltime = fcurtime;
	float fdelta = fcurtime-lltime;
	if( fdelta > 1.0f )
	{
		float fps = float(ictr)/fdelta;
		//orkprintf( "QDPS<%f>\n", fps );
		ictr = 0;
		lltime=fcurtime;
	}
	ictr++;
	////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ent::Entity* SceneInst::FindEntity(PoolString entity) const
{
	orkmap<PoolString, Entity*>::const_iterator it = mEntities.find(entity);
	return (it == mEntities.end()) ? 0 : it->second;
}
///////////////////////////////////////////////////////////////////////////////
ent::Entity* SceneInst::FindEntityLoose(PoolString entity) const
{
	const int kmaxtries = 128;

	orkmap<PoolString, Entity*>::const_iterator it = mEntities.find(entity);
	if(it == mEntities.end())
	{
		ArrayString<512> basebuffer;
		ArrayString<512> buffer;
		MutableString basestr(basebuffer);
		MutableString name_attempt(buffer);
		name_attempt = entity;

		int counter = 0;

		int i = int(name_attempt.size()) - 1;
		for(; i >= 0; i--)
			if(!isdigit(name_attempt.c_str()[i]))
				break;
		basestr = name_attempt.substr(0, i + 1);

		name_attempt = basestr;
		name_attempt += CreateFormattedString("%d", ++counter).c_str();
		PoolString pooled_name = AddPooledString(name_attempt);
		while(it == mEntities.end() && (counter<kmaxtries) )
		{
			name_attempt = basestr;
			name_attempt += CreateFormattedString("%d", ++counter).c_str();
			pooled_name = AddPooledString(name_attempt);
			it = mEntities.find(entity);
		}
	}

	if(it != mEntities.end())
		return it->second;
	return NULL;
}
///////////////////////////////////////////////////////////////////////////////
const SceneInst::ComponentList& SceneInst::GetActiveComponents(ork::PoolString family) const
{
	ActiveComponentType::const_iterator found = mActiveEntityComponents.find(family);
	if(found != mActiveEntityComponents.end())
		return (*found).second;
	else
		return mEmptyList;
}
///////////////////////////////////////////////////////////////////////////
SceneInst::ComponentList& SceneInst::GetActiveComponents(ork::PoolString family)
{
	ActiveComponentType::iterator found = mActiveEntityComponents.find(family);
	if(found != mActiveEntityComponents.end())
		return (*found).second;
	else
	{
		return mEmptyList;
	}
}
///////////////////////////////////////////////////////////////////////////
void SceneInst::UpdateActiveComponents(ork::PoolString family)
{
	UpdateEntityComponents(GetActiveComponents(family));
}
///////////////////////////////////////////////////////////////////////////
void SceneInst::AddLayer( const PoolString& name, Layer*player )
{
	orkmap<PoolString,Layer*>::const_iterator it=mLayers.find(name);
	OrkAssert(it==mLayers.end());
	mLayers[name] = player;
	player->mLayerName = name;
}
Layer* SceneInst::GetLayer( const PoolString& name )
{
	Layer* rval = 0;
	orkmap<PoolString,Layer*>::const_iterator it=mLayers.find(name);
	if( it!=mLayers.end() )
		rval = it->second;
	return rval;
}
const Layer* SceneInst::GetLayer( const PoolString& name ) const
{
	const Layer* rval = 0;
	orkmap<PoolString,Layer*>::const_iterator it=mLayers.find(name);
	if( it!=mLayers.end() )
		rval = it->second;
	return rval;
}
///////////////////////////////////////////////////////////////////////////


struct MyTimer
{
	float	mfTimeStart;
	float	mfTimeEnd;
	float	mfTimeAcc;
	int		miCounter;
	std::string	mName;

	MyTimer( const char* name) 
		: mfTimeStart(0.0f)
		, mfTimeEnd(0.0f)
		, mfTimeAcc(0.0f)
		, miCounter(0)
		, mName(name)
	{
	}
	void Start()
	{
		mfTimeStart = ork::CSystem::GetRef().GetLoResTime();
	}
	void Stop()
	{
		mfTimeEnd = ork::CSystem::GetRef().GetLoResTime();
		mfTimeAcc += (mfTimeEnd-mfTimeStart);
		miCounter++;
		if( (miCounter%30) == 0 )
		{	float favgtime = (mfTimeAcc/30.0f);
			orkprintf( "PS<%s> msec<%f>\n", mName.c_str(), favgtime*1000.0f );
			mfTimeAcc = 0.0f;
		}
	}
};


void SceneInst::Update()
{
	AssertOnOpQ2( UpdateSerialOpQ() );
	//ork::msleep(1);
	ComputeDeltaTime();
	static int ictr = 0;

	if( mDeltaTimeAccum > 1.0f ) mDeltaTimeAccum=1.0f;

	switch( this->GetSceneInstMode() )
	{
		case ork::ent::ESCENEMODE_PAUSE:
		{
			ork::lev2::CInputManager::Poll();
			if( mApplication ) mApplication->PreUpdate();
			if( mApplication ) mApplication->PostUpdate();
			break;
		}
		case ork::ent::ESCENEMODE_RUN:
		{
			ork::PerfMarkerPush( "ork.sceneinst.update.begin" );

			///////////////////////////////
			// Update Components
			///////////////////////////////

			auto cmci = GetCMCI();
			float frame_rate = cmci ? cmci->GetCurrentFrameRate() : 0.0f;
			bool externally_fixed_rate = (frame_rate!=0.0f);


			//float fdelta = 1.0f/60.0f; //GetDeltaTime();
			float fdelta = GetDeltaTime();

			float step = 0.0f; // ideally should be (1.0f/vsync rate) / some integer

			if( externally_fixed_rate )
			{
				mDeltaTimeAccum = fdelta;
				step = fdelta; //(1.0f/120.0f); // ideally should be (1.0f/vsync rate) / some integer
			}
			else 
			{
				mDeltaTimeAccum += fdelta;
				step = 1.0f/60.0f; //(1.0f/120.0f); // ideally should be (1.0f/vsync rate) / some integer
			}

			// Nasa - We are doing our own accumulator because there are frame-rate independence bugs
			// in bullet when we are not using a fixed time step around the call to bullet's
			// stepSimulation. Go figure.
			// Nasa - I just verified again that we are still not framerate independent if we take out our
			// own accumulator. 1-30-09.
			
			mDeltaTimeAccum = fdelta;
			step = fdelta;
						
			while(mDeltaTimeAccum >= step)
			{
				mDeltaTimeAccum -= step;

				SetDeltaTime(step);

				ork::lev2::CInputManager::Poll();

				bool update = true;
				if( mApplication ) update = mApplication->PreUpdate();

				if(update)
				{

					UpdateEntityComponents(GetActiveComponents(sInputFamily));
					UpdateEntityComponents(GetActiveComponents(sControlFamily));
					//timer1.Start();
					UpdateEntityComponents(GetActiveComponents(sPhysicsFamily));
					//timer1.Stop();
					//timer2.Start();
					UpdateEntityComponents(GetActiveComponents(sFrustumFamily));
					//timer2.Stop();
					UpdateEntityComponents(GetActiveComponents(sLightFamily));
					UpdateEntityComponents(GetActiveComponents(sAnimateFamily));
					UpdateEntityComponents(GetActiveComponents(sParticleFamily));
					UpdateEntityComponents(GetActiveComponents(sAudioFamily));

					///////////////////////////////
					// update the spawn/despawn queues
					///////////////////////////////

					ServiceDeactivateQueue();
					ServiceActivateQueue();

					mEntityUpdateCount += mActiveEntities.size();

				}

				if( mApplication ) mApplication->PostUpdate();
			}

			SetDeltaTime( step );

			UpdateEntityComponents(GetActiveComponents(sCameraFamily));

			size_t inumsc = mSceneComponents.size();
			for( size_t isc=0; isc<inumsc; isc++ )
			{
				SceneComponentInst* pinst = mSceneComponents.GetItemAtIndex(isc).second;
				pinst->Update( this );
			}

			ork::PerfMarkerPush( "ork.sceneinst.update.end" );

			///////////////////////////////
			break;
		}
		default:
			break;
	}
	ictr++;

}
///////////////////////////////////////////////////////////////////////////////
void SceneInst::AddSceneComponent( SceneComponentInst* pcomp )
{
	OrkAssert( mSceneComponents.find( pcomp->GetClass() ) == mSceneComponents.end() );
	mSceneComponents.AddSorted( pcomp->GetClass(), pcomp );
}
void SceneInst::ClearSceneComponents()
{
	mSceneComponents.clear();
}
///////////////////////////////////////////////////////////////////////////////
}}
