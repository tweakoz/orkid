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

struct EllipticalEmitterInst;

//////////////////////////////////////////////////////////////////////////

struct EllipticalEmitterInst : public ParticleModuleInst {

  EllipticalEmitterInst(const EllipticalEmitterData* ned, dataflow::GraphInst* ginst);

  void onLink(GraphInst* inst) final;
  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final;
  void _emit(float fdt);
  void _reap(float fdt);

  float _timeAccumulator = 0.0f;
  fvec3 _lastPosition;

  fvec3 _prevBasisX;
  fvec3 _prevBasisY;
  fvec3 _prevBasisZ;

  fvec3 _curBasisX;
  fvec3 _curBasisY;
  fvec3 _curBasisZ;
  fvec3 _curOffset;

  // NozzleDirectedEmitter _directedEmitter;
  EmitterCtx _emitter_context;

  floatxf_inp_pluginst_ptr_t _input_lifespan;
  floatxf_inp_pluginst_ptr_t _input_emissionrate;
  floatxf_inp_pluginst_ptr_t _input_emissionvelocity;
  floatxf_inp_pluginst_ptr_t _input_dispersionangle;
  floatxf_inp_pluginst_ptr_t _input_scalar;
  floatxf_inp_pluginst_ptr_t _input_minu;
  floatxf_inp_pluginst_ptr_t _input_maxu;
  floatxf_inp_pluginst_ptr_t _input_minv;
  floatxf_inp_pluginst_ptr_t _input_maxv;

  fvec3xf_inp_pluginst_ptr_t _input_p1;
  fvec3xf_inp_pluginst_ptr_t _input_p2;

  /*

  fvec3xf_inp_pluginst_ptr_t _input_directionX;
  fvec3xf_inp_pluginst_ptr_t _input_directionY;
  fvec3xf_inp_pluginst_ptr_t _input_directionZ;
  fvec3xf_inp_pluginst_ptr_t _input_offset;
  fvec3xf_inp_pluginst_ptr_t _input_offset_velocity;
  */

  float _updaterate = 30.0f;
  RandGen _randgen;
  std::uniform_int_distribution<> _distribution;
  std::function<void(float min, float max)> _rangedf;
};

//////////////////////////////////////////////////////////////////////////

EllipticalEmitterInst::EllipticalEmitterInst(const EllipticalEmitterData* ned, dataflow::GraphInst* ginst)
    : ParticleModuleInst(ned, ginst) {
  //, _directedEmitter(this){

  _curBasisX = fvec3(1, 0, 0);
  _curBasisY = fvec3(0, 1, 0);
  _curBasisZ = fvec3(0, 0, 1);
}

