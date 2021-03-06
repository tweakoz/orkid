////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
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

namespace ork::lev2 {
class RenderContextInstData;
class GfxMaterial3DSolid;
class TextureAsset;
class XgmModelAsset;
} // namespace ork::lev2

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define DeclarePoolOutPlug(name)                                                                                                   \
  psys_ptclbuf OutDataName(name);                                                                                                  \
  PtclBufOutPlug OutPlugName(name);                                                                                                \
  ork::Object* OutAccessor##name() {                                                                                               \
    return &OutPlugName(name);                                                                                                     \
  }

#define DeclarePoolInpPlug(name)                                                                                                   \
  PtclBufInpPlug InpPlugName(name);                                                                                                \
  ork::Object* InpAccessor##name() {                                                                                               \
    return &InpPlugName(name);                                                                                                     \
  }

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
  DeclareFloatOutPlug(Noise);
  DeclareFloatOutPlug(FastNoise);
  DeclareFloatOutPlug(SlowNoise);
  DeclareVect3OutPlug(RandomNormal);

  dataflow::inplugbase* GetInput(int idx) final {
    return &mPlugInpTimeScale;
  }
  dataflow::outplugbase* GetOutput(int idx) final;
  void Compute(float dt) final;
  void OnStart() final;

  float mfNoiseRat;
  float mfSlowNoiseRat;
  float mfFastNoiseRat;
  float mfNoisePrv;
  float mfNoiseNew;
  float mfSlowNoisePrv;
  float mfFastNoisePrv;
  float mfNoiseBas;
  float mfSlowNoiseBas;
  float mfFastNoiseBas;
  float mfNoiseTim;
  float mfSlowNoiseTim;
  float mfFastNoiseTim;
  float mfTimeBase;

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
  dataflow::inplugbase* GetInput(int idx) final {
    return 0;
  } // virtual
  // data currently only flows in from externals
  /////////////////////////////////////////////////////

  int GetNumOutputs() const final;
  dataflow::outplugbase* GetOutput(int idx) final; // virtual

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
  dataflow::inplugbase* GetInput(int idx) final {
    return 0;
  }
  // data currently only flows in from externals
  /////////////////////////////////////////////////////

  int GetNumOutputs() const final;
  dataflow::outplugbase* GetOutput(int idx) final;

  void Compute(float dt) final {
  }

public:
  ExtConnector();
  void BindConnector(dataflow::dyn_external* pconnector);
};

///////////////////////////////////////////////////////////////////////////////

