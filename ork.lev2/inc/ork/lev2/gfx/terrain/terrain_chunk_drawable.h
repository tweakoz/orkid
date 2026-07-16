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

} // namespace ork::lev2::terrain
