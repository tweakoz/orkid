////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/particle/particle.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/dataflow/dataflow.h>
#include <ork/math/gradient.h>
#include <ork/kernel/any.h>

#if 0

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define DeclarePoolOutPlug(name)                                                                                                   \
  psys_ptclbuf OutDataName(name);                                                                                                  \
  mutable PtclBufOutPlug OutPlugName(name);                                                                                        \
  ork::Object* OutAccessor##name() {                                                                                               \
    return &OutPlugName(name);                                                                                                     \
  }

#define DeclarePoolInpPlug(name)                                                                                                   \
  mutable PtclBufInpPlug InpPlugName(name);                                                                                        \
  ork::Object* InpAccessor##name() {                                                                                               \
    return &InpPlugName(name);                                                                                                     \
  }

#define ConstructPoolOutPlug(name, epr, __pool) mOutData##name(&__pool), OutPlugName(name)(this, epr, &mOutData##name, #name)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////

struct psys_ptclbuf {
  Pool<BasicParticle>* mPool;
  psys_ptclbuf(Pool<BasicParticle>* pl = 0)
      : mPool(pl) {
  }
};

typedef ork::dataflow::outplug<psys_ptclbuf> PtclBufOutPlug;
typedef ork::dataflow::inplug<psys_ptclbuf> PtclBufInpPlug;

///////////////////////////////////////////////////////////////////////////////

class Module : public ork::dataflow::dgmodule {
  DeclareAbstractX(Module, ork::dataflow::dgmodule);
  ////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////
protected:
  Module();
  virtual void DoLink() {
  }
  ork::lev2::particle::Context* mpParticleContext;
  Module* mpTemplateModule;
  virtual void Compute(ork::dataflow::workunit* wu) final {
  }
  virtual void CombineWork(const ork::dataflow::cluster* c) final {
  }

public:
  void Link(ork::lev2::particle::Context* pctx);
  virtual void Compute(float dt) = 0; // virtual
  virtual void Reset() {
  }
  void SetTemplateModule(Module* ptemp) {
    mpTemplateModule = ptemp;
  }
};

///////////////////////////////////////////////////////////////////////////////

class Global : public Module {
  DeclareConcreteX(Global, Module);

private:
  DeclareFloatXfPlug(TimeScale);
  DeclareFloatOutPlug(RelTime);
  DeclareFloatOutPlug(Time);
  DeclareFloatOutPlug(TimeDiv10);
  DeclareFloatOutPlug(TimeDiv100);

  DeclareFloatOutPlug(Random);
  DeclareVect3OutPlug(RandomNormal);
  DeclareFloatOutPlug(Noise);
  DeclareFloatOutPlug(SlowNoise);
  DeclareFloatOutPlug(FastNoise);

  dataflow::inplugbase* GetInput(int idx) const final {
    return &mPlugInpTimeScale;
  }
  dataflow::outplugbase* GetOutput(int idx) const final;
  void Compute(float dt) final;
  void OnStart() final;

  float mfTimeBase = 0.0f;

  float mfNoiseRat = 0.0f;
  float mfNoisePrv = 0.0f;
  float mfNoiseNew = 0.0f;
  float mfNoiseBas = 0.0f;
  float mfNoiseTim = 0.0f;

  float mfSlowNoiseRat = 0.0f;
  float mfSlowNoisePrv = 0.0f;
  float mfSlowNoiseBas = 0.0f;
  float mfSlowNoiseTim = 0.0f;

  float mfFastNoiseRat = 0.0f;
  float mfFastNoisePrv = 0.0f;
  float mfFastNoiseBas = 0.0f;
  float mfFastNoiseTim = 0.0f;

public:
  Global();
};

///////////////////////////////////////////////////////////////////////////////

class Constants : public Module {
  DeclareConcreteX(Constants, Module);

  ork::orklut<ork::PoolString, ork::dataflow::outplug<float>*> mFloatPlugs;
  ork::orklut<ork::PoolString, ork::dataflow::outplug<fvec3>*> mVect3Plugs;

  ork::orklut<ork::PoolString, float> mFloatConsts;
  ork::orklut<ork::PoolString, fvec3> mVect3Consts;

