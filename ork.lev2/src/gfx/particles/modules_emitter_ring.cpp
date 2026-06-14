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
  // In-plane reference axis ("X" of the ring's local basis). Direction
  // is treated as the ring-plane normal; the bitangent is computed via
  // Direction × Tangent. Authors thread the host entity's local-X here
  // (typically Expr.entity("X").transformDir(vec3(1,0,0))) to make the
  // ring rotate with the host. Defaults to world-X so legacy graphs that
  // don't touch Tangent reproduce the existing world-XZ ring exactly.
  fvec3 mTangent = fvec3(1, 0, 0);
};

///////////////////////////////////////////////////////////////////////////////

struct RingEmitterInst : public ParticleModuleInst {

  RingEmitterInst(const RingEmitterData* rmd, dataflow::GraphInst* ginst);

  void onLink(GraphInst* inst) final;
  void onReset(GraphInst* inst) final { _elapsed = 0.0f; } // slot recycle re-arms StartDelay
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
  float _elapsed          = 0.0f; // emitter-local time since (re)start — gates StartDelay

  floatxf_inp_pluginst_ptr_t _input_lifespan;
  floatxf_inp_pluginst_ptr_t _input_startdelay;
  floatxf_inp_pluginst_ptr_t _input_emissionrate;
  floatxf_inp_pluginst_ptr_t _input_emissionvelocity;
  floatxf_inp_pluginst_ptr_t _input_dispersionangle;
  floatxf_inp_pluginst_ptr_t _input_emissionradius;
  floatxf_inp_pluginst_ptr_t _input_emitterspinrate;

  fvec3xf_inp_pluginst_ptr_t _input_direction;
  fvec3xf_inp_pluginst_ptr_t _input_tangent;
  fvec3xf_inp_pluginst_ptr_t _input_offset;
  fvec4xf_inp_pluginst_ptr_t _input_aux;
  float_out_pluginst_ptr_t   _output_random;  // pool's Random output

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
  _input_startdelay       = typedInputNamed<FloatXfPlugTraits>("StartDelay");
  _input_emissionrate     = typedInputNamed<FloatXfPlugTraits>("EmissionRate");
  _input_emissionvelocity = typedInputNamed<FloatXfPlugTraits>("EmissionVelocity");
  _input_dispersionangle  = typedInputNamed<FloatXfPlugTraits>("DispersionAngle");
  _input_emissionradius   = typedInputNamed<FloatXfPlugTraits>("EmissionRadius");
  _input_emitterspinrate  = typedInputNamed<FloatXfPlugTraits>("EmitterSpinRate");
  _input_direction        = typedInputNamed<Vec3XfPlugTraits>("Direction");
  _input_tangent          = typedInputNamed<Vec3XfPlugTraits>("Tangent");
  _input_offset           = typedInputNamed<Vec3XfPlugTraits>("Offset");
  _input_aux              = typedInputNamed<Vec4XfPlugTraits>("Aux");
  auto pool = _graphinst->firstModuleInst<ParticlePoolModuleInst>();
  if (pool) {
    _output_random = pool->typedOutputNamed<FloatPlugTraits>("Random");
  }
  // Install per-particle aux hook — DirectedEmitter::EmitCB calls this
  // for each new particle, letting Expr.ptc.random / Expr.rand_range
  // chains on Aux re-evaluate per particle.
  _emitter_context.mPerParticleAux = [this](BasicParticle* ptc) {
    if (_output_random) _output_random->setValue(ptc->mfRandom);
    ptc->_aux = _input_aux->value();
  };
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
      _elapsed += fdelta;
      bool delayed = (_elapsed < _input_startdelay->value()); // per-emitter StartDelay
      auto ptcl_context = inst->_impl.getShared<particle::Context>();
      if (not delayed and not (ptcl_context and ptcl_context->_inhibit_emission)) {
        _emit(fdelta);
      }
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
  _directedEmitter.mTangent      = _input_tangent->value();

  auto offset = _input_offset->value();
  //printf( "OFFSET<%g %g %g>\n", offset.x, offset.y, offset.z);
  _emitter_context.mPosition          = offset;
  // mAux is only used when mPerParticleAux is null (legacy path). The hook
  // we installed in onLink always runs, so mAux is effectively unused now
  // — kept for compat with any future emitter that bypasses the hook.
  _emitter_context.mAux               = _input_aux->value();
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

