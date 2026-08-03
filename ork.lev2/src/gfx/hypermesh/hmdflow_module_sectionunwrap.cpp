////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include "../meshutil/xatlas.h" // vendored UV-unwrap (already compiled into lev2)
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <vector>

ImplementReflectionX(ork::lev2::hypermesh::SectionUnwrapData, "hypermesh::SectionUnwrapData");

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////
// SectionUnwrap (O3) — see hmdflow.h. One-shot CPU op in onTopologyReady: read the
// gid-partitioned mesh back, split faces into SECTIONS by their __tags gid band,
// xatlas-unwrap EACH section into its own 0-1 domain, and reassemble one mesh whose
// UV0.xy = per-section unwrap, UV0.z = the section's dense layer index. Faces are
// emitted grouped by section (contiguous), the per-face gid preserved. The baked
// texture-array driver renders each section into its own array layer; the forward
// material samples sampler2DArray at layer = frg_uv0.z.
///////////////////////////////////////////////////////////////////////////////

namespace {

// gid band of a face's __tags (A1 contract: [20:32)).
static inline uint32_t _gidOf(uint32_t tag) { return (tag >> 20u) & 0xFFFu; }

} // namespace

struct SectionUnwrapInst : public MeshComputeInst {
  SectionUnwrapInst(const SectionUnwrapData* d, dflow::GraphInst* g)
      : MeshComputeInst(d, g)
      , _d(d) {
  }
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _input  = typedInputNamed<MeshPlugTraits>("In");
  }
  // eval-1 (pre-build): passthrough is a no-op; the real build waits for the gid
  // partition to be computed + settled (onTopologyReady), like SdfToMeshClean.
  void compute(dflow::GraphInst*, ui::updatedata_ptr_t) final {}

  bool onTopologyReady(Context* ctx) final {
    if (_built)
      return false;
    auto in = _srcMesh(_input);
    // DEFER until the upstream mesh (SdfToMeshClean) is built AND the gid partition
    // (assign_gid compute) has run + synced — the topo cascade re-evals between each
    // onTopologyReady, so a later pass sees the settled __tags. Never read a half-built input.
    if (not in or in->_num_verts < 3 or in->_num_faces < 1)
      return true;
    auto env = _graphinst->_impl.getShared<MeshEnv>();
    auto fxi = ctx->FXI();

    const int nv = in->_num_verts;
    const int nc = in->_num_corners;
    const int nf = in->_num_faces;

    // ---- read the input SoA channels + CSR topology back (READ_ONLY map is legal here:
    //      onTopologyReady runs after the driver's eval+sync, so producer GPU writes settled).
    auto rdF = [&](MeshChannel ch, std::vector<float>& dst, int count) {
      dst.assign(size_t(count) * 4, 0.0f);
      auto c = in->channel(ch);
      if (not c) return;
      auto m = fxi->mapStorageBuffer(c->_ssbo, 0, size_t(count) * 16, BufferMapAccess::READ_ONLY);
      std::memcpy(dst.data(), m->_mappedaddr, size_t(count) * 16);
      fxi->unmapStorageBuffer(m.get());
    };
    auto rdU = [&](FxShaderStorageBuffer* ss, std::vector<uint32_t>& dst, int count) {
      dst.assign(std::max(1, count), 0u);
      if (not ss) return;
      auto m = fxi->mapStorageBuffer(ss, 0, size_t(std::max(1, count)) * 4, BufferMapAccess::READ_ONLY);
      std::memcpy(dst.data(), m->_mappedaddr, size_t(std::max(1, count)) * 4);
      fxi->unmapStorageBuffer(m.get());
    };
    std::vector<float> Pin, Nin, Bin, Cin;
    rdF(MeshChannel::POSITION, Pin, nv);
    rdF(MeshChannel::NORMAL, Nin, nv);
    rdF(MeshChannel::BINORMAL, Bin, nv);
    rdF(MeshChannel::COLOR, Cin, nv);
    std::vector<uint32_t> vidx, fo, tags;
    rdU(in->_vidx ? in->_vidx->_ssbo : nullptr, vidx, nc);
    rdU(in->_face_offsets ? in->_face_offsets->_ssbo : nullptr, fo, nf + 1);
    auto tagch = in->face("__tags");
    if (tagch)
      rdU(tagch->_ssbo, tags, nf); // gid-partitioned faces
    else
      tags.assign(nf, 0u);         // untagged mesh -> a single section (layer 0), still robust

    // ---- triangulate the CSR (fan from corner 0) and group triangles by section gid.
    struct Tri { uint32_t a, b, c; };
    std::map<uint32_t, std::vector<Tri>> byGid; // gid -> its triangles (global vertex ids)
    for (int f = 0; f < nf; f++) {
      uint32_t s = fo[f], e = fo[f + 1];
      if (e <= s + 2 or e > uint32_t(nc))
        continue; // degenerate / out of range
      uint32_t g  = _gidOf(f < int(tags.size()) ? tags[f] : 0u);
      auto& out   = byGid[g];
      uint32_t v0 = vidx[s];
      for (uint32_t k = s + 1; k + 1 < e; k++) {
        uint32_t v1 = vidx[k], v2 = vidx[k + 1];
        if (v0 == v1 or v1 == v2 or v0 == v2)
          continue;
        out.push_back({v0, v1, v2});
      }
    }
    // FAIL LOUD if the partition explodes past the reflected cap (no silent clamp).
    if (int(byGid.size()) > _d->_max_layers) {
      OrkAssertI(false,
                 FormatString("SectionUnwrap<%s>: %zu sections exceeds max_layers=%d — raise the reflected "
                              "'max_layers' cap or reduce the gid partition.",
                              _dgmodule_data->_name.c_str(), byGid.size(), _d->_max_layers)
                     .c_str());
    }

    // ---- per-section xatlas unwrap; concatenate into one output mesh (grouped by section).
    std::vector<float> Pd, Nd, Bd, Ud, Cd;          // output SoA (vec4 each)
    std::vector<uint32_t> outVI, outFO, outTags;    // CSR (all-triangle) + per-face gid<<20
    outFO.push_back(0u);
    fvec3 bbmin(1e30f, 1e30f, 1e30f), bbmax(-1e30f, -1e30f, -1e30f);
    int layer  = 0;                                 // dense layer index (sorted-gid order via std::map)
    int okSecs = 0;
    _layerGids.clear();                             // A8: the explicit layer -> section-gid table (queryable)
    for (auto& [gid, tris] : byGid) {
      _layerGids.push_back(int(gid));               // EVERY layer (incl. empty sections that still consume one)
      if (tris.empty()) { layer++; continue; }
      // compact the section's global verts -> a local vertex list for xatlas.
      std::map<uint32_t, uint32_t> g2l;
      std::vector<uint32_t> l2g;
      std::vector<fvec3> Ploc;
      std::vector<uint32_t> triLoc;
      triLoc.reserve(tris.size() * 3);
      auto local = [&](uint32_t gv) -> uint32_t {
        auto it = g2l.find(gv);
        if (it != g2l.end()) return it->second;
        uint32_t id = uint32_t(l2g.size());
        g2l[gv]     = id;
        l2g.push_back(gv);
        Ploc.emplace_back(Pin[gv * 4 + 0], Pin[gv * 4 + 1], Pin[gv * 4 + 2]);
        return id;
      };
      for (auto& t : tris) {
        triLoc.push_back(local(t.a));
        triLoc.push_back(local(t.b));
        triLoc.push_back(local(t.c));
      }
      // xatlas over the section subset.
      std::vector<fvec2> secUV;   // per output-vert uv (0..1)
      std::vector<uint32_t> secXref; // output-vert -> LOCAL input vert
      std::vector<uint32_t> secTri;  // output tri index list (into secXref)
      // FAIL LOUD (ops self-defend): a section that reaches xatlas has >=1 non-degenerate triangle,
      // so a failure here is a real geometry/atlas defect — NOT something to paper over. The old
      // zero-UV fallback collapsed the whole section onto UV(0,0), which the bake driver then
      // captured as a BLACK layer (the section vanished from the render, silently). Refuse it.
      if (not _unwrapSection(Ploc, triLoc, secUV, secXref, secTri)) {
        OrkAssertI(false,
                   FormatString("SectionUnwrap<%s>: xatlas unwrap FAILED for gid=%u (layer %d, %zu verts / "
                                "%zu tris) — the section has no valid UV atlas (degenerate/non-manifold "
                                "geometry?). Fix the source select or geometry; a zero-UV fallback would "
                                "bake this section BLACK.",
                                _dgmodule_data->_name.c_str(), gid, layer, Ploc.size(), triLoc.size() / 3)
                       .c_str());
      }
      const uint32_t vbase = uint32_t(Pd.size() / 4);
      for (uint32_t v = 0; v < secXref.size(); v++) {
        uint32_t gv = l2g[secXref[v]];
        Pd.push_back(Pin[gv * 4 + 0]); Pd.push_back(Pin[gv * 4 + 1]); Pd.push_back(Pin[gv * 4 + 2]); Pd.push_back(1.0f);
        Nd.push_back(Nin[gv * 4 + 0]); Nd.push_back(Nin[gv * 4 + 1]); Nd.push_back(Nin[gv * 4 + 2]); Nd.push_back(0.0f);
        Bd.push_back(Bin[gv * 4 + 0]); Bd.push_back(Bin[gv * 4 + 1]); Bd.push_back(Bin[gv * 4 + 2]); Bd.push_back(0.0f);
        // UV0.xy = per-section 0-1 unwrap ; UV0.z = LAYER (dense) ; UV0.w = 0
        Ud.push_back(secUV[v].x); Ud.push_back(secUV[v].y); Ud.push_back(float(layer)); Ud.push_back(0.0f);
        Cd.push_back(Cin[gv * 4 + 0]); Cd.push_back(Cin[gv * 4 + 1]); Cd.push_back(Cin[gv * 4 + 2]); Cd.push_back(Cin[gv * 4 + 3]);
        fvec3 p(Pin[gv * 4 + 0], Pin[gv * 4 + 1], Pin[gv * 4 + 2]);
        bbmin = fvec3(std::min(bbmin.x, p.x), std::min(bbmin.y, p.y), std::min(bbmin.z, p.z));
        bbmax = fvec3(std::max(bbmax.x, p.x), std::max(bbmax.y, p.y), std::max(bbmax.z, p.z));
      }
      const uint32_t gidband = (gid & 0xFFFu) << 20u;
      for (size_t t = 0; t + 2 < secTri.size(); t += 3) {
        outVI.push_back(vbase + secTri[t]);
        outVI.push_back(vbase + secTri[t + 1]);
        outVI.push_back(vbase + secTri[t + 2]);
        outFO.push_back(uint32_t(outVI.size()));
        outTags.push_back(gidband); // gid PRESERVED per output face
      }
      okSecs++;
      layer++;
    }

    const int onv = int(Pd.size() / 4);
    const int onc = int(outVI.size());
    const int onf = int(outFO.size()) - 1;
    if (onv < 3 or onf < 1) {
      printf("[sectionunwrap] WARNING: assembled mesh degenerate (nv=%d nf=%d, sections=%zu). Output empty.\n",
             onv, onf, byGid.size());
      _built = true;
      return false;
    }

    // ---- allocate + CPU-upload the reassembled mesh (the SdfToMeshClean staging path).
    auto mesh = _output->_value;
    mesh->_vattrs.clear();
    mesh->_faces.clear();
    allocMesh(env, mesh, onv, onc, onf,
              {MeshChannel::POSITION, MeshChannel::NORMAL, MeshChannel::BINORMAL, MeshChannel::UV0, MeshChannel::COLOR});
    // FACE-domain __tags channel is the module's own (allocMesh only does vertex channels + CSR).
    auto tagout = env->_pool->acquireChannel(4, onf);
    mesh->_faces["__tags"] = tagout;

    auto upf = [&](MeshChannel ch, const std::vector<float>& v) {
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
    upf(MeshChannel::POSITION, Pd);
    upf(MeshChannel::NORMAL, Nd);
    upf(MeshChannel::BINORMAL, Bd);
    upf(MeshChannel::UV0, Ud);
    upf(MeshChannel::COLOR, Cd);
    upu(mesh->_vidx->_ssbo, outVI);
    upu(mesh->_face_offsets->_ssbo, outFO);
    upu(tagout->_ssbo, outTags);
    {
      auto mp = fxi->mapStorageBuffer(mesh->_header, 0, 64, BufferMapAccess::WRITE_ONLY);
      auto hu = (uint32_t*)mp->_mappedaddr;
      auto hf = (float*)mp->_mappedaddr;
      hu[0] = uint32_t(onv); hu[1] = uint32_t(onc); hu[2] = uint32_t(onf); hu[3] = 0u;
      hf[4] = bbmin.x; hf[5] = bbmin.y; hf[6] = bbmin.z; hf[7] = 1.0f;
      hf[8] = bbmax.x; hf[9] = bbmax.y; hf[10] = bbmax.z; hf[11] = 1.0f;
      fxi->unmapStorageBuffer(mp.get());
    }
    mesh->markTopoChanged();
    _built     = true;
    _numLayers = layer;
    if (not _announced) {
      printf("SectionUnwrap<%s>: %d verts / %d faces in -> %d sections (%d unwrapped) -> %d verts / %d faces, "
             "%d layers (padding=%d)\n",
             _dgmodule_data->_name.c_str(), nv, nf, int(byGid.size()), okSecs, onv, onf, layer, _d->_padding);
      _announced = true;
    }
    return true; // eval-1 emitted nothing -> re-eval so downstream sees the reassembled mesh
  }

  // xatlas unwrap of ONE section (its own 0-1 domain). Triangulated in/out (seams duplicate verts).
  // Returns false (caller falls back) if xatlas fails. Mirrors SdfToMeshClean::_unwrapXatlas but per-section.
  bool _unwrapSection(const std::vector<fvec3>& P, const std::vector<uint32_t>& tri,
                      std::vector<fvec2>& outUV, std::vector<uint32_t>& outXref, std::vector<uint32_t>& outTri) {
    if (P.empty() or tri.size() < 3)
      return false;
    xatlas::MeshDecl decl;
    decl.vertexCount          = uint32_t(P.size());
    decl.vertexPositionData   = P.data();
    decl.vertexPositionStride = sizeof(fvec3);
    decl.indexCount           = uint32_t(tri.size());
    decl.indexData            = tri.data();
    decl.indexFormat          = xatlas::IndexFormat::UInt32;

    xatlas::Atlas* atlas = xatlas::Create();
    auto st              = xatlas::AddMesh(atlas, decl, 1);
    if (st != xatlas::AddMeshError::Success) {
      xatlas::Destroy(atlas);
      return false;
    }
    // DETERMINISM: pin every option (fixed iterations, no brute-force, single auto-sized page).
    xatlas::ChartOptions chart;
    chart.maxIterations = 2;
    xatlas::ComputeCharts(atlas, chart);
    xatlas::PackOptions pack;
    pack.padding    = std::max(0, _d->_padding);
    pack.bruteForce = false; // deterministic packing (no random search)
    pack.blockAlign = true;
    xatlas::PackCharts(atlas, pack);
    if (atlas->meshCount < 1 or atlas->width == 0 or atlas->height == 0) {
      xatlas::Destroy(atlas);
      return false;
    }
    const xatlas::Mesh& xm = atlas->meshes[0];
    const float iw         = 1.0f / float(atlas->width);
    const float ih         = 1.0f / float(atlas->height);
    outUV.resize(xm.vertexCount);
    outXref.resize(xm.vertexCount);
    for (uint32_t v = 0; v < xm.vertexCount; v++) {
      const xatlas::Vertex& xv = xm.vertexArray[v];
      outXref[v]               = xv.xref;                          // -> LOCAL input vert (seam verts duplicate)
      outUV[v]                 = fvec2(xv.uv[0] * iw, xv.uv[1] * ih); // pixels -> [0..1] (this section's OWN domain)
    }
    outTri.assign(xm.indexArray, xm.indexArray + xm.indexCount);
    xatlas::Destroy(atlas);
    return true;
  }

  // a cook-loaded mesh is ALREADY built (disk-cached mesh restored) -> skip the rebuild.
  bool cookLoad(datablock_constptr_t db) override {
    bool ok = MeshComputeInst::cookLoad(db);
    if (ok)
      _built = true;
    return ok;
  }
  void onCookEvicted() override { _built = false; }
  const char* _cookSalt() const final { return "hypermesh.sectionunwrap.v1"; }

  const SectionUnwrapData* _d;
  mesh_outpluginst_ptr_t _output;
  mesh_inpluginst_ptr_t _input;
  bool _built     = false;
  bool _announced = false;
  int _numLayers  = 0;
  std::vector<int> _layerGids; // A8: layer index -> section gid (sorted-gid order; parallels UV0.z)
};

static void _reshapeSectionUnwrapIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
SectionUnwrapData::SectionUnwrapData() {
}
std::shared_ptr<SectionUnwrapData> SectionUnwrapData::createShared() {
  auto d = std::make_shared<SectionUnwrapData>();
  _reshapeSectionUnwrapIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t SectionUnwrapData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<SectionUnwrapInst>(this, g);
}
void SectionUnwrapData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return SectionUnwrapData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeSectionUnwrapIOs(m); });
  clazz->directProperty("padding", &SectionUnwrapData::_padding);
  clazz->directProperty("max_layers", &SectionUnwrapData::_max_layers);
}

///////////////////////////////////////////////////////////////////////////////
// layer -> gid table (O3 stage 2). Prefer the SectionUnwrap inst's recorded table (fresh runs); fall
// back to DERIVING it from the OUTPUT MESH (per-face gid band + the section's per-vertex UV0.z layer) so
// a cook-loaded mesh (onTopologyReady skipped) still yields the correct mapping. NEVER assume
// ascending-gid == layer order.
///////////////////////////////////////////////////////////////////////////////
std::vector<int> sectionUnwrapLayerGids(livehypermesh_ptr_t live, Context* ctx) {
  std::vector<int> out;
  if (not live or not live->_ginst)
    return out;
  // 1) fast path: the reflected inst table (populated in onTopologyReady).
  for (auto inst : live->_ginst->_ordered_module_insts) {
    auto su = std::dynamic_pointer_cast<SectionUnwrapInst>(inst);
    if (su and not su->_layerGids.empty())
      return su->_layerGids;
  }
  // 2) robust path: derive from the assembled mesh (cook-load safe).
  auto mesh = live->_mesh;
  if (not mesh or not ctx)
    return out;
  const int nf = mesh->_num_faces;
  auto tagch   = mesh->face("__tags");
  auto uvch    = mesh->channel(MeshChannel::UV0);
  auto foch    = mesh->_face_offsets;
  auto vich    = mesh->_vidx;
  if (nf < 1 or not tagch or not uvch or not foch or not vich)
    return out;
  auto fxi = ctx->FXI();
  auto rdU = [&](FxShaderStorageBuffer* ss, int count) {
    std::vector<uint32_t> v(std::max(1, count), 0u);
    auto m = fxi->mapStorageBuffer(ss, 0, size_t(std::max(1, count)) * 4, BufferMapAccess::READ_ONLY);
    std::memcpy(v.data(), m->_mappedaddr, size_t(std::max(1, count)) * 4);
    fxi->unmapStorageBuffer(m.get());
    return v;
  };
  auto tags = rdU(tagch->_ssbo, nf);
  auto fo   = rdU(foch->_ssbo, nf + 1);
  auto vidx = rdU(vich->_ssbo, mesh->_num_corners);
  std::vector<float> uv(size_t(std::max(1, mesh->_num_verts)) * 4, 0.0f);
  {
    auto m = fxi->mapStorageBuffer(uvch->_ssbo, 0, size_t(std::max(1, mesh->_num_verts)) * 16, BufferMapAccess::READ_ONLY);
    std::memcpy(uv.data(), m->_mappedaddr, size_t(std::max(1, mesh->_num_verts)) * 16);
    fxi->unmapStorageBuffer(m.get());
  }
  std::map<int, int> layer2gid;                    // layer -> gid (ground truth, deduped)
  for (int f = 0; f < nf; f++) {
    uint32_t a = fo[f];
    if (a >= vidx.size())
      continue;
    uint32_t v0 = vidx[a];
    if (v0 >= uint32_t(mesh->_num_verts))
      continue;
    int layer = int(std::lround(uv[v0 * 4 + 2]));  // section layer carried in UV0.z
    int gid   = int((f < int(tags.size()) ? tags[f] : 0u) >> 20u) & 0xFFF;
    layer2gid[layer] = gid;
  }
  if (layer2gid.empty())
    return out;
  int maxLayer = layer2gid.rbegin()->first;
  out.assign(size_t(maxLayer + 1), 0);
  for (auto& [layer, gid] : layer2gid)
    if (layer >= 0 and layer <= maxLayer)
      out[layer] = gid;
  return out;
}

} // namespace ork::lev2::hypermesh
