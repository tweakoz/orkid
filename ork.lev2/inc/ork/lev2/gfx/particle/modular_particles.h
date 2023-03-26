////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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


} // namespace ork::lev2::particle

#endif