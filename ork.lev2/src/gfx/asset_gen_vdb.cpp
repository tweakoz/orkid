////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// asset_gen_vdb.cpp (D.5) — the C++ materializers for the VDB-side asset gens.
// 1:1 ports of the Python builds (hypergraph ecs/scene/assets.py); every callee
// was already C++ (the pyext bodies these mirror are in pyext_gfx_openvdb.cpp).
//
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/asset_gen_vdb.h>
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>
#include <ork/lev2/gfx/meshutil/geometry.h>
#include <ork/lev2/gfx/vdb_drawable.h>
#include <ork/lev2/gfx/particle/drawable_data.h>
#include <ork/lev2/gfx/particle/modular_forces.h>
#include <ork/lev2/gfx/particle/vdbcollider_holder.inl>
#include <ork/dataflow/all.h>
#include <openvdb_ax/compiler/Compiler.h>
#include <openvdb_ax/compiler/CustomData.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
// ImplicitSdf.build port — AX voxelizer:
//   1. named FloatGrid (the name is what the shader writes: f@<grid_name>)
//   2. fillBBox ACTIVATES the eval domain (AX only iterates active voxels);
//      proper worldToIndex conversion (mirrors the pyext fillBBox)
//   3. compile the AX shader with the params as CustomData scalars
//   4. execute on the grid
///////////////////////////////////////////////////////////////////////////////

vdb_floatgrid_ptr_t materializeImplicitSdf(const ImplicitSdfGenData& gen) {
  auto xform = std::make_shared<openvdb::math::Transform>();
  xform->postScale(gen._voxel_size);
  auto grid = std::make_shared<openvdb::FloatGrid>(gen._background);
  grid->setName(gen._grid_name);
  grid->setTransform(xform);

  const auto& xf = grid->transform();
  auto mi        = xf.worldToIndex(openvdb::Vec3d(gen._bbox_min.x, gen._bbox_min.y, gen._bbox_min.z));
  auto ma        = xf.worldToIndex(openvdb::Vec3d(gen._bbox_max.x, gen._bbox_max.y, gen._bbox_max.z));
  openvdb::CoordBBox bbox(
      openvdb::Coord(int(std::floor(mi.x())), int(std::floor(mi.y())), int(std::floor(mi.z()))),
      openvdb::Coord(int(std::ceil(ma.x())), int(std::ceil(ma.y())), int(std::ceil(ma.z()))));
  grid->fill(bbox, gen._background, /*active=*/true);

  auto cdata = std::make_shared<vdb_custom_data_t>();
  if (gen._params) {
    for (const auto& item : gen._params->_themap) {
      if (auto as_f = item.second.tryAs<float>()) {
        auto typed = cdata->getOrInsertData<openvdb::TypedMetadata<float>>(item.first);
        typed->setValue(as_f.value());
      } else if (auto as_d = item.second.tryAs<double>()) {
        auto typed = cdata->getOrInsertData<openvdb::TypedMetadata<float>>(item.first);
        typed->setValue(float(as_d.value()));
      } else if (auto as_i = item.second.tryAs<int>()) {
        auto typed = cdata->getOrInsertData<openvdb::TypedMetadata<int>>(item.first);
        typed->setValue(as_i.value());
      } else {
        printf(
            "materializeImplicitSdf<%s>: param<%s> has unsupported type — skipped\n",
            gen._asset_name.c_str(),
            item.first.c_str());
      }
    }
  }
  openvdb::ax::Compiler compiler;
  auto ve = compiler.compile<vdb_volume_exec_t>(gen._shader, cdata);
  ve->execute(*grid);
  // NOTE: leave the grid GRID_UNKNOWN. openvdb's volumeToMesh winds a GRID_UNKNOWN
  // (generic) negative-inside grid CCW front-face (rasterstate default) with NO flip —
  // tagging GRID_LEVEL_SET inverts volumeToMesh's winding (it would then require a flip
  // again). So the flip_windings=false default is correct here precisely because the grid
  // is generic. (The VdbLevelSetRenderer sets GRID_LEVEL_SET because its own mesher handles
  // that class's winding; this asset path does not.)
  return grid;
}

///////////////////////////////////////////////////////////////////////////////
// VdbGridToDrawable.build port — straight delegation (the worker was already C++).
///////////////////////////////////////////////////////////////////////////////

drawabledata_ptr_t materializeVdbGridToDrawable(
    const VdbGridToDrawableGenData& gen,
    Context* ctx,
    vdb_floatgrid_ptr_t grid,
    material_ptr_t material) {
  if (not grid or not material) {
    printf(
        "materializeVdbGridToDrawable<%s>: missing %s — skipped\n",
        gen._asset_name.c_str(),
        grid ? "material" : "grid");
    return nullptr;
  }
  return vdb::gridToDrawable(ctx, grid, material, gen._iso, gen._adaptivity, gen._flip_windings);
}

///////////////////////////////////////////////////////////////////////////////
// ParticleSystem.build port — MODEL B only: the embedded graph is the drawable's
// graphdata; sdf_asset references on collider modules resolve to live grids from
// the artifact registry; the probe-entity name stamps onto the drawable data
// (ParticlesGlobalSystem reads it at stage). A pure-C++ host CANNOT re-run the
// Python DSL, so a legacy model-A gendata (no embedded graph) fails LOUDLY.
///////////////////////////////////////////////////////////////////////////////