class ParticleModule : public Module {
  DeclareAbstractX(ParticleModule, Module);
  bool IsDirty(void) const {
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

class ParticlePool : public ParticleModule {
  DeclareConcreteX(ParticlePool, ParticleModule);

  DeclareFloatXfPlug(PathInterval);
  DeclareFloatXfPlug(PathProbability);
  DeclarePoolOutPlug(Output);
  DeclareFloatOutPlug(UnitAge);
  Pool<BasicParticle> mPoolOutput;

  int miPoolSize;
  lev2::particle::EventQueue* mPathStochasticEventQueue;
  PoolString mPathStochasticQueueID;
  Char4 mPathStochasticQueueID4;
  lev2::particle::EventQueue* mPathIntervalEventQueue;
  PoolString mPathIntervalQueueID;
  Char4 mPathIntervalQueueID4;

  /////////////////////////////////////////////////
  dataflow::inplugbase* GetInput(int idx) final;
  /////////////////////////////////////////////////
  ork::dataflow::outplugbase* GetOutput(int idx) final;
  /////////////////////////////////////////////////
  void Compute(float dt) final;
  void Reset() final;
  void DoLink() final;

public:
  ParticlePool();
  const Pool<BasicParticle>& GetPool() const {
    return mPoolOutput;
  }
};

struct ParticlePoolRenderBuffer {
  BasicParticle* mpParticles;
  int miMaxParticles;
  int miNumParticles;
  ParticlePoolRenderBuffer();
  ~ParticlePoolRenderBuffer();
  void Update(const Pool<BasicParticle>& pool);
  void SetCapacity(int inum);
};

///////////////////////////////////////////////////////////////////////////////

class RingEmitter;

class RingDirectedEmitter : public DirectedEmitter {
  RingEmitter& mEmitterModule;
  void ComputePosDir(float fi, fvec3& pos, fvec3& dir);

public:
  RingDirectedEmitter(RingEmitter& EmitterModule)
      : mEmitterModule(EmitterModule) {
  }
  fvec3 mUserDir;
};

class RingEmitter : public ParticleModule {
  friend class RingDirectedEmitter;

  DeclareConcreteX(RingEmitter, ParticleModule);

  float mfPhase, mfPhase2;
  float mfLastRadius, mfThisRadius;
  float mfAccumTime;
  RingDirectedEmitter mDirectedEmitter;
  EmitterDirection meDirection;
  EmitterCtx mEmitterCtx;
  ////////////////////////////////////////////////////////////////
  lev2::particle::EventQueue* mDeathEventQueue;
  PoolString mDeathQueueID;
  Char4 mDeathQueueID4;
  ////////////////////////////////////////////////////////////////

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) final;

  DeclarePoolInpPlug(Input) DeclareFloatXfPlug(Lifespan);
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

  dataflow::outplugbase* GetOutput(int idx) final;

  DeclarePoolOutPlug(Output)

      //////////////////////////////////////////////////

      void DoLink() final;
  void Compute(float dt) final;

  void Emit(float fdt);
  void Reap(float fdt);
  void Reset() final;

public:
  RingEmitter();
};

///////////////////////////////////////////////////////////////////////////////

class NozzleEmitter;

class NozzleDirectedEmitter : public DirectedEmitter {
  NozzleEmitter& mEmitterModule;
  void ComputePosDir(float fi, fvec3& pos, fvec3& dir);

public:
  NozzleDirectedEmitter(NozzleEmitter& EmitterModule)
      : mEmitterModule(EmitterModule) {
  }
};

class NozzleEmitter : public ParticleModule {
  friend class NozzleDirectedEmitter;

  DeclareConcreteX(NozzleEmitter, ParticleModule);

  float mfAccumTime;
  Pool<BasicParticle> mDeadPool;
  EmitterCtx mEmitterCtx;
  NozzleDirectedEmitter mDirectedEmitter;
  fvec3 mLastPosition;
  fvec3 mLastDirection;
  fvec3 mDirectionSample;
  fvec3 mOffsetSample;

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) final;

  DeclarePoolInpPlug(Input) DeclareFloatXfPlug(Lifespan);
  DeclareFloatXfPlug(EmissionRate);
  DeclareFloatXfPlug(EmissionVelocity);
  DeclareFloatXfPlug(DispersionAngle);
  DeclareVect3XfPlug(Offset);
  DeclareVect3XfPlug(Direction);
  DeclareVect3XfPlug(OffsetVelocity);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  dataflow::outplugbase* GetOutput(int idx) final;

  DeclarePoolOutPlug(Output) DeclarePoolOutPlug(TheDead)

      //////////////////////////////////////////////////

      void Compute(float dt) final;

  void Emit(float fdt);
  void Reap(float fdt);
  void Reset() final;

public:
  NozzleEmitter();
};

///////////////////////////////////////////////////////////////////////////////

class ReEmitter;

class ReDirectedEmitter : public DirectedEmitter {
  ReEmitter& mEmitterModule;
  void ComputePosDir(float fi, fvec3& pos, fvec3& dir);

public:
  ReDirectedEmitter(ReEmitter& EmitterModule)
      : mEmitterModule(EmitterModule) {
  }
};

class ReEmitter : public ParticleModule {
  friend class PoolDirectedEmitter;

  DeclareConcreteX(ReEmitter, ParticleModule);

