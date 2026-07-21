#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/lev2/gfx/scenegraph/sgnode_particles.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/reflect/properties/registerX.inl>   // D.2: property template defs for describeX
#include <ork/reflect/properties/DirectObject.inl>
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
ImplementReflectionX(ork::lev2::ParticlesDrawableData, "ParticlesDrawableData");
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct ParticlesDrawableInst {

  ParticlesDrawableInst(std::shared_ptr<const ParticlesDrawableData> pdd)
      : _data(pdd) {
    _updata           = std::make_shared<ui::UpdateData>();
    _updata->_abstime = 0.0f;
    _updata->_dt      = 0.003f;
    _timer.Start();

    _testlight                      = std::make_shared<DynamicPointLight>();
    _testlight->_inlineData->_radius = 10.0f;
    _testlight->_inlineData->mColor  = fvec3(1, 1, 1);
    // item E: world-space systems (trails) park their sgnode at IDENTITY —
    // when the graph publishes an emission CENTER (the renderer's weighted
    // hot-particle centroid), the light tracks IT instead of the node.
    _testlight->_xformgenerator     = [this]() -> fmtx4 {
      if (_has_emission_center) {
        fmtx4 m;
        m.setTranslation(_emission_center);
        return m;
      }
      return _mymatrix;
    };
  }
  ///////////////////////////////////////////////////////////////
  void gpuInit(lev2::Context* ctx) {

    _graphinst->_vars.makeValueForKey<float>("yo", 0.0f);

    /*
    auto load_req = std::make_shared<asset::LoadRequest>(_griddata->_colortexpath);
    auto texasset = asset::AssetManager<lev2::TextureAsset>::load(load_req);
    OrkAssert(texasset);

    _colortexture = texasset->GetTexture();
    OrkAssert(_colortexture);

    _pbrmaterial       = new PBRMaterial();
    _pbrmaterial->_shaderpath = "orkshader://grid";
    _pbrmaterial->_texColor = _colortexture;
    _pbrmaterial->gpuInit(ctx);
    _pbrmaterial->_metallicFactor  = 0.0f;
    _pbrmaterial->_roughnessFactor = 1.0f;
    _pbrmaterial->_baseColor       = fvec3(1, 1, 1);

    _fxcache = _pbrmaterial->pipelineCache();
    */
  }
  ///////////////////////////////////////////////////////////////
  void _update() {
    // Compute path: skipped when an external driver (e.g. ECS
    // ParticlesGlobalSystem) is responsible for advancing the graphinst.
    // Mirror the timer + updata state regardless so the external driver
    // can still call compute() against fresh dt/abstime if it wants to.
    if (not _data->_externalCompute) {
      float abs_time    = _timer.SecsSinceStart();
      _updata->_dt      = abs_time - _updata->_abstime;
      _updata->_abstime = abs_time;
      _graphinst->compute(_updata);
    }

    // Light state update always runs — it reads from the graphinst's vars
    // which are populated by whoever called compute() (internal or external)
    // on the previous tick.
    _testlight->_inlineData->mColor  = fvec3(1, 1, 1);
    _testlight->_inlineData->_radius = _data->_emitterRadius;

    if (auto try_avgcolor = _graphinst->_vars.typedValueForKey<fvec4>("emission_color")) {
      _testlight->_inlineData->mColor     = try_avgcolor.value().xyz();
      _testlight->_inlineData->_intensity = _data->_emitterIntensity;
    }
    // item E — emission centroid (w > 0 = valid this tick)
    _has_emission_center = false;
    if (auto try_center = _graphinst->_vars.typedValueForKey<fvec4>("emission_center")) {
      auto c = try_center.value();
      if (c.w > 0.0f) {
        _emission_center     = c.xyz();
        _has_emission_center = true;
      }
    }
  }
  ///////////////////////////////////////////////////////////////
  // PRE-RENDER GPU hook (render thread, NO active render pass): fan out the particle
  // context's gpuUpdate lambdas — each renderer refreshes its material's gradient LUT
  // texture via a CPU upload (the render-pass-safe replacement for the old mid-pass bake).
  void _gpuUpdate(ork::lev2::Context* ctx) {
    auto ptcl_context = _graphinst->_impl.getShared<particle::Context>();
    for (auto& entry : ptcl_context->gpuUpdateLambdas()) {
      if (entry._lambda)
        entry._lambda(ctx);
    }
  }
  ///////////////////////////////////////////////////////////////
  void _render(const RenderContextInstData& RCID) {

    if (_LM) {
      auto& light_container = _LM->mGlobalMovingLights;
      light_container.RemoveLight(_testlight.get());
      light_container.AddLight(_testlight.get());
    }

    auto renderable                    = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
    auto context                       = RCID.context();
    auto RCFD = RCID.rcfd();
    const auto& CPD                    = RCFD->topCPD();
    bool isPickState                   = context->FBI()->isPickState();
    if (not _initted) {
      gpuInit(context);
      _initted = true;
    }
    auto ptcl_context = _graphinst->_impl.getShared<particle::Context>();
    if (ptcl_context->_drawable and ptcl_context->_drawable->_sgnode) {
      auto node = ptcl_context->_drawable->_sgnode;
      _mymatrix = node->_dqxfdata._worldTransform->composed();
    }

    // every registered renderer draws, in EXPLICIT draw_order (ties keep
    // link order) — multi-renderer graphs (smoke under fire) compose.
    // renderLambdas() snapshots under the registry lock: dynamic-spawn slots
    // LINK on the update thread while other slots render (the race that
    // intermittently crashed spawn N).
    for (auto& entry : ptcl_context->renderLambdas()) {
      if (entry._lambda)
        entry._lambda(RCID);
    }
  }
  ///////////////////////////////////////////////////////////////
  static void renderParticles(RenderContextInstData& RCID) {
    auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
    renderable->GetDrawableDataA().getShared<ParticlesDrawableInst>()->_render(RCID);
  }
  ///////////////////////////////////////////////////////////////
  std::shared_ptr<const ParticlesDrawableData> _data;
  PBRMaterial* _pbrmaterial;
  dataflow::graphinst_ptr_t _graphinst;

  texture_ptr_t _colortexture;
  ui::updatedata_ptr_t _updata;
  fxpipelinecache_constptr_t _fxcache;
  lightmanager_ptr_t _LM;
  lightmanagerdata_ptr_t _LMD;

  dynamicpointlight_ptr_t _testlight;
  fmtx4 _mymatrix;
  fvec3 _emission_center;
  bool _has_emission_center = false;
  bool _initted = false;
  Timer _timer;
};

