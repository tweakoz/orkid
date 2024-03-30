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
#include <ork/dataflow/all.h>
#include <ork/math/gradient.h>
#include <ork/kernel/any.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////

using vertex_t        = SVtxV12C4T16;
using vertex_writer_t = lev2::VtxWriter<vertex_t>;

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
  using elemental_data_type          = ParticleBufferData;
  using elemental_inst_type          = ParticleBufferInst;
  using data_impl_type_t             = ParticleBufferData;
  using inst_impl_type_t             = ParticleBufferInst;
  using xformer_t                    = dflow::nullpassthrudata;
  using range_type = no_range;
  using out_traits_t = ParticleBufferPlugTraits;
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

using moduledata_ptr_t = std::shared_ptr<ModuleData>;

struct ModuleData : public dflow::DgModuleData {

  DeclareAbstractX(ModuleData, dflow::DgModuleData);

public:
  ////////////////////////////////////////////////////////////
  ModuleData();

  static void sharedConstructor(moduledata_ptr_t subclass_instance) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct ParticleModuleData : public ModuleData {
  DeclareAbstractX(ParticleModuleData, ModuleData);

public:
  ParticleModuleData();
  static void _initPoolIOs(dflow::dgmoduledata_ptr_t sub);

  static particlebufferdata_ptr_t _no_connection;
  particlebufferdata_ptr_t _bufferdata;
};

using ptcmoduledata_ptr_t = std::shared_ptr<ParticleModuleData>;

struct ParticleModuleInst : public dflow::DgModuleInst {

  ParticleModuleInst(const ParticleModuleData* data, dataflow::GraphInst* ginst);
  void _onLink(dflow::GraphInst* inst);
  particlebuf_inpluginst_ptr_t _input_buffer;
  particlebuf_outpluginst_ptr_t _output_buffer;
  pool_ptr_t _pool;
};

///////////////////////////////////////////////////////////////////////////////

struct GlobalModuleData : public ParticleModuleData {
  DeclareConcreteX(GlobalModuleData, ParticleModuleData);

public:
  static std::shared_ptr<GlobalModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;

public:
  GlobalModuleData();
};

using globalmodule_ptr_t = std::shared_ptr<GlobalModuleData>;

///////////////////////////////////////////////////////////////////////////////

struct ParticlePoolData : public ParticleModuleData {

  DeclareConcreteX(ParticlePoolData, ParticleModuleData);

public:
  ParticlePoolData();
  static std::shared_ptr<ParticlePoolData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;

  float _unitAge = 1.0f;
  int _poolSize  = 16384;
  // dflow::floatxfinplugdata_ptr_t _pathInterval;
  // dflow::floatxfinplugdata_ptr_t _pathProbability;
  //particlebuf_outplugdata_ptr_t _poolOutput;
  std::string _pathStochasticQueueID;
  std::string _pathIntervalQueueID;
  Char4 _pathStochasticQueueID4;
  Char4 _pathIntervalQueueID4;
};

using poolmodule_ptr_t = std::shared_ptr<ParticlePoolData>;

struct ParticlePoolRenderBuffer {

  ParticlePoolRenderBuffer(int index);
  ~ParticlePoolRenderBuffer();
  void update(const pool_t& pool);
  void setCapacity(int inum);

  particle_t* _particles = nullptr;
  int _maxParticles      = 0;
  int _numParticles      = 0;
  int _index             = 0;
};

struct ParticlePoolModuleInst : dflow::DgModuleInst {

  ParticlePoolModuleInst(const ParticlePoolData* data, dflow::GraphInst* ginst);
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t updata) final;
  void onLink(dflow::GraphInst* inst) final;

  particlebuf_outpluginst_ptr_t _output;
  const ParticlePoolData* _ppd;
};

using poolmoduleinst_ptr_t = std::shared_ptr<ParticlePoolModuleInst>;

fmtx4 createSphericalToEllipticalTransformationMatrix(const fvec3& center, const fvec3& semiMajorAxisDirection, fvec3 vscale);

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::particle
///////////////////////////////////////////////////////////////////////////////
