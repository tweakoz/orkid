///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "SimpleCharacterArchetype.h"
#include <ork/kernel/msgrouter.inl>
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::SimpleCharacterArchetype, "SimpleCharacterArchetype" );
///////////////////////////////////////////////////////////////////////////////
using namespace ork::reflect;
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
struct MyStrings{
    const PoolString kIdle = AddPooledLiteral("idle");
    const PoolString kWalk = AddPooledLiteral("walk");
    const PoolString kRun = AddPooledLiteral("run");
    const PoolString kState = AddPooledLiteral("state");
    const PoolString kSetDir =AddPooledLiteral("setDir");
    const PoolString kSetPos =AddPooledLiteral("setPos");
};
static MyStrings& STR(){
    static MyStrings ms;
    return ms;
}
///////////////////////////////////////////////////////////////////////////////
void SimpleCharControllerData::Describe()
{
    RegisterFloatMinMaxProp(& SimpleCharControllerData::mWalkSpeed, "WalkSpeed", "0", "250" );
    RegisterFloatMinMaxProp(& SimpleCharControllerData::mRunSpeed, "RunSpeed", "0", "500" );
    RegisterFloatMinMaxProp(& SimpleCharControllerData::mSpeedLerpRate, "SpeedLerpRate", "0.1", "100" );
}

class SimpleCharControllerInst : public ComponentInst
{
	RttiDeclareAbstract(SimpleCharControllerInst, ComponentInst)

public:

    ///////////////////////////////////////////////////////////////////////////

	SimpleCharControllerInst(const SimpleCharControllerData& data, Entity* pent)
		: ComponentInst(&data,pent)
        , mCCDATA(data)
	{
        //mTimer = 1.0f;
        mState = STR().kIdle;
        mCurrentSpeed = 0.0f;

        _subscriber = msgrouter::channel("eggytest")->subscribe([this](msgrouter::content_t c) {
          this->_spq.try_push(c.Get<fmtx4>());
        });


	}
	~SimpleCharControllerInst()
	{

	}

    ///////////////////////////////////////////////////////////////////////////

    void DoUpdate(SceneInst* psi) final
    {
        float dt = psi->GetDeltaTime();

        mCurrentDirection = fquat::Lerp(mCurrentDirection,mDesiredDirection,dt*3);
        auto m3 = mCurrentDirection.ToMatrix3();
        auto vv = m3.GetZNormal();

        float splerp = dt*mCCDATA.mSpeedLerpRate;
        mCurrentSpeed = (mDesiredSpeed*splerp)+(mCurrentSpeed*(1.0f-splerp));

        //mVelocity = 0.0f; //fvec2(vv.x,vv.z)*mCurrentSpeed;

        auto vel3d = fvec3(mVelocity.x,0.0f,mVelocity.y);

        mPosition += vel3d*dt;
        fmtx4 mtx;
        mtx.ComposeMatrix( mPosition, mCurrentDirection, 1.0f );

        mEntity->SetDynMatrix(mtx);


        fmtx4 popped;
        if( _spq.try_pop(popped) and _spawntimer.SecsSinceStart()>3.0f ){
          _spawntimer.Start();
          static int eid = 0;
          auto archnamestr = std::string("/arch/ball");
          auto entnamestr = FormatString("/ent/yo/%d", eid++ );
          const auto& scenedata = psi->GetData();
          auto archso = scenedata.FindSceneObjectByName(AddPooledString(archnamestr.c_str()));
          auto position = popped.GetTranslation();
          printf( "spawning at <%g %g %g>\n",position.x,position.y,position.z );
          if( const Archetype* as_arch = rtti::autocast(archso) ){
                EntData* spawner = new EntData;
              spawner->SetName(AddPooledString(entnamestr.c_str()));
              spawner->SetArchetype(as_arch);
              spawner->GetDagNode().SetTransformMatrix(popped);
              auto newent = psi->SpawnDynamicEntity(spawner);
            }
          }

    }

