////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/math/audiomath.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/particle/particle.h>
#include <ork/lev2/gfx/particle/particle.hpp>
#include <ork/reflect/properties/register.h>
#include <ork/reflect/properties/DirectTypedMap.h>
#include <ork/reflect/properties/ITyped.hpp>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/enum_serializer.inl>
#include <ork/reflect/properties/registerX.inl>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/fixedlut.hpp>

///////////////////////////////////////////////////////////////////////////////

using namespace ork::lev2::particle;

template class ork::reflect::DirectTypedMap<orkmap<float, ork::fvec4>>;
// template class ork::reflect::DirectTyped<ork::lev2::Blending>;
// template class ork::reflect::DirectTyped<ork::lev2::PrimitiveType>;
namespace ork { namespace lev2 { namespace particle {
template class Pool<BasicParticle>;
template class BasicEmitter<BasicParticle>;
template class SpiralEmitter<BasicParticle>;
template class FixedSystem<BasicParticle>;
}}} // namespace ork::lev2::particle
template class ork::fixedlut<ork::Char4, ork::lev2::particle::EventQueue*, 8>;
template class ork::fixedvector<ork::lev2::particle::Event, ork::lev2::particle::EventQueue::kmaxevents>;

// template class orklut<ork::Char4,ork::lev2::particle::EventQueue>;

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(SpiralEmitterData, "SpiralEmitterData");
ImplementReflectionX(ParticleSystemBase, "Lev2ParticleSystemBase");
ImplementReflectionX(ParticleItemBase, "Lev2ParticleItemBase");

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////

BeginEnumRegistration(ParticleItemAlignment);
RegisterEnum(ParticleItemAlignment, BILLBOARD);
RegisterEnum(ParticleItemAlignment, XZ);
RegisterEnum(ParticleItemAlignment, XY);
RegisterEnum(ParticleItemAlignment, YZ);
EndEnumRegistration();

BeginEnumRegistration(EmitterDirection);
RegisterEnum(EmitterDirection, CONSTANT);
RegisterEnum(EmitterDirection, VEL);
RegisterEnum(EmitterDirection, CROSS_X);
RegisterEnum(EmitterDirection, CROSS_Y);
RegisterEnum(EmitterDirection, CROSS_Z);
RegisterEnum(EmitterDirection, TO_POINT);
RegisterEnum(EmitterDirection, USER);
EndEnumRegistration();

EventQueue* Context::MergeQueue(Char4 qname) {
  lev2::particle::EventQueueLut::iterator it = mEventQueueLut.find(qname);
  if (it == mEventQueueLut.end()) {
    it = mEventQueueLut.AddSorted(qname, new lev2::particle::EventQueue());
  }
  return it->second;
}
EventQueue* Context::FindQueue(Char4 qname) {
  EventQueue* pret                           = 0;
  lev2::particle::EventQueueLut::iterator it = mEventQueueLut.find(qname);
  if (it != mEventQueueLut.end()) {
    pret = it->second;
  }
  return pret;
}

