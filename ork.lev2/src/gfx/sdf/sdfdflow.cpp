////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// sdfdflow.cpp — sdfgrid family commons: the SdfGrid plug glue (data_to_inst +
// the plug-class reflection specializations), the module base, and the M0 gate.
//
////////////////////////////////////////////////////////////////

#include "sdfdflow_module.h"
#include "../hypermesh/hmdflow_module.h" // M1 oracle: mesh primitives + topology helpers
#include <ork/kernel/datacache.h>        // M4c: DataBlock hasher for cookComputeHash
#include <ork/util/logger.h>
#include <openvdb/openvdb.h>                 // M1 oracle: meshToLevelSet is the sign/distance reference
#include <openvdb/tools/MeshToVolume.h>
#include <openvdb/tools/Interpolation.h>

///////////////////////////////////////////////////////////////////////////////
// plug glue — the SdfGrid interchange plug's family-side instantiations.
///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2::dflowgfx {

sdfgrid_inst_ptr_t SdfGridPlugTraits::data_to_inst(sdfgrid_data_ptr_t inp) {
  return std::make_shared<SdfGridInst>(inp);
}

} // namespace ork::lev2::dflowgfx

namespace dflow = ::ork::dataflow;
namespace dfg   = ork::lev2::dflowgfx;

template <> //
void dfg::sdfgrid_outplugdata_t::describeX(class_t* clazz) {
}
template <> //
void dfg::sdfgrid_inplugdata_t::describeX(class_t* clazz) {
}
template <> //
dflow::inpluginst_ptr_t dfg::sdfgrid_inplugdata_t::createInstance(ModuleInst* minst) const {
  return std::make_shared<dfg::sdfgrid_inpluginst_t>(this, minst);
}
template <> //
dflow::outpluginst_ptr_t dfg::sdfgrid_outplugdata_t::createInstance(ModuleInst* minst) const {
  return std::make_shared<dfg::sdfgrid_outpluginst_t>(this, minst);
}

ImplementTemplateReflectionX(dfg::sdfgrid_outplugdata_t, "dflowgfx::sdfgridoutplug");
ImplementTemplateReflectionX(dfg::sdfgrid_inplugdata_t, "dflowgfx::sdfgridinpplug");

ImplementReflectionX(ork::lev2::sdf::SdfModuleData, "sdf::SdfModuleData");

