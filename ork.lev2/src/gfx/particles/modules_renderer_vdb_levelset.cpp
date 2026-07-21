////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

// VdbLevelSetRenderer — particle terminus that splats live particles into
// an OpenVDB FloatGrid level set, extracts a triangle mesh via marching
// cubes (openvdb::tools::volumeToMesh), and draws it through an
// internally-owned RigidPrimitive. Header comment in modular_renderers.h
// has the full pipeline description.
//
// This file (50a) is scaffolding only — the splat (50b), marching cubes
// (50c), and per-frame draw (50d) land in follow-up commits. Currently
// onLink registers _rcidlambda and creates the FloatGrid + RigidPrimitive
// instances; compute() and _render() are no-op stubs (compute pushes an
// empty submesh to the triple buffer; _render does nothing). Building and
// inserting into a graph as a chain terminus already works.

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/gfx/particle/modular_renderers.h>
#include <ork/lev2/gfx/asset_gen.h> // _material_gen lazy materialize
#include <ork/lev2/gfx/particle/drawable_data.h>
#include <ork/lev2/gfx/openvdb.h>
#include <ork/lev2/gfx/vdb_drawable.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/meshutil/submesh_component.h>
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/util/triple_buffer.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>

using namespace ork::dataflow;

namespace ork::lev2::particle {

///////////////////////////////////////////////////////////////////////////////

struct VdbLevelSetRendererInst : public ParticleModuleInst {

  // Vertex format with pos+normal+binormal+UV+color — PBR-ready.
  using mesh_vtx_t       = SVtxV12N12B12T8C4;
  using rigidprim_t      = meshutil::RigidPrimitive<mesh_vtx_t>;
  using rigidprim_ptr_t  = std::shared_ptr<rigidprim_t>;

  // Direct vertex/index payload — skips submesh entirely. OpenVDB hands
  // us a clean indexed mesh; submesh's hash-dedup + adjacency-rebuild +
  // clusterizer machinery are all pure overhead for this case. We pass
  // the GPU-ready interleaved vertex array and 32-bit index buffer
  // straight through the triple buffer to _render, which uploads them
  // verbatim into a single RigidPrimitive cluster.
  //
  // concurrent_triple_buffer<T> requires T(int) ctor (it passes 0/1/2 to
  // tell slots apart).
  struct MeshSlot {
    std::vector<mesh_vtx_t> _verts;
    std::vector<uint32_t>   _idxs;
    AABox                   _aabb;
    MeshSlot(int /*idx*/) {}
  };
  using triple_mesh_t        = concurrent_triple_buffer<MeshSlot>;
  using triple_mesh_ptr_t    = std::shared_ptr<triple_mesh_t>;

  VdbLevelSetRendererInst(const VdbLevelSetRendererData* vrd, dataflow::GraphInst* ginst);
  void onLink(GraphInst* inst) final;
  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final;
  void _render(const RenderContextInstData& RCID);
  // Direct GPU upload — populates _rigidprim->_gpuClusters from a single
  // vertex/index pair. Grows the lev2 buffers as needed; reuses them when
  // capacity already suffices. Bypasses XgmClusterizerStd entirely.
  // (Context here is the gfx context — must be fully qualified because
  // ork::lev2::particle also defines its own Context type.)
  void _uploadDirect(lev2::Context* ctx, const MeshSlot& slot);

  const VdbLevelSetRendererData* _vrd;
  // lazily materialized from _vrd->_material_gen when the live _material is absent
  // (the post-deserialize case). Per-INST cache so the const module data stays pure
  // authored state.
  lev2::material_ptr_t _materialized_mtl;

  // Plugs (uniform-rate, FloatXf). Size is the per-particle multiplier;
  // declared uniform but evaluated per-particle when its upstream is
  // varying (pool.UnitAge → e.g. Expr.curve(Expr.ptc.unit_age, profile)).
  // Same pattern sprite/streak renderers use for their Size/Width/Length.
  floatxf_inp_pluginst_ptr_t _input_radius;
  floatxf_inp_pluginst_ptr_t _input_strength;
  floatxf_inp_pluginst_ptr_t _input_isolevel;
  floatxf_inp_pluginst_ptr_t _input_size;
  float_out_pluginst_ptr_t   _output_uage;   // pool's UnitAge driver