  // Build an orthonormal ring basis:
  //   Normal     = -mUserDir (Direction is the down-vector by convention,
  //                so the ring plane's normal flips to point "up" against
  //                gravity). When mUserDir isn't set (or is degenerate),
  //                fall back to world-Y.
  //   Tangent    = mTangent projected onto the plane perpendicular to Normal.
  //                Defaults to world-X for legacy parity. If the projection
  //                is degenerate (Tangent parallel to Normal) we synthesize
  //                a stable fallback.
  //   Bitangent  = Normal × Tangent.
  // The ring positions sweep in (Tangent, Bitangent) and the per-particle
  // emit direction sweeps in the same plane (offset by 90° in phase).
  fvec3 normal = mUserDir;
  if (normal.magnitudeSquared() < 1e-8f) {
    normal = fvec3(0, -1, 0);   // legacy default
  } else {
    normal = normal.normalized();
  }
  // Convention: ring plane is perpendicular to Direction (which is the
  // emission direction). Negate so the plane normal points "up" against
  // emit; the sign doesn't affect the swept ring, only the bitangent's
  // handedness — kept consistent with the legacy world-XZ orientation
  // (legacy used DY = (fdx, 0, fdz) and DX = world-Y; here normal plays
  // the role of "up-out-of-the-plane").
  fvec3 plane_normal = -normal;

  fvec3 tangent = mTangent;
  // Project tangent onto the plane perpendicular to plane_normal.
  tangent       = tangent - plane_normal * tangent.dotWith(plane_normal);
  if (tangent.magnitudeSquared() < 1e-8f) {
    // Degenerate (Tangent parallel to Normal). Pick a stable fallback:
    // world-X unless that's also degenerate, in which case world-Z.
    tangent = fvec3(1, 0, 0);
    tangent = tangent - plane_normal * tangent.dotWith(plane_normal);
    if (tangent.magnitudeSquared() < 1e-8f) {
      tangent = fvec3(0, 0, 1);
      tangent = tangent - plane_normal * tangent.dotWith(plane_normal);
    }
  }
  tangent         = tangent.normalized();
  fvec3 bitangent = plane_normal.crossWith(tangent).normalized();

  // Position swept in the (tangent, bitangent) plane around the ring.
  pos = tangent * (fpx * scaler) + bitangent * (fpz * scaler);

  if (meDirection == EmitterDirection::USER) {
    basis.setColumn(0, tangent);
    basis.setColumn(1, mUserDir);
    basis.setColumn(2, bitangent);
  } else {
    // Per-particle velocity dir sweeps 90° ahead in phase — i.e., tangent
    // to the ring at the spawn point — also lives in the same plane.
    fvec3 dy = (tangent * fdx + bitangent * fdz).normalized();
    fvec3 dx = plane_normal;
    fvec3 dz = dy.crossWith(dx);

    basis.setColumn(0, dx);
    basis.setColumn(1, dy);
    basis.setColumn(2, dz);
  }
}

///////////////////////////////////////////////////////////////////////////////

RingEmitterData::RingEmitterData() {
}

//////////////////////////////////////////////////////////////////////////

static void _reshapeRingEmitterIOs( dataflow::moduledata_ptr_t data ){
  auto typed = std::dynamic_pointer_cast<RingEmitterData>(data);
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "LifeSpan")->_range = {0,100};
  // PER-EMITTER delayed start (slot-local seconds before this emitter begins
  // emitting; existing particles still age/reap). Lets chains within ONE
  // graph stagger — e.g. smoke igniting later than fire. Default 0.
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "StartDelay")->_range = {0,30};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "EmissionRate")->_range = {0,1000};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "EmissionVelocity")->_range = {0,10};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "DispersionAngle")->_range = {0,1};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "EmissionRadius")->_range = {0,10};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "EmitterSpinRate")->_range = {0,100};
  ModuleData::createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "Direction")->_range = {-1,1};
  ModuleData::createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "Tangent")->_range = {-1,1};
  ModuleData::createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "Offset")->_range = {-10,10};
  ParticleModuleData::_initAuxIO(data);
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