  bool mbPlugsDirty;
  /////////////////////////////////////////////////////
  // data currently only flows in from externals
  dataflow::inplugbase* GetInput(int idx) const final {
    return nullptr;
  } // virtual
  // data currently only flows in from externals
  /////////////////////////////////////////////////////

  int GetNumOutputs() const final;
  dataflow::outplugbase* GetOutput(int idx) const final; // virtual

  void Compute(float dt) final {
  } // virtual

  void OnTopologyUpdate(void) final; // virtual

  void doNotify(const ork::event::Event* event) final;         // virtual
  bool postDeserialize(reflect::serdes::IDeserializer&) final; // virtual

public:
  Constants();
};

///////////////////////////////////////////////////////////////////////////////

class ExtConnector : public Module {
  DeclareConcreteX(ExtConnector, Module);

  dataflow::dyn_external* mpExternalConnector;
  ork::orklut<ork::PoolString, ork::dataflow::outplug<float>*> mFloatPlugs;
  ork::orklut<ork::PoolString, ork::dataflow::outplug<fvec3>*> mVect3Plugs;

  /////////////////////////////////////////////////////
  // data currently only flows in from externals
  dataflow::inplugbase* GetInput(int idx) const final {
    return nullptr;
  }
  // data currently only flows in from externals
  /////////////////////////////////////////////////////

  int GetNumOutputs() const final;
  dataflow::outplugbase* GetOutput(int idx) const final;

  void Compute(float dt) final {
  }

public:
  ExtConnector();
  void BindConnector(dataflow::dyn_external* pconnector);
};

///////////////////////////////////////////////////////////////////////////////

