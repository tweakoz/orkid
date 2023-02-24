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
    : dflow::DgModuleInst(data){

    }
  ParticleBufferInst _particle_buffer;

  void compute(dflow::GraphInst* inst) final {

  }

  void onLink(dflow::GraphInst* inst) final {
    _output = outputNamed("ParticleBuffer");
    OrkAssert(_output);
  }

  dflow::outpluginst_ptr_t _output;

     //void Compute(float dt) final;
  //void Reset() final;
  //void DoLink() final;
  //DeclareFloatOutPlug(UnitAge);
  //DeclareFloatXfPlug(PathInterval);
  //DeclareFloatXfPlug(PathProbability);
  //DeclarePoolOutPlug(Output);

  //EventQueue* mPathStochasticEventQueue = nullptr;
  //EventQueue* mPathIntervalEventQueue   = nullptr;

};

using poolmoduleinst_ptr_t = std::shared_ptr<ParticlePoolModuleInst>;


void ParticlePoolData::describeX(class_t* clazz) {

}

ParticlePoolData::ParticlePoolData(){


}

std::shared_ptr<ParticlePoolData> ParticlePoolData::createShared() {
    auto data = std::make_shared<ParticlePoolData>();
    //ParticlePoolData::sharedConstructor(gmd);
    //createInputPlug<Img32>(gmd, EPR_UNIFORM, gmd->_image_input, "Input");
    //createInputPlug<float>(gmd, EPR_UNIFORM, gmd->_paramA, "ParamA");
    //createInputPlug<float>(gmd, EPR_UNIFORM, gmd->_paramB, "ParamB");
    createOutputPlug<ParticleBufferPlugTraits>(data, dflow::EPR_UNIFORM, "ParticleBuffer");
    return data;
}


dflow::dgmoduleinst_ptr_t ParticlePoolData::createInstance() const {
  return std::make_shared<ParticlePoolModuleInst>(this);
}

}

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::ParticlePoolData, "psys::ParticlePoolData");
