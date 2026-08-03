////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "sdfdflow_module.h"
#include "../hypermesh/hmdflow_module.h" // MeshComputeInst + allocMesh + the CPU-fill precedent
#include <ork/lev2/gfx/openvdb.h>        // FloatGrid + volumeToMesh + signedFloodFill (CPU/bake-time only)
#include "../meshutil/xatlas.h"          // vendored UV-unwrap (already compiled into lev2)
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <tuple>
#include <vector>

ImplementReflectionX(ork::lev2::sdf::SdfToMeshCleanData, "sdf::SdfToMeshCleanData");

namespace ork::lev2::sdf {

namespace hm = ork::lev2::hypermesh;

///////////////////////////////////////////////////////////////////////////////
// SdfToMeshClean (M2 clean-remesh) — the shape-aware / low-poly / UV path (see the
// header). ALL work is one-shot CPU in onTopologyReady: read the dense brick back,
// build an openvdb narrow-band level set (per-voxel-CENTER transform + signedFloodFill),
// volumeToMesh (curvature-adaptive), optional xatlas unwrap, then CPU-fill the GpuMesh
// SoA channels + CSR topology (the MergeMesh upload precedent). Eval-1 emits nothing
// (the SDF input is a grid, no trunk to alias); onTopologyReady builds + re-evals.
///////////////////////////////////////////////////////////////////////////////

namespace {

// area-weighted per-vertex normals from a triangle list over a shared vertex array.
static void _accumNormals(const std::vector<fvec3>& P, const std::vector<uint32_t>& tri,
                          std::vector<fvec3>& N) {
  N.assign(P.size(), fvec3(0, 0, 0));
  for (size_t i = 0; i + 3 <= tri.size(); i += 3) {
    uint32_t a = tri[i], b = tri[i + 1], c = tri[i + 2];
    fvec3 fn = (P[b] - P[a]).crossWith(P[c] - P[a]); // magnitude = 2*area (area weighting)
    N[a] += fn; N[b] += fn; N[c] += fn;
  }
  for (auto& n : N) {
    float m = std::sqrt(n.dotWith(n));
    n = (m > 1e-12f) ? (n * (1.0f / m)) : fvec3(0, 1, 0);
  }
}

// a stable arbitrary tangent perpendicular to n (the SdfToMesh emit convention; the
// material recomputes TBN — this only needs to be non-degenerate).
static inline fvec3 _arbitraryTangent(const fvec3& n) {
  fvec3 ref = (std::abs(n.y) < 0.99f) ? fvec3(0, 1, 0) : fvec3(1, 0, 0);
  fvec3 t   = ref.crossWith(n);
  float m   = std::sqrt(t.dotWith(t));
  return (m > 1e-12f) ? (t * (1.0f / m)) : fvec3(1, 0, 0);
}

} // namespace

struct SdfToMeshCleanInst : public hm::MeshComputeInst {
  SdfToMeshCleanInst(const SdfToMeshCleanData* d, dflow::GraphInst* g)
      : hm::MeshComputeInst(d, g)
      , _d(d) {
  }
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<hm::MeshPlugTraits>("Out");
    _input  = typedInputNamed<dflowgfx::SdfGridPlugTraits>("In");
  }
  dflowgfx::sdfgrid_inst_ptr_t srcGrid() const {
    auto out = std::dynamic_pointer_cast<dflowgfx::sdfgrid_outpluginst_t>(_input->_connectedOutput);
    return out ? out->_value : nullptr;
  }
  // eval-1 (pre-build): produce nothing. The SDF input is a grid, so there is no trunk
  // mesh to alias (unlike MergeMesh); the output stays an empty GpuMesh until onTopologyReady
  // builds it and requests a re-eval. Once built, this is a no-op.
  void compute(dflow::GraphInst*, ui::updatedata_ptr_t) final {}