  ReDirectedEmitter mDirectedEmitter;
  EmitterDirection meDirection;
  EmitterCtx mEmitterCtx;
  lev2::particle::EventQueue* mSpawnEventQueue;
  lev2::particle::EventQueue* mDeathEventQueue;
  PoolString mSpawnQueueID;
  PoolString mDeathQueueID;
  Char4 mSpawnQueueID4;
  Char4 mDeathQueueID4;
  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) final;

  DeclarePoolInpPlug(Input) DeclareFloatXfPlug(SpawnProbability);
  DeclareFloatXfPlug(SpawnMultiplier);
  DeclareFloatXfPlug(Lifespan);
  DeclareFloatXfPlug(EmissionRate);
  DeclareFloatXfPlug(EmissionVelocity);
  DeclareFloatXfPlug(DispersionAngle);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  dataflow::outplugbase* GetOutput(int idx) final;

  DeclarePoolOutPlug(Output)

      //////////////////////////////////////////////////

      void DoLink() final;
  void Compute(float dt) final;

  void Emit(float fdt);
  void Reap(float fdt);

public:
  ReEmitter();
};

///////////////////////////////////////////////////////////////////////////////

class WindModule : public ParticleModule {
  DeclareConcreteX(WindModule, ParticleModule);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) final;

  DeclarePoolInpPlug(Input) DeclareFloatXfPlug(Force);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  dataflow::outplugbase* GetOutput(int idx) final {
    return &mPlugOutOutput;
  }

  DeclarePoolOutPlug(Output)

      //////////////////////////////////////////////////

      void Compute(float dt) final;

public:
  WindModule();
};

///////////////////////////////////////////////////////////////////////////////

class GravityModule : public ParticleModule {
  DeclareConcreteX(GravityModule, ParticleModule);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) final;

  DeclarePoolInpPlug(Input) DeclareFloatXfPlug(Mass);
  DeclareFloatXfPlug(OthMass);
  DeclareFloatXfPlug(G);
  DeclareFloatXfPlug(MinDistance);
  DeclareVect3XfPlug(Center)

      //////////////////////////////////////////////////
      // outputs
      //////////////////////////////////////////////////

      dataflow::outplugbase* GetOutput(int idx) final {
    return &mPlugOutOutput;
  }

  DeclarePoolOutPlug(Output)

      //////////////////////////////////////////////////

      void Compute(float dt) final;

public:
  GravityModule();
};

///////////////////////////////////////////////////////////////////////////////

class PlanarColliderModule : public ParticleModule {
  DeclareConcreteX(PlanarColliderModule, ParticleModule);

  int miDiodeDirection;

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) final;

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

  dataflow::outplugbase* GetOutput(int idx) final {
    return &mPlugOutOutput;
  }

  DeclarePoolOutPlug(Output)

      //////////////////////////////////////////////////

      void Compute(float dt) final;

public:
  PlanarColliderModule();
};

///////////////////////////////////////////////////////////////////////////////

class SphericalColliderModule : public ParticleModule {
  DeclareConcreteX(SphericalColliderModule, ParticleModule);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) final;

  DeclarePoolInpPlug(Input);
  DeclareFloatXfPlug(CenterX);
  DeclareFloatXfPlug(CenterY);
  DeclareFloatXfPlug(CenterZ);
  DeclareFloatXfPlug(Radius);
  DeclareFloatXfPlug(Absorbtion);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  dataflow::outplugbase* GetOutput(int idx) final {
    return &mPlugOutOutput;
  }

  DeclarePoolOutPlug(Output)

      //////////////////////////////////////////////////

      void Compute(float dt) final;

public:
  SphericalColliderModule();
};

///////////////////////////////////////////////////////////////////////////////

enum class PSYS_FLOATOP {
  ADD = 0,
  SUB,
  MUL,
};

class FloatOp2Module : public ParticleModule {
  DeclareConcreteX(FloatOp2Module, ParticleModule);