class ParticleModule : public Module {
  DeclareAbstractX(ParticleModule, Module);
  bool IsDirty(void) const final {
    return true;
  } // virtual
protected:
  static psys_ptclbuf gNoCon;
  ParticleModule() {
  }
  ////////////////////////////////////////////////////////////
  void MarkClean() {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct ParticlePool : public ParticleModule {

  DeclareConcreteX(ParticlePool, ParticleModule);

public:
  void Compute(float dt) final;
  void Reset() final;
  void DoLink() final;

  ParticlePool();
  const Pool<BasicParticle>& GetPool() const {
    return mPoolOutput;
  }

  DeclareFloatOutPlug(UnitAge);
  DeclareFloatXfPlug(PathInterval);
  DeclareFloatXfPlug(PathProbability);
  DeclarePoolOutPlug(Output);

  EventQueue* mPathStochasticEventQueue = nullptr;
  EventQueue* mPathIntervalEventQueue   = nullptr;

  int miPoolSize = 40;
  Char4 mPathStochasticQueueID4;

  Pool<BasicParticle> mPoolOutput;
  PoolString mPathStochasticQueueID;
  PoolString mPathIntervalQueueID;
  Char4 mPathIntervalQueueID4;

  /////////////////////////////////////////////////
  dataflow::inplugbase* GetInput(int idx) const final;
  /////////////////////////////////////////////////
  ork::dataflow::outplugbase* GetOutput(int idx) const final;
  /////////////////////////////////////////////////
};

struct ParticlePoolRenderBuffer {
  ParticlePoolRenderBuffer();
  ~ParticlePoolRenderBuffer();
  void Update(const Pool<BasicParticle>& pool);
  void SetCapacity(int inum);

  BasicParticle* mpParticles = nullptr;
  int miMaxParticles = 0;
  int miNumParticles = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct RingEmitter;

class RingDirectedEmitter : public DirectedEmitter {
  RingEmitter& mEmitterModule;
  void ComputePosDir(float fi, fvec3& pos, fvec3& dir);

public:
  RingDirectedEmitter(RingEmitter& EmitterModule)
      : mEmitterModule(EmitterModule) {
  }
  fvec3 mUserDir;
};

struct RingEmitter : public ParticleModule {

  DeclareConcreteX(RingEmitter, ParticleModule);

  friend class RingDirectedEmitter;

public:

  void DoLink() final;
  void Compute(float dt) final;

  void Emit(float fdt);
  void Reap(float fdt);
  void Reset() final;

  RingEmitter();


  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) const final;

  DeclarePoolInpPlug(Input);
  DeclareFloatXfPlug(Lifespan);
  DeclareFloatXfPlug(EmissionRadius);
  DeclareFloatXfPlug(EmissionRate);
  DeclareFloatXfPlug(EmissionVelocity);
  DeclareFloatXfPlug(EmitterSpinRate);
  DeclareFloatXfPlug(DispersionAngle);
  DeclareFloatXfPlug(OffsetX);
  DeclareFloatXfPlug(OffsetY);
  DeclareFloatXfPlug(OffsetZ);
  DeclareVect3XfPlug(Direction);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  dataflow::outplugbase* GetOutput(int idx) const final;

  DeclarePoolOutPlug(Output);

  //////////////////////////////////////////////////

  lev2::particle::EventQueue* mDeathEventQueue = nullptr;

  float mfPhase = 0.0f;
  float mfPhase2 = 0.0f;
  float mfLastRadius = 0.0f;
  float mfThisRadius = 0.0f;
  float mfAccumTime = 0.0f;
  RingDirectedEmitter mDirectedEmitter;
  EmitterDirection meDirection = EmitterDirection::VEL;
  EmitterCtx mEmitterCtx;
  ////////////////////////////////////////////////////////////////
  PoolString mDeathQueueID;
  Char4 mDeathQueueID4;

};

///////////////////////////////////////////////////////////////////////////////

struct NozzleEmitter;

class NozzleDirectedEmitter : public DirectedEmitter {
  NozzleEmitter& mEmitterModule;
  void ComputePosDir(float fi, fvec3& pos, fvec3& dir);

public:
  NozzleDirectedEmitter(NozzleEmitter& EmitterModule)
      : mEmitterModule(EmitterModule) {
  }
};

struct NozzleEmitter : public ParticleModule {

  DeclareConcreteX(NozzleEmitter, ParticleModule);
  friend class NozzleDirectedEmitter;

public:

  void Compute(float dt) final;

  void Emit(float fdt);
  void Reap(float fdt);
  void Reset() final;

  NozzleEmitter();

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) const final;

  DeclarePoolInpPlug(Input);
  DeclareFloatXfPlug(Lifespan);
  DeclareFloatXfPlug(EmissionRate);
  DeclareFloatXfPlug(EmissionVelocity);
  DeclareFloatXfPlug(DispersionAngle);
  DeclareVect3XfPlug(Offset);
  DeclareVect3XfPlug(Direction);
  DeclareVect3XfPlug(OffsetVelocity);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  dataflow::outplugbase* GetOutput(int idx) const final;

  DeclarePoolOutPlug(Output);
  DeclarePoolOutPlug(TheDead);

  //////////////////////////////////////////////////

  NozzleDirectedEmitter mDirectedEmitter;

  float mfAccumTime = 0.0f;

  Pool<BasicParticle> mDeadPool;
  EmitterCtx mEmitterCtx;
  fvec3 mLastPosition;
  fvec3 mLastDirection;
  fvec3 mDirectionSample;
  fvec3 mOffsetSample;


};

///////////////////////////////////////////////////////////////////////////////

struct ReEmitter;

class ReDirectedEmitter : public DirectedEmitter {

  ReEmitter& mEmitterModule;
  void ComputePosDir(float fi, fvec3& pos, fvec3& dir);

public:
  ReDirectedEmitter(ReEmitter& EmitterModule)
      : mEmitterModule(EmitterModule) {
  }
};

struct ReEmitter : public ParticleModule {

  DeclareConcreteX(ReEmitter, ParticleModule);
  friend class PoolDirectedEmitter;

public:

  ReEmitter();

  void DoLink() final;
  void Compute(float dt) final;

  void Emit(float fdt);
  void Reap(float fdt);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) const final;

  DeclarePoolInpPlug(Input);
   DeclareFloatXfPlug(SpawnProbability);
  DeclareFloatXfPlug(SpawnMultiplier);
  DeclareFloatXfPlug(Lifespan);
  DeclareFloatXfPlug(EmissionRate);
  DeclareFloatXfPlug(EmissionVelocity);
  DeclareFloatXfPlug(DispersionAngle);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  dataflow::outplugbase* GetOutput(int idx) const final;

  DeclarePoolOutPlug(Output);

