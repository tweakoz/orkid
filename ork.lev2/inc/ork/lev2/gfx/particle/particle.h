////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/lev2_types.h>
#include <ork/object/Object.h>
#include <ork/kernel/tempstring.h>
#include <ork/kernel/string/ArrayString.h>
#include <ork/math/cvector4.h>
#include <ork/math/cmatrix3.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/kernel/fixedlut.h>
#include <ork/rtti/RTTIX.inl>
#include <random>
#include <mutex>
#include <vector>

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
  inline int numFree() const {
    return GetMax()-GetNumAlive();
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

  inline void updateUnitAges() {
    size_t num_active = GetNumAlive();
    for(size_t i=0; i<num_active; i++){
      auto ptcl    = GetActiveParticle(i);
      float fage   = ptcl->mfAge;
      float flspan = (ptcl->mfLifeSpan != 0.0f) //
                   ? ptcl->mfLifeSpan     //
                   : 0.01f;
      float clamped_unitage = std::clamp<float>((fage / flspan), 0, 1);
      ptcl->_unit_age = clamped_unitage;
    }
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
  float _unit_age = 0.0f;
  uint32_t mColliderStates = 0;

  ork::fvec3 mPosition;
  ork::fvec3 mLastPosition;
  ork::fvec3 mVelocity;
  ork::fvec3 mLastVelocity;
  ork::fvec3 mOrigin;

  // Generic per-particle scratch slot. Default vec4(0) at emit; the only
  // consumer in v1 is GradientAtlasMaterial (samples atlas Y from _aux.x).
  // Emitters can override via the Aux input plug (#47). Forces and the pool
  // do not read/write this — it's emitter→renderer only.
  ork::fvec4 _aux = ork::fvec4(0, 0, 0, 0);

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
  drawable_ptr_t _drawable;
  // Per-renderer render lambdas, keyed by the registering renderer INSTANCE
  // (idempotent across graph re-links — a reset/relink REPLACES, never
  // duplicates). Draw order is EXPLICIT via the renderer data's reflected
  // draw_order (lower draws first — smoke under fire); ties keep
  // registration (link/topo) order via the stable insert.
  //
  // THREAD CONTRACT: registration happens on the UPDATE thread (graph link —
  // which for ECS dynamic spawns can interleave with rendering of already-
  // attached slots), iteration on the RENDER thread. Mutating the vector
  // under a reader was an intermittent spawn-time SEGV — so registration
  // locks, and renderers snapshot via renderLambdas() (tiny copy, per draw).
  struct RcidLambdaEntry {
    const void* _key   = nullptr;
    int _order         = 0;
    rcid_lambda_t _lambda;
  };
  void setRenderLambda(const void* key, int order, rcid_lambda_t lambda) {
    std::lock_guard<std::mutex> lock(_rcid_mutex);
    for (auto it = _rcidlambdas.begin(); it != _rcidlambdas.end(); ++it) {
      if (it->_key == key) { // re-link: remove, then re-insert at sorted spot
        _rcidlambdas.erase(it);
        break;
      }
    }
    auto pos = _rcidlambdas.begin();
    while (pos != _rcidlambdas.end() and pos->_order <= order)
      ++pos; // insert AFTER equal orders = stable tie-break on link order
    _rcidlambdas.insert(pos, RcidLambdaEntry{key, order, std::move(lambda)});
  }
  std::vector<RcidLambdaEntry> renderLambdas() const {
    std::lock_guard<std::mutex> lock(_rcid_mutex);
    return _rcidlambdas;
  }

private:
  mutable std::mutex _rcid_mutex;
  std::vector<RcidLambdaEntry> _rcidlambdas;

public:

  // When true, emitter modules SKIP _emit() in their compute() — existing
  // particles continue to age/move/render, but no new ones are produced.
  // Used by ECS ParticlesComponent's DRAINING state (STOP event): freeze
  // emission while letting in-flight particles complete naturally.
  bool _inhibit_emission = false;

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

enum class EmitterDirection : uint64_t {
  CrcEnum(CONSTANT),
  CrcEnum(VEL),
  CrcEnum(CROSS_X),
  CrcEnum(CROSS_Y),
  CrcEnum(CROSS_Z),
  CrcEnum(TO_POINT),
  CrcEnum(USER),
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
  fvec3 _userDirection;

  // Per-emit aux value — written into each new particle's _aux at spawn.
  // Emitters set this from their Aux input plug before calling Emit().
  // Default vec4(0) matches the particle struct's default.
  fvec4 mAux = fvec4(0, 0, 0, 0);

  // Per-particle aux hook. When set, DirectedEmitter calls this for each
  // newly-spawned particle INSTEAD of writing mAux directly. The emitter
  // installs a lambda that (a) writes the particle's mfRandom into the
  // pool's Random output plug, then (b) reads the Aux input plug — so
  // chains containing Expr.ptc.random / Expr.rand_range evaluate per
  // particle. When null, the simple per-cohort mAux path is used.
  std::function<void(BasicParticle*)> mPerParticleAux;

  EmitterCtx();
};

struct RandGen{
  RandGen();
  std::mt19937 _randgen;
  std::uniform_int_distribution<> _distribution;
  float ranged_rand(float min, float max);
};
class DirectedEmitter {
public:
  void Emit(EmitterCtx& ctx);
  void EmitCB(EmitterCtx& ctx);
  void EmitSQ(EmitterCtx& ctx);
  void Reap(EmitterCtx& ctx);
  EmitterDirection meDirection;
  float mDispersionAngle;
  RandGen _randgen;

  virtual void computePosDir(float fi, fvec3& pos, fmtx3& basis) = 0;
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