  bool onTopologyReady(Context* ctx) final {
    if (_built)
      return false;
    auto in = srcGrid();
    // DEFER (like MergeMesh's settled() guard) until the upstream SDF is computed + settled.
    // The cascade re-evals after each true, and mesh_to_sdf's own onTopologyReady refit runs
    // BEFORE this (topo order), so by here the brick is current.
    if (not in or in->_repr != dflowgfx::SdfRepr::DENSE or not in->_ssbo or in->_dim[0] < 2)
      return true;
    auto env = _graphinst->_impl.getShared<hm::MeshEnv>();
    auto fxi = ctx->FXI();

    const int dx = in->_dim[0], dy = in->_dim[1], dz = in->_dim[2];
    const size_t nvox = size_t(dx) * dy * dz;
    // ---- read the dense brick back (READ_ONLY map is legal here: onTopologyReady runs after
    //      the driver's first full eval + sync, so the producer's GPU writes are settled —
    //      NEVER a device-local readStorageBuffer mid-graph, which aborts the CB / reboots MoltenVK).
    std::vector<float> brick(nvox);
    {
      auto m = fxi->mapStorageBuffer(in->_ssbo, 0, nvox * 4, BufferMapAccess::READ_ONLY);
      std::memcpy(brick.data(), m->_mappedaddr, nvox * 4);
      fxi->unmapStorageBuffer(m.get());
    }

    // ---- build an openvdb narrow-band level set (the sdfDenseToNano grid-construction half).
    openvdb::initialize(); // idempotent
    const float voxel     = in->_voxel;
    const float halfWidth = 3.0f;                 // narrow-band radius in voxels
    const float bg        = halfWidth * voxel;    // world-space background / outside value
    auto grid = openvdb::FloatGrid::create(bg);
    grid->setGridClass(openvdb::GRID_LEVEL_SET);
    // per-voxel-CENTER transform: index (i,j,k) -> world = origin + (i,j,k)*voxel (SdfGridInst
    // _origin is voxel (0,0,0)'s CENTER, interchange.h). postTranslate applies AFTER the scale.
    auto xform = openvdb::math::Transform::createLinearTransform(double(voxel));
    xform->postTranslate(openvdb::Vec3d(in->_origin[0], in->_origin[1], in->_origin[2]));
    grid->setTransform(xform);
    {
      auto acc = grid->getAccessor();
      for (int k = 0; k < dz; k++)
        for (int j = 0; j < dy; j++)
          for (int i = 0; i < dx; i++) {
            float v = brick[size_t(i) + size_t(dx) * (size_t(j) + size_t(dy) * k)];
            if (std::abs(v) < bg) // activate only the narrow band; floodfill signs the rest
              acc.setValue(openvdb::Coord(i, j, k), v);
          }
    }
    openvdb::tools::signedFloodFill(grid->tree());

    // ---- adaptive extraction (curvature-driven decimation; the shape-aware step).
    std::vector<openvdb::Vec3s> vpts;
    std::vector<openvdb::Vec3I> vtris;
    std::vector<openvdb::Vec4I> vquads;
    const float adapt = std::min(std::max(_d->_adaptivity, 0.0f), 1.0f);
    openvdb::tools::volumeToMesh(*grid, vpts, vtris, vquads, _d->_isovalue, adapt, /*relax=*/false);
    if (vpts.empty() or (vtris.empty() and vquads.empty())) {
      // ops fail LOUDLY (never a silent black mesh): name the op + the likely cause.
      printf("[sdf2meshclean] WARNING: volumeToMesh produced NO geometry at iso=%g adaptivity=%g "
             "(field empty / iso off the zero-set / band clipped at the brick boundary). Output empty.\n",
             _d->_isovalue, adapt);
      _built = true;
      return false;
    }

    // ---- CPU vertex array (optional pre-unwrap position weld). volumeToMesh already emits a
    //      shared-vertex mesh, so weld_tol defaults to 0 (no-op); >0 snaps near-coincident points.
    std::vector<fvec3> P(vpts.size());
    for (size_t i = 0; i < vpts.size(); i++)
      P[i] = fvec3(vpts[i].x(), vpts[i].y(), vpts[i].z());
    std::vector<uint32_t> remap(P.size());
    for (size_t i = 0; i < P.size(); i++)
      remap[i] = uint32_t(i);
    if (_d->_weld_tol > 0.0f) {
      const double q = 1.0 / double(_d->_weld_tol);
      std::map<std::tuple<int64_t, int64_t, int64_t>, uint32_t> uniq;
      std::vector<fvec3> Pw;
      Pw.reserve(P.size());
      for (size_t i = 0; i < P.size(); i++) {
        auto key = std::make_tuple(int64_t(std::llround(P[i].x * q)),
                                   int64_t(std::llround(P[i].y * q)),
                                   int64_t(std::llround(P[i].z * q)));
        auto it = uniq.find(key);
        if (it == uniq.end()) {
          uint32_t id = uint32_t(Pw.size());
          uniq[key]   = id;
          remap[i]    = id;
          Pw.push_back(P[i]);
        } else {
          remap[i] = it->second;
        }
      }
      P.swap(Pw);
    }

    // ---- WINDING: openvdb volumeToMesh emits faces wound CCW viewed from the NEGATIVE
    //      (inside) side of the level set — i.e. INWARD against the engine's CCW-front
    //      convention. The original assumption here ("outward, keep as-is") was measured
    //      WRONG on 2026-07-22: every output had NEGATIVE signed volume (box fixture
    //      -1.693 vs +1.728; owner saw the kiva inside-out live). Reverse every source
    //      face ONCE here, before BOTH consumers (tri fan + quad CSR) — normals accumulate
    //      from these lists, so faces and normals flip together. The committed gate now
    //      asserts signed volume > 0 (the orientation check the original battery lacked).
    for (auto& t : vtris)
      std::swap(t[1], t[2]); // (x,y,z) -> (x,z,y)
    for (auto& qd : vquads)
      std::swap(qd[1], qd[3]); // (x,y,z,w) -> (x,w,z,y)
    std::vector<uint32_t> tri;
    tri.reserve(vtris.size() * 3 + vquads.size() * 6);
    auto pushTri = [&](uint32_t a, uint32_t b, uint32_t c) {
      a = remap[a]; b = remap[b]; c = remap[c];
      if (a == b or b == c or a == c) return; // drop degenerate / welded-away slivers
      tri.push_back(a); tri.push_back(b); tri.push_back(c);
    };
    for (auto& t : vtris)
      pushTri(t.x(), t.y(), t.z());
    for (auto& qd : vquads) {
      pushTri(qd.x(), qd.y(), qd.z());
      pushTri(qd.x(), qd.z(), qd.w());
    }

    // ---- assemble the output mesh (two forms; see the header) ----
    std::vector<fvec3> outP, outN;
    std::vector<fvec2> outUV;
    std::vector<uint32_t> outVI;   // corner -> vertex
    std::vector<uint32_t> outFO;   // CSR face offsets over corners

    bool unwrapped = false;
    if (_d->_unwrap and not tri.empty()) {
      unwrapped = _unwrapXatlas(P, tri, outP, outUV, outVI, outFO);
      if (unwrapped) {
        std::vector<uint32_t> flatTri(outVI); // triangulated CSR -> vidx already the tri list
        _accumNormals(outP, flatTri, outN);
      }
    }
    if (not unwrapped) {
      // QUAD-DOMINANT form: keep volumeToMesh quads as quads (cleaner topology; triangulation at
      // draw). UV0 = 0 (no unwrap). Normals accumulate over the fully-triangulated helper list.
      _buildQuadForm(P, remap, vtris, vquads, outP, outN, outUV, outVI, outFO);
    }

    const int nv = int(outP.size());
    const int nc = int(outVI.size());
    const int nf = int(outFO.size()) - 1;
    if (nv < 3 or nf < 1) {
      printf("[sdf2meshclean] WARNING: assembled mesh degenerate (nv=%d nf=%d). Output empty.\n", nv, nf);
      _built = true;
      return false;
    }

    // ---- allocate + CPU-upload (the MergeMesh staging path) ----
    auto mesh = _output->_value;
    mesh->_vattrs.clear();
    mesh->_faces.clear();
    allocMesh(env, mesh, nv, nc, nf,
              {hm::MeshChannel::POSITION, hm::MeshChannel::NORMAL, hm::MeshChannel::BINORMAL,
               hm::MeshChannel::UV0, hm::MeshChannel::COLOR});

    std::vector<float> Pd(size_t(nv) * 4), Nd(size_t(nv) * 4), Bd(size_t(nv) * 4),
        Ud(size_t(nv) * 4, 0.0f), Cd(size_t(nv) * 4, 1.0f);
    fvec3 bbmin(1e30f, 1e30f, 1e30f), bbmax(-1e30f, -1e30f, -1e30f);
    for (int v = 0; v < nv; v++) {
      const fvec3& p = outP[v];
      const fvec3& n = outN.empty() ? fvec3(0, 1, 0) : outN[v];
      fvec3 b        = _arbitraryTangent(n);
      Pd[v * 4 + 0] = p.x; Pd[v * 4 + 1] = p.y; Pd[v * 4 + 2] = p.z; Pd[v * 4 + 3] = 1.0f;
      Nd[v * 4 + 0] = n.x; Nd[v * 4 + 1] = n.y; Nd[v * 4 + 2] = n.z; Nd[v * 4 + 3] = 0.0f;
      Bd[v * 4 + 0] = b.x; Bd[v * 4 + 1] = b.y; Bd[v * 4 + 2] = b.z; Bd[v * 4 + 3] = 0.0f;
      if (not outUV.empty()) { Ud[v * 4 + 0] = outUV[v].x; Ud[v * 4 + 1] = outUV[v].y; } // .zw stay 0
      bbmin.x = std::min(bbmin.x, p.x); bbmin.y = std::min(bbmin.y, p.y); bbmin.z = std::min(bbmin.z, p.z);
      bbmax.x = std::max(bbmax.x, p.x); bbmax.y = std::max(bbmax.y, p.y); bbmax.z = std::max(bbmax.z, p.z);
    }
    auto upf = [&](hm::MeshChannel ch, const std::vector<float>& v) {
      auto ss = mesh->channel(ch)->_ssbo;
      auto mp = fxi->mapStorageBuffer(ss, 0, v.size() * sizeof(float), BufferMapAccess::WRITE_ONLY);
      std::memcpy(mp->_mappedaddr, v.data(), v.size() * sizeof(float));
      fxi->unmapStorageBuffer(mp.get());
    };
    auto upu = [&](FxShaderStorageBuffer* ss, const std::vector<uint32_t>& v) {
      auto mp = fxi->mapStorageBuffer(ss, 0, v.size() * sizeof(uint32_t), BufferMapAccess::WRITE_ONLY);
      std::memcpy(mp->_mappedaddr, v.data(), v.size() * sizeof(uint32_t));
      fxi->unmapStorageBuffer(mp.get());
    };
    upf(hm::MeshChannel::POSITION, Pd);
    upf(hm::MeshChannel::NORMAL, Nd);
    upf(hm::MeshChannel::BINORMAL, Bd);
    upf(hm::MeshChannel::UV0, Ud);
    upf(hm::MeshChannel::COLOR, Cd);
    upu(mesh->_vidx->_ssbo, outVI);
    upu(mesh->_face_offsets->_ssbo, outFO);
    {
      auto mp = fxi->mapStorageBuffer(mesh->_header, 0, 64, BufferMapAccess::WRITE_ONLY);
      auto hu = (uint32_t*)mp->_mappedaddr;
      auto hf = (float*)mp->_mappedaddr;
      hu[0] = uint32_t(nv); hu[1] = uint32_t(nc); hu[2] = uint32_t(nf); hu[3] = 0u;
      hf[4] = bbmin.x; hf[5] = bbmin.y; hf[6] = bbmin.z; hf[7] = 1.0f;
      hf[8] = bbmax.x; hf[9] = bbmax.y; hf[10] = bbmax.z; hf[11] = 1.0f;
      fxi->unmapStorageBuffer(mp.get());
    }
    mesh->markTopoChanged();
    _built = true;
    if (not _announced) {
      printf("SdfToMeshClean<%s>: brick %dx%dx%d -> %d verts / %d faces (adaptivity=%.2f unwrap=%d%s)\n",
             _dgmodule_data->_name.c_str(), dx, dy, dz, nv, nf, adapt, _d->_unwrap ? 1 : 0,
             unwrapped ? " [xatlas UV]" : "");
      _announced = true;
    }
    return true; // eval-1 emitted nothing -> re-eval so downstream sees the built mesh
  }

