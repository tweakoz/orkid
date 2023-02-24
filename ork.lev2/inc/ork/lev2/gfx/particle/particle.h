////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/object/Object.h>
#include <ork/kernel/string/ArrayString.h>
#include <ork/dataflow/dataflow.h>
#include <ork/math/cvector4.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/kernel/fixedlut.h>

namespace ork { namespace lev2 { namespace particle {

///////////////////////////////////////////////////////////////////////////////

template <typename ptype> class Pool {
public:
  orkvector<ptype*> mActiveParticles;
  orkvector<ptype*> mInactiveParticles;
  orkvector<ptype> mParticleBlock;

  int miMaxParticles;

  Pool();
  void Init(int imax);
  void Copy(const Pool& oth);

  inline int GetMax() const {
    return miMaxParticles;
  }
  inline int GetNumAlive() const {
    return int(mActiveParticles.size());
  }
  inline int GetNumDead() const {
    return int(mInactiveParticles.size());
  }

  inline const ptype* GetActiveParticle(int idx) const {
    return mActiveParticles[idx];
  }
  inline ptype* GetActiveParticle(int idx) {
    return mActiveParticles[idx];
  }
  inline void Reset() {
    mActiveParticles.clear();
    mInactiveParticles.clear();
    for (int i = 0; i < miMaxParticles; i++) {
      mInactiveParticles.push_back(&mParticleBlock[i]);
    }
  }

  inline ptype* FastAlloc() {
    ptype* rval  = 0;
    int inumdead = GetNumDead();
    if (inumdead > 0) {
      ptype* ptc = mInactiveParticles[inumdead - 1];
      mInactiveParticles.erase(mInactiveParticles.begin() + inumdead - 1);
      mActiveParticles.push_back(ptc);
      rval = ptc;
    }
    return rval;
  }

  inline void FastFree(int iactiveindex) {
    ptype* ptc = mActiveParticles[iactiveindex];
    mInactiveParticles.push_back(ptc);
    int ilast_alive                = GetNumAlive() - 1;
    ptype* ptclast                 = mActiveParticles[ilast_alive];
    mActiveParticles[iactiveindex] = ptclast;
    mActiveParticles.erase(mActiveParticles.begin() + ilast_alive);
  }
};

//////////////////////////////////////////////////////////////////////////////

template <typename ptype> class Emitter {
public:
  virtual void Emit(Pool<ptype>& pool, float dt) = 0;
  virtual void Reap(Pool<ptype>& pool, float dt) = 0;
  virtual void Reset() {
  }
};

//////////////////////////////////////////////////////////////////////////////

template <typename ptype> class BasicEmitter : public Emitter<ptype> {
public:
  virtual void Reap(Pool<ptype>& pool, float dt);
};

//////////////////////////////////////////////////////////////////////////////

class SpiralEmitterData : public ork::Object {
  DeclareConcreteX(SpiralEmitterData, ork::Object);

public:
  SpiralEmitterData();