drawabledata_ptr_t materializeMeshGen(
    const MeshGenData& gen,
    Context* ctx,
    material_ptr_t material) {
  auto path = file::Path::expandPathString(gen._geometry_path);
  auto geo  = meshutil::Geometry::readChunkfile(file::Path(path));
  if (not geo) {
    printf(
        "materializeMeshGen<%s>: geometry chunkfile MISSING/unreadable <%s> — no drawable\n",
        gen._asset_name.c_str(), path.c_str());
    return nullptr;
  }
  auto P = geo->_point.channelAs<fvec3>("P");
  if (not P) {
    printf("materializeMeshGen<%s>: geometry has no 'P' channel — no drawable\n", gen._asset_name.c_str());
    return nullptr;
  }
  auto N  = geo->_point.channelAs<fvec3>("N");
  auto B  = geo->_point.channelAs<fvec3>("binormal");
  auto UV = geo->_point.channelAs<fvec2>("uv");
  auto CD = geo->_point.channelAs<fvec4>("Cd");
  size_t npts = P->_data.size();
  auto sm     = std::make_shared<meshutil::submesh>();
  auto mkvert = [&](int i) -> meshutil::vertex {
    fvec3 p = P->_data[size_t(i)];
    fvec3 n = (N and size_t(i) < N->_data.size()) ? N->_data[size_t(i)] : fvec3(0, 1, 0);
    fvec3 b = (B and size_t(i) < B->_data.size()) ? B->_data[size_t(i)] : fvec3(1, 0, 0);
    fvec2 uv = (UV and size_t(i) < UV->_data.size()) ? UV->_data[size_t(i)] : fvec2(0, 0);
    fvec4 cd = (CD and size_t(i) < CD->_data.size()) ? CD->_data[size_t(i)] : fvec4(1, 1, 1, 1);
    return meshutil::vertex(p, n, b, uv, cd);
  };
  int np = geo->numPolys();
  for (int ip = 0; ip < np; ip++) {
    int cnt        = geo->polyVertexCount(ip);
    const int* idx = geo->polyPointIndices(ip);
    if (cnt < 3)
      continue; // line/degenerate polys don't rasterize in a rigid prim
    auto v0 = sm->mergeVertex(mkvert(idx[0]));
    for (int k = 1; k + 1 < cnt; k++) { // fan (authored ngons; baked meshes are tri-dominant)
      meshutil::vertex_vect_t tri;
      tri.push_back(v0);
      tri.push_back(sm->mergeVertex(mkvert(idx[k])));
      tri.push_back(sm->mergeVertex(mkvert(idx[k + 1])));
      sm->mergePoly(tri);
    }
  }
  auto prim = std::make_shared<meshutil::rigidprim_V12N12B12T8C4_t>();
  prim->fromSubMesh(*sm, ctx);
  auto dd        = std::make_shared<meshutil::RigidPrimitiveDrawableData>();
  dd->_material  = material;
  dd->_primitive = prim;
  printf(
      "materializeMeshGen<%s>: points<%zu> polys<%d> -> rigid primitive\n",
      gen._asset_name.c_str(), npts, np);
  return dd;
}

///////////////////////////////////////////////////////////////////////////////

particles_drawable_data_ptr_t materializeParticleSystem(
    const ParticleSystemGenData& gen,
    const varmap::VarMap& artifacts) {
  if (not gen._graph_data) {
    printf(
        "materializeParticleSystem<%s>: NO embedded graph (legacy model-A gendata) — a "
        "pure-C++ host cannot re-run the Python DSL. Re-serialize the scene.\n",
        gen._asset_name.c_str());
    return nullptr;
  }
  auto dd        = std::make_shared<ParticlesDrawableData>();
  dd->_graphdata = gen._graph_data;
  if (not gen._probe_entity_name.empty())
    dd->_probeEntityName = gen._probe_entity_name;
  dd->_emitterIntensity = gen._emitter_intensity;
  dd->_emitterRadius    = gen._emitter_radius;
  // resolve sdf_asset references — every collider module carrying a name gets its
  // live grid from the registry (declaration order = dependency order).
  auto g = gen._graph_data;
  for (size_t i = 0; i < g->numModules(); i++) {
    auto vdbc = std::dynamic_pointer_cast<particle::VdbColliderModuleData>(g->module(i));
    if (not vdbc or vdbc->_sdf_asset_name.empty())
      continue;
    auto attempt = artifacts.typedValueForKey<vdb_floatgrid_ptr_t>(vdbc->_sdf_asset_name);
    if (attempt) {
      auto holder  = std::make_shared<particle::VdbColliderGridHolder>();
      holder->grid = attempt.value();
      vdbc->_sdfGrid = holder;
      printf(
          "materializeParticleSystem<%s>: resolved sdf asset<%s> onto collider\n",
          gen._asset_name.c_str(),
          vdbc->_sdf_asset_name.c_str());
    } else {
      printf(
          "materializeParticleSystem<%s>: sdf asset<%s> NOT in the registry — collider "
          "will have no grid (is the SDF gen declared before this system?)\n",
          gen._asset_name.c_str(),
          vdbc->_sdf_asset_name.c_str());
    }
  }
  return dd;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
