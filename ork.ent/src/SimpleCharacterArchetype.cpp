///////////////////////////////////////////////////////////////
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
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/drawable.h>
#include <pkg/ent/ModelArchetype.h>
#include <pkg/ent/SimpleAnimatable.h>
#include <pkg/ent/SimpleCharacterArchetype.h>
#include <pkg/ent/ModelComponent.h>
#include <pkg/ent/event/MeshEvent.h>
#include <pkg/ent/event/AnimFinishEvent.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/AccessorObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/gfx/camera.h>
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::SimpleCharacterArchetype, "SimpleCharacterArchetype" );
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

class SimpleCharControllerData : public ComponentData
{
	RttiDeclareConcrete(SimpleCharControllerData, ComponentData)

	ComponentInst *CreateComponent(Entity *pent) const override;

public:

	SimpleCharControllerData() {}
};

void SimpleCharControllerData::Describe()
{
	ork::ent::RegisterFamily<SimpleCharControllerData>(ork::AddPooledLiteral("control"));	
}
class SimpleCharControllerInst : public ComponentInst
{
	RttiDeclareAbstract(SimpleCharControllerInst, ComponentInst)

public:

	SimpleCharControllerInst(const SimpleCharControllerData& data, Entity* pent)
		: ComponentInst(&data,pent)
	{

	}
	~SimpleCharControllerInst()
	{

	}
	bool DoNotify(const ork::event::Event *event) override
	{
		if(const event::AnimFinishEvent* afe = ork::rtti::autocast(event))
		{
			int numa = int(mAnimVect.size());
			int ia = rand()%numa;
			const PoolString& next = mAnimVect[ia];
			mAnima->PlayAnimation(next);
		}

		return true;
	}
	bool DoLink(ork::ent::SceneInst *psi) override
	{
		mAnima = GetEntity()->GetTypedComponent<ork::ent::SimpleAnimatableInst>();
		if( nullptr == mAnima) return false;
		const auto& sad = mAnima->GetData();
		const auto& amap = sad.GetAnimationMap();
		for( const auto& item : amap )
		{
			mAnimVect.push_back(item.first);
		}

	}

	SimpleAnimatableInst* mAnima;
	std::vector<PoolString> mAnimVect;
};
void SimpleCharControllerInst::Describe() {}


void SimpleCharacterArchetype::Describe()
{
}

ComponentInst* SimpleCharControllerData::CreateComponent(Entity *pent) const
{
	return new SimpleCharControllerInst(*this,pent);
}

//////////////////////////////////////////////////////////

SimpleCharacterArchetype::SimpleCharacterArchetype()
{
}

void SimpleCharacterArchetype::DoCompose(ork::ent::ArchComposer& composer) 
{
	composer.Register<EditorPropMapData>();
	composer.Register<ork::ent::ModelComponentData>();
	composer.Register<ork::ent::SimpleAnimatableData>();
	composer.Register<SimpleCharControllerData>();
	//pedpropmapdata->SetProperty( "visual.lighting.reciever.scope", "static" );
}

}} // namespace ork { namespace ent {

INSTANTIATE_TRANSPARENT_RTTI( ork::ent::SimpleCharControllerData, "SimpleCharControllerData" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::SimpleCharControllerInst, "SimpleCharControllerInst" );