  // xatlas UV unwrap. Triangulates for xatlas (its output is triangulated + reindexed along
  // seams — duplicate verts get distinct UVs); the GpuMesh then uses xatlas's OUTPUT indexing.
  // Returns false (caller falls back to the no-UV quad form) if xatlas fails.
  bool _unwrapXatlas(const std::vector<fvec3>& P, const std::vector<uint32_t>& tri,
                     std::vector<fvec3>& outP, std::vector<fvec2>& outUV,
                     std::vector<uint32_t>& outVI, std::vector<uint32_t>& outFO) {
    xatlas::MeshDecl decl;
    decl.vertexCount          = uint32_t(P.size());
    decl.vertexPositionData   = P.data();
    decl.vertexPositionStride  = sizeof(fvec3);
    decl.indexCount           = uint32_t(tri.size());
    decl.indexData            = tri.data();
    decl.indexFormat          = xatlas::IndexFormat::UInt32;

    xatlas::Atlas* atlas = xatlas::Create();
    auto st = xatlas::AddMesh(atlas, decl, 1);
    if (st != xatlas::AddMeshError::Success) {
      printf("[sdf2meshclean] WARNING: xatlas AddMesh failed (%s) — emitting mesh WITHOUT UVs.\n",
             xatlas::StringForEnum(st));
      xatlas::Destroy(atlas);
      return false;
    }
    // DETERMINISM: pin every option (fixed iterations, no brute-force, single auto-sized page).
    xatlas::ChartOptions chart;
    chart.maxIterations = 2;
    xatlas::ComputeCharts(atlas, chart);
    xatlas::PackOptions pack;
    pack.padding    = 2;
    pack.bruteForce = false; // deterministic packing (no random search)
    pack.blockAlign = true;
    xatlas::PackCharts(atlas, pack);

    if (atlas->meshCount < 1 or atlas->width == 0 or atlas->height == 0) {
      printf("[sdf2meshclean] WARNING: xatlas produced an empty atlas — emitting mesh WITHOUT UVs.\n");
      xatlas::Destroy(atlas);
      return false;
    }
    const xatlas::Mesh& xm = atlas->meshes[0];
    const float iw = 1.0f / float(atlas->width);
    const float ih = 1.0f / float(atlas->height);
    outP.resize(xm.vertexCount);
    outUV.resize(xm.vertexCount);
    for (uint32_t v = 0; v < xm.vertexCount; v++) {
      const xatlas::Vertex& xv = xm.vertexArray[v];
      outP[v]  = P[xv.xref]; // reindexed: xref -> original position (seam verts duplicate)
      outUV[v] = fvec2(xv.uv[0] * iw, xv.uv[1] * ih); // pixels -> [0..1]
    }
    outVI.assign(xm.indexArray, xm.indexArray + xm.indexCount);
    outFO.resize(size_t(xm.indexCount / 3) + 1);
    for (size_t f = 0; f < outFO.size(); f++)
      outFO[f] = uint32_t(f * 3); // all-triangle CSR
    xatlas::Destroy(atlas);
    return true;
  }

