////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#pragma once

// GrassDrawableData (GRASS phase 2) — the reflected, round-trippable description of the
// task+mesh grass carpet. The SHADER half of the contract (task payload, tile grid, blade
// amplification, the sif_grass_field decode) is baked into the referenced ptex3d material's
// generated fxv2 at AUTHORING (GrassFieldSource, model B); this class rebuilds only the
// BUFFER half at load:
//   manifest (by HF asset name) -> the baked channel EXRs (height / normal / density / dryness)
//   one SSBO [ CamBlk | g_meta | g_meta2 | g_field[] ] resampled to _field_dim
//   ComputeDrawable: camera params + a DIRECT mesh draw whose {gx,gy,gz} are TASK workgroups
// There is deliberately NO taskless twin: a device or material that cannot run the task stage
// gets a NAMED refusal, never a lesser picture (the opposite of the terrain chunk drawable's
// mesh->pull step-down cascade). The tile grid is camera-relative in the task stage, so the
// CPU never re-dispatches as the walker moves.

#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/material_pbr.inl>

namespace ork::lev2::terrain {

// the task payload's baked survivor capacity, in tiles per supertile edge — GrassFieldSource
// .SUPERTILE, restated on this side because the dispatch count is derived from it here while the
// payload is sized by it there. The shader clamps the edge it is handed to its OWN baked value, so
// the two disagreeing costs coverage (a smaller supertile than the dispatch assumed leaves tiles
// unvisited) and never a payload overrun; this constant is what keeps the authored value inside a
// range the shader can honor.
constexpr int kSupertileMax = 4;

struct GrassDrawableData final : public DrawableData {

  DeclareConcreteX(GrassDrawableData, DrawableData);

public:
  GrassDrawableData();
  ~GrassDrawableData();
  drawable_ptr_t createDrawable() const final;

  ///////////////////////////////////////////////////////////////
  // reflected (authored) state — both referenced assets resolve BY NAME from the
  // AssetSystem registry at load (the wire step fills the runtime fields below).
  ///////////////////////////////////////////////////////////////

  std::string _hf_asset_name;       // HeightFieldGenData asset (its artifact = the manifest path)
  std::string _material_asset_name; // ptex3d material authored with GrassFieldSource

  // FIELD — the resampled carpet lookup the task/mesh stages read. _field_dim is a pure
  // sampling rate over the terrain's extent_m (512^2 vec4 = 4MB); the baked channels are
  // whatever resolution the terrain baked at and are filtered down to it once, at bootstrap.
  int _field_dim = 512;
  // TILE GRID — structural for the shader (the task stage's local_size is baked), parametric
  // here: _grid_dim^2 task workgroups, each owning a _tile_size-meter square, centered on the
  // eye. grid*tile MUST cover 2*_cull_radius or the carpet ends inside the fade.
  float _tile_size = 2.0f;
  int _grid_dim    = 96;
  // SUPERTILE COMPACTION — one task workgroup owns a _supertile x _supertile BLOCK of tiles and
  // loops it, appending the survivors to its payload, so the dispatch is ceil(_grid_dim/_supertile)^2
  // workgroups and the mesh grid it emits is the VISIBLE set rather than the whole tile grid. The
  // shader clamps this to its baked payload capacity (GrassFieldSource.SUPERTILE); anything
  // outside [1, kSupertileMax] is refused loudly here rather than silently truncated there.
  int _supertile = 4;
  // OCCLUSION — the tile's world box against the engine's 1-phase max-depth pyramid (the same one
  // the terrain chunk cull reads). _hzb_bias is the standard-Z margin that keeps a tile from culling
  // itself where the depth curve is flat; off, or with no pyramid built, every tile passes.
  bool _hzb_occlusion = true;
  float _hzb_bias     = 0.005f;

  // LOD/CULL radii (meters) + the falloff shape. Blades dissolve between _lod0_radius and
  // _cull_radius; _fade_pow shapes the smoothstep; _lod_ceiling caps clusters per tile.
  float _lod0_radius = 24.0f;
  float _lod1_radius = 64.0f;
  float _cull_radius = 96.0f;
  float _fade_pow    = 1.5f;
  float _lod_ceiling = 16.0f;

  // BAKED CHANNEL NAMES — keys into the terrain manifest's channels map. Renaming a capture
  // in the terrain DSL is then a Python-only change.
  std::string _height_channel  = "height";
  std::string _normal_channel  = "normal";
  std::string _density_channel = "grass_density";
  std::string _dryness_channel = "grass_dryness";

  // PARAM DEFAULTS (A8) — every one of these is bound into the material's UBO packs at
  // bootstrap and re-bindable live afterwards; NONE is ever baked into shader text.
  float _density_scale = 1.0f;
  float _blade_height     = 0.35f;
  float _blade_height_var = 0.40f;
  float _blade_width      = 0.012f;
  float _blade_taper      = 0.60f;
  float _clump_radius    = 0.35f;
  float _clump_lean      = 0.25f;
  float _clump_phase_var = 1.0f;
  float _clump_hue_var   = 0.15f;
  // per-CLUMP height amplitude (GrassClump2.x): 0 = every clump the same height (the carpet as
  // it was before tussock scaling existed); the blades of one clump then scale together, which
  // _blade_height_var cannot do (it is hashed per blade).
  float _clump_height_var = 0.0f;
  fvec3 _color_a   = fvec3(0.16, 0.30, 0.09);
  fvec3 _color_b   = fvec3(0.28, 0.42, 0.14);
  fvec3 _dry_color = fvec3(0.46, 0.40, 0.20);
  float _dry_fraction   = 0.25f;
  float _dry_fade_start = 0.35f;
  float _dry_fade_end   = 0.75f;
  float _backlit        = 0.6f;
  // WIND OVERRIDE — _wind_amp <= 0 means INHERIT (the scene's shared wind dict already bound
  // WindDir/WindParams on the material; the bootstrap leaves them alone). > 0 overrides all
  // three, so a carpet can be tuned against the trees without unbinding them.
  fvec3 _wind_dir = fvec3(1, 0, 0);
  float _wind_amp  = 0.0f;
  float _wind_freq = 0.3f;
  // STEREO — the _ST task twin culls with the CENTER (between-eyes) camera widened by this
  // radius in meters, so neither eye loses tiles the other keeps.
  float _stereo_widen = 0.35f;

  ///////////////////////////////////////////////////////////////
  // runtime (NEVER reflected) — wire-resolved before first render
  ///////////////////////////////////////////////////////////////

  mutable std::string _resolved_manifest;       // .terrain.json path (the HF artifact)
  mutable pbrmaterial_ptr_t _resolved_material; // the live generated material
};

using grass_drawable_data_ptr_t = std::shared_ptr<GrassDrawableData>;

} // namespace ork::lev2::terrain
