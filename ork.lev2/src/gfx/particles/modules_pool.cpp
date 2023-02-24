////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/reflect/enum_serializer.inl>
#include <ork/math/collision_test.h>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/kernel/orklut.hpp>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/lev2_asset.h>
#include <signal.h>

namespace dflow = ::ork::dataflow;

namespace ork::lev2::particle {

struct ParticlePoolModuleInst : dflow::DgModuleInst {

  ParticlePoolModuleInst(const ParticlePoolData* data)
      : dflow::DgModuleInst(data)
      , _ppd(data) {
  }
//  ParticleBufferInst _particle_buffer;

  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t updata) final {
  }

  void onLink(dflow::GraphInst* inst) final {

    auto ptcl_context = inst->_impl.getShared<Context>();

    _output = typedOutputNamed<ParticleBufferPlugTraits>("ParticleBuffer");
    OrkAssert(_output);
    auto buffer   = _output->_value;
    buffer->_pool = std::make_shared<pool_t>();
    buffer->_pool->Init(_ppd->_poolSize);
  }

  particlebuf_outpluginst_ptr_t _output;
  const ParticlePoolData* _ppd;
  // void Compute(float dt) final;
  // void Reset() final;
  // void DoLink() final;
  // DeclareFloatOutPlug(UnitAge);
  // DeclareFloatXfPlug(PathInterval);
  // DeclareFloatXfPlug(PathProbability);
  // DeclarePoolOutPlug(Output);

  // EventQueue* mPathStochasticEventQueue = nullptr;
  // EventQueue* mPathIntervalEventQueue   = nullptr;
};

using poolmoduleinst_ptr_t = std::shared_ptr<ParticlePoolModuleInst>;

void ParticlePoolData::describeX(class_t* clazz) {
}

ParticlePoolData::ParticlePoolData() {
}

std::shared_ptr<ParticlePoolData> ParticlePoolData::createShared() {
  auto data = std::make_shared<ParticlePoolData>();
  createOutputPlug<ParticleBufferPlugTraits>(data, dflow::EPR_UNIFORM, "ParticleBuffer");
  return data;
}

dflow::dgmoduleinst_ptr_t ParticlePoolData::createInstance() const {
  return std::make_shared<ParticlePoolModuleInst>(this);
}

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::ParticlePoolData, "psys::ParticlePoolData");
