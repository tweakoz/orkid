////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/application/application.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/scene.h>
#include <pkg/ent/drawable.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/renderer.h>
#include <ork/gfx/camera.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <pkg/ent/ParticleControllable.h>
#include <ork/reflect/enum_serializer.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/kernel/orklut.hpp>
#include <pkg/ent/dataflow.h>
#include <pkg/ent/PerfController.h>

template class ork::orklut<ork::PoolString,ork::psys::NovaParticleItemBase*>;
static ork::PoolString sOnStartString;

///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::psys::NovaParticleSystem, "NovaParticleSystem");
INSTANTIATE_TRANSPARENT_RTTI(ork::psys::ParticleControllableData, "ParticleControllableData");
INSTANTIATE_TRANSPARENT_RTTI(ork::psys::ParticleControllableInst, "ParticleControllableInst");
INSTANTIATE_TRANSPARENT_RTTI(ork::psys::NovaParticleItemBase, "NovaParticleItemBase");
INSTANTIATE_TRANSPARENT_RTTI(ork::psys::ParticleArchetype, "ParticleArchetype");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace psys {
///////////////////////////////////////////////////////////////////////////////

void NovaParticleSystem::Describe() {}
void NovaParticleItemBase::Describe() {}

///////////////////////////////////////////////////////////////////////////////

void ParticleControllableData::Describe()
{
	ork::reflect::RegisterMapProperty( "Systems", & ParticleControllableData::mItems );
	ork::reflect::AnnotatePropertyForEditor< ParticleControllableData >("Systems", "editor.factorylistbase", "Lev2ParticleItemBase" );
	ork::reflect::AnnotatePropertyForEditor< ParticleControllableData >("Systems", "editor.map.policy.impexp", "true" );

	ork::reflect::RegisterProperty("DefaultEnable", &ParticleControllableData::mDefaultEnable);
	ork::reflect::RegisterProperty("EntAttachment", &ParticleControllableData::mEntAttachment);
}

///////////////////////////////////////////////////////////////////////////////
ParticleControllableData::ParticleControllableData() : mDefaultEnable(true)
{
}
ParticleControllableData::~ParticleControllableData()
{
	for( ork::orklut<ork::PoolString, ParticleItemBase*>::const_iterator it=mItems.begin(); it!=mItems.end(); it++ )
	{
		ParticleItemBase* pitem = it->second;
		delete pitem;
	}
}
///////////////////////////////////////////////////////////////////////////////
ork::ent::ComponentInst *ParticleControllableData::createComponent(ork::ent::Entity *pent) const
{
	return OrkNew ParticleControllableInst(*this, pent);
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ParticleControllableInst::Describe()
{
	ork::reflect::RegisterFunctor("IsEnabled", &ParticleControllableInst::IsEnabled);
	ork::reflect::RegisterFunctor("SetEnable", &ParticleControllableInst::SetEnable);
}
///////////////////////////////////////////////////////////////////////////////
ParticleControllableInst::ParticleControllableInst(const ParticleControllableData &data, ork::ent::Entity *pent)
	: ork::ent::ComponentInst(&data, pent)
	, mData(data)
	, mbEnable( data.IsDefaultEnable() )
	, mpAttachEntity(0)
{

	const ork::orklut<ork::PoolString,ParticleItemBase*>& items = data.GetItems();
	for( ork::orklut<ork::PoolString,ParticleItemBase*>::const_iterator it=items.begin(); it!=items.end(); it++ )
	{
		const NovaParticleItemBase* pitembase = rtti::autocast(it->second);

		ork::PoolString name = it->first;
		if( pitembase )
		{
			NovaParticleSystem* psys = pitembase->CreateSystem( pent );
			psys->SetName( name );

			if( psys )
			{
				_systems.push_back( psys );
			}

		}
	}
	const ork::ent::DataflowRecieverComponentData* dflowreciever = pent->GetEntData().GetTypedComponent<ork::ent::DataflowRecieverComponentData>();
	if( dflowreciever )
	{
		//ork::dataflow::dyn_dgmodule& dgmod = dflowreciever->RefDgModule();
		//if( mGraphInstance )
		//{
		//}
	}
}
ParticleControllableInst::~ParticleControllableInst()
{
	int inumsys = int(_systems.size());
	for( int i=0; i<inumsys; i++ )
	{
		NovaParticleSystem* psys = _systems[i];
		delete psys;
	}
}
///////////////////////////////////////////////////////////////////////////////
bool ParticleControllableInst::DoStart(ork::ent::SceneInst *inst, const ork::fmtx4 &world)
{
	ork::fvec3 pos = world.GetTranslation();

	mPrevTime = 0.0f;

	//orkprintf( "start psys<%p> at pos<%f %f %f>\n", this, pos.GetX(), pos.GetY(), pos.GetZ() );
	Reset();
	for( orkvector<NovaParticleSystem*>::const_iterator it=_systems.begin(); it!=_systems.end(); it++ )
	{
		NovaParticleSystem* psys = (*it);

		if(psys)
		{
			psys->StartSystem( inst, GetEntity() );
		}
	}
	return true;
}
///////////////////////////////////////////////////////////////////////////////
bool ParticleControllableInst::DoLink( ork::ent::SceneInst *psi )
{
	////////////////////////////////
	// first check archetype for attachment
	////////////////////////////////

	mpAttachEntity = psi->FindEntity(mData.GetEntAttachment());

	////////////////////////////////
	// now check entdata EPMI for attachment
	////////////////////////////////

	const ent::EntData& ED = GetEntity()->GetEntData();
	ConstString att = ED.GetUserProperty("ParentAttachment");
	PoolString attps(AddPooledString(att.c_str()));
	ent::Entity* pna = psi->FindEntity(attps);
	if( pna )
		mpAttachEntity = pna;

	mParticleContext.mfCurrentTime = 0.0f;

	////////////////////////////////

	for( orkvector<NovaParticleSystem*>::const_iterator it=_systems.begin(); it!=_systems.end(); it++ )
	{
		NovaParticleSystem* psys = (*it);

		if(psys)
		{
			psys->LinkSystem( psi, GetEntity() );
		}
	}
	return true;
}
///////////////////////////////////////////////////////////////////////////////
void ParticleControllableInst::DoUpdate(ork::ent::SceneInst *inst)
{
	if( mpAttachEntity )
	{
		ent::DagNode& dnodeSRC = mpAttachEntity->GetDagNode();
		TransformNode& t3dSRC = dnodeSRC.GetTransformNode();
		ent::DagNode& dnodeDST = GetEntity()->GetDagNode();
		TransformNode& t3dDST = dnodeDST.GetTransformNode();
		t3dDST = t3dSRC;
	}

	const ParticleControllableData& pcd = GetData();
	///////////////////////////////////

	float fcurtime = inst->GetGameTime();
	float deltime = fcurtime-mPrevTime;
	mPrevTime = fcurtime;

	if( mbEnable )
	{

		mParticleContext.BeginFrame(fcurtime);

		for( orkvector<NovaParticleSystem*>::const_iterator it=_systems.begin(); it!=_systems.end(); it++ )
		{
			NovaParticleSystem* psys = (*it);

			psys->Update(deltime);
		}
	}
}
///////////////////////////////////////////////////////////////////////////////
bool ParticleControllableInst::DoNotify(const ork::event::Event *event)
{
	if( const ork::ent::PerfControlEvent* pce = rtti::autocast(event) )
	{
		printf( "ParticleControllableInst<%p> PerfControlEvent<%p> key<%s>\n", this, pce, pce->mTarget.c_str() );
		PoolString k = AddPooledString(pce->mTarget.c_str());

		ork::ent::PerfControlEvent pce2 = *pce;
		for( orkvector<NovaParticleSystem*>::const_iterator it=_systems.begin(); it!=_systems.end(); it++ )
		{
			NovaParticleSystem* psys = (*it);
			psys->Notify( pce );
		}
		return true;

	}
	if( const ork::ent::PerfSnapShotEvent* psse = rtti::autocast(event) )
	{
		for( orkvector<NovaParticleSystem*>::const_iterator it=_systems.begin(); it!=_systems.end(); it++ )
		{
			NovaParticleSystem* psys = (*it);
			psys->Notify( psse );
		}
		return true;
	}

/*	if(const prodigy::ent::event::StartParticlesEvent *start = ork::rtti::autocast(event))
	{
		mbEnable = true;
		return true;
	}
	if(const prodigy::ent::event::StopParticlesEvent *play = ork::rtti::autocast(event))
	{
		mbEnable = false;
		return true;
	}
*/
	return false;
}
///////////////////////////////////////////////////////////////////////////////
void ParticleControllableInst::Reset()
{
	for( orkvector<NovaParticleSystem*>::const_iterator it=_systems.begin(); it!=_systems.end(); it++ )
	{
		NovaParticleSystem* system = (*it);
		system->Reset();
	}

}
///////////////////////////////////////////////////////////////////////////////
void NovaParticleSystem::StartSystem( const ork::ent::SceneInst* psi, ork::ent::Entity*pent )
{
	DoStartSystem( psi, pent );
}


void ParticleArchetype::Describe()
{
	sOnStartString = ork::AddPooledLiteral("on_start");
}

///////////////////////////////////////////////////////////////////////////////

ParticleArchetype::ParticleArchetype()
{
}

///////////////////////////////////////////////////////////////////////////////

void ParticleArchetype::DoCompose(ork::ent::ArchComposer& composer)
{
	composer.Register<ork::psys::ParticleControllableData>();
	composer.Register<ork::ent::DataflowRecieverComponentData>();
	//composer.Register<ork::ent::EditorPropMapData>();

//	composer.Register<prodigy::ent::AudioEffectComponentData>();
}

///////////////////////////////////////////////////////////////////////////////

void ParticleArchetype::DoLinkEntity(ork::ent::SceneInst* inst, ork::ent::Entity *pent) const
{
}

void ParticleArchetype::DoStartEntity(ork::ent::SceneInst*, const ork::fmtx4& mtx, ork::ent::Entity* pent ) const
{
	pent->GetDagNode().GetTransformNode().GetTransform().SetMatrix(mtx);

	ParticleControllableInst* pci = pent->GetTypedComponent<ParticleControllableInst>();
	if(pci)
		pci->Reset();
//	prodigy::ent::AudioEffectComponentInst* aeci = pent->GetTypedComponent<prodigy::ent::AudioEffectComponentInst>();
//	if( aeci )
//	{
//		const ork::orklut<ork::PoolString,prodigy::ent::AudioEffectPlayDataBase*>& smap = aeci->GetData().GetSoundMap();
//		ork::orklut<ork::PoolString,prodigy::ent::AudioEffectPlayDataBase*>::const_iterator it = smap.find(sOnStartString );
//		if( it != smap.end() )
//		{
//			orkprintf( "on_start sound found\n" );
//		}
//		aeci->PlaySound( sOnStartString );
//	}
}


///////////////////////////////////////////////////////////////////////////////
} }
///////////////////////////////////////////////////////////////////////////////

template const ork::psys::ParticleControllableData* ork::ent::EntData::GetTypedComponent<ork::psys::ParticleControllableData>() const;
template ork::psys::ParticleControllableInst* ork::ent::Entity::GetTypedComponent<ork::psys::ParticleControllableInst>(bool);
