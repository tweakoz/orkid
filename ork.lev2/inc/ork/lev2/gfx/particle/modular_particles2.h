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

namespace dflow = ::ork::dataflow;
using particle_t = BasicParticle;
using pool_t = Pool<particle_t>;
using pool_ptr_t = std::shared_ptr<pool_t>;
using float_ptr_t = std::shared_ptr<float>;

struct ParticleBufferData {
};
using particlebufferdata_ptr_t = std::shared_ptr<ParticleBufferData>;

struct ParticleBufferInst {
  ParticleBufferInst() {
  }
  pool_ptr_t _pool;
};

using particlebuf_inplugdata_t = dflow::inplugdata<ParticleBufferData>;
using particlebuf_inplugdata_ptr_t = std::shared_ptr<particlebuf_inplugdata_t>;

using particlebuf_outplugdata_t = dflow::outplugdata<ParticleBufferData>;
using particlebuf_outplugdata_ptr_t = std::shared_ptr<particlebuf_outplugdata_t>;

using particlebuf_inpluginst_t = dflow::inpluginst<ParticleBufferData>;
using particlebuf_inpluginst_ptr_t = std::shared_ptr<particlebuf_inpluginst_t>;

using particlebuf_outpluginst_t = dflow::outpluginst<ParticleBufferData>;
using particlebuf_outpluginst_ptr_t = std::shared_ptr<particlebuf_outpluginst_t>;

struct ModuleData;
using ptclmoduledata_ptr_t = std::shared_ptr<ModuleData>;

struct ModuleData : public dflow::DgModuleData {

  DeclareAbstractX(ModuleData, dflow::DgModuleData);

public:
  ////////////////////////////////////////////////////////////
  ModuleData();

  static void sharedConstructor(ptclmoduledata_ptr_t subclass_instance) {
    //auto as_basemod = std::dynamic_pointer_cast<ModuleData>(subclass_instance);
    //createOutputPlug<Img32>(subclass_instance, EPR_UNIFORM, as_im32mod->_image_out, "Output");
  }

  //virtual void DoLink() {
  //}
  //ork::lev2::particle::Context* mpParticleContext;
  //Module* mpTemplateModule;

  void Link(ork::lev2::particle::Context* pctx);
  //virtual void Reset() {
  //}
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

  float_ptr_t _timeBase;

  float_ptr_t _noiseRat;
  float_ptr_t _noisePrv;
  float_ptr_t _noiseNew;
  float_ptr_t _noiseBas;
  float_ptr_t _noiseTim;

  float_ptr_t _slowNoiseRat;
  float_ptr_t _slowNoisePrv;
  float_ptr_t _slowNoiseBas;
  float_ptr_t _slowNoiseTim;

  float_ptr_t _fastNoiseRat;
  float_ptr_t _fastNoisePrv;
  float_ptr_t _fastNoiseBas;
  float_ptr_t _fastNoiseTim;

public:
  GlobalModuleData();
};

///////////////////////////////////////////////////////////////////////////////

struct ParticlePoolData : public ParticleModuleData {

  DeclareConcreteX(ParticlePoolData, ParticleModuleData);

public:

  static std::shared_ptr<ParticlePoolData> createShared();

  ParticlePoolData();
  dflow::dgmoduleinst_ptr_t createInstance() const final;

  float _unitAge = 1.0f;
  int _poolSize = 40;
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
  int _maxParticles = 0;
  int _numParticles = 0;
};

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////
