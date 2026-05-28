////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

// Shared mesh-extraction + one-shot drawable build for OpenVDB
// FloatGrids. Both VdbLevelSetRenderer (per-frame, particle-driven)
// and the HYPERECS asset-DSL path call into extractMeshFromGrid here
// so the marching-cubes-to-vertex-array algorithm exists in exactly
// one place. gridToDrawable adds the GPU upload + CallbackDrawable
// wrap on top of that for the one-shot asset path.

#include <ork/pch.h>
#include <algorithm>
#include <limits>
#include <cstring>

#include <ork/lev2/gfx/vdb_drawable.h>
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/gfxenv.h>

namespace ork::lev2::vdb {

///////////////////////////////////////////////////////////////////////////////

void extractMeshFromGrid(
    const vdb_floatgrid_t&    grid,
    float                     iso,
    float                     adaptivity,
    std::vector<mesh_vtx_t>&  out_verts,
    std::vector<uint32_t>&    out_idxs,
    AABox&                    out_aabb,
    bool                      flip_windings) {

  out_verts.clear();
  out_idxs.clear();

  std::vector<openvdb::Vec3s> points;
  std::vector<openvdb::Vec3I> triangles;
  std::vector<openvdb::Vec4I> quads;
  // adaptivity=0 → no edge collapse, max detail. Bump (0..1) to trade
  // quality for triangle count. relax=false to match the renderer.
  openvdb::tools::volumeToMesh(
      grid, points, triangles, quads, iso, adaptivity, /*relax=*/false);

  if (points.empty()) {
    out_aabb.SetMinMax(fvec3(0, 0, 0), fvec3(0, 0, 0));
    return;
  }

  const size_t n_verts = points.size();
  const size_t n_idxs  = triangles.size() * 3 + quads.size() * 6;
  out_verts.reserve(n_verts);
  out_idxs.reserve(n_idxs);

  // ---- positions + AABB, zero-init normals (accumulators) ----
  fvec3 bbmin( std::numeric_limits<float>::max());
  fvec3 bbmax(-std::numeric_limits<float>::max());
  out_verts.resize(n_verts);
  for (size_t i = 0; i < n_verts; ++i) {
    auto const& p = points[i];
    fvec3 pos(p.x(), p.y(), p.z());
    out_verts[i]._position = pos;
    out_verts[i]._normal   = fvec3(0, 0, 0);
    out_verts[i]._binormal = fvec3(0, 0, 0);
    out_verts[i]._uv       = fvec2(0, 0);
    out_verts[i]._color    = 0xffffffff;
    bbmin.x = std::min(bbmin.x, pos.x); bbmin.y = std::min(bbmin.y, pos.y); bbmin.z = std::min(bbmin.z, pos.z);
    bbmax.x = std::max(bbmax.x, pos.x); bbmax.y = std::max(bbmax.y, pos.y); bbmax.z = std::max(bbmax.z, pos.z);
  }
  out_aabb.SetMinMax(bbmin, bbmax);

  // ---- accumulate area-weighted face normals + emit indices ----
  // OpenVDB outputs CCW viewed from the LOW-value side. For density
  // splats (high = inside) that's outward; for SDFs (negative = inside)
  // that's inward — flip_windings lets the caller invert the winding
  // (swap the trailing two indices) so the cross-product face normal,
  // and thus the accumulated smoothed normal, reverses direction
  // alongside the visible triangle orientation.
  //
  // Cross-product magnitude is 2× triangle area, so summing
  // un-normalized face normals gives the standard area-weighted average
  // — equivalent to what submeshWithSmoothNormals does internally, but
  // in a single pass over the triangle list with no adjacency map.
  auto accum_tri = [&](uint32_t i0, uint32_t i1, uint32_t i2) {
    uint32_t b = flip_windings ? i2 : i1;
    uint32_t c = flip_windings ? i1 : i2;
    const fvec3& p0 = out_verts[i0]._position;
    const fvec3& pb = out_verts[b]._position;
    const fvec3& pc = out_verts[c]._position;
    fvec3 e1 = pb - p0;
    fvec3 e2 = pc - p0;
    fvec3 fn = e1.crossWith(e2);
    out_verts[i0]._normal += fn;
    out_verts[b]._normal  += fn;
    out_verts[c]._normal  += fn;
    out_idxs.push_back(i0); out_idxs.push_back(b); out_idxs.push_back(c);
  };
  for (auto const& t : triangles) {
    accum_tri(t.x(), t.y(), t.z());
  }
  for (auto const& q : quads) {
    accum_tri(q.x(), q.y(), q.z());
    accum_tri(q.x(), q.z(), q.w());
  }

  // ---- normalize accumulated normals ----
  for (size_t i = 0; i < n_verts; ++i) {
    out_verts[i]._normal.normalizeInPlace();
  }
}

///////////////////////////////////////////////////////////////////////////////

drawabledata_ptr_t gridToDrawable(
    Context*            ctx,
    vdb_floatgrid_ptr_t grid,
    material_ptr_t      material,
    float               iso,
    float               adaptivity,
    bool                flip_windings) {

  OrkAssert(ctx);
  OrkAssert(grid);
  OrkAssert(material);

  using rigidprim_t     = meshutil::RigidPrimitive<mesh_vtx_t>;
  using rigidprim_ptr_t = std::shared_ptr<rigidprim_t>;

  std::vector<mesh_vtx_t> verts;
  std::vector<uint32_t>   idxs;
  AABox                   aabb;
  extractMeshFromGrid(*grid, iso, adaptivity, verts, idxs, aabb, flip_windings);

  const int n_verts = int(verts.size());
  const int n_idxs  = int(idxs.size());
  if (n_verts == 0 || n_idxs == 0) {
    return nullptr;
  }

  // Build a single-cluster, single-primgroup RigidPrimitive — same
  // layout VdbLevelSetRenderer's _uploadDirect produces, but sized to
  // exact counts (no power-of-two growth path).
  auto prim    = std::make_shared<rigidprim_t>();
  auto cluster = std::make_shared<rigidprim_t::PrimGroupCluster>();
  auto pg      = std::make_shared<meshutil::RigidPrimitiveBase::PrimitiveGroup>();
  pg->_primtype = PrimitiveType::TRIANGLES;
  cluster->_primgroups.push_back(pg);
  cluster->_aabb = aabb;
  prim->_gpuClusters.push_back(cluster);

  auto GBI = ctx->GBI();

  // ---- vertex buffer ----
  cluster->_vtxbuffer = std::make_shared<rigidprim_t::vtxbuf_t>(n_verts, 0);
  cluster->_vtxbuffer->SetNumVertices(n_verts);
  {
    auto vdst = (mesh_vtx_t*)GBI->LockVB(*cluster->_vtxbuffer, 0, n_verts);
    std::memcpy(vdst, verts.data(), n_verts * sizeof(mesh_vtx_t));
    GBI->UnLockVB(*cluster->_vtxbuffer);
  }

  // ---- index buffer ----
  // Warm-lock pattern: the VK backend sizes the underlying VkBuffer on
  // the FIRST lock from icount (vulkan_gbi.cpp:547). Pass n_idxs once
  // with a warm lock to fix the allocation at exact size, then the
  // real copy below also runs at n_idxs — both locks see the same
  // allocation. This mirrors VdbLevelSetRendererInst::_uploadDirect
  // (modules_renderer_vdb_levelset.cpp ~line 392).
  pg->_idxbuffer = std::make_shared<meshutil::RigidPrimitiveBase::idxbuf_t>(n_idxs);
  pg->_idxbuffer->SetNumIndices(n_idxs);
  GBI->LockIB(*pg->_idxbuffer, 0, n_idxs);
  GBI->UnLockIB(*pg->_idxbuffer);
  {
    auto idst = (uint32_t*)GBI->LockIB(*pg->_idxbuffer, 0, n_idxs);
    std::memcpy(idst, idxs.data(), n_idxs * sizeof(uint32_t));
    GBI->UnLockIB(*pg->_idxbuffer);
  }

  // Wrap as RigidPrimitiveDrawableData — the DrawableData form that
  // SceneGraphComponent.declareNodeOnLayer accepts (same slot as
  // ModelDrawableData / ParticlesDrawableData). It holds the
  // shared_ptr<RigidPrimitive> internally, so the GPU buffers stay
  // alive for the data's lifetime; createDrawable() called by ECS
  // returns CallbackDrawables that share the primitive.
  auto data = std::make_shared<meshutil::RigidPrimitiveDrawableData>();
  data->_primitive = prim;
  data->_material  = material;
  return data;
}

} // namespace ork::lev2::vdb