    msgrouter::subscriber_t _subscriber;
    ork::MpMcBoundedQueue<fmtx4,2> _spq;
    Timer _spawntimer;
    ///////////////////////////////////////////////////////////////////////////

	bool DoNotify(const ork::event::Event* event) final
	{
        if(const ork::event::VEvent* vev = ork::rtti::autocast(event))
        {   const auto& LR = vev->mData.Get<LuaRef>();
            if(vev->mCode==STR().kState)
            {   auto state = LR.get<std::string>("id");
                mState = AddPooledString(state.c_str());
                //printf( "charcon got state change request id<%s>\n", state.c_str() );
            }
            if(vev->mCode==STR().kSetDir)
            {   float dir = LR.toValue<float>();
                mDesiredDirection.FromAxisAngle(fvec4(0,1,0,dir));
                //printf( "charcon got direction change request dir<%f>\n", dir );
            }
            if(vev->mCode==STR().kSetPos)
            {   auto pos = LR.toValue<fvec3>();
                mPosition = pos;
                //printf( "charcon got position change request pos<%f %f %f>\n", pos.x, pos.y, pos.z );
            }
        }
		else if(const event::AnimFinishEvent* afe = ork::rtti::autocast(event))
		{
            if( mState==STR().kIdle)
            {
                mAnima->PlayAnimation(STR().kIdle);
                mDesiredSpeed = 0.0;
            }
            else if( mState==STR().kWalk)
            {
                mAnima->PlayAnimation(STR().kWalk);
                mDesiredSpeed = mCCDATA.mWalkSpeed;
            }
            else if( mState==STR().kRun)
            {
                mAnima->PlayAnimation(STR().kRun);
                mDesiredSpeed = mCCDATA.mRunSpeed;
            }
            else
            {
                mAnima->PlayAnimation(mState);
                mDesiredSpeed = 0.0;
            }
		}

		return true;
	}

    ///////////////////////////////////////////////////////////////////////////

	bool DoLink(ork::ent::SceneInst *psi) final
	{
    _spawntimer.Start();
		mAnima = GetEntity()->GetTypedComponent<ork::ent::SimpleAnimatableInst>();
		if( nullptr == mAnima) return false;
		const auto& sad = mAnima->GetData();
		const auto& amap = sad.GetAnimationMap();
		for( const auto& item : amap )
		{
			mAnimSet.insert(item.first);
		}
        if(mAnimSet.find(STR().kIdle)==mAnimSet.end())
            return false;
        if(mAnimSet.find(STR().kWalk)==mAnimSet.end())
            return false;
        if(mAnimSet.find(STR().kRun)==mAnimSet.end())
            return false;

		return true;
	}

    ///////////////////////////////////////////////////////////////////////////

    const SimpleCharControllerData& mCCDATA;
	SimpleAnimatableInst* mAnima;
	std::set<PoolString> mAnimSet;
    PoolString mCurState;
    fvec2 mVelocity;
    PoolString mState;
    fvec3 mPosition;
    float mCurrentSpeed;
    float mDesiredSpeed;
    fquat mCurrentDirection;
    fquat mDesiredDirection;
};

void SimpleCharControllerInst::Describe()
{
}


void SimpleCharacterArchetype::Describe()
{
}

ComponentInst* SimpleCharControllerData::createComponent(Entity *pent) const
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
  composer.Register<ork::ent::InputComponentData>();
	composer.Register<SimpleCharControllerData>();
    composer.Register<BulletObjectControllerData>();
    composer.Register<AudioEffectComponentData>();
	//pedpropmapdata->SetProperty( "visual.lighting.reciever.scope", "static" );
}

}} // namespace ork { namespace ent {

INSTANTIATE_TRANSPARENT_RTTI( ork::ent::SimpleCharControllerData, "SimpleCharControllerData" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::SimpleCharControllerInst, "SimpleCharController" );