  EmitterDirection meDirection = EmitterDirection::VEL;

  ReDirectedEmitter mDirectedEmitter;
  EmitterCtx mEmitterCtx;
  lev2::particle::EventQueue* mSpawnEventQueue;
  lev2::particle::EventQueue* mDeathEventQueue;
  PoolString mSpawnQueueID;
  PoolString mDeathQueueID;
  Char4 mSpawnQueueID4;
  Char4 mDeathQueueID4;

};

///////////////////////////////////////////////////////////////////////////////

struct WindModule : public ParticleModule {
  DeclareConcreteX(WindModule, ParticleModule);

public:
  void Compute(float dt) final;
  WindModule();

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) const final;

  DeclarePoolInpPlug(Input);
  DeclareFloatXfPlug(Force);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  dataflow::outplugbase* GetOutput(int idx) const final {
    return &mPlugOutOutput;
  }

  DeclarePoolOutPlug(Output);

  //////////////////////////////////////////////////

};

///////////////////////////////////////////////////////////////////////////////

struct GravityModule : public ParticleModule {
  DeclareConcreteX(GravityModule, ParticleModule);

public:
  void Compute(float dt) final;
  GravityModule();

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) const final;

  DeclarePoolInpPlug(Input);
  DeclareFloatXfPlug(G);
  DeclareFloatXfPlug(Mass);
  DeclareFloatXfPlug(OthMass);
  DeclareFloatXfPlug(MinDistance);
  DeclareVect3XfPlug(Center);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  dataflow::outplugbase* GetOutput(int idx) const final {
    return &mPlugOutOutput;
  }

  DeclarePoolOutPlug(Output);

  //////////////////////////////////////////////////


};

///////////////////////////////////////////////////////////////////////////////

struct PlanarColliderModule : public ParticleModule {
  DeclareConcreteX(PlanarColliderModule, ParticleModule);

public:
  PlanarColliderModule();
  void Compute(float dt) final;


  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) const final;

  DeclarePoolInpPlug(Input);
  DeclareFloatXfPlug(NormalX);
  DeclareFloatXfPlug(NormalY);
  DeclareFloatXfPlug(NormalZ);
  DeclareFloatXfPlug(OriginX);
  DeclareFloatXfPlug(OriginY);
  DeclareFloatXfPlug(OriginZ);
  DeclareFloatXfPlug(Absorbtion);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  dataflow::outplugbase* GetOutput(int idx) const final {
    return &mPlugOutOutput;
  }

  DeclarePoolOutPlug(Output);

  //////////////////////////////////////////////////

  int miDiodeDirection = 0;

};

///////////////////////////////////////////////////////////////////////////////

struct SphericalColliderModule : public ParticleModule {
  DeclareConcreteX(SphericalColliderModule, ParticleModule);

public:
  SphericalColliderModule();
  void Compute(float dt) final;

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) const final;

  DeclarePoolInpPlug(Input);
  DeclareFloatXfPlug(CenterX);
  DeclareFloatXfPlug(CenterY);
  DeclareFloatXfPlug(CenterZ);
  DeclareFloatXfPlug(Radius);
  DeclareFloatXfPlug(Absorbtion);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  dataflow::outplugbase* GetOutput(int idx) const final {
    return &mPlugOutOutput;
  }

  DeclarePoolOutPlug(Output);

  //////////////////////////////////////////////////

};

///////////////////////////////////////////////////////////////////////////////

enum class PSYS_FLOATOP {
  ADD = 0,
  SUB,
  MUL,
};

struct FloatOp2Module : public ParticleModule {

  DeclareConcreteX(FloatOp2Module, ParticleModule);

public:
  void Compute(float dt) final;
  FloatOp2Module();

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) const final;

  DeclareFloatXfPlug(InputA);
  DeclareFloatXfPlug(InputB);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  dataflow::outplugbase* GetOutput(int idx) const final {
    return &mPlugOutOutput;
  }

  DeclareFloatOutPlug(Output);

  PSYS_FLOATOP meOp = PSYS_FLOATOP::ADD;

  //////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

enum class PSYS_VEC3OP {
  ADD = 0,
  SUB,
  MUL,
  DOT,
  CROSS,
};