  PSYS_FLOATOP meOp;

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) final;

  DeclareFloatXfPlug(InputA) DeclareFloatXfPlug(InputB);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  dataflow::outplugbase* GetOutput(int idx) final {
    return &mPlugOutOutput;
  }

  DeclareFloatOutPlug(Output);

  //////////////////////////////////////////////////

  void Compute(float dt) final;

public:
  FloatOp2Module();
};

///////////////////////////////////////////////////////////////////////////////

enum class PSYS_VEC3OP {
  ADD = 0,
  SUB,
  MUL,
  DOT,
  CROSS,
};

class Vec3Op2Module : public ParticleModule {
  DeclareConcreteX(Vec3Op2Module, ParticleModule);

  PSYS_VEC3OP meOp;

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) final;

  DeclareVect3XfPlug(InputA) DeclareVect3XfPlug(InputB);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  dataflow::outplugbase* GetOutput(int idx) final {
    return &mPlugOutOutput;
  }

  DeclareVect3OutPlug(Output);

  //////////////////////////////////////////////////

  void Compute(float dt) final;

public:
  Vec3Op2Module();
};

///////////////////////////////////////////////////////////////////////////////

class Vec3SplitModule : public ParticleModule {
  DeclareConcreteX(Vec3SplitModule, ParticleModule);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) final;

  DeclareVect3XfPlug(Input);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  dataflow::outplugbase* GetOutput(int idx) final {
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

  void Compute(float dt) final;

public:
  Vec3SplitModule();
};

///////////////////////////////////////////////////////////////////////////////

class DecayModule : public ParticleModule {
  DeclareConcreteX(DecayModule, ParticleModule);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) final;

  DeclarePoolInpPlug(Input) DeclareFloatXfPlug(Decay);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  dataflow::outplugbase* GetOutput(int idx) final {
    return &mPlugOutOutput;
  }

  DeclarePoolOutPlug(Output)

      //////////////////////////////////////////////////

      void Compute(float dt) final;

public:
  DecayModule();
};

///////////////////////////////////////////////////////////////////////////////

class TurbulenceModule : public ParticleModule {
  DeclareConcreteX(TurbulenceModule, ParticleModule);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) final;

  DeclareFloatXfPlug(AmountX);
  DeclareFloatXfPlug(AmountY);
  DeclareFloatXfPlug(AmountZ);
  DeclarePoolInpPlug(Input)

      //////////////////////////////////////////////////
      // outputs
      //////////////////////////////////////////////////

      DeclarePoolOutPlug(Output)

          dataflow::outplugbase* GetOutput(int idx) final {
    return &mPlugOutOutput;
  }

  //////////////////////////////////////////////////

  void Compute(float dt) final;

public:
  TurbulenceModule();
};

///////////////////////////////////////////////////////////////////////////////

class VortexModule : public ParticleModule {
  DeclareConcreteX(VortexModule, ParticleModule);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) final;

  DeclareFloatXfPlug(Falloff);
  DeclareFloatXfPlug(VortexStrength);
  DeclareFloatXfPlug(OutwardStrength);
  DeclarePoolInpPlug(Input)

      //////////////////////////////////////////////////
      // outputs
      //////////////////////////////////////////////////

      DeclarePoolOutPlug(Output)

          dataflow::outplugbase* GetOutput(int idx) final {
    return &mPlugOutOutput;
  }

  //////////////////////////////////////////////////

  void Compute(float dt) final;

public:
  VortexModule();
};

///////////////////////////////////////////////////////////////////////////////

class RendererModule : public ParticleModule {
  DeclareAbstractX(RendererModule, ParticleModule);

protected:
  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) override {
    return &mPlugInpInput;
  }

  DeclarePoolInpPlug(Input)

      //////////////////////////////////////////////////

      public :

      RendererModule();

  virtual void Render(
      const fmtx4& mtx,
      const ork::lev2::RenderContextInstData& rcid,
      const ParticlePoolRenderBuffer& buffer,
      ork::lev2::Context* targ) = 0;

  const Pool<BasicParticle>* GetPool();
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