  float GetLifespan() const {
    return mfLifespan;
  }
  float GetEmitScale() const {
    return mfEmitScale;
  }
  float GetEmissionUp() const {
    return mfEmissionUp;
  }
  float GetEmissionOut() const {
    return mfEmissionOut;
  }
  float GetEmissionRate() const {
    return mfEmissionRate;
  }
  float GetSpinRate() const {
    return mfSpinRate;
  }

private:
  float mfLifespan = 0.0f;
  float mfEmitScale = 0.0f;
  float mfEmissionUp = 0.0f;
  float mfEmissionOut = 0.0f;
  float mfEmissionRate = 0.0f;
  float mfSpinRate = 0.0f;
};

//////////////////////////////////////////////////////////////////////////////

template <typename ptype> class SpiralEmitter : public BasicEmitter<ptype> {
public:
  virtual void Emit(Pool<ptype>& pool, float dt);

  SpiralEmitter(const SpiralEmitterData& sed)
      : mSed(sed)
      , mfEmitterMark(0.0f)
      , miEmitterMark(0)
      , mfPhase(0.0f) {
  }

private:
  const SpiralEmitterData& mSed;
  float mfEmitterMark;
  int miEmitterMark;
  float mfPhase;
};

//////////////////////////////////////////////////////////////////////////////

template <typename ptype> class Controller {
public:
  virtual void Update(Pool<ptype>& pool, float dt) = 0;
  virtual void Reset() {
  }
};

//////////////////////////////////////////////////////////////////////////////

template <typename ptype> class FixedSystem {
  Pool<ptype> mPool;
  Emitter<ptype>* mEmitter;
  Controller<ptype>* mController;
  float mElapsed;
  bool mbEmitEnable;
  // float				mDuration;

public: //
  void Reset();

  //////////////////////////

  void SetMaxParticles(int imax) {
    mPool.Init(imax);
  }
  int GetNumAlive(void) const {
    return mPool.GetNumAlive();
  }
  const ptype* GetActiveParticle(int idx) const {
    return mPool.GetActiveParticle(idx);
  }
  // void SetDuration(float fv) { mDuration=fv; }
  void SetElapsed(float fv) {
    mElapsed = fv;
  }
  inline void update(float dt);
  void SetEmitEnable(bool bv) {
    mbEmitEnable = bv;
  }

  //////////////////////////

  FixedSystem(Emitter<ptype>* Emitter = 0, Controller<ptype>* Controller = 0);
  virtual ~FixedSystem();
};

///////////////////////////////////////////////////////////////////////////////

template <typename ptype> class ForceField {
public: //
  virtual void Apply(Pool<ptype>& pool, float dt) {
  }

  ForceField() {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct BasicParticle {

  void* mKey = nullptr;

  float mfRandom = 0.0f;
  float mfAge = 0.0f;
  float mfLifeSpan = 0.0f;
  uint32_t mColliderStates = 0;

  ork::fvec3 mPosition;
  ork::fvec3 mLastPosition;
  ork::fvec3 mVelocity;
  ork::fvec3 mLastVelocity;

  bool IsDead(void) {
    return (mfAge >= mfLifeSpan);
  }

  BasicParticle() {
    // todo - this prob should still be deterministic,
    //  have the emitter assign the random val

    int irandom = (rand() & 255) | (rand() & 255) << 8;
    mfRandom    = float(irandom) / 65536.0f;
  }
};

///////////////////////////////////////////////////////////////////////////////

struct Event {
  Char4 mEventType;
  Char4 mEventId;      // 8
  fvec3 mPosition;     // 20
  fvec3 mVelocity;     // 32
  fvec3 mLastPosition; // 44

  Event(const Event& oth)
      : mEventType(oth.mEventType)
      , mEventId(oth.mEventId)
      , mPosition(oth.mPosition)
      , mVelocity(oth.mVelocity)
      , mLastPosition(oth.mLastPosition) {
  }
  Event()
      : mEventType("NULL")
      , mEventId("NULL") {
  }
};

struct EventQueue {

  static const int kmaxevents = 65536;

  int miSize = 0;
  int mInIndex = 0;
  int mOutIndex = 0;

  fixedvector<Event, kmaxevents> mEvents;

  int GetNumEvents() const {
    return miSize;
  }
  EventQueue();
  void QueueEvent(const Event& ev);
  Event DequeueEvent();
  void Clear();
};

typedef orklut<Char4, ork::lev2::particle::EventQueue*> EventQueueLut;
struct Context {
  Context()
      : mfCurrentTime(0.0f) {
  }

  EventQueueLut mEventQueueLut;
  float mfCurrentTime;

  EventQueue* MergeQueue(Char4 qname);
  EventQueue* FindQueue(Char4 qname);

  void BeginFrame(float currenttime);
  float CurrentTime() const {
    return mfCurrentTime;
  }
};

using context_ptr_t = std::shared_ptr<Context>;

inline Char4 PoolString2Char4(const PoolString& ps) {
  Char4 rval;

  int ilen = ps.c_str() ? strlen(ps.c_str()) : 0;

  if (ilen > 0 && ilen <= 4)
    rval = Char4(ps.c_str());
  else
    rval.SetU32(0);

  return rval;
}
///////////////////////////////////////////////////////////////////////////////

enum EmitterDirection {
  CONSTANT = 0,
  VEL,
  CROSS_X,
  CROSS_Y,
  CROSS_Z,
  TO_POINT,
  USER,
};

struct EmitterCtx {

  Pool<BasicParticle>* mPool = nullptr;
  EventQueue* mSpawnQueue = nullptr;
  EventQueue* mDeathQueue = nullptr;
  void* mKey = nullptr;

  float mfSpawnProbability = 1.0f;
  float mfSpawnMultiplier = 1.0f;
  float mDispersion = 0.0f;
  float mfDeltaTime = 0.0f;
  float mfEmitterMark = 0.0f;
  float mfEmissionRate = 0.0f;
  float mfEmissionVelocity = 0.0f;
  float mfLifespan = 0.0f;
  int miEmitterMark = 0.0f;

  fvec3 mPosition;
  fvec3 mLastPosition;
  fvec3 mOffsetVelocity;

  EmitterCtx();
};

class DirectedEmitter {
public:
  void Emit(EmitterCtx& ctx);
  void EmitCB(EmitterCtx& ctx);
  void EmitSQ(EmitterCtx& ctx);
  void Reap(EmitterCtx& ctx);
  EmitterDirection meDirection;
  float mDispersionAngle;

  virtual void computePosDir(float fi, fvec3& pos, fvec3& dir) = 0;
};

enum class ParticleItemAlignment {
  BILLBOARD = 0,
  XZ,
  XY,
  YZ,
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class ParticleItemBase;

class ParticleSystemBase : public ork::Object {
  DeclareAbstractX(ParticleSystemBase, ork::Object);

public:
  ParticleSystemBase(const ParticleItemBase& itemb);
  ~ParticleSystemBase();

  void Reset();
  void Update(float fdt);
  float GetElapsed() const {
    return mElapsed;
  }
  void SetElapsed(float fv);

protected:

  const ParticleItemBase& mItem;

  float mElapsed = 0.0f;

  virtual void DoUpdate(float fdt) = 0;
  virtual void DoReset() {
  }
  virtual void DoSetElapsed(float fv) {
  }
  virtual void SetEmitterEnable(bool bv) = 0;
};

///////////////////////////////////////////////////////////////////////////////

class ParticleItemBase : public ork::Object {
  DeclareAbstractX(ParticleItemBase, ork::Object);

public:
  float GetPreCharge() const {
    return mfPreCharge;
  }
  float GetStartTime() const {
    return mfStartTime;
  }
  float GetDuration() const {
    return mfDuration;
  }
  float GetSortValue() const {
    return mfSortValue;
  }
  bool IsWorldSpace() const {
    return mbWorldSpace;
  }

  ~ParticleItemBase();

protected:
  ParticleItemBase();

  float mfPreCharge;
  float mfStartTime;
  float mfDuration;
  float mfSortValue;
  bool mbWorldSpace;
};

///////////////////////////////////////////////////////////////////////////////

}}} // namespace ork::lev2::particle

///////////////////////////////////////////////////////////////////////////////
