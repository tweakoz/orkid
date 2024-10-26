////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/gfx/particle/modular_emitters.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>

using namespace ork::dataflow;
namespace ork::lev2::particle {

struct RingEmitterInst;

struct RingDirectedEmitter : public DirectedEmitter {
  RingDirectedEmitter(RingEmitterInst* module);
  void computePosDir(float fi, fvec3& pos, fmtx3& basis);
  RingEmitterInst* _emitterModule;
  fvec3 mUserDir;
};

///////////////////////////////////////////////////////////////////////////////

struct RingEmitterInst : public ParticleModuleInst {

  RingEmitterInst(const RingEmitterData* rmd, dataflow::GraphInst* ginst);

  void onLink(GraphInst* inst) final;
  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final;
  void _emit(float fdt);
  void _reap(float fdt);

  // lev2::particle::EventQueue* mDeathEventQueue = nullptr;
  // float mfPhase = 0.0f;
  // float mfPhase2 = 0.0f;
  // float mfLastRadius = 0.0f;
  // float mfThisRadius = 0.0f;
  // float _timeAccumulator = 0.0f;
  // RingDirectedEmitter _directedEmitter;
  // EmitterDirection meDirection = EmitterDirection::VEL;
  // EmitterCtx _emitter_context;
  ////////////////////////////////////////////////////////////////
  // PoolString mDeathQueueID;
  // Char4 mDeathQueueID4;

  float mfPhase      = 0.0f;
  float mfPhase2     = 0.0f;
  float mfLastRadius = 0.0f;
  float mfThisRadius = 0.0f;
  float _timeAccumulator  = 0.0f;

  floatxf_inp_pluginst_ptr_t _input_lifespan;
  floatxf_inp_pluginst_ptr_t _input_emissionrate;
  floatxf_inp_pluginst_ptr_t _input_emissionvelocity;
  floatxf_inp_pluginst_ptr_t _input_dispersionangle;
  floatxf_inp_pluginst_ptr_t _input_emissionradius;
  floatxf_inp_pluginst_ptr_t _input_emitterspinrate;

  fvec3xf_inp_pluginst_ptr_t _input_direction;
  fvec3xf_inp_pluginst_ptr_t _input_offset;