struct Vec3Op2Module : public ParticleModule {

  DeclareConcreteX(Vec3Op2Module, ParticleModule);

public:
  Vec3Op2Module();
  void Compute(float dt) final;

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) const final;

  DeclareVect3XfPlug(InputA);
  DeclareVect3XfPlug(InputB);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  dataflow::outplugbase* GetOutput(int idx) const final {
    return &mPlugOutOutput;
  }

  DeclareVect3OutPlug(Output);

  //////////////////////////////////////////////////

  PSYS_VEC3OP meOp = PSYS_VEC3OP::ADD;
};

///////////////////////////////////////////////////////////////////////////////

struct Vec3SplitModule : public ParticleModule {
  DeclareConcreteX(Vec3SplitModule, ParticleModule);

public:
  Vec3SplitModule();
  void Compute(float dt) final;

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) const final;

  DeclareVect3XfPlug(Input);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  dataflow::outplugbase* GetOutput(int idx) const final {
    switch (idx) {
      case 0:
        return &mPlugOutOutputX;
      case 1:
        return &mPlugOutOutputY;
      case 2:
        return &mPlugOutOutputZ;
    }
    return 0;
  }

  DeclareFloatOutPlug(OutputX);
  DeclareFloatOutPlug(OutputY);
  DeclareFloatOutPlug(OutputZ);

  //////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

struct DecayModule : public ParticleModule {
  DeclareConcreteX(DecayModule, ParticleModule);

public:
  DecayModule();
  void Compute(float dt) final;

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) const final;

  DeclarePoolInpPlug(Input);
  DeclareFloatXfPlug(Decay);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  dataflow::outplugbase* GetOutput(int idx) const final {
    return &mPlugOutOutput;
  }

  DeclarePoolOutPlug(Output);

  //////////////////////////////////////////////////


};

///////////////////////////////////////////////////////////////////////////////

struct TurbulenceModule : public ParticleModule {
  DeclareConcreteX(TurbulenceModule, ParticleModule);

public:
  TurbulenceModule();
  void Compute(float dt) final;

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) const final;

  DeclarePoolInpPlug(Input);
  DeclareFloatXfPlug(AmountX);
  DeclareFloatXfPlug(AmountY);
  DeclareFloatXfPlug(AmountZ);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  DeclarePoolOutPlug(Output);

  dataflow::outplugbase* GetOutput(int idx) const final {
    return &mPlugOutOutput;
  }

  //////////////////////////////////////////////////


};

///////////////////////////////////////////////////////////////////////////////

struct VortexModule : public ParticleModule {
  DeclareConcreteX(VortexModule, ParticleModule);

public:
  VortexModule();
  void Compute(float dt) final;

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) const final;

  DeclarePoolInpPlug(Input);
  DeclareFloatXfPlug(Falloff);
  DeclareFloatXfPlug(VortexStrength);
  DeclareFloatXfPlug(OutwardStrength);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  DeclarePoolOutPlug(Output);

  dataflow::outplugbase* GetOutput(int idx) const final {
    return &mPlugOutOutput;
  }

  //////////////////////////////////////////////////


};

///////////////////////////////////////////////////////////////////////////////

struct RendererModule : public ParticleModule {
  DeclareAbstractX(RendererModule, ParticleModule);

public:
  RendererModule();

  virtual void Render(
      const fmtx4& mtx,
      const ork::lev2::RenderContextInstData& rcid,
      const ParticlePoolRenderBuffer& buffer,
      ork::lev2::Context* targ) = 0;

  const Pool<BasicParticle>* GetPool();

protected:
  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) const override {
    return &mPlugInpInput;
  }

  DeclarePoolInpPlug(Input);

  //////////////////////////////////////////////////

};

///////////////////////////////////////////////////////////////////////////////

class MaterialBase : public ork::Object {
  DeclareAbstractX(MaterialBase, ork::Object);

public:
  virtual lev2::GfxMaterial* Bind(lev2::Context* pT) = 0;
  virtual void Update(float ftexframe)               = 0;

protected:
  GfxMaterial3DSolid* mpMaterial;
  MaterialBase()
      : mpMaterial(0) {
  }
};

class TextureMaterial : public MaterialBase {
  DeclareConcreteX(TextureMaterial, MaterialBase);

public:
  TextureMaterial();