  // Per-graphinst owned objects. Created in onLink (FloatGrid + transform)
  // or lazily on first render (RigidPrimitive needs a Context).
  vdb_floatgrid_ptr_t _grid;
  rigidprim_ptr_t     _rigidprim;
  triple_mesh_ptr_t   _triple_mesh;

  // EWMA on previous frame's element counts → drives reserve() in compute,
  // and steady-state-zero-realloc when the metaball topology is stable.
  size_t _ewma_vert_count = 0;
  size_t _ewma_idx_count  = 0;
  // Current GPU buffer capacities (>= actual counts); resize-up only.
  int _vb_capacity = 0;
  int _ib_capacity = 0;
};

///////////////////////////////////////////////////////////////////////////////
// Kernel evaluators. Each takes a squared-normalized distance (r/R)² in
// [0, 1] (caller already early-outs if outside) and returns a falloff in
// [0, 1]. Gaussian is the exception — it never quite reaches 0 but we
// clamp by caller via the same r²<=1 test so the splat sphere stays
// compact; tail beyond r=1 just gets cut off.
static inline float _kernel_eval(VdbLevelSetKernel k, float r2) {
  switch (k) {
    case VdbLevelSetKernel::WYVILL: {
      // (1 - r²)³ — smooth, the metaball classic. r2 ∈ [0,1].
      float one_minus = 1.0f - r2;
      return one_minus * one_minus * one_minus;
    }
    case VdbLevelSetKernel::CUBIC: {
      // (1 - r)³ — sharper falloff, cheaper to evaluate than Wyvill.
      float r = std::sqrt(r2);
      float one_minus = 1.0f - r;
      return one_minus * one_minus * one_minus;
    }
    case VdbLevelSetKernel::QUARTIC: {
      // (1 - r²)² — between Cubic and Wyvill in sharpness.
      float one_minus = 1.0f - r2;
      return one_minus * one_minus;
    }
    case VdbLevelSetKernel::GAUSSIAN: {
      // exp(-α r²) with α chosen so the value at r=1 is ~0.0067
      // (≈ e⁻⁵). Compact-supported version: caller already gates r²<=1.
      return std::exp(-5.0f * r2);
    }
  }
  return 0.0f;
}

///////////////////////////////////////////////////////////////////////////////
// Splat one particle: deposit its falloff kernel into the FloatGrid.
// Iterates the axis-aligned voxel cube containing the radius-R sphere and
// accumulates kernel*strength for every voxel inside the sphere. O(R³/V³)
// per particle where V is the voxel size — picking R carefully matters.
static void _splat_particle(
    vdb_floatgrid_t& grid,
    const fvec3& world_pos,
    float radius,
    float strength,
    VdbLevelSetKernel kernel) {
  if (radius <= 0.0f or strength == 0.0f) return;

  auto accessor   = grid.getAccessor();
  const auto& xf  = grid.transform();
  float r2_max    = radius * radius;
  float inv_r2    = 1.0f / r2_max;

  // Convert center to index space; iterate the axis-aligned bbox of the
  // splat sphere in index coords; world-distance test per voxel.
  openvdb::Vec3d center_world(world_pos.x, world_pos.y, world_pos.z);
  openvdb::Vec3d center_idx_d = xf.worldToIndex(center_world);
  // Voxel-radius (index space) — assumes uniform-scale linear transform,
  // which is what we set up in onLink. If the transform ever becomes
  // anisotropic this needs revisiting.
  double voxel_size = xf.voxelSize().x();
  int    r_idx     = int(std::ceil(radius / voxel_size));
  openvdb::Coord center_idx(
      int(std::floor(center_idx_d.x())),
      int(std::floor(center_idx_d.y())),
      int(std::floor(center_idx_d.z())));

  for (int dz = -r_idx; dz <= r_idx; ++dz) {
    for (int dy = -r_idx; dy <= r_idx; ++dy) {
      for (int dx = -r_idx; dx <= r_idx; ++dx) {
        openvdb::Coord c(
            center_idx.x() + dx,
            center_idx.y() + dy,
            center_idx.z() + dz);
        openvdb::Vec3d voxel_world = xf.indexToWorld(c);
        openvdb::Vec3d d = voxel_world - center_world;
        float r2 = float(d.dot(d));
        if (r2 > r2_max) continue;
        float w = _kernel_eval(kernel, r2 * inv_r2);
        float prev = accessor.getValue(c);
        accessor.setValue(c, prev + w * strength);
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

VdbLevelSetRendererInst::VdbLevelSetRendererInst(const VdbLevelSetRendererData* vrd, dataflow::GraphInst* ginst)
    : ParticleModuleInst(vrd, ginst)
    , _vrd(vrd) {
  OrkAssert(vrd);
  _triple_mesh = std::make_shared<triple_mesh_t>();
}

///////////////////////////////////////////////////////////////////////////////

void VdbLevelSetRendererInst::onLink(GraphInst* inst) {
  _onLink(inst);
  auto ptcl_context         = inst->_impl.getShared<Context>();
  ptcl_context->setRenderLambda(this, _vrd->_draw_order, [this](const RenderContextInstData& RCID) { this->_render(RCID); });

  _input_radius   = typedInputNamed<FloatXfPlugTraits>("Radius");
  _input_strength = typedInputNamed<FloatXfPlugTraits>("Strength");
  _input_isolevel = typedInputNamed<FloatXfPlugTraits>("IsoLevel");
  _input_size     = typedInputNamed<FloatXfPlugTraits>("Size");
  // Driver for per-particle Size eval: write each particle's unit_age
  // into the pool's UnitAge output, then read _input_size->value() which
  // re-runs the transform chain (multicurve / scale / bias / ...) wired
  // upstream of Size. Same setup sprite/streak use; pool always exposes
  // UnitAge (modules_pool.cpp:115).
  auto pool = inst->firstModuleInst<ParticlePoolModuleInst>();
  OrkAssert(pool);
  _output_uage = pool->typedOutputNamed<FloatPlugTraits>("UnitAge");
  OrkAssert(_output_uage);

  // Build the FloatGrid with a transform matching the configured voxel
  // size. Background is 0 (vacuum); the splat adds positive density.
  _grid = vdb_floatgrid_t::create(/*background=*/0.0f);
  _grid->setTransform(openvdb::math::Transform::createLinearTransform(_vrd->_voxelSize));
  _grid->setGridClass(openvdb::GRID_LEVEL_SET);
}

///////////////////////////////////////////////////////////////////////////////

void VdbLevelSetRendererInst::compute(GraphInst* inst, ui::updatedata_ptr_t updata) {
  if (_pool == nullptr) return;

  float radius   = _input_radius->value();
  float strength = _input_strength->value();
  float iso      = _input_isolevel->value();

  // Size is a per-particle multiplier on radius (default 1.0). When its
  // upstream is varying (e.g. Expr.curve(Expr.ptc.unit_age, profile)),
  // we drive _output_uage->setValue(unit_age) in the loop and re-pull
  // _input_size->value() — same pattern as sprite/streak. Otherwise
  // sample once and reuse.
  const bool size_is_varying = _input_size && _input_size->connectedIsVarying();
  float size_uniform = (_input_size && !size_is_varying) ? _input_size->value() : 1.0f;

  // Clear last frame's field — v1 doesn't persist density across frames.
  _grid->clear();

  // Splat every live particle with the configured kernel.
  // Per-particle effective radius = Radius * Size * (aux.x if > 0).
  //   - Size: lifetime/varying multiplier driven by the Size plug's
  //     transform chain (Expr.curve(Expr.ptc.unit_age, ...) etc.).
  //   - aux.x: per-particle constant set at emit (e.g. Aux=Expr.rand_range(...))
  //     for static random variation. Defaults to 0 → no contribution.
  // Both default to "no effect" so existing graphs keep their old look.
  const int n = _pool->GetNumAlive();
  for (int i = 0; i < n; i++) {
    const BasicParticle* ptc = _pool->GetActiveParticle(i);
    float size_eff;
    if (size_is_varying) {
      _output_uage->setValue(ptc->_unit_age);
      size_eff = _input_size->value();
    } else {
      size_eff = size_uniform;
    }
    float r_eff = radius * size_eff;
    if (ptc->_aux.x > 0.0f) r_eff *= ptc->_aux.x;
    if (r_eff <= 0.0f) continue;  // skip degenerate kernels (curve at endpoints)
    _splat_particle(*_grid, ptc->mPosition, r_eff, strength, _vrd->_kernel);
  }

  // Marching cubes + smooth-normals + indexed-mesh emission live in
  // ork::lev2::vdb::extractMeshFromGrid (shared with the one-shot
  // asset path). This call fills the triple-buffer slot's vectors
  // directly — no copy.
  auto out = _triple_mesh->begin_push();
  vdb::extractMeshFromGrid(
      *_grid, /*iso=*/iso, /*adaptivity=*/0.0f,
      out->_verts, out->_idxs, out->_aabb);

  // EWMA (α=0.25) on the post-extract counts. Reserved capacity isn't
  // pre-applied to the slot vectors anymore — the helper does an exact
  // reserve once volumeToMesh's count is known — but the EWMA still
  // drives the GPU buffer pow-2 growth in _uploadDirect.
  _ewma_vert_count = (_ewma_vert_count * 3 + out->_verts.size()) / 4;
  _ewma_idx_count  = (_ewma_idx_count  * 3 + out->_idxs.size() ) / 4;

  _triple_mesh->end_push(out);
}

///////////////////////////////////////////////////////////////////////////////

void VdbLevelSetRendererInst::_uploadDirect(lev2::Context* ctx, const MeshSlot& slot) {
  auto GBI = ctx->GBI();
  const int n_verts = int(slot._verts.size());
  const int n_idxs  = int(slot._idxs.size());

  // Lazy-allocate the RigidPrimitive on first call, plus the single
  // cluster + primgroup we use forever. We never call fromSubMesh — we
  // own _gpuClusters directly.
  if (not _rigidprim) {
    _rigidprim = std::make_shared<rigidprim_t>();
  }
  if (_rigidprim->_gpuClusters.empty()) {
    auto cluster = std::make_shared<rigidprim_t::PrimGroupCluster>();
    auto pg = std::make_shared<meshutil::RigidPrimitiveBase::PrimitiveGroup>();
    pg->_primtype = PrimitiveType::TRIANGLES;
    cluster->_primgroups.push_back(pg);
    _rigidprim->_gpuClusters.push_back(cluster);
  }
  auto cluster = _rigidprim->_gpuClusters[0];
  auto pg      = cluster->_primgroups[0];

  // ---- grow vertex buffer if needed ----
  // Round up to next 2^k to amortize realloc cost across many frames.
  auto next_pow2 = [](int v) { int r = 64; while (r < v) r <<= 1; return r; };
  if (n_verts > _vb_capacity) {
    int new_cap = next_pow2(n_verts);
    if (cluster->_vtxbuffer) GBI->ReleaseVB(*cluster->_vtxbuffer);
    cluster->_vtxbuffer = std::make_shared<rigidprim_t::vtxbuf_t>(new_cap, 0);
    _vb_capacity = new_cap;
  }
  if (n_idxs > _ib_capacity) {
    int new_cap = next_pow2(n_idxs);
    if (pg->_idxbuffer) GBI->ReleaseIB(*pg->_idxbuffer);
    pg->_idxbuffer = std::make_shared<meshutil::RigidPrimitiveBase::idxbuf_t>(new_cap);
    // The VK index-buffer allocation is sized on FIRST lock from icount
    // (vulkan_gbi.cpp:547). If our first real lock passes a smaller
    // icount=n_idxs, the allocation is undersized forever. Warm-lock at
    // full capacity now so the underlying VkBuffer holds new_cap entries;
    // subsequent per-frame locks only map a sub-range.
    pg->_idxbuffer->SetNumIndices(new_cap);
    GBI->LockIB(*pg->_idxbuffer, 0, new_cap);
    GBI->UnLockIB(*pg->_idxbuffer);
    _ib_capacity = new_cap;
  }

  // ---- upload vertices ----
  cluster->_vtxbuffer->SetNumVertices(n_verts);
  if (n_verts > 0) {
    auto vdst = (mesh_vtx_t*)GBI->LockVB(*cluster->_vtxbuffer, 0, n_verts);
    std::memcpy(vdst, slot._verts.data(), n_verts * sizeof(mesh_vtx_t));
    GBI->UnLockVB(*cluster->_vtxbuffer);
  }
  cluster->_aabb = slot._aabb;

  // ---- upload indices ----
  // Pass explicit icount=n_idxs to map exactly the live region (the VK
  // buffer was already allocated at full capacity above). Setting
  // miNumIndices must come AFTER any defaulted-icount lock paths — but
  // here we always pass explicit icount so order doesn't matter; we set
  // it for the DrawIndexed call which reads GetNumIndices().
  if (n_idxs > 0) {
    auto idst = (uint32_t*)GBI->LockIB(*pg->_idxbuffer, 0, n_idxs);
    std::memcpy(idst, slot._idxs.data(), n_idxs * sizeof(uint32_t));
    GBI->UnLockIB(*pg->_idxbuffer);
  }
  pg->_idxbuffer->SetNumIndices(n_idxs);
}

///////////////////////////////////////////////////////////////////////////////

void VdbLevelSetRendererInst::_render(const RenderContextInstData& RCID) {
  auto context = RCID.context();
  auto RCFD    = RCID.rcfd();
  const auto& CPD = RCFD->topCPD();

  // Don't render during picking passes — the marching-cubes mesh isn't
  // pickable today.
  if (CPD.isPicking()) return;

  auto material = _vrd->_material;
  if (not material and _materialized_mtl)
    material = _materialized_mtl;
  if (not material and _vrd->_material_gen) {
    // model B: the live material dropped on round-trip; materialize the reflected
    // recipe ONCE (the D.1 C++ materializer — ctx is available here).
    _materialized_mtl = _vrd->_material_gen->materialize(context);
    material          = _materialized_mtl;
  }
  if (not material) return;

  // Pull the most recent vertex/index pair produced by compute().
  // begin_pull returns nullptr if compute hasn't run yet (ECS slot still
  // FREE).
  auto slot = _triple_mesh->begin_pull();
  if (not slot) return;
  if (slot->_idxs.empty()) {
    _triple_mesh->end_pull(slot);
    return;
  }

  // Direct upload — no submesh, no clusterizer. Reuses GPU buffers
  // whenever counts fit, grows pow-2 when they don't.
  _uploadDirect(context, *slot);

  // Mirror RigidPrimitive::installInCallbackDrawable's draw path: build
  // the permutation, look up the pipeline from the material's cache,
  // wrappedDrawCall → renderEML. renderEML just walks _gpuClusters and
  // issues DrawIndexedPrimitiveEML — nothing in it cares whether the
  // clusters were built by fromSubMesh or by us.
  bool is_alpha = false;
  if (auto as_pbr = std::dynamic_pointer_cast<PBRMaterial>(material)) {
    is_alpha = as_pbr->_alphaBlend;
  }
  FxPipelinePermutation permu;
  permu._stereo          = false;
  permu._instanced       = false;
  permu._skinned         = false;
  permu._is_picking      = false;
  permu._has_vtxcolors   = true;
  permu._is_alpha        = is_alpha;
  permu._rendering_model = RCFD->_renderingmodel._modelID;
  auto fxcache  = material->pipelineCache();
  auto pipeline = fxcache->findPipeline(permu);
  OrkAssert(pipeline != nullptr);

  pipeline->wrappedDrawCall(RCID, [&]() {
    _rigidprim->renderEML(context);
  });

  _triple_mesh->end_pull(slot);
}

///////////////////////////////////////////////////////////////////////////////

VdbLevelSetRendererData::VdbLevelSetRendererData() {
  // _material is null by default — author must supply one (PBRMaterial
  // is the expected default; vdb_sculpt.py uses shaders.createPbrMaterialWithColor).
}

///////////////////////////////////////////////////////////////////////////////

static void _reshapeVdbLevelSetIOs(dataflow::moduledata_ptr_t mdata) {
  auto typed = std::dynamic_pointer_cast<VdbLevelSetRendererData>(mdata);
  // Each plug carries BOTH the eval-clamp _range AND the E1 editor-slider annotateRange
  // (the propsheet reads annotateRange via plugSpec; _range alone left the slider on the
  // absurd default). Ranges are justified per the level-set kernel semantics:
  //   Radius   — per-particle splat-sphere falloff radius (world units): 0.01 (a sub-voxel
  //              dot) .. 10 (a broad merged blob). Default ~0.5.
  //   Strength — per-particle kernel density accumulated into the field: 0 (no contribution)
  //              .. 100 (dense; a few overlapping particles already exceed a high iso).
  //   IsoLevel — the density threshold the surface is extracted at: 0 .. 10; ~0.5 is the
  //              two-overlapping-kernels-visible default, higher needs proportionally
  //              denser Strength.
  //   Size     — per-particle radius multiplier around a unit default: 0 .. 10.
  auto radius = ModuleData::createInputPlug<FloatXfPlugTraits>(mdata, EPR_UNIFORM, "Radius");
  radius->_range = {0.01f, 10.0f};
  radius->annotateRange(0.01f, 10.0f);
  auto strength = ModuleData::createInputPlug<FloatXfPlugTraits>(mdata, EPR_UNIFORM, "Strength");
  strength->_range = {0.0f, 100.0f};
  strength->annotateRange(0.0f, 100.0f);
  auto iso = ModuleData::createInputPlug<FloatXfPlugTraits>(mdata, EPR_UNIFORM, "IsoLevel");
  iso->_range = {0.0f, 10.0f};
  iso->annotateRange(0.0f, 10.0f);
  // Size is declared UNIFORM (matches the sprite/streak Size/Width/Length pattern) but
  // compute() reads it per-particle when connectedIsVarying() is true. Listed in
  // _VARYING_FRIENDLY_PLUGS in the DSL bindings so per-particle exprs
  // (Expr.curve(Expr.ptc.unit_age, ...)) are accepted on it.
  auto size = ModuleData::createInputPlug<FloatXfPlugTraits>(mdata, EPR_UNIFORM, "Size");
  size->_range = {0.0f, 10.0f};
  size->annotateRange(0.0f, 10.0f);
}

///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VdbLevelSetRendererData> VdbLevelSetRendererData::createShared() {
  auto data = std::make_shared<VdbLevelSetRendererData>();
  _initPoolIOs(data);
  _reshapeVdbLevelSetIOs(data);
  // Default plug values.
  auto in_radius   = std::dynamic_pointer_cast<inplugdata<FloatXfPlugTraits>>(data->inputNamed("Radius"));
  auto in_strength = std::dynamic_pointer_cast<inplugdata<FloatXfPlugTraits>>(data->inputNamed("Strength"));
  auto in_iso      = std::dynamic_pointer_cast<inplugdata<FloatXfPlugTraits>>(data->inputNamed("IsoLevel"));
  auto in_size     = std::dynamic_pointer_cast<inplugdata<FloatXfPlugTraits>>(data->inputNamed("Size"));
  if (in_radius)   in_radius->setValue(0.5f);
  if (in_strength) in_strength->setValue(1.0f);
  if (in_iso)      in_iso->setValue(0.5f);
  if (in_size)     in_size->setValue(1.0f);   // identity multiplier by default
  return data;
}

///////////////////////////////////////////////////////////////////////////////

dgmoduleinst_ptr_t VdbLevelSetRendererData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<VdbLevelSetRendererInst>(this, ginst);
}

///////////////////////////////////////////////////////////////////////////////

void VdbLevelSetRendererData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t {
    return VdbLevelSetRendererData::createShared();
  });
  clazz->directProperty("sort", &VdbLevelSetRendererData::_sort);
  // voxel_size = the VDB grid voxel edge in WORLD units. Floor 0.01: below this each
  // particle's splat sphere rasterizes into a cubically-growing voxel count (OOM risk);
  // ceiling 1.0: coarser than ~particle spacing and the level set stops forming a
  // connected surface. Default 0.1 sits mid-slider. (editor.range.min/max scalars are
  // what moduleClasses()->prop_meta surfaces to the propsheet; a bare directProperty
  // otherwise falls back to the absurd default slider.)
  clazz->directProperty("voxel_size", &VdbLevelSetRendererData::_voxelSize)
      ->annotate<ConstString>("editor.range.min", "0.01")
      ->annotate<ConstString>("editor.range.max", "1.0");
  // the SERIALIZABLE material recipe (model B) — the live _material is runtime-only
  // and silently dropped on round-trip (invisible blobs); this gen re-materializes
  // it at first render. (kernel enum reflection deferred; WYVILL default matches.)
  clazz->directObjectProperty("material_gen", &VdbLevelSetRendererData::_material_gen);
  clazz->annotateTyped<moduleIOreshape_fn_t>("reshapeIOs", [](dataflow::moduledata_ptr_t mdata) {
    _reshapeVdbLevelSetIOs(mdata);
  });
}

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::VdbLevelSetRendererData, "psys::VdbLevelSetRendererData");
