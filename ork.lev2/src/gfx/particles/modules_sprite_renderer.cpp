////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/gfx/particle/modular_forces.h>
#include <ork/lev2/gfx/particle/modular_renderers.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>

using namespace ork::dataflow;

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::particle {
/////////////////////////////////////////

struct SpriteRendererInst : public DgModuleInst {

//  DeclareConcreteX(SpriteRenderer, RendererModule);

public:
  SpriteRendererInst( const SpriteRendererData* smd )
    : DgModuleInst(smd){

}

  void onLink(GraphInst* inst) final{

  }
  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final{

  }


  /*void DrawParticle(const ork::lev2::particle::BasicParticle* ptcl);

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
  PoolString mActiveMaterial;*/
};


void RendererModuleData::describeX(class_t* clazz) {
}

RendererModuleData::RendererModuleData(){

}

//////////////////////////////////////////////////////////////////////////

void SpriteRendererData::describeX(class_t* clazz) {
}

//////////////////////////////////////////////////////////////////////////

SpriteRendererData::SpriteRendererData() {
}

//////////////////////////////////////////////////////////////////////////

std::shared_ptr<SpriteRendererData> SpriteRendererData::createShared() {
  auto data = std::make_shared<SpriteRendererData>();

  createInputPlug<ParticleBufferPlugTraits>(data, EPR_UNIFORM, "ParticleBuffer");
  //createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "LifeSpan");
  //createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "EmissionRate");
  //createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "EmissionVelocity");
  //createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "DispersionAngle");
  //createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "Direction");
  //createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "Offset");
  //createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "OffsetVelocity");
  //createOutputPlug<ParticleBufferPlugTraits>(data, EPR_UNIFORM, "ParticleBuffer");
  return data;
}

//////////////////////////////////////////////////////////////////////////

dgmoduleinst_ptr_t SpriteRendererData::createInstance() const {
  return std::make_shared<SpriteRendererInst>(this);
}

/////////////////////////////////////////
} // namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::RendererModuleData, "psys::RendererModuleData");
ImplementReflectionX(ptcl::SpriteRendererData, "psys::SpriteRendererData");