  // QUAD-DOMINANT form (no unwrap): keep volumeToMesh's tris + quads in the CSR. Normals
  // accumulate over the fully-triangulated helper list (quads split for weighting only).
  void _buildQuadForm(const std::vector<fvec3>& P, const std::vector<uint32_t>& remap,
                      const std::vector<openvdb::Vec3I>& vtris,
                      const std::vector<openvdb::Vec4I>& vquads,
                      std::vector<fvec3>& outP, std::vector<fvec3>& outN,
                      std::vector<fvec2>& outUV, std::vector<uint32_t>& outVI,
                      std::vector<uint32_t>& outFO) {
    outP = P;
    outUV.assign(P.size(), fvec2(0, 0));
    std::vector<uint32_t> ntri; // for normal accumulation
    outVI.clear();
    outFO.clear();
    outFO.push_back(0);
    auto tri3 = [&](uint32_t a, uint32_t b, uint32_t c) {
      ntri.push_back(a); ntri.push_back(b); ntri.push_back(c);
    };
    for (auto& t : vtris) {
      uint32_t a = remap[t.x()], b = remap[t.y()], c = remap[t.z()];
      if (a == b or b == c or a == c) continue;
      outVI.push_back(a); outVI.push_back(b); outVI.push_back(c);
      outFO.push_back(uint32_t(outVI.size()));
      tri3(a, b, c);
    }
    for (auto& qd : vquads) {
      uint32_t a = remap[qd.x()], b = remap[qd.y()], c = remap[qd.z()], dd = remap[qd.w()];
      // keep as a quad face (4 corners) unless it collapsed under the weld
      if (a != b and b != c and c != dd and dd != a) {
        outVI.push_back(a); outVI.push_back(b); outVI.push_back(c); outVI.push_back(dd);
        outFO.push_back(uint32_t(outVI.size()));
        tri3(a, b, c); tri3(a, c, dd);
      }
    }
    _accumNormals(outP, ntri, outN);
  }