///////////////////////////////////////////////////////////////////////////////
void EllipticalEmitterInst::onLink(GraphInst* inst) {
  _onLink(inst);
  _input_lifespan         = typedInputNamed<FloatXfPlugTraits>("LifeSpan");
  _input_emissionrate     = typedInputNamed<FloatXfPlugTraits>("EmissionRate");
  _input_emissionvelocity = typedInputNamed<FloatXfPlugTraits>("EmissionVelocity");
  _input_scalar = typedInputNamed<FloatXfPlugTraits>("Scalar");
  _input_minu = typedInputNamed<FloatXfPlugTraits>("MinU");
  _input_maxu = typedInputNamed<FloatXfPlugTraits>("MaxU");
  _input_minv = typedInputNamed<FloatXfPlugTraits>("MinV");
  _input_maxv = typedInputNamed<FloatXfPlugTraits>("MaxV");
  _input_p1               = typedInputNamed<Vec3XfPlugTraits>("P1");
  _input_p2               = typedInputNamed<Vec3XfPlugTraits>("P2");
  _input_dispersionangle  = typedInputNamed<FloatXfPlugTraits>("DispersionAngle");
}
///////////////////////////////////////////////////////////////////////////////
void EllipticalEmitterInst::compute(GraphInst* inst, ui::updatedata_ptr_t updata) {

  if (_pool == nullptr)
    return;

  _timeAccumulator += updata->_dt;

  _lastPosition = _curOffset;
  _prevBasisX   = _curBasisX;
  _prevBasisY   = _curBasisY;
  _prevBasisZ   = _curBasisZ;

  //_curBasisX = _input_directionX->value();
  //_curBasisY = _input_directionY->value();
  //_curBasisZ = _input_directionZ->value();
  //_curOffset    = _input_offset->value();

  if (_curBasisY.magnitudeSquared() == 0.0f) {
    _curBasisX  = fvec3(1, 0, 0);
    _curBasisY  = fvec3(0, 1, 0);
    _curBasisZ  = fvec3(0, 0, 1);
    _prevBasisX = _curBasisX;
    _prevBasisY = _curBasisY;
    _prevBasisZ = _curBasisZ;
  }

  float fdelta = 1.0f / _updaterate;

  if (_timeAccumulator >= fdelta) { // limit to 30hz
    _timeAccumulator -= fdelta;
    _reap(fdelta);
    _emit(fdelta);
  }
  _pool->updateUnitAges();
}
///////////////////////////////////////////////////////////////////////////////
void EllipticalEmitterInst::_emit(float fdt) {

  float femitvel                      = _input_emissionvelocity->value();
  float lifespan                      = std::clamp<float>(_input_lifespan->value(), 0.00f, 10.0f);
  float emissionrate                  = _input_emissionrate->value();
  _emitter_context.mPool              = _pool.get();
  _emitter_context.mfEmissionRate     = emissionrate;
  _emitter_context.mKey               = (void*)this;
  _emitter_context.mfLifespan         = lifespan;
  _emitter_context.mfDeltaTime        = fdt;
  _emitter_context.mfEmissionVelocity = femitvel;
  _emitter_context.mDispersion        = _input_dispersionangle->value();
  //_directedEmitter.meDirection        = EmitterDirection::USER;
  //_emitter_context.mPosition          = _input_offset->value();

  auto P1 = _input_p1->value();
  auto P2 = _input_p2->value();
  fmtx3 basis;
  fvec3 dirY = (P2 - P1).normalized();
  fvec3 dirX = dirY.crossWith(fvec3(0, 1, 1)).normalized();
  fvec3 dirZ = dirX.crossWith(dirY).normalized();
  dirX       = dirY.crossWith(dirZ).normalized();

  Pool<BasicParticle>& the_pool = *_emitter_context.mPool;
  float fdeltap                 = (_emitter_context.mfEmissionRate * _emitter_context.mfDeltaTime);
  _emitter_context.mfEmitterMark += fdeltap;
  if (std::isnan(_emitter_context.mfEmitterMark)) {
    _emitter_context.mfEmitterMark = 0.0f;
  }
  // printf("emitrate<%f> deltat<%f> deltap<%f> mark<%f>\n", _emitter_context.mfEmissionRate, _emitter_context.mfDeltaTime, fdeltap,
  // _emitter_context.mfEmitterMark);
  int icount = int(_emitter_context.mfEmitterMark);
  fvec3 pos, disp, yo;
  fvec3 center                 = (P1 + P2) * 0.5f;
  fvec3 semiMajorAxisDirection = (P1 - P2).normalized();
  float d                      = (P1 - P2).magnitude(); // Distance between the foci
  float b                      = 1 + d;                 // Linear relationship for b based on the distance

  // Assuming a and c should be constants, or determined by another specific requirement
  float a                    = std::max(1.0f, log(b)); // Placeholder value, adjust based on your requirements
  float c                    = a;                      // Same as a, assuming symmetry about the Y-axis
  fmtx4 transform_sph_to_wld = createSphericalToEllipticalTransformationMatrix(center, semiMajorAxisDirection, fvec3(a, b, c));

  if(lifespan==0.0f){
    lifespan = 1.0f;

    if(_pool->numFree() == 0){
      return;
    }
  }

  float scalar = _input_scalar->value();
  float minu = _input_minu->value();
  float maxu = _input_maxu->value();
  float minv = _input_minv->value();
  float maxv = _input_maxv->value();
  for (int ic = 0; ic < icount; ic++) {
    float fi = float(ic) / float(icount);
    float u  = _randgen.ranged_rand(minu, maxu);
    float v  = _randgen.ranged_rand(minv,maxv);

    float theta = u * PI2; // Theta ranges from 0 to 2*PI
    float phi = v * PI2; // Theta ranges from 0 to 2*PI

    // Assuming v maps linearly from the center to the surface
    float r = v; // Radius factor (since v is normalized to [0, 1])

    // Compute Cartesian coordinates for the point on the ellipsoid
    float x = sin(theta)*cos(phi)*scalar;
    float y = sin(theta)*sin(phi)*scalar;
    float z = cos(theta)*scalar;

    //

    fvec3 pos = fvec3(x, y, z);
    pos       = fvec4(pos, 1.0).transform(transform_sph_to_wld).xyz(); // Transform back to world space
    // pos.lerp(P1,P2, fi);
    // computePosDir(fi, pos, basis);
    pos += _emitter_context.mPosition;
    BasicParticle* __restrict ptc = the_pool.FastAlloc();
    if (ptc) {
      ptc->mfAge      = 0.0f;
      ptc->mfLifeSpan = lifespan;
      fvec3 dir       = dirY;
      fvec3 vbin      = dirX;
      fvec3 vtan      = dirZ;
      float fu        = _randgen.ranged_rand(-0.5, 0.5);
      float fv        = _randgen.ranged_rand(-0.5, 0.5);
      disp            = (vbin * fu) + (vtan * fv);
      yo.lerp(dir, disp, _emitter_context.mDispersion);
      dir = pos.normalized();

      ptc->mPosition = pos;
      // printf("EmitCB::pos<%g %g %g> dir<%g %g %g>\n", pos.x, pos.y, pos.z, dir.x, dir.y, dir.z);
      ptc->mVelocity     = dir * _emitter_context.mfEmissionVelocity;
      ptc->mLastPosition = pos - (ptc->mVelocity * _emitter_context.mfDeltaTime);
      ptc->mKey          = (void*)_emitter_context.mKey;
    }
  }
  _emitter_context.mfEmitterMark -= float(icount);
}
///////////////////////////////////////////////////////////////////////////////
void EllipticalEmitterInst::_reap(float fdt) {

  if(_emitter_context.mfLifespan==0.0f)
    return;

  _emitter_context.mPool       = _pool.get();
  _emitter_context.mfDeltaTime = fdt;
  _emitter_context.mKey        = (void*)this;
  Event DeathEv;
  DeathEv.mEventType = Char4("KILL");

  auto pool = _emitter_context.mPool;

  int inumalive = pool->GetNumAlive();
  // static const int kkillbufsize = 32<<10;
  // static BasicParticle* gpKillBuf = new int[kkillbufsize];

  // printf( "REAP numalive<%d>\n", inumalive );
  int inumkilled = 0;
  for (int i = 0; i < inumalive; i++) {
    BasicParticle* ptc = pool->mActiveParticles[i];
    bool bkeymatch     = (ptc->mKey == _emitter_context.mKey);
    if (ptc->IsDead() && bkeymatch) // kill particle
    {
      if (_emitter_context.mDeathQueue) {
        DeathEv.mPosition     = ptc->mPosition;
        DeathEv.mLastPosition = ptc->mLastPosition;
        DeathEv.mVelocity     = ptc->mVelocity;
        _emitter_context.mDeathQueue->QueueEvent(DeathEv);
        /*printf( "sending particle<%p> kill event DQ<%p> pos<%f %f %f>\n",
                ptc,
                _emitter_context.mDeathQueue,
                DeathEv.mPosition.x,
                DeathEv.mPosition.y,
                DeathEv.mPosition.z
                );*/
      }
      pool->mInactiveParticles.push_back(ptc);
      int ilast_alive           = pool->GetNumAlive() - 1;
      BasicParticle* ptclast    = pool->mActiveParticles[ilast_alive];
      pool->mActiveParticles[i] = ptclast;
      pool->mActiveParticles.erase(pool->mActiveParticles.begin() + ilast_alive);
      inumalive--;
    }
  }
  // _emitter_context.mPool->FastFree(ikill)
}