void Context::BeginFrame(float fcurtime) {
  mfCurrentTime = fcurtime;

  // printf( "pctx curtime<%f>\n", mfCurrentTime );
  for (lev2::particle::EventQueueLut::iterator it = mEventQueueLut.begin(); it != mEventQueueLut.end(); it++) {
    it->second->Clear();
  }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SpiralEmitterData::describeX(class_t* clazz) {
  /*reflect::RegisterProperty("Lifespan", &SpiralEmitterData::mfLifespan);
  reflect::RegisterProperty("EmitScale", &SpiralEmitterData::mfEmitScale);
  reflect::RegisterProperty("EmissionRate", &SpiralEmitterData::mfEmissionRate);
  reflect::RegisterProperty("EmissionUp", &SpiralEmitterData::mfEmissionUp);
  reflect::RegisterProperty("EmissionOut", &SpiralEmitterData::mfEmissionOut);
  reflect::RegisterProperty("SpinRate", &SpiralEmitterData::mfSpinRate);

  reflect::annotatePropertyForEditor<SpiralEmitterData>("Lifespan", "editor.range.min", "0.1");
  reflect::annotatePropertyForEditor<SpiralEmitterData>("Lifespan", "editor.range.max", "10.0");
  reflect::annotatePropertyForEditor<SpiralEmitterData>("Lifespan", "editor.range.log", "true");

  reflect::annotatePropertyForEditor<SpiralEmitterData>("EmitScale", "editor.range.min", "0.001");
  reflect::annotatePropertyForEditor<SpiralEmitterData>("EmitScale", "editor.range.max", "100.0");
  reflect::annotatePropertyForEditor<SpiralEmitterData>("EmitScale", "editor.range.log", "true");

  reflect::annotatePropertyForEditor<SpiralEmitterData>("EmissionRate", "editor.range.min", "0.01");
  reflect::annotatePropertyForEditor<SpiralEmitterData>("EmissionRate", "editor.range.max", "100.0");
  reflect::annotatePropertyForEditor<SpiralEmitterData>("EmissionRate", "editor.range.log", "true");

  reflect::annotatePropertyForEditor<SpiralEmitterData>("EmissionUp", "editor.range.min", "-100");
  reflect::annotatePropertyForEditor<SpiralEmitterData>("EmissionUp", "editor.range.max", "100");
  reflect::annotatePropertyForEditor<SpiralEmitterData>("EmissionOut", "editor.range.min", "-100");
  reflect::annotatePropertyForEditor<SpiralEmitterData>("EmissionOut", "editor.range.max", "100");

  reflect::annotatePropertyForEditor<SpiralEmitterData>("SpinRate", "editor.range.min", "0.001");
  reflect::annotatePropertyForEditor<SpiralEmitterData>("SpinRate", "editor.range.max", "100.0");
  reflect::annotatePropertyForEditor<SpiralEmitterData>("SpinRate", "editor.range.log", "true");
*/}

  ///////////////////////////////////////////////////////////////////////////////

  SpiralEmitterData::SpiralEmitterData() {
  }

  ///////////////////////////////////////////////////////////////////////////////

  EmitterCtx::EmitterCtx() {
  }

  ///////////////////////////////////////////////////////////////////////////////

  RandGen::RandGen()
    : _randgen(10)
    , _distribution(0,2000000000){

    }
  float RandGen::ranged_rand(float fmin, float fmax){
    constexpr double inv = 1.0 / double(2000000000);
    double unit = double(_distribution(_randgen)) * inv;
    return ork::audiomath::lerp(fmin, fmax, unit);
  }

  ///////////////////////////////////////////////////////////////////////////////

  void DirectedEmitter::EmitCB(EmitterCtx& ctx) {
    if (0 == ctx.mPool)
      return;
    Pool<BasicParticle>& the_pool = *ctx.mPool;
    float fdeltap                 = (ctx.mfEmissionRate * ctx.mfDeltaTime);
    ctx.mfEmitterMark += fdeltap;
    if (std::isnan(ctx.mfEmitterMark)) {
      ctx.mfEmitterMark = 0.0f;
    }
    //printf("emitrate<%f> deltat<%f> deltap<%f> mark<%f>\n", ctx.mfEmissionRate, ctx.mfDeltaTime, fdeltap, ctx.mfEmitterMark);
    int icount = int(ctx.mfEmitterMark);
    fvec3 dir, pos, disp, vbin, vtan, yo;
    for (int ic = 0; ic < icount; ic++) {
      float fi = float(ic) / float(icount);
      computePosDir(fi, pos, dir);
      pos += ctx.mPosition;
      BasicParticle* __restrict ptc = the_pool.FastAlloc();
      if (ptc) {
        ptc->mfAge      = 0.0f;
        ptc->mfLifeSpan = ctx.mfLifespan;
        switch (meDirection) {
          case EmitterDirection::CROSS_X:
            dir = dir.crossWith(fvec3::Red());
            break;
          case EmitterDirection::CROSS_Y:
            dir = dir.crossWith(fvec3::Green());
            break;
          case EmitterDirection::CROSS_Z:
            dir = dir.crossWith(fvec3::Blue());
            break;
          case EmitterDirection::CONSTANT:
          case EmitterDirection::VEL:
          case EmitterDirection::TO_POINT:
          default:
            break;
        }
        vbin = dir.crossWith(fvec3::Green());
        if (vbin.magnitudeSquared() < .1f)
          vbin = dir.crossWith(fvec3::Red());
        vbin.normalizeInPlace();
        vtan     = vbin.crossWith(dir);
        float fu = _randgen.ranged_rand(-0.5,0.5);
        float fv = _randgen.ranged_rand(-0.5,0.5);
        disp     = (vbin * fu) + (vtan * fv);
        yo.lerp(dir, disp, ctx.mDispersion);
        dir                = yo.normalized();
        ptc->mPosition     = pos;
        ptc->mVelocity     = dir * ctx.mfEmissionVelocity + ctx.mOffsetVelocity;
        ptc->mLastPosition = pos - (ptc->mVelocity * ctx.mfDeltaTime);
        ptc->mKey          = (void*)ctx.mKey;
      }
    }
    ctx.mfEmitterMark -= float(icount);
  }
  void DirectedEmitter::EmitSQ(EmitterCtx& ctx) {
    if (0 == ctx.mPool)
      return;

    Pool<BasicParticle>& the_pool = *ctx.mPool;
    float fdeltap                 = (ctx.mfEmissionRate * ctx.mfDeltaTime);
    ctx.mfEmitterMark += fdeltap;
    int icount = ctx.mSpawnQueue->GetNumEvents();
    fvec3 odir, dir, pos, disp, vbin, vtan, yo;

    // printf( "DirectedEmitter<%p>::EmitSQ() count<%d>\n", this, icount );
    float EmitterSpeed    = ctx.mfEmissionVelocity;
    bool bUseEmitterSpeed = (EmitterSpeed != 0.0f);

    for (int index = 0; index < icount; index++) {
      OrkAssert(false);
      //////////////////////////////////////////////////
      Event ev = ctx.mSpawnQueue->DequeueEvent();
      //////////////////////////////////////////////////
      // prababalistic spawning
      float fran   = _randgen.ranged_rand(0,1);
      //////////////////////////////////////////////////
      if (fran < ctx.mfSpawnProbability) {
        //////////////////////////////////////////////
        pos  = ev.mPosition + ctx.mPosition;
        odir = (pos - ev.mLastPosition).normalized();
        dir  = odir;
        if (false == bUseEmitterSpeed)
          EmitterSpeed = ev.mVelocity.magnitude();
        //////////////////////////////////////////////
        for (float ii = 0.0f; ii < ctx.mfSpawnMultiplier; ii += 1.0f) {
          BasicParticle* __restrict ptc = the_pool.FastAlloc();
          if (ptc) {
            ptc->mfAge      = 0.0f;
            ptc->mfLifeSpan = ctx.mfLifespan;
            //////////////////////////////////////////////
            // calc dispersion
            //////////////////////////////////////////////
            vbin = dir.crossWith(fvec3::Green());
            if (vbin.magnitudeSquared() < .1f)
              vbin = dir.crossWith(fvec3::Red());
            vbin.normalizeInPlace();
            vtan     = vbin.crossWith(dir);
            float fu = ((std::rand() % 256) / 256.0f) - 0.5f;
            float fv = ((std::rand() % 256) / 256.0f) - 0.5f;
            disp     = (vbin * fu) + (vtan * fv);
            yo.lerp(dir, disp, ctx.mDispersion);
            dir = yo.normalized();
            //////////////////////////////////////////////
            ptc->mPosition = pos;
            ptc->mVelocity = dir * EmitterSpeed;
            // ctx.mfEmissionVelocity + ctx.mOffsetVelocity;
            ptc->mLastPosition = pos - (ptc->mVelocity * ctx.mfDeltaTime);
            ptc->mKey          = (void*)ctx.mKey;
          }
        }
      }
    }
    ctx.mfEmitterMark -= float(icount);
  }

  ///////////////////////////////////////////////////////////////////////////////

  void DirectedEmitter::Emit(EmitterCtx& ctx) {
    if (ctx.mSpawnQueue)
      EmitSQ(ctx);
    else
      EmitCB(ctx);
  }

  ///////////////////////////////////////////////////////////////////////////////

  void DirectedEmitter::Reap(EmitterCtx& ctx) {
    if (ctx.mPool) { //////////////////////////
      // kill particles
      //////////////////////////

      Event DeathEv;
      DeathEv.mEventType = Char4("KILL");

      int inumalive = ctx.mPool->GetNumAlive();
      // static const int kkillbufsize = 32<<10;
      // static BasicParticle* gpKillBuf = new int[kkillbufsize];

      // printf( "REAP numalive<%d>\n", inumalive );
      int inumkilled = 0;
      for (int i = 0; i < inumalive; i++) {
        BasicParticle* ptc = ctx.mPool->mActiveParticles[i];
        bool bkeymatch     = (ptc->mKey == ctx.mKey);
        if (ptc->IsDead() && bkeymatch) // kill particle
        {
          if (ctx.mDeathQueue) {
            DeathEv.mPosition     = ptc->mPosition;
            DeathEv.mLastPosition = ptc->mLastPosition;
            DeathEv.mVelocity     = ptc->mVelocity;
            ctx.mDeathQueue->QueueEvent(DeathEv);
            /*printf( "sending particle<%p> kill event DQ<%p> pos<%f %f %f>\n",
                    ptc,
                    ctx.mDeathQueue,
                    DeathEv.mPosition.x,
                    DeathEv.mPosition.y,
                    DeathEv.mPosition.z
                    );*/
          }
          ctx.mPool->mInactiveParticles.push_back(ptc);
          int ilast_alive                = ctx.mPool->GetNumAlive() - 1;
          BasicParticle* ptclast         = ctx.mPool->mActiveParticles[ilast_alive];
          ctx.mPool->mActiveParticles[i] = ptclast;
          ctx.mPool->mActiveParticles.erase(ctx.mPool->mActiveParticles.begin() + ilast_alive);
          inumalive--;
          // ctx.mPool->FastFree(ikill);
        }
      }
    } else if (0) // ctx.mPool )
    {
      for (int i = 0; i < ctx.mPool->GetNumAlive(); i++) {
        BasicParticle* __restrict ptc = ctx.mPool->mActiveParticles[i];
        if (ptc->IsDead() && ptc->mKey == ctx.mKey) // kill particle
        {
          ctx.mPool->FastFree(i);
        }
      }
    }
  }

  ///////////////////////////////////////////////////////////////////////////////

  void ParticleItemBase::describeX(class_t* clazz) {
  /*ork::reflect::RegisterProperty("Duration", &ParticleItemBase::mfDuration);
  ork::reflect::annotatePropertyForEditor<ParticleItemBase>("Duration", "editor.range.min", "0");
  ork::reflect::annotatePropertyForEditor<ParticleItemBase>("Duration", "editor.range.max", "60.0f");
  ork::reflect::annotatePropertyForEditor<ParticleItemBase>("Duration", "editor.range.log", "true");

  ork::reflect::RegisterProperty("PreCharge", &ParticleItemBase::mfPreCharge);
  ork::reflect::annotatePropertyForEditor<ParticleItemBase>("PreCharge", "editor.range.min", "0");
  ork::reflect::annotatePropertyForEditor<ParticleItemBase>("PreCharge", "editor.range.max", "60.0f");
  ork::reflect::annotatePropertyForEditor<ParticleItemBase>("PreCharge", "editor.range.log", "true");

  ork::reflect::RegisterProperty("StartTime", &ParticleItemBase::mfStartTime);
  ork::reflect::annotatePropertyForEditor<ParticleItemBase>("StartTime", "editor.range.min", "0");
  ork::reflect::annotatePropertyForEditor<ParticleItemBase>("StartTime", "editor.range.max", "60.0f");
  ork::reflect::annotatePropertyForEditor<ParticleItemBase>("StartTime", "editor.range.log", "true");

  ork::reflect::RegisterProperty("SortValue", &ParticleItemBase::mfSortValue);
  ork::reflect::annotatePropertyForEditor<ParticleItemBase>("SortValue", "editor.range.min", "0");
  ork::reflect::annotatePropertyForEditor<ParticleItemBase>("SortValue", "editor.range.max", "1.0f");
  ork::reflect::annotatePropertyForEditor<ParticleItemBase>("SortValue", "editor.range.log", "true");

  ork::reflect::RegisterProperty("IsWorldSpace", &ParticleItemBase::mbWorldSpace);
*/}

  ///////////////////////////////////////////////////////////////////////////////

  ParticleItemBase::ParticleItemBase()
      : mfPreCharge(0.0f)
      , mfStartTime(0.0f)
      , mfDuration(0.0f)
      , mfSortValue(0.0f)
      , mbWorldSpace(true) {
  }

  ParticleItemBase::~ParticleItemBase() {
  }

  ///////////////////////////////////////////////////////////////////////////////

  void ParticleSystemBase::describeX(class_t* clazz) {
  }

  ///////////////////////////////////////////////////////////////////////////////

  ParticleSystemBase::ParticleSystemBase(const ParticleItemBase& itemb)
      : mItem(itemb) {
  }
  ParticleSystemBase::~ParticleSystemBase() {
  }

  ///////////////////////////////////////////////////////////////////////////////

  void ParticleSystemBase::SetElapsed(float fv) {
    mElapsed = 0.0f;
    DoSetElapsed(fv);
  }

  ///////////////////////////////////////////////////////////////////////////////

  void ParticleSystemBase::Reset() {
    mElapsed = 0.0f;
    DoReset();
    SetEmitterEnable(true);
  }

  ///////////////////////////////////////////////////////////////////////////////

  void ParticleSystemBase::Update(float fdt) {
    ///////////////////////////////////
    // precharge the system
    ///////////////////////////////////

    if (mElapsed == 0.0f) {
      const float kprechargestep = 1.0f / 16.0f;

      float fprecharge = mItem.GetPreCharge();

      for (float t = 0.0f; t < fprecharge; t += kprechargestep) // precharge at 16 fps
      {
        DoUpdate(kprechargestep);
      }
      SetElapsed(0.0f);
    }

    ///////////////////////////////////
    // check duration
    ///////////////////////////////////

    float fstarttime = mItem.GetStartTime();
    float fduration  = mItem.GetDuration();
    float fendtime   = fstarttime + fduration;

    if (mElapsed >= fstarttime) {
      if ((fduration != 0.0f) && (mElapsed >= fendtime)) {
        SetEmitterEnable(false);
      }
    }

    ///////////////////////////////////
    // steady state update the system
    ///////////////////////////////////

    if (mElapsed >= fstarttime) {
      DoUpdate(fdt);
    }

    mElapsed += fdt;
  }

  ///////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////

  EventQueue::EventQueue() {
    for (int i = 0; i < kmaxevents; i++) {
      mEvents.push_back(Event());
    }
  }

  void EventQueue::QueueEvent(const Event& ev) {
    OrkAssert((miSize + 1) < kmaxevents);
    mEvents[mInIndex] = ev;
    mInIndex          = (mInIndex + 1) % kmaxevents;
    // mEvents.push(ev);
    miSize++;
  }
  Event EventQueue::DequeueEvent() {
    OrkAssert(miSize > 0);
    Event rv;
    if (miSize > 0) {
      rv        = mEvents[mOutIndex];
      mOutIndex = (mOutIndex + 1) % kmaxevents;
      // rv = mEvents.front();
      // mEvents.pop();
      miSize--;
    }
    return rv;
  }
  void EventQueue::Clear() {
    // printf( "Clearing EVQ<%p>\n", this );
    // while(mEvents.empty()==false) mEvents.pop();
    miSize    = 0;
    mInIndex  = 0;
    mOutIndex = 0;
  }

  } // namespace ork::lev2::particle
