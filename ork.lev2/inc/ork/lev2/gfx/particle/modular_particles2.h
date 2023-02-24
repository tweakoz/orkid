////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
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

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////

namespace dflow   = ::ork::dataflow;
using particle_t  = BasicParticle;
using pool_t      = Pool<particle_t>;
using pool_ptr_t  = std::shared_ptr<pool_t>;
using float_ptr_t = std::shared_ptr<float>;

struct ParticleBufferData {};
using particlebufferdata_ptr_t = std::shared_ptr<ParticleBufferData>;

struct ParticleBufferInst {


  ParticleBufferInst(std::shared_ptr<ParticleBufferData> data)
    : _data(data) {
      _pool = std::make_shared<pool_t>();
  }

  pool_ptr_t _pool;
  std::shared_ptr<ParticleBufferData> _data;
};

struct ParticleBufferPlugTraits {
  using elemental_data_type = ParticleBufferData;
  using elemental_inst_type = ParticleBufferInst;
  using data_impl_type_t                  = ParticleBufferData;
  using inst_impl_type_t                  = ParticleBufferInst;
  static constexpr size_t max_fanout = 1;
  static std::shared_ptr<ParticleBufferInst> data_to_inst(std::shared_ptr<ParticleBufferData> inp);
};

using particlebuf_inplugdata_t      = dflow::inplugdata<ParticleBufferPlugTraits>;
using particlebuf_inplugdata_ptr_t  = std::shared_ptr<particlebuf_inplugdata_t>;
using particlebuf_outplugdata_t     = dflow::outplugdata<ParticleBufferPlugTraits>;
using particlebuf_outplugdata_ptr_t = std::shared_ptr<particlebuf_outplugdata_t>;

using particlebuf_inpluginst_t      = dflow::inpluginst<ParticleBufferPlugTraits>;
using particlebuf_inpluginst_ptr_t  = std::shared_ptr<particlebuf_inpluginst_t>;
using particlebuf_outpluginst_t     = dflow::outpluginst<ParticleBufferPlugTraits>;
using particlebuf_outpluginst_ptr_t = std::shared_ptr<particlebuf_outpluginst_t>;

struct ModuleData;
using ptclmoduledata_ptr_t = std::shared_ptr<ModuleData>;

struct ModuleData : public dflow::DgModuleData {

  DeclareAbstractX(ModuleData, dflow::DgModuleData);

public:
  ////////////////////////////////////////////////////////////
  ModuleData();

  static void sharedConstructor(ptclmoduledata_ptr_t subclass_instance) {
  }

  void Link(ork::lev2::particle::Context* pctx);
};

///////////////////////////////////////////////////////////////////////////////

struct ParticleModuleData : public ModuleData {
  DeclareAbstractX(ParticleModuleData, ModuleData);

public:
  ParticleModuleData();

  static particlebufferdata_ptr_t _no_connection;
  particlebufferdata_ptr_t _bufferdata;
};

///////////////////////////////////////////////////////////////////////////////

struct GlobalModuleData : public ModuleData {
  DeclareConcreteX(GlobalModuleData, ModuleData);

public:
  static std::shared_ptr<GlobalModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance() const final;

public:
  GlobalModuleData();
};

///////////////////////////////////////////////////////////////////////////////

struct ParticlePoolData : public ParticleModuleData {

  DeclareConcreteX(ParticlePoolData, ParticleModuleData);

public:
  ParticlePoolData();
  static std::shared_ptr<ParticlePoolData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance() const final;

  float _unitAge = 1.0f;
  int _poolSize  = 40;
  dflow::floatxfinplugdata_ptr_t _pathInterval;
  dflow::floatxfinplugdata_ptr_t _pathProbability;
  particlebuf_outplugdata_ptr_t _poolOutput;
  std::string _pathStochasticQueueID;
  std::string _pathIntervalQueueID;
  Char4 _pathStochasticQueueID4;
  Char4 _pathIntervalQueueID4;
};

struct ParticlePoolRenderBuffer {

  ParticlePoolRenderBuffer();
  ~ParticlePoolRenderBuffer();
  void Update(const pool_t& pool);
  void SetCapacity(int inum);

  particle_t* _particles = nullptr;
  int _maxParticles      = 0;
  int _numParticles      = 0;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::particle
///////////////////////////////////////////////////////////////////////////////