namespace ork::lev2::sdf {

void SdfModuleData::describeX(class_t* clazz) {
}

// M4c (hash) — content-distinct cook hash for SDF nodes (see SdfComputeInst in the header).
// Mirrors MeshComputeInst::cookComputeHash with an "sdf" epoch: class + reflected content
// (uuid-stripped, via hypermeshModuleIdentityHash — captures expression/op/max_iterations +
// dim/extent/center plug values) + context + the upstream Merkle. Makes a downstream
// SdfToMesh's input_hashes DISTINGUISH different SDF subgraphs, so its mesh cook key no
// longer collides across `<sdf>.to_mesh()` graphs.
uint64_t SdfComputeInst::cookComputeHash(const std::vector<uint64_t>& input_hashes, uint64_t context) const {
  auto h = DataBlock::createHasher();
  h->accumulateString("sdf.cook.v1");
  h->accumulateString(std::string(_dgmodule_data->GetClass()->Name().c_str()));
  h->accumulateItem<uint64_t>(hypermesh::hypermeshModuleIdentityHash(_dgmodule_data));
  h->accumulateItem<uint64_t>(context);
  for (auto x : input_hashes)
    h->accumulateItem<uint64_t>(x);
  h->finish();
  return h->result();
}

///////////////////////////////////////////////////////////////////////////////
// M0 gate — see sdfdflow.h. Hand-built SdfEval graphs through the REAL driver
// machinery (registers + MeshEnv + pool + dispatch phases); brick readback vs
// the ANALYTIC oracle.
///////////////////////////////////////////////////////////////////////////////

static dflow::dgcontext_ptr_t _sdfTestRegisters() {
  auto dgctx = std::make_shared<dflow::dgcontext>();
  dgctx->createRegisters<dflowgfx::SdfGridData>("sdf_grid", 16);
  dgctx->createRegisters<hypermesh::GpuMeshData>("sdf_mesh", 16); // M1: mesh -> sdf chains
  dgctx->createRegisters<int>("sdf_int", 64);
  dgctx->createRegisters<float>("sdf_float", 64);
  dgctx->createRegisters<fvec3>("sdf_vec3", 64);
  return dgctx;
}

// one full eval of a (mesh-free) sdf graph: pre-phase host writes + one dispatch phase.
static void _sdfTestEval(Context* ctx, dflow::graphinst_ptr_t ginst) {
  auto updata      = std::make_shared<ui::UpdateData>();
  updata->_abstime = 0.0;
  updata->_dt      = 0.0;
  ctx->beginFrame();
  for (auto inst : ginst->_ordered_module_insts)
    if (auto pp = std::dynamic_pointer_cast<dflowgfx::IPrePhaseParams>(inst))
      pp->writeParams(ctx);
  auto ci = ctx->CI();
  ci->beginDispatchPhase();
  for (auto inst : ginst->_ordered_module_insts)
    inst->compute(ginst.get(), updata);
  ci->endDispatchPhase(); // submit + WAIT -> readback below is valid
  ctx->endFrame();
}

static std::vector<uint32_t> _readU(FxInterface* fxi, FxShaderStorageBuffer* b, int count) {
  std::vector<uint32_t> v(count);
  auto m = fxi->mapStorageBuffer(b, 0, size_t(count) * 4, BufferMapAccess::READ_ONLY);
  std::memcpy(v.data(), m->_mappedaddr, size_t(count) * 4);
  fxi->unmapStorageBuffer(m.get());
  return v;
}

static std::vector<float> _sdfReadBrick(Context* ctx, const dflowgfx::SdfGridInst& g) {
  size_t n = size_t(g._dim[0]) * g._dim[1] * g._dim[2];
  std::vector<float> v(n);
  auto fxi = ctx->FXI();
  auto m   = fxi->mapStorageBuffer(g._ssbo, 0, n * 4, BufferMapAccess::READ_ONLY);
  std::memcpy(v.data(), m->_mappedaddr, n * 4);
  fxi->unmapStorageBuffer(m.get());
  return v;
}

int sdfGridSelfTest(Context* ctx) {
  int fails  = 0;
  auto CHECK = [&](bool ok, const char* what, double detail = 0.0) {
    printf("[sdfgrid oracle] %s %s (%g)\n", ok ? "PASS" : "FAIL", what, detail);
    if (not ok)
      fails++;
  };

  // analytic oracles, mirrored exactly by the GLSL expressions below
  auto sphere = [](const fvec3& p, const fvec3& c, float r) -> float { return (p - c).length() - r; };

  struct Case {
    const char* name;
    std::string expr;
    std::function<float(const fvec3&)> oracle;
  };
  std::vector<Case> cases = {
      {"sphere",
       "length(p - vec3(0.5, 0.0, 0.0)) - 1.25",
       [&](const fvec3& p) { return sphere(p, fvec3(0.5f, 0, 0), 1.25f); }},
      {"two-sphere-union",
       "min(length(p) - 1.0, length(p - vec3(1.5, 0.0, 0.0)) - 0.8)",
       [&](const fvec3& p) { return std::min(sphere(p, fvec3(0, 0, 0), 1.0f), sphere(p, fvec3(1.5f, 0, 0), 0.8f)); }},
  };

  for (const auto& C : cases) {
    auto g    = std::make_shared<dflow::GraphData>();
    auto node = SdfEvalData::createShared();
    node->_expression = C.expr;
    node->typedInputNamed<dflow::IntPlugTraits>("dim")->setValue(32);
    node->typedInputNamed<dflow::FloatPlugTraits>("extent")->setValue(4.0f);
    dflow::GraphData::addModule(g, "eval", node);
    auto dgctx  = _sdfTestRegisters();
    auto sorter = std::make_shared<dflow::DgSorter>(g.get(), dgctx);
    auto topo   = sorter->generateTopology();
    OrkAssert(topo);
    auto ginst = dflow::GraphData::createGraphInst(g);
    auto env   = std::make_shared<hypermesh::MeshEnv>();
    env->_ctx  = ctx;
    env->_pool = std::make_shared<hypermesh::MeshPool>(ctx);
    ginst->_impl.setShared<hypermesh::MeshEnv>(env);
    ginst->updateTopology(topo);
    _sdfTestEval(ctx, ginst);

    auto outp = ginst->_ordered_module_insts[0]->typedOutputNamed<dflowgfx::SdfGridPlugTraits>("Out");
    OrkAssert(outp and outp->_value);
    auto& grid = *outp->_value;
    CHECK(grid._ssbo != nullptr and grid._dim[0] == 32, (std::string(C.name) + " brick allocated").c_str(), grid._dim[0]);
    auto vals = _sdfReadBrick(ctx, grid);
    // every voxel vs the analytic distance (same float math modulo reassociation)
    double maxerr = 0.0;
    for (int iz = 0; iz < grid._dim[2]; iz++)
      for (int iy = 0; iy < grid._dim[1]; iy++)
        for (int ix = 0; ix < grid._dim[0]; ix++) {
          fvec3 p(
              grid._origin[0] + ix * grid._voxel,
              grid._origin[1] + iy * grid._voxel,
              grid._origin[2] + iz * grid._voxel);
          size_t i   = size_t(ix) + size_t(grid._dim[0]) * (size_t(iy) + size_t(grid._dim[1]) * iz);
          double err = std::abs(double(vals[i]) - double(C.oracle(p)));
          maxerr     = std::max(maxerr, err);
        }
    CHECK(maxerr < 1e-3, (std::string(C.name) + " analytic max-error").c_str(), maxerr);
    // sign sanity at known points: the brick center is INSIDE every case here;
    // the corner voxel is OUTSIDE.
    {
      int cx = grid._dim[0] / 2, cy = grid._dim[1] / 2, cz = grid._dim[2] / 2;
      size_t ci = size_t(cx) + size_t(grid._dim[0]) * (size_t(cy) + size_t(grid._dim[1]) * cz);
      CHECK(vals[ci] < 0.0f, (std::string(C.name) + " center inside").c_str(), vals[ci]);
      CHECK(vals[0] > 0.0f, (std::string(C.name) + " corner outside").c_str(), vals[0]);
    }
    // RUNTIME PLUG poke: dim 32 -> 48 reallocs the brick + recomputes (no recompile)
    if (std::string(C.name) == "sphere") {
      node->typedInputNamed<dflow::IntPlugTraits>("dim")->setValue(48);
      _sdfTestEval(ctx, ginst);
      CHECK(grid._dim[0] == 48 and grid._ssbo != nullptr, "dim poke reallocs + recomputes", grid._dim[0]);
      auto v2 = _sdfReadBrick(ctx, grid);
      fvec3 p0(grid._origin[0], grid._origin[1], grid._origin[2]);
      CHECK(std::abs(v2[0] - C.oracle(p0)) < 1e-3, "post-poke corner value", v2[0]);
    }
  }

  printf("=== sdfgrid selftest %s (%d failures) ===\n", fails ? "FAILED" : "PASSED", fails);
  return fails;
}

///////////////////////////////////////////////////////////////////////////////
// M1 oracle — see sdfdflow.h. openvdb::meshToLevelSet (built from the SAME
// readback geometry, identical world frame) is the battle-tested sign/distance
// reference; the FORCED pseudonormal-vs-winding equivalence cross-checks the
// two sign paths against each other (any feature-classification or edge-table
// misread shows as a field divergence).
///////////////////////////////////////////////////////////////////////////////

namespace {
struct VoxRun { // one prim -> mesh_to_sdf chain, evaluated through the real machinery
  dflow::graphinst_ptr_t _ginst;
  meshtosdfdata_ptr_t _m2s;
  dflowgfx::sdfgrid_inst_ptr_t _grid;
  hypermesh::gpumesh_ptr_t _mesh;
};
} // namespace

static void _voxEval(Context* ctx, dflow::graphinst_ptr_t ginst) {
  _sdfTestEval(ctx, ginst);
  // the topology-readback cascade (subdivide-style ops build CPU tables after eval 1)
  bool reeval = false;
  for (auto inst : ginst->_ordered_module_insts)
    if (auto mci = std::dynamic_pointer_cast<hypermesh::MeshComputeInst>(inst))
      reeval |= mci->onTopologyReady(ctx);
  if (reeval)
    _sdfTestEval(ctx, ginst);
}

static VoxRun _voxRun(Context* ctx, dflow::dgmoduledata_ptr_t prim, int dim, int sign_mode, float extent) {
  VoxRun r;
  auto g   = std::make_shared<dflow::GraphData>();
  auto m2s = MeshToSdfData::createShared();
  m2s->typedInputNamed<dflow::IntPlugTraits>("dim")->setValue(dim);
  // the MATH oracle uses an EXPLICIT extent — a deterministic brick frame in ONE
  // eval (AUTO-fit's pre-compute readback latency is a separate convenience, not
  // what this gate verifies). extent>0 skips the readback entirely.
  m2s->typedInputNamed<dflow::FloatPlugTraits>("extent")->setValue(extent);
  m2s->_sign_mode = sign_mode;
  dflow::GraphData::addModule(g, "prim", prim);
  dflow::GraphData::addModule(g, "m2s", m2s);
  g->safeConnect(m2s->typedInputNamed<hypermesh::MeshPlugTraits>("In"), prim->typedOutputNamed<hypermesh::MeshPlugTraits>("Out"));
  auto dgctx  = _sdfTestRegisters();
  auto sorter = std::make_shared<dflow::DgSorter>(g.get(), dgctx);
  auto topo   = sorter->generateTopology();
  OrkAssert(topo);
  r._ginst   = dflow::GraphData::createGraphInst(g);
  auto env   = std::make_shared<hypermesh::MeshEnv>();
  env->_ctx  = ctx;
  env->_pool = std::make_shared<hypermesh::MeshPool>(ctx);
  r._ginst->_impl.setShared<hypermesh::MeshEnv>(env);
  r._ginst->updateTopology(topo);
  _voxEval(ctx, r._ginst);
  r._m2s = m2s;
  for (auto inst : r._ginst->_ordered_module_insts) {
    if (auto mo = inst->typedOutputNamed<hypermesh::MeshPlugTraits>("Out"))
      if (mo->_value and not mo->_value->_channels.empty())
        r._mesh = mo->_value;
    if (auto go = inst->typedOutputNamed<dflowgfx::SdfGridPlugTraits>("Out"))
      if (go->_value and go->_value->_ssbo)
        r._grid = go->_value;
  }
  return r;
}

int sdfVoxelizeSelfTest(Context* ctx) {
  openvdb::initialize(); // idempotent
  int fails  = 0;
  auto CHECK = [&](bool ok, const char* what, double detail = 0.0) {
    printf("[sdf voxelize oracle] %s %s (%g)\n", ok ? "PASS" : "FAIL", what, detail);
    if (not ok)
      fails++;
  };

  struct Case {
    const char* name;
    float extent; // explicit brick edge (generous: contains the mesh + band margin)
    std::function<dflow::dgmoduledata_ptr_t()> make;
  };
  std::vector<Case> cases = {
      {"box", 3.0f, []() -> dflow::dgmoduledata_ptr_t {
         auto b = hypermesh::BoxData::createShared();
         b->typedInputNamed<dflow::FloatPlugTraits>("size")->setValue(1.6f);
         return b;
       }},
      {"icosphere", 3.2f, []() -> dflow::dgmoduledata_ptr_t {
         auto s = hypermesh::IcoSphereData::createShared();
         s->typedInputNamed<dflow::FloatPlugTraits>("radius")->setValue(1.2f);
         s->typedInputNamed<dflow::IntPlugTraits>("subdivisions")->setValue(2);
         return s;
       }},
  };

  const int DIM = 40;
  for (const auto& C : cases) {
    auto run = _voxRun(ctx, C.make(), DIM, -1 /*AUTO sign mode*/, C.extent);
    CHECK(run._grid != nullptr and run._mesh != nullptr, (std::string(C.name) + " chain materialized").c_str());
    if (not run._grid or not run._mesh)
      continue;
    auto& grid = *run._grid;
    auto vals  = _sdfReadBrick(ctx, grid);
    float voxel = grid._voxel;

    // ---- the openvdb reference, built from the SAME readback geometry ----
    auto fxi = ctx->FXI();
    int nv = run._mesh->_num_verts, nf = run._mesh->_num_faces, nc = run._mesh->_num_corners;
    std::vector<float> P(size_t(nv) * 4);
    {
      auto m = fxi->mapStorageBuffer(run._mesh->channel(hypermesh::MeshChannel::POSITION)->_ssbo, 0, size_t(nv) * 16, BufferMapAccess::READ_ONLY);
      std::memcpy(P.data(), m->_mappedaddr, size_t(nv) * 16);
      fxi->unmapStorageBuffer(m.get());
    }
    auto VI = _readU(fxi, run._mesh->_vidx->_ssbo, nc);
    auto FO = _readU(fxi, run._mesh->_face_offsets->_ssbo, nf + 1);
    std::vector<openvdb::Vec3s> pts(nv);
    for (int i = 0; i < nv; i++)
      pts[i] = openvdb::Vec3s(P[i * 4], P[i * 4 + 1], P[i * 4 + 2]);
    std::vector<openvdb::Vec3I> tris;
    for (int f = 0; f < nf; f++)
      for (uint32_t k = FO[f] + 1; k + 1 < FO[f + 1]; k++)
        tris.push_back(openvdb::Vec3I(VI[FO[f]], VI[k], VI[k + 1]));
    auto xform = openvdb::math::Transform::createLinearTransform(double(voxel));
    auto vdb   = openvdb::tools::meshToLevelSet<openvdb::FloatGrid>(*xform, pts, tris, 4.0f);
    openvdb::tools::GridSampler<openvdb::FloatGrid, openvdb::tools::BoxSampler> samp(*vdb);
    float bg = float(vdb->background());

    // ---- sign agreement everywhere vdb is RESOLVED; distance agreement near band ----
    int sign_mismatch = 0, dist_checked = 0;
    double maxderr = 0.0;
    for (int iz = 0; iz < grid._dim[2]; iz++)
      for (int iy = 0; iy < grid._dim[1]; iy++)
        for (int ix = 0; ix < grid._dim[0]; ix++) {
          size_t i = size_t(ix) + size_t(grid._dim[0]) * (size_t(iy) + size_t(grid._dim[1]) * iz);
          double px = grid._origin[0] + ix * voxel;
          double py = grid._origin[1] + iy * voxel;
          double pz = grid._origin[2] + iz * voxel;
          float v  = samp.wsSample(openvdb::Vec3R(px, py, pz));
          if (std::abs(v) >= bg * 0.98f)
            continue; // beyond the band's flood-filled plateau edge -> interpolation junk
          if (std::abs(v) > 0.35f * voxel) // exempt the immediate surface straddle
            if ((vals[i] < 0.0f) != (v < 0.0f))
              sign_mismatch++;
          if (std::abs(v) < 2.0f * voxel) {
            maxderr = std::max(maxderr, std::abs(double(vals[i]) - double(v)));
            dist_checked++;
          }
        }
    CHECK(sign_mismatch == 0, (std::string(C.name) + " sign agreement vs openvdb").c_str(), sign_mismatch);
    CHECK(dist_checked > 100 and maxderr < 0.75 * voxel, (std::string(C.name) + " near-band distance vs openvdb (max err / voxel)").c_str(), maxderr / voxel);

    // ---- FORCED pseudonormal vs winding: the two sign paths must agree ----
    run._m2s->_sign_mode = 0; // pseudonormal
    _voxEval(ctx, run._ginst);
    auto valsP = _sdfReadBrick(ctx, grid);
    run._m2s->_sign_mode = 1; // winding
    _voxEval(ctx, run._ginst);
    auto valsW = _sdfReadBrick(ctx, grid);
    int mode_mismatch = 0;
    for (size_t i = 0; i < valsP.size(); i++) {
      if (std::abs(valsP[i]) <= 0.25f * voxel)
        continue; // surface straddle: either sign is defensible
      if ((valsP[i] < 0.0f) != (valsW[i] < 0.0f))
        mode_mismatch++;
    }
    CHECK(mode_mismatch == 0, (std::string(C.name) + " pseudonormal == winding sign field").c_str(), mode_mismatch);
  }

  printf("=== sdf voxelize selftest %s (%d failures) ===\n", fails ? "FAILED" : "PASSED", fails);
  return fails;
}

///////////////////////////////////////////////////////////////////////////////
// M2 csg oracle — two analytic SdfEval bricks (same frame) -> Csg -> assert every
// voxel against the analytic boolean. Both inputs share the frame, so the
// trilinear B-sample reduces to the identity and the result is exact.
///////////////////////////////////////////////////////////////////////////////

int sdfCsgSelfTest(Context* ctx) {
  int fails  = 0;
  auto CHECK = [&](bool ok, const char* what, double detail = 0.0) {
    printf("[sdf csg oracle] %s %s (%g)\n", ok ? "PASS" : "FAIL", what, detail);
    if (not ok)
      fails++;
  };
  auto sphere = [](const fvec3& p, const fvec3& c, float r) -> float { return (p - c).length() - r; };
  auto oracleA = [&](const fvec3& p) { return sphere(p, fvec3(-0.4f, 0, 0), 1.2f); };
  auto oracleB = [&](const fvec3& p) { return sphere(p, fvec3(0.4f, 0, 0), 1.0f); };

  struct Case { const char* name; int op; std::function<float(float, float)> combine; };
  std::vector<Case> cases = {
      {"union", 0, [](float a, float b) { return std::min(a, b); }},
      {"intersect", 1, [](float a, float b) { return std::max(a, b); }},
      {"subtract", 2, [](float a, float b) { return std::max(a, -b); }},
  };

  for (const auto& C : cases) {
    auto g  = std::make_shared<dflow::GraphData>();
    auto eA = SdfEvalData::createShared();
    auto eB = SdfEvalData::createShared();
    eA->_expression = "length(p - vec3(-0.4, 0.0, 0.0)) - 1.2";
    eB->_expression = "length(p - vec3(0.4, 0.0, 0.0)) - 1.0";
    for (auto e : {eA, eB}) {
      e->typedInputNamed<dflow::IntPlugTraits>("dim")->setValue(32);
      e->typedInputNamed<dflow::FloatPlugTraits>("extent")->setValue(4.0f);
    }
    auto csg   = CsgData::createShared();
    csg->_op   = C.op;
    dflow::GraphData::addModule(g, "evalA", eA);
    dflow::GraphData::addModule(g, "evalB", eB);
    dflow::GraphData::addModule(g, "csg", csg);
    g->safeConnect(csg->typedInputNamed<dflowgfx::SdfGridPlugTraits>("A"), eA->typedOutputNamed<dflowgfx::SdfGridPlugTraits>("Out"));
    g->safeConnect(csg->typedInputNamed<dflowgfx::SdfGridPlugTraits>("B"), eB->typedOutputNamed<dflowgfx::SdfGridPlugTraits>("Out"));
    auto dgctx  = _sdfTestRegisters();
    auto sorter = std::make_shared<dflow::DgSorter>(g.get(), dgctx);
    auto topo   = sorter->generateTopology();
    OrkAssert(topo);
    auto ginst = dflow::GraphData::createGraphInst(g);
    auto env   = std::make_shared<hypermesh::MeshEnv>();
    env->_ctx  = ctx;
    env->_pool = std::make_shared<hypermesh::MeshPool>(ctx);
    ginst->_impl.setShared<hypermesh::MeshEnv>(env);
    ginst->updateTopology(topo);
    _sdfTestEval(ctx, ginst);

    dflowgfx::sdfgrid_inst_ptr_t grid;
    for (auto inst : ginst->_ordered_module_insts)
      if (dynamic_cast<const CsgData*>(inst->_dgmodule_data))
        grid = inst->typedOutputNamed<dflowgfx::SdfGridPlugTraits>("Out")->_value;
    CHECK(grid and grid->_ssbo, (std::string(C.name) + " csg brick allocated").c_str());
    if (not grid)
      continue;
    auto vals = _sdfReadBrick(ctx, *grid);
    double maxerr = 0.0;
    for (int iz = 0; iz < grid->_dim[2]; iz++)
      for (int iy = 0; iy < grid->_dim[1]; iy++)
        for (int ix = 0; ix < grid->_dim[0]; ix++) {
          fvec3 p(grid->_origin[0] + ix * grid->_voxel, grid->_origin[1] + iy * grid->_voxel, grid->_origin[2] + iz * grid->_voxel);
          size_t i  = size_t(ix) + size_t(grid->_dim[0]) * (size_t(iy) + size_t(grid->_dim[1]) * iz);
          double e  = std::abs(double(vals[i]) - double(C.combine(oracleA(p), oracleB(p))));
          maxerr    = std::max(maxerr, e);
        }
    CHECK(maxerr < 1e-3, (std::string(C.name) + " analytic max-error").c_str(), maxerr);
  }
  printf("=== sdf csg selftest %s (%d failures) ===\n", fails ? "FAILED" : "PASSED", fails);
  return fails;
}

///////////////////////////////////////////////////////////////////////////////
// M4a redistance oracle — see sdfdflow.h. Feed a SCALED sphere (|grad|=3, the
// "not a true SDF" case) through SdfEval -> Redistance and assert the output is
// the TRUE unit sphere field: zero-set preserved (sign vs analytic), near-band
// distance == analytic, |grad|->1 (vs the input's ~3). The EDT-correctness proof.
///////////////////////////////////////////////////////////////////////////////
int sdfRedistanceSelfTest(Context* ctx) {
  int fails  = 0;
  auto CHECK = [&](bool ok, const char* what, double detail = 0.0) {
    printf("[sdf redistance oracle] %s %s (%g)\n", ok ? "PASS" : "FAIL", what, detail);
    if (not ok)
      fails++;
  };
  const float R     = 1.25f;
  const int DIM     = 48;
  const float EXT   = 4.0f;
  const float voxel = EXT / float(DIM);
  auto trueSphere   = [&](const fvec3& p) { return p.length() - R; };

  auto g  = std::make_shared<dflow::GraphData>();
  auto ev = SdfEvalData::createShared();
  ev->_expression = "(length(p) - 1.25) * 3.0"; // NON-unit field (|grad|=3); zero-set at r=1.25
  ev->typedInputNamed<dflow::IntPlugTraits>("dim")->setValue(DIM);
  ev->typedInputNamed<dflow::FloatPlugTraits>("extent")->setValue(EXT);
  auto rd = RedistanceData::createShared();
  dflow::GraphData::addModule(g, "eval", ev);
  dflow::GraphData::addModule(g, "redist", rd);
  g->safeConnect(rd->typedInputNamed<dflowgfx::SdfGridPlugTraits>("In"), ev->typedOutputNamed<dflowgfx::SdfGridPlugTraits>("Out"));
  auto dgctx  = _sdfTestRegisters();
  auto sorter = std::make_shared<dflow::DgSorter>(g.get(), dgctx);
  auto topo   = sorter->generateTopology();
  OrkAssert(topo);
  auto ginst = dflow::GraphData::createGraphInst(g);
  auto envp  = std::make_shared<hypermesh::MeshEnv>();
  envp->_ctx  = ctx;
  envp->_pool = std::make_shared<hypermesh::MeshPool>(ctx);
  ginst->_impl.setShared<hypermesh::MeshEnv>(envp);
  ginst->updateTopology(topo);
  _sdfTestEval(ctx, ginst);

  dflowgfx::sdfgrid_inst_ptr_t evgrid, rdgrid;
  for (auto inst : ginst->_ordered_module_insts) {
    if (dynamic_cast<const SdfEvalData*>(inst->_dgmodule_data))
      evgrid = inst->typedOutputNamed<dflowgfx::SdfGridPlugTraits>("Out")->_value;
    if (dynamic_cast<const RedistanceData*>(inst->_dgmodule_data))
      rdgrid = inst->typedOutputNamed<dflowgfx::SdfGridPlugTraits>("Out")->_value;
  }
  CHECK(rdgrid and rdgrid->_ssbo and rdgrid->_dim[0] == DIM, "redistance brick allocated", rdgrid ? rdgrid->_dim[0] : 0);
  if (not rdgrid or not evgrid) {
    printf("=== sdf redistance selftest %s (%d failures) ===\n", fails ? "FAILED" : "PASSED", fails);
    return fails + 1;
  }
  auto inv  = _sdfReadBrick(ctx, *evgrid); // the non-unit input
  auto outv = _sdfReadBrick(ctx, *rdgrid); // the redistanced output
  int dx = rdgrid->_dim[0], dy = rdgrid->_dim[1], dz = rdgrid->_dim[2];
  auto IDX = [&](int x, int y, int z) { return size_t(x) + size_t(dx) * (size_t(y) + size_t(dy) * z); };
  auto POS = [&](int x, int y, int z) {
    return fvec3(rdgrid->_origin[0] + x * voxel, rdgrid->_origin[1] + y * voxel, rdgrid->_origin[2] + z * voxel);
  };
  // (1) zero-set preserved: output sign == analytic sign (exclude the |o|<0.5voxel ambiguous band)
  int signmis = 0;
  for (int z = 0; z < dz; z++)
    for (int y = 0; y < dy; y++)
      for (int x = 0; x < dx; x++) {
        float o = trueSphere(POS(x, y, z));
        if (std::abs(o) < 0.5f * voxel)
          continue;
        if ((o < 0.0f) != (outv[IDX(x, y, z)] < 0.0f))
          signmis++;
      }
  CHECK(signmis == 0, "zero-set preserved (sign mismatches vs analytic)", signmis);
  // (2) near-band distance accuracy vs the TRUE sphere distance
  double maxerr = 0.0;
  for (int z = 0; z < dz; z++)
    for (int y = 0; y < dy; y++)
      for (int x = 0; x < dx; x++) {
        fvec3 p = POS(x, y, z);
        float o = trueSphere(p);
        if (std::abs(o) > 5.0f * voxel)
          continue;
        maxerr = std::max(maxerr, std::abs(double(outv[IDX(x, y, z)]) - double(o)));
      }
  CHECK(maxerr < 1.0 * voxel, "near-band distance vs analytic (max err / voxel)", maxerr / voxel);
  // (3) |grad| of OUTPUT -> 1 (interior, skip surface+center kinks); INPUT was ~3 (non-unit)
  auto gradmag = [&](const std::vector<float>& v, int x, int y, int z) -> double {
    int xm = std::max(0, x - 1), xp = std::min(dx - 1, x + 1);
    int ym = std::max(0, y - 1), yp = std::min(dy - 1, y + 1);
    int zm = std::max(0, z - 1), zp = std::min(dz - 1, z + 1);
    double gx = (double(v[IDX(xp, y, z)]) - double(v[IDX(xm, y, z)])) / (2.0 * voxel);
    double gy = (double(v[IDX(x, yp, z)]) - double(v[IDX(x, ym, z)])) / (2.0 * voxel);
    double gz = (double(v[IDX(x, y, zp)]) - double(v[IDX(x, y, zm)])) / (2.0 * voxel);
    return std::sqrt(gx * gx + gy * gy + gz * gz);
  };
  double gsum_out = 0, gsum_in = 0;
  int gn = 0;
  for (int z = 1; z < dz - 1; z++)
    for (int y = 1; y < dy - 1; y++)
      for (int x = 1; x < dx - 1; x++) {
        fvec3 p = POS(x, y, z);
        float o = trueSphere(p);
        if (std::abs(o) < voxel || p.length() < 0.3f)
          continue; // skip the surface + center kinks
        gsum_out += gradmag(outv, x, y, z);
        gsum_in += gradmag(inv, x, y, z);
        gn++;
      }
  double gmean_out = gn ? gsum_out / gn : 0.0, gmean_in = gn ? gsum_in / gn : 0.0;
  CHECK(gmean_out > 0.9 && gmean_out < 1.1, "output |grad| -> 1 (mean)", gmean_out);
  CHECK(gmean_in > 2.5, "input was NON-unit (mean |grad| ~ 3)", gmean_in);

  // ---- UNION SEAM case: redistance min(sphereA, sphereB) of two OVERLAPPING spheres. The union
  // gradient CREASES at the seam (x=0 plane) — the old Newton-projection seed lands on the wrong
  // lobe there and corrupts the field; the edge-crossing seed must keep |grad|->1 + the zero-set
  // EVEN at the seam. (The bug the owner caught: redistance made an animated sphere-union worse.)
  {
    const float SR = 0.9f, SC = 0.6f; // two r=0.9 spheres at x=+-0.6 (overlap -> a lens seam)
    auto distA = [&](const fvec3& p) { return (p - fvec3(-SC, 0, 0)).length() - SR; };
    auto distB = [&](const fvec3& p) { return (p - fvec3(SC, 0, 0)).length() - SR; };
    auto ug    = std::make_shared<dflow::GraphData>();
    auto uev   = SdfEvalData::createShared();
    uev->_expression = "min(length(p - vec3(-0.6,0,0)) - 0.9, length(p - vec3(0.6,0,0)) - 0.9)";
    uev->typedInputNamed<dflow::IntPlugTraits>("dim")->setValue(48);
    uev->typedInputNamed<dflow::FloatPlugTraits>("extent")->setValue(4.0f);
    auto urd = RedistanceData::createShared();
    dflow::GraphData::addModule(ug, "eval", uev);
    dflow::GraphData::addModule(ug, "redist", urd);
    ug->safeConnect(urd->typedInputNamed<dflowgfx::SdfGridPlugTraits>("In"), uev->typedOutputNamed<dflowgfx::SdfGridPlugTraits>("Out"));
    auto udg = _sdfTestRegisters();
    auto uso = std::make_shared<dflow::DgSorter>(ug.get(), udg);
    auto uto = uso->generateTopology();
    OrkAssert(uto);
    auto ugi = dflow::GraphData::createGraphInst(ug);
    auto uen = std::make_shared<hypermesh::MeshEnv>();
    uen->_ctx  = ctx;
    uen->_pool = std::make_shared<hypermesh::MeshPool>(ctx);
    ugi->_impl.setShared<hypermesh::MeshEnv>(uen);
    ugi->updateTopology(uto);
    _sdfTestEval(ctx, ugi);
    dflowgfx::sdfgrid_inst_ptr_t urg;
    for (auto inst : ugi->_ordered_module_insts)
      if (dynamic_cast<const RedistanceData*>(inst->_dgmodule_data))
        urg = inst->typedOutputNamed<dflowgfx::SdfGridPlugTraits>("Out")->_value;
    if (urg and urg->_ssbo) {
      auto uv  = _sdfReadBrick(ctx, *urg);
      int ud   = urg->_dim[0];
      float uvx = urg->_voxel;
      auto uIDX = [&](int x, int y, int z) { return size_t(x) + size_t(ud) * (size_t(y) + size_t(ud) * z); };
      auto uPOS = [&](int x, int y, int z) {
        return fvec3(urg->_origin[0] + x * uvx, urg->_origin[1] + y * uvx, urg->_origin[2] + z * uvx);
      };
      // zero-set preserved vs the analytic union sign, AND no |grad| SPIKE. (NOT distance-vs-min:
      // min(sdfA,sdfB) is NOT the true distance near a concave overlap seam / interior — the EDT
      // redistance is MORE correct there, so min is a bad oracle. The real seam-corruption signature
      // is a |grad|>1 spike: a true SDF is 1-Lipschitz (|grad|<=1, dipping <1 only at medial sets);
      // the old Newton-projection seed put seeds on the wrong lobe -> distance jumps -> |grad|>>1.)
      int usign      = 0;
      double ugmax   = 0.0;
      for (int z = 1; z < ud - 1; z++)
        for (int y = 1; y < ud - 1; y++)
          for (int x = 1; x < ud - 1; x++) {
            fvec3 p  = uPOS(x, y, z);
            double o = std::min(distA(p), distB(p));
            float v  = uv[uIDX(x, y, z)];
            if (std::abs(o) > 0.5 * uvx && (o < 0.0) != (v < 0.0))
              usign++;
            if (std::abs(o) < 0.75 * uvx)
              continue; // skip the immediate surface band (a sign-flip voxel reads |grad|~1, fine)
            double gx = (double(uv[uIDX(x + 1, y, z)]) - double(uv[uIDX(x - 1, y, z)])) / (2.0 * uvx);
            double gy = (double(uv[uIDX(x, y + 1, z)]) - double(uv[uIDX(x, y - 1, z)])) / (2.0 * uvx);
            double gz = (double(uv[uIDX(x, y, z + 1)]) - double(uv[uIDX(x, y, z - 1)])) / (2.0 * uvx);
            ugmax     = std::max(ugmax, std::sqrt(gx * gx + gy * gy + gz * gz));
          }
      CHECK(usign == 0, "UNION zero-set preserved (sign vs analytic min)", usign);
      CHECK(ugmax < 1.25, "UNION no |grad| spike (1-Lipschitz; old seam-seed corruption spikes >1)", ugmax);
    } else {
      CHECK(false, "UNION redistance brick allocated", 0);
    }
  }
  printf("=== sdf redistance selftest %s (%d failures) ===\n", fails ? "FAILED" : "PASSED", fails);
  return fails;
}

///////////////////////////////////////////////////////////////////////////////
// M2 mesh oracle — marching-tetrahedra a sphere SDF (SdfEval -> SdfToMesh) through
// the REAL mesh driver (the onTopologyReady alloc+re-eval cascade), read the
// vidx/face_offsets/positions back, and assert: a non-trivial tri count, EVERY
// vertex on the iso surface (analytic |sdf|<tol), and POSITION-based
// watertightness (each undirected edge of the soup shared by exactly 2 tris —
// validates the tet topology without needing the weld). Returns failure count.
///////////////////////////////////////////////////////////////////////////////

int sdfMeshSelfTest(Context* ctx) {
  int fails  = 0;
  auto CHECK = [&](bool ok, const char* what, double detail = 0.0) {
    printf("[sdf mesh oracle] %s %s (%g)\n", ok ? "PASS" : "FAIL", what, detail);
    if (not ok)
      fails++;
  };

  // weld the marching-tets SOUP at near-exact precision (shared-edge verts are
  // bit-identical across adjacent cells -> merge; distinct crossings stay) and
  // assert: closed 2-manifold (every non-sliver edge shared by exactly 2 tris)
  // + the Euler characteristic of a genus-0 solid (V-E+F == 2). `surf` (optional)
  // asserts every vertex lies on the analytic iso surface.
  auto checkMesh = [&](const char* label, hypermesh::gpumesh_ptr_t mesh, double voxel,
                       std::function<double(const fvec3&)> surf) {
    auto fxi = ctx->FXI();
    int nf = mesh ? mesh->_num_faces : 0, nc = mesh ? mesh->_num_corners : 0, nv = mesh ? mesh->_num_verts : 0;
    // M2.5: for a GPU-resident-count mesh, _num_* hold the fixed CAPACITY; the LIVE
    // counts live in the header (uint[0]=nv,[1]=nc,[2]=nf — cs_writehdr). Read them so
    // the oracle measures EXACTLY the live primitives, not the cs_ident degenerate
    // origin tail (which would inflate chi by 1 and flood the sliver fraction).
    if (mesh and mesh->_gpuResidentCount and mesh->_header) {
      auto hmap = fxi->mapStorageBuffer(mesh->_header, 0, 12, BufferMapAccess::READ_ONLY);
      uint32_t h[3]; std::memcpy(h, hmap->_mappedaddr, 12); fxi->unmapStorageBuffer(hmap.get());
      nv = int(h[0]); nc = int(h[1]); nf = int(h[2]);
    }
    if (not(mesh and nf > 100)) {
      CHECK(false, (std::string(label) + " meshed (tri count)").c_str(), nf);
      return;
    }
    CHECK(true, (std::string(label) + " meshed (tri count)").c_str(), nf);
    auto VI = _readU(fxi, mesh->_vidx->_ssbo, nc);
    auto FO = _readU(fxi, mesh->_face_offsets->_ssbo, nf + 1);
    std::vector<float> P(size_t(nv) * 4);
    {
      auto m = fxi->mapStorageBuffer(mesh->channel(hypermesh::MeshChannel::POSITION)->_ssbo, 0, size_t(nv) * 16, BufferMapAccess::READ_ONLY);
      std::memcpy(P.data(), m->_mappedaddr, size_t(nv) * 16);
      fxi->unmapStorageBuffer(m.get());
    }
    if (surf) {
      double maxoff = 0.0;
      for (int v = 0; v < nv; v++)
        maxoff = std::max(maxoff, std::abs(surf(fvec3(P[v * 4], P[v * 4 + 1], P[v * 4 + 2]))));
      CHECK(maxoff < 0.75 * voxel, (std::string(label) + " verts on iso surface (off/voxel)").c_str(), maxoff / voxel);
    }
    auto qkey = [&](int v) -> uint64_t {
      auto q = [&](float x) -> int64_t { return int64_t(std::llround(double(x) / 1.0e-4)); };
      return (uint64_t(q(P[v * 4]) & 0x1FFFFF) << 42) | (uint64_t(q(P[v * 4 + 1]) & 0x1FFFFF) << 21) | uint64_t(q(P[v * 4 + 2]) & 0x1FFFFF);
    };
    std::map<uint64_t, int> vid;
    std::vector<int> wv(nv);
    for (int v = 0; v < nv; v++) {
      auto k = qkey(v);
      auto it = vid.find(k);
      if (it == vid.end()) { int id = int(vid.size()); vid[k] = id; wv[v] = id; }
      else wv[v] = it->second;
    }
    std::map<std::pair<int, int>, int> edge_count;
    int degen = 0, used_f = 0;
    for (int f = 0; f < nf; f++) {
      int a = wv[VI[FO[f] + 0]], b = wv[VI[FO[f] + 1]], c = wv[VI[FO[f] + 2]];
      if (a == b or b == c or a == c) { degen++; continue; }
      used_f++;
      auto add = [&](int u, int w) { edge_count[{std::min(u, w), std::max(u, w)}]++; };
      add(a, b); add(b, c); add(c, a);
    }
    int bad = 0;
    for (auto& kv : edge_count)
      if (kv.second != 2)
        bad++;
    int chi = int(vid.size()) - int(edge_count.size()) + used_f; // V - E + F
    CHECK(degen < nf / 100, (std::string(label) + " sliver fraction < 1%").c_str(), double(degen) / double(nf));
    CHECK(bad == 0, (std::string(label) + " watertight (edges shared by exactly 2 tris)").c_str(), bad);
    CHECK(chi == 2, (std::string(label) + " euler characteristic == 2 (genus 0)").c_str(), chi);
    printf("[sdf mesh oracle] %s: faces=%d welded_verts=%zu edges=%zu slivers=%d chi=%d\n",
           label, nf, vid.size(), edge_count.size(), degen, chi);

    // M2 WELD proof — using vidx (the on-GPU shared-vertex indices) DIRECTLY, with
    // NO position re-weld. If the weld produced proper shared topology, the index
    // mesh is itself a closed manifold (every edge shared by exactly 2) with the
    // SAME welded-vertex count and chi==2 as the position analysis above.
    std::map<std::pair<int, int>, int> iedge;
    std::set<int> ivset;
    int idegen = 0, iused = 0;
    for (int f = 0; f < nf; f++) {
      int a = int(VI[FO[f] + 0]), b = int(VI[FO[f] + 1]), c = int(VI[FO[f] + 2]);
      if (a == b or b == c or a == c) { idegen++; continue; }
      iused++;
      ivset.insert(a); ivset.insert(b); ivset.insert(c);
      auto add = [&](int u, int w) { iedge[{std::min(u, w), std::max(u, w)}]++; };
      add(a, b); add(b, c); add(c, a);
    }
    int ibad = 0;
    for (auto& kv : iedge)
      if (kv.second != 2)
        ibad++;
    int ichi = int(ivset.size()) - int(iedge.size()) + iused;
    CHECK(ibad == 0, (std::string(label) + " WELD: vidx index-mesh watertight").c_str(), ibad);
    CHECK(ichi == 2, (std::string(label) + " WELD: vidx index-mesh chi == 2").c_str(), ichi);
    CHECK(std::abs(int(ivset.size()) - int(vid.size())) <= 2,
          (std::string(label) + " WELD: vidx shares to the position-distinct count").c_str(),
          double(int(ivset.size()) - int(vid.size())));
    printf("[sdf mesh oracle] %s WELD: vidx_distinct=%zu edges=%zu chi=%d (vs position welded_verts=%zu)\n",
           label, ivset.size(), iedge.size(), ichi, vid.size());
  };

  using sdfout_ptr = std::shared_ptr<dflowgfx::sdfgrid_outplugdata_t>;
  auto buildRun = [&](const std::function<sdfout_ptr(dflow::graphdata_ptr_t)>& build) -> hypermesh::gpumesh_ptr_t {
    auto g      = std::make_shared<dflow::GraphData>();
    auto srcOut = build(g);
    auto s2m    = SdfToMeshData::createShared();
    dflow::GraphData::addModule(g, "s2m", s2m);
    g->safeConnect(s2m->typedInputNamed<dflowgfx::SdfGridPlugTraits>("In"), srcOut);
    auto dgctx  = _sdfTestRegisters();
    auto sorter = std::make_shared<dflow::DgSorter>(g.get(), dgctx);
    auto topo   = sorter->generateTopology();
    OrkAssert(topo);
    auto ginst = dflow::GraphData::createGraphInst(g);
    auto env   = std::make_shared<hypermesh::MeshEnv>();
    env->_ctx  = ctx;
    env->_pool = std::make_shared<hypermesh::MeshPool>(ctx);
    ginst->_impl.setShared<hypermesh::MeshEnv>(env);
    ginst->updateTopology(topo);
    _voxEval(ctx, ginst);
    hypermesh::gpumesh_ptr_t mesh;
    for (auto inst : ginst->_ordered_module_insts) {
      auto mo = inst->typedOutputNamed<hypermesh::MeshPlugTraits>("Out");
      if (mo and mo->_value and not mo->_value->_channels.empty())
        mesh = mo->_value;
    }
    return mesh;
  };

  // (1) a plain SPHERE — a known genus-0 manifold + an analytic surface oracle
  {
    const float R = 1.3f;
    auto mesh = buildRun([&](dflow::graphdata_ptr_t g) {
      auto ev = SdfEvalData::createShared();
      ev->_expression = "length(p) - 1.3";
      ev->typedInputNamed<dflow::IntPlugTraits>("dim")->setValue(40);
      ev->typedInputNamed<dflow::FloatPlugTraits>("extent")->setValue(3.2f);
      dflow::GraphData::addModule(g, "eval", ev);
      return ev->typedOutputNamed<dflowgfx::SdfGridPlugTraits>("Out");
    });
    checkMesh("sphere", mesh, 3.2 / 40.0, [R](const fvec3& p) { return double(p.length() - R); });
  }

  // (2) the CSG-subtract CHAIN (box minus a corner sphere) -> still a single
  // watertight genus-0 solid: proves marching-tets on a CSG (max-of-bricks)
  // field stays watertight (the continuous-field guarantee), end to end.
  {
    auto mesh = buildRun([&](dflow::graphdata_ptr_t g) {
      auto eb = SdfEvalData::createShared();
      auto es = SdfEvalData::createShared();
      eb->_expression = "(length(max(abs(p) - vec3(1.0,1.0,1.0), vec3(0.0,0.0,0.0))) + min(max(abs(p).x-1.0, max(abs(p).y-1.0, abs(p).z-1.0)), 0.0))";
      es->_expression = "length(p - vec3(0.9,0.9,0.9)) - 0.95";
      for (auto e : {eb, es}) {
        e->typedInputNamed<dflow::IntPlugTraits>("dim")->setValue(48);
        e->typedInputNamed<dflow::FloatPlugTraits>("extent")->setValue(4.0f);
      }
      auto csg = CsgData::createShared();
      csg->_op = 2; // subtract
      dflow::GraphData::addModule(g, "box", eb);
      dflow::GraphData::addModule(g, "sphere", es);
      dflow::GraphData::addModule(g, "csg", csg);
      g->safeConnect(csg->typedInputNamed<dflowgfx::SdfGridPlugTraits>("A"), eb->typedOutputNamed<dflowgfx::SdfGridPlugTraits>("Out"));
      g->safeConnect(csg->typedInputNamed<dflowgfx::SdfGridPlugTraits>("B"), es->typedOutputNamed<dflowgfx::SdfGridPlugTraits>("Out"));
      return csg->typedOutputNamed<dflowgfx::SdfGridPlugTraits>("Out");
    });
    checkMesh("csg-subtract", mesh, 4.0 / 48.0, nullptr);
  }

  printf("=== sdf mesh selftest %s (%d failures) ===\n", fails ? "FAILED" : "PASSED", fails);
  return fails;
}

} // namespace ork::lev2::sdf