  void SetTextureAccessor(ork::rtti::ICastable* const& tex);
  void GetTextureAccessor(ork::rtti::ICastable*& tex) const;
  ork::lev2::Texture* GetTexture() const;

private:
  void Update(float ftexframe) final;
  ork::lev2::TextureAsset* mTexture;
  lev2::GfxMaterial* Bind(lev2::Context* pT) final;
};

class VolTexMaterial : public MaterialBase {
  DeclareConcreteX(VolTexMaterial, MaterialBase);

public:
  VolTexMaterial();

  void SetTextureAccessor(ork::rtti::ICastable* const& tex);
  void GetTextureAccessor(ork::rtti::ICastable*& tex) const;
  ork::lev2::Texture* GetTexture() const;

private:
  void Update(float ftexframe) final;

  ork::lev2::TextureAsset* mTexture;
  lev2::GfxMaterial* Bind(lev2::Context* pT) final;
};

///////////////////////////////////////////////////////////////////////////////

struct SpriteRenderer : public RendererModule {

  DeclareConcreteX(SpriteRenderer, RendererModule);

public:
  SpriteRenderer();

  void DrawParticle(const ork::lev2::particle::BasicParticle* ptcl);

  void Compute(float dt) final;
  void Render(
      const fmtx4& mtx,
      const ork::lev2::RenderContextInstData& rcid,
      const ParticlePoolRenderBuffer& buffer,
      ork::lev2::Context* targ) final;
  void DoLink() final;

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) const final;

  DeclareFloatXfPlug(Size);
  DeclareFloatXfPlug(Rot);
  DeclareFloatXfPlug(AnimFrame);
  DeclareFloatXfPlug(GradientIntensity);

  DeclareFloatXfPlug(NoiseAmp0);
  DeclareFloatXfPlug(NoiseAmp1);
  DeclareFloatXfPlug(NoiseAmp2);
  DeclareFloatXfPlug(NoiseFreq0);
  DeclareFloatXfPlug(NoiseFreq1);
  DeclareFloatXfPlug(NoiseFreq2);
  DeclareFloatXfPlug(NoiseShift0);
  DeclareFloatXfPlug(NoiseShift1);
  DeclareFloatXfPlug(NoiseShift2);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  DeclareFloatOutPlug(UnitAge);
  DeclareFloatOutPlug(PtcRandom);
  dataflow::outplugbase* GetOutput(int idx) const final;

  //////////////////////////////////////////////////

  Blending meBlendMode              = Blending::ALPHA;
  ParticleItemAlignment meAlignment = ParticleItemAlignment::BILLBOARD;
  bool mbSort                       = false;
  int miImageFrame                  = 0;
  float mCurFGI                     = 0.0f;
  int miTexCnt                      = 0;
  float mfTexs                      = 0.0f;
  int miAnimTexDim                  = 1;

  ork::fvec2 uvr0;
  ork::fvec2 uvr1;
  ork::fvec2 uvr2;
  ork::fvec2 uvr3;
  ork::fvec3 NX_NY;
  ork::fvec3 PX_NY;
  ork::fvec3 PX_PY;
  ork::fvec3 NX_PY;
  ork::lev2::CVtxBuffer<ork::lev2::SVtxV12C4T16>* mpVB;

  //////////////////////////////////////////////////

  orklut<PoolString, ork::Object*> mGradients;
  orklut<PoolString, ork::Object*> mMaterials;
  PoolString mActiveGradient;
  PoolString mActiveMaterial;
};

///////////////////////////////////////////////////////////////////////////////

struct StreakRenderer : public RendererModule {
  DeclareConcreteX(StreakRenderer, RendererModule);

public:

  StreakRenderer();

  ork::Object* GradientAccessor() {
    return &mGradient;
  }
  void SetTextureAccessor(ork::rtti::ICastable* const& tex);
  void GetTextureAccessor(ork::rtti::ICastable*& tex) const;

  void Compute(float dt) final {
  }

  void Render(
      const fmtx4& mtx,
      const ork::lev2::RenderContextInstData& rcid,
      const ParticlePoolRenderBuffer& buffer,
      ork::lev2::Context* targ) final;

