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
#include <ork/math/quaternion.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/drawable.h>
#include <pkg/ent/ModelArchetype.h>
#include <pkg/ent/SimpleAnimatable.h>
#include <pkg/ent/ScriptComponent.h>
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
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::SimpleCharacterArchetype, "SimpleCharacterArchetype" );
///////////////////////////////////////////////////////////////////////////////
using namespace ork::reflect;
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

class SimpleCharControllerData : public ComponentData
{
	RttiDeclareConcrete(SimpleCharControllerData, ComponentData)

	ComponentInst *CreateComponent(Entity *pent) const override;

public:

    float mWalkSpeed = 30.0f;
    float mRunSpeed = 50.0f;
    float mSpeedLerpRate = 1.0f;
};

void SimpleCharControllerData::Describe()
{
	ent::RegisterFamily<SimpleCharControllerData>(ork::AddPooledLiteral("control"));	
    RegisterFloatMinMaxProp(& SimpleCharControllerData::mWalkSpeed, "WalkSpeed", "0", "250" );
    RegisterFloatMinMaxProp(& SimpleCharControllerData::mRunSpeed, "RunSpeed", "0", "500" );
    RegisterFloatMinMaxProp(& SimpleCharControllerData::mSpeedLerpRate, "SpeedLerpRate", "0.1", "100" );
}
class SimpleCharControllerInst : public ComponentInst
{
	//RttiDeclareAbstract(SimpleCharControllerInst, ComponentInst)

public:

    ///////////////////////////////////////////////////////////////////////////

	SimpleCharControllerInst(const SimpleCharControllerData& data, Entity* pent)
		: ComponentInst(&data,pent)
        , mCCDATA(data)
        , kIdle(AddPooledLiteral("idle"))
        , kWalk(AddPooledLiteral("walk"))
        , kRun(AddPooledLiteral("run"))
	{
        mTimer = 1.0f;
        mState = kIdle;
        mCurrentSpeed = 0.0f;
	}
	~SimpleCharControllerInst()
	{

	}

    ///////////////////////////////////////////////////////////////////////////

    void DoUpdate(SceneInst* psi) final
    {
        float dt = psi->GetDeltaTime();
        if( mTimer <= 0.0f )
        {
            mTimer = psi->random(2,5);
            float dir = psi->random(-PI,PI);
            mDesiredDirection.FromAxisAngle(fvec4(0,1,0,dir));
        }

        mCurrentDirection = fquat::Lerp(mCurrentDirection,mDesiredDirection,dt*3);
        auto m3 = mCurrentDirection.ToMatrix3();
        auto vv = m3.GetZNormal();

        float splerp = dt*mCCDATA.mSpeedLerpRate;
        mCurrentSpeed = (mDesiredSpeed*splerp)+(mCurrentSpeed*(1.0f-splerp));

        mVelocity = fvec2(vv.x,vv.z)*mCurrentSpeed;

        mPosition += mVelocity*dt;
        auto pos = fvec3(mPosition.x,0.0f,mPosition.y);
        fmtx4 mtx;
        mtx.ComposeMatrix( pos, mCurrentDirection, 1.0f );

        mEntity->SetDynMatrix(mtx);

        mTimer -= dt;


    }

    ///////////////////////////////////////////////////////////////////////////

	bool DoNotify(const ork::event::Event *event) final
	{
		if(const event::AnimFinishEvent* afe = ork::rtti::autocast(event))
		{
            if( mState==kIdle)
            {
                mAnima->PlayAnimation(kIdle);
                mState = kWalk;
                mDesiredSpeed = 0.0;

            }
            else if( mState==kWalk)
            {
                mAnima->PlayAnimation(kWalk);
                mDesiredSpeed = mCCDATA.mWalkSpeed;
                mState = kRun;

            }
            else if( mState==kRun)
            {
                mAnima->PlayAnimation(kRun);
                mDesiredSpeed = mCCDATA.mRunSpeed;
                mState = kIdle;

            }
            else
            {
                mState = kIdle;
            }
		}

		return true;
	}

    ///////////////////////////////////////////////////////////////////////////

	bool DoLink(ork::ent::SceneInst *psi) final
	{
		mAnima = GetEntity()->GetTypedComponent<ork::ent::SimpleAnimatableInst>();
		if( nullptr == mAnima) return false;
		const auto& sad = mAnima->GetData();
		const auto& amap = sad.GetAnimationMap();
		for( const auto& item : amap )
		{
			mAnimSet.insert(item.first);
		}
        if(mAnimSet.find(kIdle)==mAnimSet.end())
            return false;
        if(mAnimSet.find(kWalk)==mAnimSet.end())
            return false;
        if(mAnimSet.find(kRun)==mAnimSet.end())
            return false;

		return true;
	}

    ///////////////////////////////////////////////////////////////////////////

    const SimpleCharControllerData& mCCDATA;
	SimpleAnimatableInst* mAnima;
	std::set<PoolString> mAnimSet;
    PoolString mCurState;
    const PoolString kIdle;
    const PoolString kWalk;
    const PoolString kRun;
    fvec2 mVelocity;
    PoolString mState;
    fvec2 mPosition;
    float mCurrentSpeed;
    float mDesiredSpeed;
    fquat mCurrentDirection;
    fquat mDesiredDirection;
    float mTimer;
};


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
	composer.Register<ork::ent::ScriptComponentData>();
	composer.Register<SimpleCharControllerData>();
	//pedpropmapdata->SetProperty( "visual.lighting.reciever.scope", "static" );
}

}} // namespace ork { namespace ent {

INSTANTIATE_TRANSPARENT_RTTI( ork::ent::SimpleCharControllerData, "SimpleCharControllerData" );
