////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#pragma once

// HypermeshDrawableData (HYPERECS D.3) — the reflected, round-trippable description of a
// hypermesh render: the C++ port of the Python make_drawable orchestration
// (hypergraph dflow/hypermesh/__init__.py). Authored state only:
//   { graphdata, material asset ref, viz flags, instance source, vtx budget }
// createDrawable() returns a ComputeDrawable whose GPU side materializes LAZILY on its
// first onGpuUpdate (the first moment a Context exists on the render thread):
//   materializeLive(graph) -> bind the 5 vertex-channel SSBOs to the material's storage
//   blocks -> setupMeshRender (on-GPU fan-triangulate + DrawIndexedIndirect + the per-frame
//   live hook). All callees are already C++.
// The MATERIAL is resolved by name through the host (ECS HypermeshComponent looks it up in
// the AssetSystem artifact registry and assigns _resolved_material / sets _material_resolver);
// until it resolves, the drawable simply skips frames (loud log once).

#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/hypermesh/hmdflow.h>
#include <functional>

namespace ork::lev2::hypermesh {

struct HypermeshDrawableData final : public DrawableData {

  DeclareConcreteX(HypermeshDrawableData, DrawableData);

public:
  HypermeshDrawableData();
  ~HypermeshDrawableData();
  drawable_ptr_t createDrawable() const final;

  ///////////////////////////////////////////////////////////////
  // reflected (authored) state
  ///////////////////////////////////////////////////////////////

  dflow::graphdata_ptr_t _graphdata;     // the embedded hypermesh graph (model B)
  std::string _material_asset_name;      // PbrMaterialGenData asset to bind (artifact registry key)
  bool _animated  = false;               // re-evaluate the graph each frame (S.time assets)
  bool _face_viz  = false;               // per-triangle face-id buffer -> the face-viz FS
  bool _tag_viz   = false;               // __tags FACE channel -> the selection-group FS (implies face_viz)
  bool _wireframe = false;               // per-edge LINE overlay (needs _resolved_overlay_material to draw)
  int  _vtx_budget = 1 << 20;            // GraphInst SSBO-pool vertex capacity
  // INSTANCING (Phase 1, static): N*16 column-major mat4 floats -> the mesh draws N times in
  // ONE indirect call (FWD_SSBO_CUSTOM_INSTANCED reads storage_inst_mtx by gl_InstanceIndex).
  std::vector<float> _instance_matrices;
  // E.2 forward slot — name of an InstanceSet/ScatterSource asset that will supply the
  // matrices SSBO dynamically. Reflected now so authored scenes can carry the reference;
  // unused until the typed instance edge lands.
  std::string _instance_source_name;

  ///////////////////////////////////////////////////////////////
  // runtime state (NEVER reflected)
  ///////////////////////////////////////////////////////////////

  // the live material — resolved BY NAME by the host. Either assign directly (in-process
  // authoring / tests) or install _material_resolver (ECS: artifacts may materialize after
  // stage time, so the drawable retries each frame until the resolver yields).
  mutable pbrmaterial_ptr_t _resolved_material;
  mutable std::function<pbrmaterial_ptr_t()> _material_resolver;
  // E.4 — per-view GPU frustum cull (instanced graphs only). cull_bound is the
  // object-space bounding sphere (xyz=center, w=radius); w <= 0 = AUTO (computed
  // once at bootstrap from a position readback of the materialized mesh, padded
  // 5% — override for meshes that ANIMATE beyond their static bounds).
  bool _cull = false;
  fvec4 _cull_bound = fvec4(0, 0, 0, 0);
  // E.3 — per-gid material bindings (gid-as-string -> material asset NAME;
  // reflected). Faces whose __tags gid matches draw with that material in
  // their own bucket; unbound gids fold to the default _material_asset_name.
  std::map<std::string, std::string> _gid_material_assets;
  mutable std::map<int, pbrmaterial_ptr_t> _resolved_gid_materials;
  // generic by-name resolver (ECS installs it; same retry contract as
  // _material_resolver — polled each frame until every gid material yields)
  mutable std::function<pbrmaterial_ptr_t(const std::string&)> _material_resolver_named;
  // optional wireframe-overlay material (a Lines ptex3d) — same resolution contract.
  mutable material_ptr_t _resolved_overlay_material;
  // filled by the lazy bootstrap on first onGpuUpdate; the host reads it for the pause
  // contract (live->_paused) and introspection. One live per drawable; with the current
  // one-drawable-per-data usage this back-pointer is unambiguous.
  mutable livehypermesh_ptr_t _live;
};

using hypermesh_drawable_data_ptr_t = std::shared_ptr<HypermeshDrawableData>;

} // namespace ork::lev2::hypermesh