  // a cook-loaded clean-mesh is ALREADY built (the disk-cached mesh is restored) -> skip the rebuild.
  bool cookLoad(datablock_constptr_t db) override {
    bool ok = MeshComputeInst::cookLoad(db);
    if (ok)
      _built = true;
    return ok;
  }
  // live-poke eviction: re-arm the rebuild (mirror MergeMesh — else the eviction's re-cascade is a no-op).
  void onCookEvicted() override { _built = false; }
  // cook-cache identity salt (reflected params fold in automatically via hypermeshModuleIdentityHash).
  const char* _cookSalt() const final { return "sdf.meshclean.v2"; }

  const SdfToMeshCleanData* _d;
  hm::mesh_outpluginst_ptr_t _output;
  dflowgfx::sdfgrid_inpluginst_ptr_t _input;
  bool _built     = false;
  bool _announced = false;
};

static void _reshapeSdfToMeshCleanIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<dflowgfx::SdfGridPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createOutputPlug<hm::MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
SdfToMeshCleanData::SdfToMeshCleanData() {
}
std::shared_ptr<SdfToMeshCleanData> SdfToMeshCleanData::createShared() {
  auto d = std::make_shared<SdfToMeshCleanData>();
  _reshapeSdfToMeshCleanIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t SdfToMeshCleanData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<SdfToMeshCleanInst>(this, g);
}
void SdfToMeshCleanData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return SdfToMeshCleanData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeSdfToMeshCleanIOs(m); });
  clazz->directProperty("adaptivity", &SdfToMeshCleanData::_adaptivity);
  clazz->directProperty("isovalue", &SdfToMeshCleanData::_isovalue);
  clazz->directProperty("unwrap", &SdfToMeshCleanData::_unwrap);
  clazz->directProperty("weld_tol", &SdfToMeshCleanData::_weld_tol);
}

} // namespace ork::lev2::sdf