///////////////////////////////////////////////////////////////////////////////

void ParticlesDrawableData::describeX(class_t* c) {
  // D.2 (particles model B): the drawable recipe is fully reflected — the particle graph embeds
  // INLINE (owned, not a cross-asset ref), so a serialized drawable/scene reloads with no Python
  // and no DSL file (the dsl_file on ParticleSystemGenData remains as PROVENANCE only).
  c->directObjectProperty("graphdata", &ParticlesDrawableData::_graphdata);
  c->directProperty("emitter_intensity", &ParticlesDrawableData::_emitterIntensity);
  c->directProperty("emitter_radius", &ParticlesDrawableData::_emitterRadius);
  c->directProperty("external_compute", &ParticlesDrawableData::_externalCompute);
  c->directProperty("probe_entity_name", &ParticlesDrawableData::_probeEntityName);
}

///////////////////////////////////////////////////////////////////////////////

drawable_ptr_t ParticlesDrawableData::createDrawable() const {

  if (nullptr == _graphdata)
    return nullptr;

  using namespace ork::dataflow;
  using namespace particle;

  ////////////////////////////////////////////////////
  // create topology
  ////////////////////////////////////////////////////

  auto dg_context = std::make_shared<dgcontext>();
  dg_context->createRegisters<float>("ptc_float", 16);
  dg_context->createRegisters<fvec3>("ptc_vec3f", 16);
  dg_context->createRegisters<fvec4>("ptc_vec4f", 16);
  dg_context->createRegisters<ParticleBufferData>("ptc_buffer", 4);
  auto dg_sorter = std::make_shared<DgSorter>(_graphdata.get(), dg_context);
  // (channel enables removed — they FORCE-ENABLED dgsorter-std/-reg here,
  // overriding the ctor's enabled=false and spamming TOPO lines on every
  // particle graph link. Re-enable per run via ORKID_LOGCHAN_0dgsorter0=1.)
  auto topo = dg_sorter->generateTopology();

  ////////////////////////////////////////////////////
  // create graphinst
  ////////////////////////////////////////////////////

  auto graphinst    = GraphData::createGraphInst(_graphdata);
  auto ptcl_context = graphinst->_impl.makeShared<particle::Context>();
  graphinst->updateTopology(topo);

  // S8.5 (owner law): a partial / invalid topology (e.g. an unwired module just added in the
  // editor) must NOT crash. updateTopology left the graph unstaged and flagged it; return null
  // here rather than build a drawable over a half-linked graph. The pyext createDrawable path
  // surfaces this as a loud None. Once the module is wired, the next createDrawable links clean.
  if (not graphinst->_topologyValid) {
    printf("[particles] ParticlesDrawableData::createDrawable: INVALID topology (an unwired "
           "module) — returning null (no drawable built until the graph is fully wired).\n");
    return nullptr;
  }

  ////////////////////////////////////////////////////

  auto impl        = std::make_shared<ParticlesDrawableInst>(dataShared<ParticlesDrawableData>());
  impl->_graphinst = graphinst;

  auto rval                   = std::make_shared<CallbackDrawable>(nullptr);
  rval->_drawable_type        = "particles"_crcu;
  rval->_enqueueOnLayerLambda = [impl](drawqueueitem_constptr_t cdb) { impl->_update(); };
  rval->setOnGpuUpdateLambda([impl](ork::lev2::Context* ctx) { impl->_gpuUpdate(ctx); });
  rval->SetRenderCallback(ParticlesDrawableInst::renderParticles);
  rval->SetUserDataA(impl);
  ptcl_context->_drawable = rval;
  rval->_sortkey          = 20;
  // PBR2 Phase 0 — apply per-drawable HDRI override read from
  // DrawableData::_environmentMapPath (set by wire_scene_data from the
  // ParticleSystem(probe=...) wrapper). Mirrors ModelDrawableData's
  // _loadEnvMapOverride call.
  loadEnvMapOverride(rval.get(), _environmentMapPath);
  // Particles default to NOT appearing in probe cubemap captures —
  // they're noisy/transient and would feedback-loop through their
  // own probe override. Authors can flip this if they actually want
  // particles in reflections (e.g. emissive fireflies in a still scene).
  rval->_excludeFromProbe = true;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void ParticlesDrawableData::_doAttachSGDrawable(drawable_ptr_t drw, scenegraph::scene_ptr_t SG) const { // final
  auto impl  = drw->_implA.getShared<ParticlesDrawableInst>();
  impl->_LM  = SG->_lightManager;
  impl->_LMD = SG->_lightManagerData;
  OrkAssert(impl->_LM != nullptr);
  OrkAssert(impl->_LMD != nullptr);
}

///////////////////////////////////////////////////////////////////////////////

ParticlesDrawableData::ParticlesDrawableData() {
  //_colortexpath = "lev2://textures/gridcell_grey";
}

///////////////////////////////////////////////////////////////////////////////

ParticlesDrawableData::~ParticlesDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////
// Accessor for external drivers — fish the graphinst out of the drawable
// created by ParticlesDrawableData::createDrawable(). Used by ECS
// ParticlesGlobalSystem when _externalCompute is set: the system grabs the
// graphinst at stage time, calls compute() on it from _onUpdate.
dataflow::graphinst_ptr_t particles_drawable_graphinst(drawable_ptr_t drw) {
  if (!drw) return nullptr;
  auto impl = drw->_implA.getShared<ParticlesDrawableInst>();
  if (!impl) return nullptr;
  return impl->_graphinst;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