//////////////////////////////////////////////////////////////////////////

EllipticalEmitterData::EllipticalEmitterData() {
}

//////////////////////////////////////////////////////////////////////////

static void _reshapeEllipticalEmitterIOs(dataflow::moduledata_ptr_t data) {

  auto typed = std::dynamic_pointer_cast<EllipticalEmitterData>(data);
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "LifeSpan")->_range         = {0, 20};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "EmissionRate")->_range     = {0, 1000};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "EmissionVelocity")->_range = {-100, 100};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "DispersionAngle")->_range  = {0, 1};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Scalar")->_range  = {0, 100};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "MinU")->_range  = {0, 1};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "MaxU")->_range  = {0, 1};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "MinV")->_range  = {0, 1};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "MaxV")->_range  = {0, 1};
  ModuleData::createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "P1")->_range                = {-1000.0f, 1000.0f};
  ModuleData::createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "P2")->_range                = {-1000.0f, 1000.0f};
}

//////////////////////////////////////////////////////////////////////////

std::shared_ptr<EllipticalEmitterData> EllipticalEmitterData::createShared() {
  auto data = std::make_shared<EllipticalEmitterData>();
  _initPoolIOs(data);
  _reshapeEllipticalEmitterIOs(data);
  return data;
}

//////////////////////////////////////////////////////////////////////////

dgmoduleinst_ptr_t EllipticalEmitterData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<EllipticalEmitterInst>(this, ginst);
}

//////////////////////////////////////////////////////////////////////////

void EllipticalEmitterData::describeX(class_t* clazz) {
  clazz->setSharedFactory([] -> rtti::castable_ptr_t { return EllipticalEmitterData::createShared(); });

  clazz->annotateTyped<moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t mdata) { _reshapeEllipticalEmitterIOs(mdata); });
}

//////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::particle
//////////////////////////////////////////////////////////////////////////

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::EllipticalEmitterData, "psys::EllipticalEmitterData");
