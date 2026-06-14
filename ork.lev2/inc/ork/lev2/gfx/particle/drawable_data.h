#pragma once
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/particle/modular_particles2.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct ParticlesDrawableData final : public DrawableData {

  DeclareConcreteX(ParticlesDrawableData, DrawableData);

public:
  drawable_ptr_t createDrawable() const final;
  ParticlesDrawableData();
  ~ParticlesDrawableData();
  void _doAttachSGDrawable(drawable_ptr_t drw, scenegraph::scene_ptr_t SG) const final;

  dataflow::graphdata_ptr_t _graphdata;
  float _emitterIntensity = 1.0f;
  float _emitterRadius = 1.0f;
  // When true, the drawable's internal enqueue lambda SKIPS its
  // graphinst->compute() call; compute is driven externally (e.g. by an
  // ECS ParticlesGlobalSystem iterating its components). Defaults to false
  // so standalone drawables (ork.particles.player) keep their self-driven
  // behavior. ECS-hosted drawables flip this to true and own the per-
  // frame compute on the update thread themselves.
  bool _externalCompute = false;
  // PBR2 Phase 0 — reflection-probe entity name for this particle
  // system's per-drawable cube override. Resolved by ParticlesGlobalSystem
  // at stage time → live LightProbe → drawable->_probeOverride. Empty =
  // no override (falls through to scene-global _lightprobes[0]).
  // Set by Python wire_scene_data from ParticleSystemGenData._probe_entity_name.
  std::string _probeEntityName;
};

// Helper for external drivers (ECS ParticlesGlobalSystem) — extract the
// graphinst owned by a Particles drawable created from a ParticlesDrawableData
// with _externalCompute=true. Returns nullptr if the drawable wasn't created
// by ParticlesDrawableData::createDrawable() or has no graphinst.
dataflow::graphinst_ptr_t particles_drawable_graphinst(drawable_ptr_t drw);

using particles_drawable_data_ptr_t = std::shared_ptr<ParticlesDrawableData>;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
