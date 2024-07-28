////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>

namespace dflow = ::ork::dataflow;

namespace ork::lev2::particle {

void computeAges();

ParticlePoolModuleInst::ParticlePoolModuleInst(const ParticlePoolData* data, dataflow::GraphInst* ginst)
    : dflow::DgModuleInst(data, ginst)
    , _ppd(data) {
}
//  ParticleBufferInst _particle_buffer;

void ParticlePoolModuleInst::compute(dflow::GraphInst* inst, ui::updatedata_ptr_t updata) { // final

  float fdt = updata->_dt;

  auto buffer        = _output->_value;
  auto pool          = buffer->_pool;
  int pool_size      = _ppd->_poolSize;
  float unit_age_ref = _ppd->_unitAge;

  if (pool->GetMax() != pool_size) {
    pool->Init(pool_size);
  }
  int inumalive = pool->GetNumAlive();
  for (int i = 0; i < inumalive; i++) {
    BasicParticle* ptc = pool->mActiveParticles[i];
    /////////////////////////////////
    float fage     = ptc->mfAge;
    float unit_age = (fage / ptc->mfLifeSpan);

    /* mOutDataUnitAge = std::clamp(unit_age, 0.001f, 0.999f);

     // printf( "ptcl<%d> age<%f> ls<%f> IsDead<%d> unitage<%f>\n", i, fage, ptc->mfLifeSpan, int(ptc->IsDead()), mOutDataUnitAge
     );
     /////////////////////////////////
     int ia1 = int(ptc->mfAge / mfPathInterval);
     int ia2 = int((ptc->mfAge + fdt) / mfPathInterval);
     if ((mPathIntervalEventQueue != 0) && (ia2 > ia1)) {
       Event PathEv;
       PathEv.mEventType    = Char4("PATH");
       PathEv.mPosition     = ptc->mPosition;
       PathEv.mLastPosition = ptc->mLastPosition;
       PathEv.mVelocity     = ptc->mVelocity;
       mPathIntervalEventQueue->QueueEvent(PathEv);
     }
     /////////////////////////////////
     if (mPathStochasticEventQueue != 0) {
       int irand   = rand() % 1000;
       float frand = float(irand) * 0.001f;
       float fprob = mPlugInpPathProbability.GetValue();
       if (frand < fprob) {
         Event PathEv;
         PathEv.mEventType    = Char4("PATH");
         PathEv.mPosition     = ptc->mPosition;
         PathEv.mLastPosition = ptc->mLastPosition;
         PathEv.mVelocity     = ptc->mVelocity;
         mPathStochasticEventQueue->QueueEvent(PathEv);
       }
     }*/
    /////////////////////////////////
    ptc->mfAge += fdt;
    ptc->mLastPosition = ptc->mPosition;
    ptc->mPosition += ptc->mVelocity * fdt;
  }
}

void ParticlePoolModuleInst::onLink(dflow::GraphInst* inst) { // final

  auto ptcl_context = inst->_impl.getShared<Context>();

  _output = typedOutputNamed<ParticleBufferPlugTraits>("pool");
  OrkAssert(_output);
  auto buffer   = _output->_value;
  buffer->_pool = std::make_shared<pool_t>();
  buffer->_pool->Init(_ppd->_poolSize);
}

using poolmoduleinst_ptr_t = std::shared_ptr<ParticlePoolModuleInst>;


ParticlePoolData::ParticlePoolData() {
}

static void _reshapePoolIOs( dataflow::moduledata_ptr_t data ){
  auto typed = std::dynamic_pointer_cast<ParticlePoolData>(data);
  ModuleData::createOutputPlug<ParticleBufferPlugTraits>(data, dflow::EPR_UNIFORM, "pool");
  ModuleData::createOutputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_VARYING1, "UnitAge");
}

std::shared_ptr<ParticlePoolData> ParticlePoolData::createShared() {
  auto data = std::make_shared<ParticlePoolData>();
  _reshapePoolIOs(data);
  return data;
}

dflow::dgmoduleinst_ptr_t ParticlePoolData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<ParticlePoolModuleInst>(this, ginst);
}

void ParticlePoolData::describeX(class_t* clazz) {
  clazz->setSharedFactory([] -> rtti::castable_ptr_t { return ParticlePoolData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",[](dataflow::moduledata_ptr_t mdata){
    _reshapePoolIOs(mdata);
  });
}

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::ParticlePoolData, "psys::ParticlePoolData");
