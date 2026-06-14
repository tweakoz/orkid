////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#pragma once

// D.5 — the C++ materializers for the VDB-side asset gens (a separate TU/header from
// asset_gen.h because <openvdb> is heavy and most asset_gen consumers don't need it).
// 1:1 ports of the Python builds in hypergraph ecs/scene/assets.py:
//   ImplicitSdf.build        -> materializeImplicitSdf       (AX voxelizer -> FloatGrid)
//   VdbGridToDrawable.build  -> materializeVdbGridToDrawable (volumeToMesh -> drawable)
//   ParticleSystem.build     -> materializeParticleSystem    (embedded graph -> drawable
//                               data + sdf_asset reference resolution + probe stamp)

#include <ork/lev2/gfx/asset_gen.h>
#include <ork/lev2/gfx/openvdb.h>
#include <ork/lev2/lev2_types.h>

namespace ork::lev2 {

struct ParticlesDrawableData;
using particles_drawable_data_ptr_t = std::shared_ptr<ParticlesDrawableData>;

vdb_floatgrid_ptr_t materializeImplicitSdf(const ImplicitSdfGenData& gen);

drawabledata_ptr_t materializeVdbGridToDrawable(
    const VdbGridToDrawableGenData& gen,
    Context* ctx,
    vdb_floatgrid_ptr_t grid,
    material_ptr_t material);

// MeshGenData — baked-geometry mesh: read the .ogeo sidecar chunkfile back,
// rebuild the rigid primitive (Geometry -> submesh -> fromSubMesh), attach the
// material BY NAME-resolved artifact. The C++ twin of the Python wire's
// MeshAsset.from_gendata().build() (closes the "left to the Python wire path"
// gap that null-crashed mtl_lilies in the zero-Python player).
drawabledata_ptr_t materializeMeshGen(
    const MeshGenData& gen,
    Context* ctx,
    material_ptr_t material);

// `artifacts` = the in-progress AssetSystem registry (declaration order = dependency
// order) — sdf_asset references on the embedded graph's collider modules resolve
// against it (vdb_floatgrid_ptr_t entries). Returns null (LOUDLY) for a legacy
// model-A gendata with no embedded graph — a pure-C++ host cannot re-run Python.
particles_drawable_data_ptr_t materializeParticleSystem(
    const ParticleSystemGenData& gen,
    const varmap::VarMap& artifacts);

} // namespace ork::lev2
