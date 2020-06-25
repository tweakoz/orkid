////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/particle/particle.h>
#include <ork/lev2/gfx/particle/particle.hpp>
#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/properties/DirectMapTyped.h>
#include <ork/reflect/IObjectPropertyType.hpp>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/DirectMapTyped.hpp>
#include <ork/reflect/enum_serializer.inl>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/fixedlut.hpp>

///////////////////////////////////////////////////////////////////////////////

using namespace ork::lev2::particle;

template class ork::reflect::DirectMapPropertyType<orkmap<float, ork::fvec4>>;
template class ork::reflect::DirectPropertyType<ork::lev2::EBlending>;
template class ork::reflect::DirectPropertyType<ork::lev2::EPrimitiveType>;
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

INSTANTIATE_TRANSPARENT_RTTI(SpiralEmitterData, "SpiralEmitterData");
INSTANTIATE_TRANSPARENT_RTTI(ParticleSystemBase, "Lev2ParticleSystemBase");
INSTANTIATE_TRANSPARENT_RTTI(ParticleItemBase, "Lev2ParticleItemBase");

///////////////////////////////////////////////////////////////////////////////

BEGIN_ENUM_SERIALIZER(ork::lev2::particle, ParticleItemAlignment)
DECLARE_ENUM(EPIA_BILLBOARD)
DECLARE_ENUM(EPIA_XZ)
DECLARE_ENUM(EPIA_XY)
DECLARE_ENUM(EPIA_YZ)
END_ENUM_SERIALIZER()

BEGIN_ENUM_SERIALIZER(ork::lev2::particle, EmitterDirection)
DECLARE_ENUM(EMITDIR_CONSTANT)
DECLARE_ENUM(EMITDIR_VEL)
DECLARE_ENUM(EMITDIR_CROSS_X)
DECLARE_ENUM(EMITDIR_CROSS_Y)
DECLARE_ENUM(EMITDIR_CROSS_Z)
DECLARE_ENUM(EMITDIR_TO_POINT)
DECLARE_ENUM(EMITDIR_USER)
END_ENUM_SERIALIZER()

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 { namespace particle {
///////////////////////////////////////////////////////////////////////////////

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

void SpiralEmitterData::Describe() {
  reflect::RegisterProperty("Lifespan", &SpiralEmitterData::mfLifespan);
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
}

///////////////////////////////////////////////////////////////////////////////

SpiralEmitterData::SpiralEmitterData()
    : mfLifespan(0.0f)
    , mfEmitScale(0.0f)
    , mfEmissionRate(0.0f)
    , mfEmissionUp(0.0f)
    , mfEmissionOut(0.0f)
    , mfSpinRate(0.0f) {
}

///////////////////////////////////////////////////////////////////////////////

EmitterCtx::EmitterCtx()
    : mPool(0)
    , mfDeltaTime(0.0f)
    , mfEmitterMark(0.0f)
    , miEmitterMark(0)
    , mfEmissionRate(0.0f)
    , mfEmissionVelocity(0.0f)
    , mfLifespan(0.0f)
    , mDispersion(0.0f)
    , mSpawnQueue(0)
    , mDeathQueue(0)
    , mfSpawnProbability(1.0f)
    , mfSpawnMultiplier(1.0f) {
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
  printf("emitrate<%f> deltat<%f> deltap<%f> mark<%f>\n", ctx.mfEmissionRate, ctx.mfDeltaTime, fdeltap, ctx.mfEmitterMark);
  int icount = int(ctx.mfEmitterMark);
  fvec3 dir, pos, disp, vbin, vtan, yo;
  for (int ic = 0; ic < icount; ic++) {
    float fi = float(ic) / float(icount);
    ComputePosDir(fi, pos, dir);
    pos += ctx.mPosition;
    BasicParticle* __restrict ptc = the_pool.FastAlloc();
    if (ptc) {
      ptc->mfAge      = 0.0f;
      ptc->mfLifeSpan = ctx.mfLifespan;
      switch (meDirection) {
        case EMITDIR_CROSS_X:
          dir = dir.Cross(fvec3::Red());
          break;
        case EMITDIR_CROSS_Y:
          dir = dir.Cross(fvec3::Green());
          break;
        case EMITDIR_CROSS_Z:
          dir = dir.Cross(fvec3::Blue());
          break;
        case EMITDIR_CONSTANT:
        case EMITDIR_VEL:
        case EMITDIR_TO_POINT:
          break;
      }
      vbin = dir.Cross(fvec3::Green());
      if (vbin.MagSquared() < .1f)
        vbin = dir.Cross(fvec3::Red());
      vbin.Normalize();
      vtan     = vbin.Cross(dir);
      float fu = ((std::rand() % 256) / 256.0f) - 0.5f;
      float fv = ((std::rand() % 256) / 256.0f) - 0.5f;
      disp     = (vbin * fu) + (vtan * fv);
      yo.Lerp(dir, disp, ctx.mDispersion);
      dir                = yo.Normal();
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
    int iran   = rand() % 100;
    float fran = float(iran) * 0.01f;
    //////////////////////////////////////////////////
    if (fran < ctx.mfSpawnProbability) {
      //////////////////////////////////////////////
      pos  = ev.mPosition + ctx.mPosition;
      odir = (pos - ev.mLastPosition).Normal();
      dir  = odir;
      if (false == bUseEmitterSpeed)
        EmitterSpeed = ev.mVelocity.Mag();
      //////////////////////////////////////////////
      for (float ii = 0.0f; ii < ctx.mfSpawnMultiplier; ii += 1.0f) {
        BasicParticle* __restrict ptc = the_pool.FastAlloc();
        if (ptc) {
          ptc->mfAge      = 0.0f;
          ptc->mfLifeSpan = ctx.mfLifespan;
          //////////////////////////////////////////////
          // calc dispersion
          //////////////////////////////////////////////
          vbin = dir.Cross(fvec3::Green());
          if (vbin.MagSquared() < .1f)
            vbin = dir.Cross(fvec3::Red());
          vbin.Normalize();
          vtan     = vbin.Cross(dir);
          float fu = ((std::rand() % 256) / 256.0f) - 0.5f;
          float fv = ((std::rand() % 256) / 256.0f) - 0.5f;
          disp     = (vbin * fu) + (vtan * fv);
          yo.Lerp(dir, disp, ctx.mDispersion);
          dir = yo.Normal();
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
                  DeathEv.mPosition.GetX(),
                  DeathEv.mPosition.GetY(),
                  DeathEv.mPosition.GetZ()
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

void ParticleItemBase::Describe() {
  ork::reflect::RegisterProperty("Duration", &ParticleItemBase::mfDuration);
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
}

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

void ParticleSystemBase::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

ParticleSystemBase::ParticleSystemBase(const ParticleItemBase& itemb)
    : mItem(itemb)
    , mElapsed(0.0f) {
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

EventQueue::EventQueue()
    : mInIndex(0)
    , mOutIndex(0)
    , miSize(0) {

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

}}} // namespace ork::lev2::particle
