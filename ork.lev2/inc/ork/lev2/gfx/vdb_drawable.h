////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include <cstdint>
#include <ork/math/box.h>
#include <ork/lev2/lev2_types.h>
#include <ork/lev2/gfx/openvdb.h>
#include <ork/lev2/gfx/gfxvtxbuf_structs.h>

namespace ork::lev2 {

// Shared mesh-extraction + drawable build path for OpenVDB FloatGrids.
// VdbLevelSetRenderer (per-frame, particle-driven) and the one-shot
// asset-DSL path (HYPERECS MeshToDrawable / vdb.gridToDrawable) both
// route through the same helpers here; the extraction algorithm is
// not duplicated between the two callers.
namespace vdb {

// PBR-ready interleaved vertex used by both paths.
using mesh_vtx_t = SVtxV12N12B12T8C4;

// Pure-CPU mesh extraction. Runs openvdb::tools::volumeToMesh on `grid`
// at the given iso and adaptivity, then builds a triangle-only indexed
// mesh: positions copied verbatim, normals = area-weighted face-normal
// average (single pass, no adjacency map), indices flattened from VDB's
// triangle+quad outputs (each quad → 2 triangles). out_aabb is the
// position-space bbox of the extracted vertices.
//
// On an empty grid: out_verts and out_idxs are cleared, out_aabb is
// set to a degenerate zero box, and the function returns. Callers must
// gate any GPU upload on out_idxs.empty().
//
// flip_windings: OpenVDB's volumeToMesh winds faces by SIGN — it treats
// `value < isovalue` as INSIDE (VolumeToMesh.h::isInsideValue) and reverses the
// quad order for inside cells, giving ONE globally-consistent orientation that is
// SHAPE-INDEPENDENT. For SDFs (inside = NEGATIVE / low value — ImplicitSdf,
// meshToLevelSet) that orientation is OUTWARD and reads correctly against the CCW
// front face (rasterstate.h), so leave flip_windings FALSE (the default). Pass
// TRUE only when the SOLID is the HIGH side — a density/fog grid where "inside" is
// high (e.g. metaball splats with background=0): openvdb then oriented for the low
// region, so the winding must be flipped. The knob is retained for that case.
void extractMeshFromGrid(
    const vdb_floatgrid_t&    grid,
    float                     iso,
    float                     adaptivity,
    std::vector<mesh_vtx_t>&  out_verts,
    std::vector<uint32_t>&    out_idxs,
    AABox&                    out_aabb,
    bool                      flip_windings = false);

// One-shot DrawableData build for an asset-style FloatGrid:
//   extract → GPU upload → wrap as RigidPrimitiveDrawableData.
//
// Returns a DrawableData usable as `drawable=` on a SceneGraphComponent
// node (the same slot ModelDrawableData / ParticlesDrawableData go into).
// The returned data owns the RigidPrimitive (and its GPU buffers); ECS
// calls createDrawable() on the data at the right time to obtain an
// actual CallbackDrawable per instance/replication. Returns nullptr if
// the grid produces an empty mesh at the given iso.
//
// Sized to exact extracted counts — no per-frame growth bookkeeping
// (this is the static-asset path; if you need per-frame growth use
// VdbLevelSetRenderer instead).
drawabledata_ptr_t gridToDrawable(
    Context*            ctx,
    vdb_floatgrid_ptr_t grid,
    material_ptr_t      material,
    float               iso           = 0.0f,
    float               adaptivity    = 0.0f,
    bool                flip_windings = false);

} // namespace vdb
} // namespace ork::lev2
