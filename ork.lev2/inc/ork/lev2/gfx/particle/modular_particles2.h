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
  // Adds the "Aux" Vec4f input plug. Emitters call this from their reshape
  // function (which gets the base moduledata_ptr_t) so authors can write
  // emitter.Aux.x/.y/.z/.w bindings — the value is read per-emit and
  // written into each new particle's _aux.
  static void _initAuxIO(dflow::moduledata_ptr_t sub);

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
// EntityRefModuleData — parametric host-entity transform decomposition.
//
// Outputs the SRT components of the live transform of an ECS-published
// entity (looked up via GraphInst::_resolveEntityXf at compute time).
// One instance per unique entity name in the graph — the HyperSyn DSL
// lowerer creates them lazily from Expr.entity("name").pos / .quat /
// .scale references and connects the right output plug to consumers.
//
// Standalone graphs (no resolver bound) or unresolved names → outputs
// stay at the identity defaults (pos=0, unit quat, scale=1), so the
// DSL author surface degrades gracefully when there's no ECS host.
struct EntityRefModuleData : public ParticleModuleData {
  DeclareConcreteX(EntityRefModuleData, ParticleModuleData);

public:
  static std::shared_ptr<EntityRefModuleData> createShared();
  static std::shared_ptr<EntityRefModuleData> createWithName(const std::string& name);
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;

public:
  EntityRefModuleData();

  // The published entity key (e.g. "saddle_ptc0") to track. Empty →
  // module's outputs stay at identity. Set at graph-construction time;
  // the lowerer assigns this when materializing an Expr.entity(...)
  // reference. Round-trips through JSON.
  std::string _entity_name;
};

using entityrefmodule_ptr_t = std::shared_ptr<EntityRefModuleData>;

///////////////////////////////////////////////////////////////////////////////
// TransformPointModuleData — apply a published entity's full SRT to a
// vec3 input (interpreted as a point: includes translation).
//   world_point = host_xf * local_point
// DSL: Expr.entity("name").transformPoint(local_vec3). Standalone graphs
// or unresolved names → output stays equal to input (identity).
struct TransformPointModuleData : public ParticleModuleData {
  DeclareConcreteX(TransformPointModuleData, ParticleModuleData);

public:
  static std::shared_ptr<TransformPointModuleData> createShared();
  static std::shared_ptr<TransformPointModuleData> createWithName(const std::string& name);
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
  TransformPointModuleData();
  std::string _entity_name;
};
using transform_point_module_ptr_t = std::shared_ptr<TransformPointModuleData>;

///////////////////////////////////////////////////////////////////////////////
// TransformDirModuleData — apply a published entity's rotation+scale to
// a vec3 input (interpreted as a direction: NO translation; the 3x3
// part of the host_xf, including non-uniform scale, is applied).
//   world_dir = (host_xf 3x3) * local_dir
// DSL: Expr.entity("name").transformDir(local_vec3).
struct TransformDirModuleData : public ParticleModuleData {
  DeclareConcreteX(TransformDirModuleData, ParticleModuleData);

public:
  static std::shared_ptr<TransformDirModuleData> createShared();
  static std::shared_ptr<TransformDirModuleData> createWithName(const std::string& name);
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
  TransformDirModuleData();
  std::string _entity_name;
};
using transform_dir_module_ptr_t = std::shared_ptr<TransformDirModuleData>;

///////////////////////////////////////////////////////////////////////////////
// Vec3AddModuleData — two-input componentwise vec3 add. The DSL lowerer
// emits one of these whenever an Expr addition combines two vec3-typed
// sources (e.g. Expr.entity(...).transformPoint(...) + Expr.vec3(...)).
struct Vec3AddModuleData : public ParticleModuleData {
  DeclareConcreteX(Vec3AddModuleData, ParticleModuleData);

public:
  static std::shared_ptr<Vec3AddModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
  Vec3AddModuleData();
};
using vec3add_module_ptr_t = std::shared_ptr<Vec3AddModuleData>;

///////////////////////////////////////////////////////////////////////////////
// Vec3CombineModuleData — takes three scalar (float) inputs X/Y/Z and emits an
// fvec3 output named "value". Used by the HyperSyn DSL lowerer to materialize
// Expr.vec3(x, y, z) bindings: each axis lowers independently to its own
// scalar chain, then combines through this module before connecting to the
// target vec3 plug.
struct Vec3CombineModuleData : public ParticleModuleData {
  DeclareConcreteX(Vec3CombineModuleData, ParticleModuleData);

public:
  static std::shared_ptr<Vec3CombineModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;

public:
  Vec3CombineModuleData();
};

using vec3combinemodule_ptr_t = std::shared_ptr<Vec3CombineModuleData>;

///////////////////////////////////////////////////////////////////////////////
// ParametersModule — runtime-mutable scalar source. The DSL's self.expose()
// creates one output plug per exposed parameter; downstream chains read the
// current value via a connection. ECS gameplay code mutates values via
// componentNotify(SET_PARAM, {name, value}) — the change is visible on the
// next compute.
struct ParametersModuleData : public ParticleModuleData {
  DeclareConcreteX(ParametersModuleData, ParticleModuleData);

public:
  static std::shared_ptr<ParametersModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
  ParametersModuleData();

  // Add a float parameter. Creates an output plug with the given name and
  // records the default. Called by DSL self.expose(name, default). Idempotent
  // — re-exposing the same name updates the default but doesn't dup the plug.
  // Param plugs are kept NAME-SORTED: plug values deserialize positionally
  // (skew gate), and on load the plugs rebuild by iterating _defaults — plug
  // order must be a deterministic function of the name set on both the
  // authoring and the rebuild path. std::map (sorted) for the same reason.
  void addFloatParam(const std::string& name, float default_value);

  float defaultFor(const std::string& name) const;
  bool  hasParam(const std::string& name) const;
  std::vector<std::string> paramNames() const;

  std::map<std::string, float> _defaults;
};

using parametersmodule_ptr_t = std::shared_ptr<ParametersModuleData>;

// Helper for ECS layer / pyext — set a named param on the ParametersModule
// instance owned by `ginst`. Returns false if no ParametersModule exists in
// this graphinst or if the name isn't exposed. Cheap (hash lookup).
bool set_param_on_graphinst(dataflow::graphinst_ptr_t ginst,
                            const std::string& name, float value);

// Total live particles across every pool module in `ginst`. The ECS layer's
// drain-completion probe — an orphaned (host-despawned) trail keeps computing
// with emission inhibited and is released once this reaches zero.
int aliveCountOnParticleGraph(dataflow::graphinst_ptr_t ginst);

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
  void onReset(dflow::GraphInst* inst) final;

  particlebuf_outpluginst_ptr_t _output;
  const ParticlePoolData* _ppd;
};

using poolmoduleinst_ptr_t = std::shared_ptr<ParticlePoolModuleInst>;

fmtx4 createSphericalToEllipticalTransformationMatrix(const fvec3& center, const fvec3& semiMajorAxisDirection, fvec3 vscale);

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::particle
///////////////////////////////////////////////////////////////////////////////