  RingDirectedEmitter _directedEmitter;
  EmitterCtx _emitter_context;
};

///////////////////////////////////////////////////////////////////////////////

RingEmitterInst::RingEmitterInst(const RingEmitterData* rmd, dataflow::GraphInst* ginst)
    : ParticleModuleInst(rmd, ginst)
    , _directedEmitter(this) {
}
///////////////////////////////////////////////////////////////////////////////
void RingEmitterInst::onLink(GraphInst* inst) {
  _onLink(inst);
  _input_lifespan         = typedInputNamed<FloatXfPlugTraits>("LifeSpan");
  _input_emissionrate     = typedInputNamed<FloatXfPlugTraits>("EmissionRate");
  _input_emissionvelocity = typedInputNamed<FloatXfPlugTraits>("EmissionVelocity");
  _input_dispersionangle  = typedInputNamed<FloatXfPlugTraits>("DispersionAngle");
  _input_emissionradius   = typedInputNamed<FloatXfPlugTraits>("EmissionRadius");
  _input_emitterspinrate  = typedInputNamed<FloatXfPlugTraits>("EmitterSpinRate");
  _input_direction        = typedInputNamed<Vec3XfPlugTraits>("Direction");
  _input_offset           = typedInputNamed<Vec3XfPlugTraits>("Offset");
}
///////////////////////////////////////////////////////////////////////////////
void RingEmitterInst::compute(GraphInst* inst, ui::updatedata_ptr_t updata) {

  if(_pool == nullptr)
    return;

  _timeAccumulator += updata->_dt;
  const float fstep = updata->_dt; 

  if (_timeAccumulator >= fstep) { // limit to 30hz
    float fdelta = fstep;
    _timeAccumulator -= fstep;
      _reap(fdelta);
      _emit(fdelta);    
  }
  if (_timeAccumulator < 0.01f) {
    _timeAccumulator = 0.0f;
  }
  _pool->updateUnitAges();
}
///////////////////////////////////////////////////////////////////////////////
void RingEmitterInst::_emit(float fdt) {
  float femitvel                      = _input_emissionvelocity->value();
  float lifespan                      = std::clamp<float>(_input_lifespan->value(), 0.01f, 10.0f);
  float emissionrate                  = _input_emissionrate->value();
  _emitter_context.mPool              = _pool.get();

  float scaler              = _input_emissionradius->value();
  float fspr                = _input_emitterspinrate->value() * PI2;
  float fadaptive           = fabs((mfPhase2 - mfPhase) / fdt);
  if (fadaptive == 0.0f)
    fadaptive = 1.0f;
  fadaptive                      = std::clamp(fadaptive, 0.1f, 1.0f);
  _emitter_context.mfEmissionRate     = emissionrate * fadaptive;
  _emitter_context.mKey               = (void*)this;
  _emitter_context.mfLifespan         = lifespan;
  _emitter_context.mfDeltaTime        = fdt;
  _emitter_context.mfEmissionVelocity = femitvel;
  _emitter_context.mDispersion        = _input_dispersionangle->value();
  _directedEmitter.meDirection   = EmitterDirection::CONSTANT;
  _directedEmitter.mUserDir      = _input_direction->value();

  auto offset = _input_offset->value();
  //printf( "OFFSET<%g %g %g>\n", offset.x, offset.y, offset.z);
  _emitter_context.mPosition          = offset;
  _directedEmitter.Emit(_emitter_context);
  float fphaseINC = fspr * fdt;
  mfPhase         = fmodf(mfPhase + fphaseINC, PI2 * 1000.0f);
  mfPhase2        = fmodf(mfPhase + fphaseINC, PI2 * 1000.0f);
  mfLastRadius    = mfThisRadius;
  mfThisRadius    = _input_emissionradius->value();
}
///////////////////////////////////////////////////////////////////////////////
void RingEmitterInst::_reap(float fdt) {
  _emitter_context.mPool       = _pool.get();
  _emitter_context.mfDeltaTime = fdt;
  _emitter_context.mKey        = (void*)this;
  ///////////////////////////////////////
 // _emitter_context.mDeathQueue = mDeathEventQueue;
  _directedEmitter.Reap(_emitter_context);
  //_emitter_context.mDeathQueue = 0;
  ///////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////

RingDirectedEmitter::RingDirectedEmitter(RingEmitterInst* module)
    : _emitterModule(module) {
}

///////////////////////////////////////////////////////////////////////////////

void RingDirectedEmitter::computePosDir(float fi, fvec3& pos, fmtx3& basis) {
  float scaler = (fi * _emitterModule->mfThisRadius) + ((1.0f - fi) * _emitterModule->mfLastRadius);
  float phase  = (fi * _emitterModule->mfPhase2) + ((1.0f - fi) * _emitterModule->mfPhase);
  float fpx    = cosf(phase);
  float fpz    = sinf(phase);
  float fdx    = cosf(phase + PI_DIV_2);
  float fdz    = sinf(phase + PI_DIV_2);
  pos          = fvec3((fpx * scaler), 0.0f, (fpz * scaler));
  if (meDirection == EmitterDirection::USER) {
    basis.setColumn(0,fvec3(1,0,0));
    basis.setColumn(1,mUserDir);
    basis.setColumn(2,fvec3(0,0,1));
  } else {
    //dir = fvec3(fdx, 0.0f, fdz);
    fvec3 DY = fvec3(fdx, 0.0f, fdz).normalized();
    fvec3 DX = fvec3(0,1,0);
    fvec3 DZ = DY.crossWith(DX);

    basis.setColumn(0,DX);
    basis.setColumn(1,DY);
    basis.setColumn(2,DZ);
  }
}

///////////////////////////////////////////////////////////////////////////////

RingEmitterData::RingEmitterData() {
}

//////////////////////////////////////////////////////////////////////////

static void _reshapeRingEmitterIOs( dataflow::moduledata_ptr_t data ){
  auto typed = std::dynamic_pointer_cast<RingEmitterData>(data);
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "LifeSpan")->_range = {0,100};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "EmissionRate")->_range = {0,1000};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "EmissionVelocity")->_range = {0,10};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "DispersionAngle")->_range = {0,1};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "EmissionRadius")->_range = {0,10};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "EmitterSpinRate")->_range = {0,100};
  ModuleData::createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "Direction")->_range = {-1,1};
  ModuleData::createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "Offset")->_range = {-10,10};
}

//////////////////////////////////////////////////////////////////////////

std::shared_ptr<RingEmitterData> RingEmitterData::createShared() {
  auto data = std::make_shared<RingEmitterData>();
  _initPoolIOs(data);
  _reshapeRingEmitterIOs(data);
  return data;
}

dgmoduleinst_ptr_t RingEmitterData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<RingEmitterInst>(this, ginst);
}

void RingEmitterData::describeX(class_t* clazz) {
  clazz->setSharedFactory( [] -> rtti::castable_ptr_t {
    return RingEmitterData::createShared();
  });
  clazz->annotateTyped<moduleIOreshape_fn_t>("reshapeIOs",[](dataflow::moduledata_ptr_t mdata){
    _reshapeRingEmitterIOs(mdata);
  });
}

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::RingEmitterData, "psys::RingEmitterData");
