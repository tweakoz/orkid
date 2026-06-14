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
  int _chunk = 128;                 // chunk size — must match the value the material was authored with

  ///////////////////////////////////////////////////////////////
  // runtime (NEVER reflected) — wire-resolved before first render
  ///////////////////////////////////////////////////////////////

  mutable std::string _resolved_manifest;       // .terrain.json path (the HF artifact)
  mutable pbrmaterial_ptr_t _resolved_material; // the live generated material
};

using terrain_chunk_drawable_data_ptr_t = std::shared_ptr<TerrainChunkDrawableData>;

} // namespace ork::lev2::terrain