  ork::lev2::Texture* GetTexture() const;

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) const final;

  DeclareFloatXfPlug(Length);
  DeclareFloatXfPlug(Width);
  DeclareFloatOutPlug(UnitAge);
  DeclareFloatXfPlug(GradientIntensity);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  dataflow::outplugbase* GetOutput(int idx) const final;

  //////////////////////////////////////////////////

  ork::lev2::TextureAsset* mTexture = nullptr;
  GfxMaterial3DSolid* mpMaterial = nullptr;

  ork::lev2::Blending meBlendMode = Blending::ALPHA;
  bool mbSort = false;

  ork::Gradient<ork::fvec4> mGradient;
  ork::fvec4 mAlphaMux;

};

///////////////////////////////////////////////////////////////////////////////

struct ModelRenderer : public RendererModule {
  DeclareConcreteX(ModelRenderer, RendererModule);

public:

  ModelRenderer();

  void SetModelAccessor(ork::rtti::ICastable* const& tex);
  void GetModelAccessor(ork::rtti::ICastable*& tex) const;

  void Compute(float dt) final {
  }
  void Render(
      const fmtx4& mtx,
      const ork::lev2::RenderContextInstData& rcid,
      const ParticlePoolRenderBuffer& buffer,
      ork::lev2::Context* targ) final;

  ork::lev2::XgmModel* GetModel() const;

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) const final;

  DeclareFloatXfPlug(AnimScale);
  DeclareFloatXfPlug(AnimRot);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  dataflow::outplugbase* GetOutput(int idx) const final;

  DeclareFloatOutPlug(UnitAge);

  //////////////////////////////////////////////////

  ork::lev2::XgmModelAsset* mModel = nullptr;
  fvec3 mUpVector;
  fvec4 mBaseRotAxisAngle;
  fvec3 mAnimRotAxis;
};

///////////////////////////////////////////////////////////////////////////////

struct psys_graph_pool;

///////////////////////////////////////////////////////////////////////////////

struct psys_graph : public ork::dataflow::graph_inst {
  friend struct psys_graph_pool;

  DeclareAbstractX(psys_graph, ork::dataflow::graph_inst);

  bool CanConnect(const ork::dataflow::inplugbase* pin, const ork::dataflow::outplugbase* pout) const final;
  ork::dataflow::dgregisterblock mdflowregisters;
  ork::dataflow::dgcontext mdflowctx;
  bool mbEmitEnable;
  float mfElapsed;

  bool Query(event::Event* event) const; // virtual

public:
  psys_graph();
  ~psys_graph();
  void operator=(const psys_graph& oth);
  void compute();
  void Update(ork::lev2::particle::Context* pctx, float fdt);
  void Reset(ork::lev2::particle::Context* pctx);
  bool GetEmitEnable() const {
    return mbEmitEnable;
  }
  void SetEmitEnable(bool bv) {
    mbEmitEnable = bv;
  }

  void PrepForStart();
};

///////////////////////////////////////////////////////////////////////////////

struct psys_graph_pool : public ork::Object {
  DeclareConcreteX(psys_graph_pool, ork::Object);

public:
  psys_graph_pool();

  psys_graph* Allocate();
  void Free(psys_graph*);

  void BindTemplate(const psys_graph& InTemplate);

  const psys_graph* mNewTemplate = nullptr; // deprecated
  ork::pool<psys_graph>* mGraphPool = nullptr;
  int miPoolSize = 1;
};

///////////////////////////////////////////////////////////////////////////////

// int								miAnimTexDim;
// int								miImgSequenceBegin;
// int								miImgSequenceEnd;
// bool							mbImageSequence;
// bool							mbImageSequenceOK;
// std::vector<ork::lev2::TextureAsset*>	mSequenceTextures;

// ork::Object* GradientAccessor() { return & mGradient; }
// void SetTextureAccessor( ork::rtti::ICastable* const & tex);
// void GetTextureAccessor( ork::rtti::ICastable* & tex) const;
// void SetVolumeTextureAccessor( ork::rtti::ICastable* const & tex);
// void GetVolumeTextureAccessor( ork::rtti::ICastable* & tex) const;

} // namespace ork::lev2::particle

#endif