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

  dflow::graphdata_ptr_t _graphdata;     // the embedded hypermesh graph (model B); LOD tier 0
  // Phase 3c — DISTANCE LOD. _lod_graphs[i] is the coarser mesh graph drawn for instances beyond
  // _lod_distances[i] meters (ascending, parallel arrays; up to 3 entries -> 4 tiers). Authored via
  // drawable_data(lods={dist: forked_hypermesh}); materialized to N lives + fed to setupMeshRender.
  std::vector<dflow::graphdata_ptr_t> _lod_graphs;
  std::vector<float> _lod_distances;
  // LOD step #3 — IMPOSTOR tiers. Each entry is an index into the EXTRA-tier arrays (_lod_graphs /
  // _lod_distances) whose tier draws a baked hemi-octahedral billboard instead of a mesh: its _lod_graphs
  // slot is null (no mesh), the base mesh's PBR atlas is baked once at materialize, and the cull routes
  // that distance band's instances to one camera-facing quad each. Authored via lods={dist: m.imposter()}.
  std::vector<int> _impostor_lods;
  int _impostor_grid = 8;   // hemi-oct atlas view count (grid×grid); from imposter(grid=)
  int _impostor_tile = 512; // per-view atlas tile pixels (atlas = grid*tile square); from imposter(tile=)
  int _impostor_ssaa = 2;   // bake supersample factor (render tile*ssaa per view, resolve down); imposter(ssaa=)
  int _impostor_msaa = 4;   // bake multisample count; from imposter(msaa=)
  // optional PER-LOD material override (LOD-index-as-string -> material asset NAME). A tier with an
  // entry draws its WHOLE mesh with that one material (the far-LOD case: a cheaper shader); a tier with
  // no entry mirrors tier 0 (the main material + its gid buckets). Resolved by the host like _gid_*.
  std::map<std::string, std::string> _lod_material_assets;
  std::string _material_asset_name;      // PbrMaterialGenData asset to bind (artifact registry key)
  // O3 stage 3 — STORED-MODE per-section texture-ARRAY bake (opt-in, additive; the E.3 bucket path is
  // untouched). When _section_bake is set, THIS drawable draws with a SINGLE material (_material_asset_name,
  // the stored SAMPLER — surface_stored sampling one sampler2DArray per capture target at the section's
  // layer) and _gid_material_assets takes its SECOND role as the per-gid BAKE MAP: each section-layer is
  // baked with THAT gid's material's capture technique (adobe content into adobe layers, timber into timber).
  // COLD = in-frame GPU bake + content-addressed PNG cache write + placeholder->rebind; WARM = load cache.
  // Unbound gids fall back to the main material. Nothing is deprecated: _section_bake=false keeps the whole
  // classic bucket path (gid_materials -> per-gid draws) byte-identical.
  bool _section_bake     = false;
  int  _section_bake_res = 256;          // per-layer bake resolution (A8: reflected, tweakable)
  bool _section_mips     = true;         // A8: build trilinear mip chains for the baked section arrays
                                         // (minification anti-aliasing; default ON). env ORKID_SECTION_MIPS overrides.
  std::vector<std::string> _section_targets; // capture-target names (== array-sampler names) in MRT order
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
  // LOD/Phase 2 — DRAWABLE-LEVEL instance source. When _instance_sink (portable) or
  // _instance_ogeo_path (direct) is set, the drawable resolves the baked ScatterSet
  // ITSELF (fillInstanceSetFromScatter) at first build and binds live->_instances —
  // decoupled from the geometry graph, so the same shared InstanceSet can route to N
  // LOD meshes. Overrides any graph-carried ScatterSource. Authored via
  // drawable_data(instance_source=(asset, sink, type_id)).
  std::string _instance_scatter_asset; // portable: HeightField asset name
  std::string _instance_sink;          // portable: scatter sink name -> .../<asset>/<sink>.ogeo
  std::string _instance_ogeo_path;     // direct: explicit .ogeo path (tools/viewers; wins)
  int _instance_type_id = -1;          // filter to one scatter type (-1 = whole set)

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
  // occludee decomposition for HZB occlusion (tighter than the cull sphere): cull_slabs 1 = whole-mesh
  // AABB (default), N = N vertical slabs (thin trunk / wide canopy occlude independently). cull_tightness
  // scales the occludee box about its center (<1 culls harder — good for see-through foliage; 1 = exact).
  int _cull_slabs = 1;
  float _cull_tightness = 1.0f;
  float _cull_distance = 0.0f; // radial distance cull from the eye (meters; 0 = off)
  // E.3 — per-gid material bindings (gid-as-string -> material asset NAME;
  // reflected). Faces whose __tags gid matches draw with that material in
  // their own bucket; unbound gids fold to the default _material_asset_name.
  std::map<std::string, std::string> _gid_material_assets;
  mutable std::map<int, pbrmaterial_ptr_t> _resolved_gid_materials;
  // resolved per-LOD material overrides (LOD index -> material); same by-name resolver contract.
  mutable std::map<int, pbrmaterial_ptr_t> _resolved_lod_materials;
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
