////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#pragma once

// TerrainChunkDrawableData (E.6, pulled into D.5) — the reflected, round-trippable
// description of the GPU chunked-terrain render: the C++ lowering of the Python
// TerrainChunkVertexSource CONSUMER side (terrain/gpu_chunk.py + viewer2). The shader
// half of the contract (SSBO layout / pull-VS / reset-cull-finalize compute) is BAKED
// into the referenced ptex3d material's generated fxv2 at AUTHORING (model B — it rides
// PbrMaterialGenData.shaderpath); this class rebuilds only the BUFFER half at load:
//   manifest (by HF asset name) -> dim/extent/height + the height channel EXR
//   one SSBO  [ CamBlk | indirect args | visible-chunk list | heights ]
//   ComputeDrawable: camera params + 3 compute passes + indirect TRIANGLES draw
// The layout arithmetic here MUST mirror gpu_chunk.py (the single source of truth for
// the GLSL side) — chunk size is reflected so both sides agree through the scene file.

#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/material_pbr.inl>

namespace ork::lev2::terrain {

struct TerrainChunkDrawableData final : public DrawableData {

  DeclareConcreteX(TerrainChunkDrawableData, DrawableData);

public:
  TerrainChunkDrawableData();
  ~TerrainChunkDrawableData();
  drawable_ptr_t createDrawable() const final;

  ///////////////////////////////////////////////////////////////
  // reflected (authored) state — both referenced assets resolve BY NAME from the
  // AssetSystem registry at load (the wire step fills the runtime fields below).
  ///////////////////////////////////////////////////////////////

  std::string _hf_asset_name;       // HeightFieldGenData asset (its artifact = the manifest path)
  std::string _material_asset_name; // ptex3d material authored with TerrainChunkVertexSource
  // [M] material-override cycle — the scene-declared ORDERED debug-material asset names (each a
  // ptex3d material sharing the terrain vertex_source; siblings of material_asset). Mode 0 is the
  // declared material; mode i (1-based) is _debug_material_assets[i-1]. Reflected DATA — adding a
  // debug look is a Python-only change (one DSL material + one name here), NO C++ edit. Empty list
  // => the [M] cycle is a no-op (mode 0 only).
  std::vector<std::string> _debug_material_assets;
  int _chunk = 128;                 // chunk size — must match the value the material was authored with
  int _layout_dim_cap = 0;          // SSBO-layout dim cap (0 => == manifest dim). The per-chunk array
                                    // caps + byte offsets bake this into the vertex-source SHADER TEXT —
                                    // MUST equal the gpu_chunk.py bake_dim the material was generated
                                    // with. A fixed cap (e.g. the editor's max dim) keeps the text
                                    // CONSTANT across dims: changing dim recompiles no materials.
  int _render_dimension = 0;        // render-mesh + physics grid res (0 => == manifest dim). The manifest
                                    // dim is the BAKE resolution; heights downsample bake_dim -> this for
                                    // the render SSBO. The material bake stays at the full manifest dim.
  std::string _capture_mode = "proc"; // "proc" (live) | "stored" (terrain proctex texture-bake)
  int _capture_res = 2048;            // baked atlas resolution (stored mode)
  std::string _capture_dir;           // (legacy) author-derived cache dir; unused by the same-session bind
  std::vector<std::string> _capture_targets; // explicit-capture target names (MRT order) -> N atlas textures

  ///////////////////////////////////////////////////////////////
  // runtime (NEVER reflected) — wire-resolved before first render
  ///////////////////////////////////////////////////////////////

  mutable std::string _resolved_manifest;       // .terrain.json path (the HF artifact)
  mutable pbrmaterial_ptr_t _resolved_material; // the live generated material
  // [M] material-override cycle: resolved materials indexed by mode. [0] == _resolved_material
  // (identity for mode 0); [i] (i>=1) is the resolved _debug_material_assets[i-1] (null + logged
  // if that asset is missing). SIZED (1 + debug list) by the wire step — no fixed count. NON-
  // reflected (wire-resolved). The render lambda swaps _material to _mode_materials[mode], clamping
  // mode against this vector's size, so it stays correct for any list length.
  mutable std::vector<pbrmaterial_ptr_t> _mode_materials;
};

using terrain_chunk_drawable_data_ptr_t = std::shared_ptr<TerrainChunkDrawableData>;

// #88 v2 — in-place display REVISIT fast path. Load a baked height product (channel-0
// FLOAT, meters — the SAME read materialize does at :594) and publish it as one whole,
// frame-coherent plane to the LiveFieldBuffer the HELD terrain drawable already consumes
// (its materialize-time height key). The drawable's s4LiveAccept morphs the presenting
// SSBO in place next GPU frame, so an interior->interior display revisit skips the full
// scene swap (scenegraph + sim + camera stay live). `held_field_key` is the buffer key
// (the held drawable's product height path); `height_exr_path` is the target product's
// on-disk height EXR. Returns the plane dim (>0) on success; 0 (LOUD) when the buffer is
// unarmed, the product is unreadable, or S4 is disabled — the caller then takes the
// full-swap path (never a wrong-plane bind).
int publishHeightPlaneFromExr(const std::string& held_field_key, const std::string& height_exr_path);

// SHIPPED MESHLET DIMENSION n: one mesh workgroup emits an n x n quad patch — (n+1)^2 corners,
// 2n^2 triangles. BOUND TO gpu_chunk.py's DEFAULT_MESHLET_DIM (that value sizes the generated
// payload, this one sizes the dispatch grid that consumes it); a split default dispatches a grid
// the shader does not agree with. Move them together or not at all. 8 (not the taskless tier's
// max of 11) because Metal tile memory is shared between the multisampled attachment set and the
// mesh stage's output payload, and mesh cost falls off a cliff between 9 and 8 — see the
// step-down policy below and gpu_chunk.py for the measurement.
constexpr int kTerrainDefaultMeshletDim = 8;

// EFFECTIVE meshlet dimension: ORKID_TERRAIN_MESHLET when set (1..11, the diagnostic knob
// gpu_chunk.py._meshlet_dim reads), else kTerrainDefaultMeshletDim. Read once per process.
int terrainMeshletDim();

// MESH-MODE MSAA STEP-DOWN POLICY (measured 2026-07-26; Metal/MoltenVK only).
// Metal tile memory is shared between the multisampled attachment set and the mesh stage's
// per-workgroup output payload, so the mesh path's MSAA cost is a function of the PAYLOAD, and it
// falls off a cliff between meshlet 9 and 8: at the 11-corner-per-side payload mesh LOSES to the
// SSBO-pull VS at 2x/4x (1.18-1.69x slower from 2560x1280 up), at the shipped meshlet 8 it WINS
// even at 4x (130.5 vs 120.3 fps in the real forward pass). So the step-down is conditioned on the
// payload ACTUALLY in use, not on the platform alone: it fires only on darwin, only into a
// multisampled forward target, and only above kTerrainDefaultMeshletDim — which means it is
// DORMANT at the shipped default and arms only when a fatter payload is selected. Off darwin it
// never fires (the discrete-GPU platforms are payload-flat). `forward_samples` is the forward
// target's hw sample count (msaaForwardSampleCount). ORKID_TERRAIN_MESH_FORCE=1 bypasses the
// step-down when it would fire, so the perf harness can still measure a fat payload under MSAA on
// mac. Returns true => the caller MUST take the pull-VS path; LOGS its own decision (one loud
// named line for both the step-down and the force, naming the payload and the attachment context).
bool terrainMeshMsaaStepDown(int forward_samples);

} // namespace ork::lev2::terrain