class SpriteRenderer : public RendererModule {
  DeclareConcreteX(SpriteRenderer, RendererModule);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) final;

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
  dataflow::outplugbase* GetOutput(int idx) final;

  //////////////////////////////////////////////////

  void DrawParticle(const ork::lev2::particle::BasicParticle* ptcl);
  ork::fvec2 uvr0;
  ork::fvec2 uvr1;
  ork::fvec2 uvr2;
  ork::fvec2 uvr3;
  ork::fvec3 NX_NY;
  ork::fvec3 PX_NY;
  ork::fvec3 PX_PY;
  ork::fvec3 NX_PY;
  float mCurFGI;
  int miTexCnt;
  float mfTexs;
  int miAnimTexDim;
  ork::lev2::CVtxBuffer<ork::lev2::SVtxV12C4T16>* mpVB;

  //////////////////////////////////////////////////

  orklut<PoolString, ork::Object*> mGradients;
  orklut<PoolString, ork::Object*> mMaterials;
  PoolString mActiveGradient;
  PoolString mActiveMaterial;

  ork::lev2::Blending meBlendMode;
  ParticleItemAlignment meAlignment;
  bool mbSort;
  int miImageFrame;

  void Compute(float dt) final;
  void Render(
      const fmtx4& mtx,
      const ork::lev2::RenderContextInstData& rcid,
      const ParticlePoolRenderBuffer& buffer,
      ork::lev2::Context* targ) final;
  void DoLink() final;
  // void doNotify(const ork::event::Event* event) final;

public:
  SpriteRenderer();
};

///////////////////////////////////////////////////////////////////////////////

class StreakRenderer : public RendererModule {
  DeclareConcreteX(StreakRenderer, RendererModule);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) final;

  DeclareFloatXfPlug(Length);
  DeclareFloatXfPlug(Width);
  DeclareFloatOutPlug(UnitAge);
  DeclareFloatXfPlug(GradientIntensity);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  dataflow::outplugbase* GetOutput(int idx) final;

  //////////////////////////////////////////////////

  ork::Gradient<ork::fvec4> mGradient;
  ork::lev2::Blending meBlendMode;
  GfxMaterial3DSolid* mpMaterial;
  ork::lev2::TextureAsset* mTexture;
  bool mbSort;
  ork::fvec4 mAlphaMux;

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

public:
  StreakRenderer();
};

///////////////////////////////////////////////////////////////////////////////

class ModelRenderer : public RendererModule {
  DeclareConcreteX(ModelRenderer, RendererModule);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  dataflow::inplugbase* GetInput(int idx) final;

  DeclareFloatXfPlug(AnimScale);
  DeclareFloatXfPlug(AnimRot);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  dataflow::outplugbase* GetOutput(int idx) final;

  DeclareFloatOutPlug(UnitAge);

  //////////////////////////////////////////////////

  ork::lev2::XgmModelAsset* mModel;
  fvec3 mUpVector;
  fvec4 mBaseRotAxisAngle;
  fvec3 mAnimRotAxis;

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

public:
  ModelRenderer();
};

///////////////////////////////////////////////////////////////////////////////

class psys_graph_pool;

///////////////////////////////////////////////////////////////////////////////

class psys_graph : public ork::dataflow::graph_inst {
  friend class psys_graph_pool;

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

class psys_graph_pool : public ork::Object {
  DeclareConcreteX(psys_graph_pool, ork::Object);

  const psys_graph* mNewTemplate; // deprecated
  ork::pool<psys_graph>* mGraphPool;
  int miPoolSize;

public:
  psys_graph_pool();

  psys_graph* Allocate();
  void Free(psys_graph*);

  void BindTemplate(const psys_graph& InTemplate);
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
