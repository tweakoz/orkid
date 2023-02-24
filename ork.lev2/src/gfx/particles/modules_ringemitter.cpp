////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/gfx/particle/modular_emitters.h>

namespace dflow = ::ork::dataflow;
namespace ork::lev2::particle {

///////////////////////////////////////////////////////////////////////////////

struct RingEmitterInst : public dflow::DgModuleInst {

  RingEmitterInst(const RingEmitterData* rmd);

  // void DoLink() final;
  // void Compute(float dt) final;
  // void Emit(float fdt);
  // void Reap(float fdt);
  // void Reset() final;

  // lev2::particle::EventQueue* mDeathEventQueue = nullptr;
  // float mfPhase = 0.0f;
  // float mfPhase2 = 0.0f;
  // float mfLastRadius = 0.0f;
  // float mfThisRadius = 0.0f;
  // float mfAccumTime = 0.0f;
  // RingDirectedEmitter mDirectedEmitter;
  // EmitterDirection meDirection = EmitterDirection::VEL;
  // EmitterCtx mEmitterCtx;
  ////////////////////////////////////////////////////////////////
  // PoolString mDeathQueueID;
  // Char4 mDeathQueueID4;

  float mfPhase      = 0.0f;
  float mfPhase2     = 0.0f;
  float mfLastRadius = 0.0f;
  float mfThisRadius = 0.0f;
  float mfAccumTime  = 0.0f;
};

using ringemitterinst_ptr_t = std::shared_ptr<RingEmitterInst>;

///////////////////////////////////////////////////////////////////////////////

RingEmitterInst::RingEmitterInst(const RingEmitterData* rmd)
    : dflow::DgModuleInst(rmd) {
}

///////////////////////////////////////////////////////////////////////////////

struct RingDirectedEmitter : public DirectedEmitter {
  RingDirectedEmitter(ringemitterinst_ptr_t module);
  void computePosDir(float fi, fvec3& pos, fvec3& dir);
  ringemitterinst_ptr_t _emitterModule;
  fvec3 mUserDir;
};

///////////////////////////////////////////////////////////////////////////////

RingDirectedEmitter::RingDirectedEmitter(ringemitterinst_ptr_t module)
    : _emitterModule(module) {
}

///////////////////////////////////////////////////////////////////////////////

void RingDirectedEmitter::computePosDir(float fi, fvec3& pos, fvec3& dir) {
  float scaler = (fi * _emitterModule->mfThisRadius) + ((1.0f - fi) * _emitterModule->mfLastRadius);
  float phase  = (fi * _emitterModule->mfPhase2) + ((1.0f - fi) * _emitterModule->mfPhase);
  float fpx    = cosf(phase);
  float fpz    = sinf(phase);
  float fdx    = cosf(phase + PI_DIV_2);
  float fdz    = sinf(phase + PI_DIV_2);
  pos          = fvec3((fpx * scaler), 0.0f, (fpz * scaler));
  if (meDirection == EmitterDirection::USER) {
    dir = mUserDir;
  } else {
    dir = fvec3(fdx, 0.0f, fdz);
  }
}

///////////////////////////////////////////////////////////////////////////////

void RingEmitterData::describeX(class_t* clazz) {

}

RingEmitterData::RingEmitterData() {

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  // dataflow::inplugbase* GetInput(int idx) const final;
  // DeclarePoolInpPlug(Input);
  // DeclareFloatXfPlug(Lifespan);
  // DeclareFloatXfPlug(EmissionRadius);
  // DeclareFloatXfPlug(EmissionRate);
  // DeclareFloatXfPlug(EmissionVelocity);
  // DeclareFloatXfPlug(EmitterSpinRate);
  // DeclareFloatXfPlug(DispersionAngle);
  // DeclareFloatXfPlug(OffsetX);
  // DeclareFloatXfPlug(OffsetY);
  // DeclareFloatXfPlug(OffsetZ);
  // DeclareVect3XfPlug(Direction);

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  // dataflow::outplugbase* GetOutput(int idx) const final;
  // DeclarePoolOutPlug(Output);
}

std::shared_ptr<RingEmitterData> RingEmitterData::createShared() {
    auto data = std::make_shared<RingEmitterData>();
    createOutputPlug<ParticleBufferPlugTraits>(data, dflow::EPR_UNIFORM, "ParticleBuffer");
    return data;
}

dflow::dgmoduleinst_ptr_t RingEmitterData::createInstance() const {
  return std::make_shared<RingEmitterInst>(this);
}

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::RingEmitterData, "psys::RingEmitterData");
