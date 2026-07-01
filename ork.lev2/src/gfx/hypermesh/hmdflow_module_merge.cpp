////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include <algorithm>
#include <cstring>

ImplementReflectionX(ork::lev2::hypermesh::MergeMeshData, "hypermesh::MergeMeshData");

namespace ork::lev2::hypermesh {

namespace dgfx = ork::lev2::dflowgfx;

///////////////////////////////////////////////////////////////////////////////
// MergeMeshModuleInst — CPU concat of two input GpuMeshes (A,B) into one. Both inputs must be
// host-readable, so the build runs in onTopologyReady (post first-eval sync), like subdivide's CPU
// path. Eval-1 (pre-build) the output PASSES THROUGH input A (the trunk) so the frame is valid;
// onTopologyReady then emits the real merged mesh and requests a re-eval. A cook-loaded node skips
// the rebuild (the merged mesh is restored from disk).
///////////////////////////////////////////////////////////////////////////////
struct MergeMeshModuleInst : public MeshComputeInst {
  MergeMeshModuleInst(const MergeMeshData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}

  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _inA    = typedInputNamed<MeshPlugTraits>("A");
    _inB    = typedInputNamed<MeshPlugTraits>("B");
  }

  // eval-1 passthrough: until onTopologyReady builds the merge, alias input A so the output is a valid
  // mesh (shows the trunk). Once built, leave the merged output untouched.
  void compute(dflow::GraphInst*, ui::updatedata_ptr_t) final {
    if (_built)
      return;
    auto A   = _srcMesh(_inA);
    auto out = _output->_value;
    if (A and A->_num_faces > 0 and A->_vidx) {
      out->_channels     = A->_channels;
      out->_vattrs       = A->_vattrs;
      out->_faces        = A->_faces;
      out->_vidx         = A->_vidx;
      out->_face_offsets = A->_face_offsets;
      out->_header       = A->_header;
      out->_capacity     = A->_capacity;
      out->_num_verts    = A->_num_verts;
      out->_num_corners  = A->_num_corners;
      out->_num_faces    = A->_num_faces;
      out->markTopoChanged();
    }
  }

  bool onTopologyReady(Context* ctx) final {
    if (_built)
      return false;
    auto A = _srcMesh(_inA);
    auto B = _srcMesh(_inB);
    if (not A or not B)
      return false; // inputs not wired yet
    // The CPU concat needs the inputs TOPOLOGICALLY SETTLED. Passthrough ops upstream (select/assign_gid)
    // wire their output topology in the PRE-compute phase from a not-yet-computed input, so a chain like
    // select->assign_gid only settles on the SECOND eval. DEFER (request another cascade pass) until both
    // inputs expose real topology buffers — never read a half-wired mesh (null face table = the crash).
    auto settled = [](gpumesh_ptr_t M) {
      return M->_face_offsets and M->_face_offsets->_ssbo
         and M->_vidx and M->_vidx->_ssbo
         and M->channel(MeshChannel::POSITION) and M->channel(MeshChannel::POSITION)->_ssbo;
    };
    if (not settled(A) or not settled(B))
      return true; // not settled -> re-eval + re-check next pass (the fixpoint cascade re-calls us)
    const int nvA = A->_num_verts, ncA = A->_num_corners, nfA = A->_num_faces;
    const int nvB = B->_num_verts, ncB = B->_num_corners, nfB = B->_num_faces;
    if ((nfA + nfB) == 0)
      return false; // both empty -> nothing to merge
    auto env = _graphinst->_impl.getShared<MeshEnv>();
    auto fxi = ctx->FXI();

    const int nv = nvA + nvB, nc = ncA + ncB, nf = nfA + nfB;

    // ---- readback helpers (mid-graph READ_ONLY map is legal here: onTopologyReady runs after the
    //      driver's first full eval + sync, so both producers' GPU writes are settled + host-readable).
    auto rdF = [&](gpumesh_ptr_t M, MeshChannel ch, int nverts, std::vector<float>& dst, int voff) {
      auto c = M->channel(ch);
      if (not c or nverts == 0)
        return; // missing channel -> leave the slot's default (P/N/B/UV=0, COLOR=1)
      auto mp = fxi->mapStorageBuffer(c->_ssbo, 0, size_t(nverts) * 16, BufferMapAccess::READ_ONLY);
      std::memcpy(dst.data() + size_t(voff) * 4, mp->_mappedaddr, size_t(nverts) * 16);
      fxi->unmapStorageBuffer(mp.get());
    };
    auto rdU = [&](FxShaderStorageBuffer* ss, int count, std::vector<uint32_t>& dst, int off) {
      if (not ss or count == 0)
        return;
      auto mp = fxi->mapStorageBuffer(ss, 0, size_t(count) * 4, BufferMapAccess::READ_ONLY);
      std::memcpy(dst.data() + off, mp->_mappedaddr, size_t(count) * 4);
      fxi->unmapStorageBuffer(mp.get());
    };

    std::vector<float> P(size_t(nv) * 4, 0), Nr(size_t(nv) * 4, 0), Bn(size_t(nv) * 4, 0),
        UV(size_t(nv) * 4, 0), Cl(size_t(nv) * 4, 1.0f);
    rdF(A, MeshChannel::POSITION, nvA, P, 0);   rdF(B, MeshChannel::POSITION, nvB, P, nvA);
    rdF(A, MeshChannel::NORMAL, nvA, Nr, 0);    rdF(B, MeshChannel::NORMAL, nvB, Nr, nvA);
    rdF(A, MeshChannel::BINORMAL, nvA, Bn, 0);  rdF(B, MeshChannel::BINORMAL, nvB, Bn, nvA);
    rdF(A, MeshChannel::UV0, nvA, UV, 0);       rdF(B, MeshChannel::UV0, nvB, UV, nvA);
    rdF(A, MeshChannel::COLOR, nvA, Cl, 0);     rdF(B, MeshChannel::COLOR, nvB, Cl, nvA);

    // corner -> vertex indices: B's are rebased past A's verts.
    std::vector<uint32_t> VI(size_t(nc), 0);
    rdU(A->_vidx->_ssbo, ncA, VI, 0);
    rdU(B->_vidx->_ssbo, ncB, VI, ncA);
    for (int i = ncA; i < nc; i++)
      VI[i] += uint32_t(nvA);

    // CSR face_offsets: concat, rebasing B's by A's corner count. FO[nfA]=ncA is set by A's terminal.
    std::vector<uint32_t> foA(size_t(nfA) + 1, 0), foB(size_t(nfB) + 1, 0), FO(size_t(nf) + 1, 0);
    rdU(A->_face_offsets->_ssbo, nfA + 1, foA, 0);
    rdU(B->_face_offsets->_ssbo, nfB + 1, foB, 0);
    for (int f = 0; f <= nfA; f++)
      FO[f] = foA[f];
    for (int f = 1; f <= nfB; f++)
      FO[nfA + f] = foB[f] + uint32_t(ncA);

    // __tags gid band [20:31] per source. gid >= 0 RESTAMPS the band (preserving the source's low/select
    // bits); gid < 0 PRESERVES the source's existing gids untouched (so a trunk that already split its
    // upper-branch faces into gid 1 keeps that split, and only the leaves get restamped).
    std::vector<uint32_t> TG(size_t(nf), 0);
    auto tagSrc = [&](gpumesh_ptr_t M, int nfaces, int off, int gid) {
      auto tc = M->face("__tags");
      if (tc)
        rdU(tc->_ssbo, nfaces, TG, off); // carry the source's __tags through (gids + select bits)
      if (gid < 0)
        return;                          // PRESERVE — leave the source's existing gid band as-is
      const uint32_t g = (uint32_t(gid) & 0xFFFu) << 20u;
      for (int f = 0; f < nfaces; f++)
        TG[size_t(off) + f] = (TG[size_t(off) + f] & 0x000FFFFFu) | g;
    };
    tagSrc(A, nfA, 0, _d->_gid_a);
    tagSrc(B, nfB, nfA, _d->_gid_b);

    fvec3 bbmin(1e30f, 1e30f, 1e30f), bbmax(-1e30f, -1e30f, -1e30f);
    for (int v = 0; v < nv; v++) {
      float x = P[v * 4], y = P[v * 4 + 1], z = P[v * 4 + 2];
      bbmin.x = std::min(bbmin.x, x); bbmin.y = std::min(bbmin.y, y); bbmin.z = std::min(bbmin.z, z);
      bbmax.x = std::max(bbmax.x, x); bbmax.y = std::max(bbmax.y, y); bbmax.z = std::max(bbmax.z, z);
    }

    // ---- allocate the combined mesh + the face __tags channel, then upload (same staging path as LSweep).
    auto mesh = _output->_value;
    allocMesh(env, mesh, nv, nc, nf,
              {MeshChannel::POSITION, MeshChannel::NORMAL, MeshChannel::BINORMAL,
               MeshChannel::UV0, MeshChannel::COLOR});
    // drop any channels the eval-1 trunk-passthrough aliased in (allocMesh only refreshes the VERTEX
    // _channels, not the FACE _faces / named _vattrs) — a stale material_id sized to A would be undersized
    // for the merged face count (OOB at render + refuses to cook-cache). The merge owns only __tags.
    mesh->_vattrs.clear();
    mesh->_faces.clear();
    mesh->_faces["__tags"] = env->_pool->acquireChannel(4, std::max(1, nf));

    {
      auto mp = fxi->mapStorageBuffer(mesh->_header, 0, 64, BufferMapAccess::WRITE_ONLY);
      auto hu = (uint32_t*)mp->_mappedaddr;
      auto hf = (float*)mp->_mappedaddr;
      hu[0] = uint32_t(nv); hu[1] = uint32_t(nc); hu[2] = uint32_t(nf); hu[3] = 0u;
      hf[4] = bbmin.x; hf[5] = bbmin.y; hf[6] = bbmin.z; hf[7] = 1;
      hf[8] = bbmax.x; hf[9] = bbmax.y; hf[10] = bbmax.z; hf[11] = 1;
      fxi->unmapStorageBuffer(mp.get());
    }
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
    upf(MeshChannel::POSITION, P);
    upf(MeshChannel::NORMAL, Nr);
    upf(MeshChannel::BINORMAL, Bn);
    upf(MeshChannel::UV0, UV);
    upf(MeshChannel::COLOR, Cl);
    upu(mesh->_vidx->_ssbo, VI);
    upu(mesh->_face_offsets->_ssbo, FO);
    upu(mesh->_faces["__tags"]->_ssbo, TG);

    mesh->markTopoChanged();
    _built = true;
    ackSrcTopo(_inA);
    ackSrcTopo(_inB);
    if (not _announced) {
      printf("MergeMesh<%s>: A(%dv/%df gid%d) + B(%dv/%df gid%d) -> %dv/%df\n",
             _dgmodule_data->_name.c_str(), nvA, nfA, _d->_gid_a, nvB, nfB, _d->_gid_b, nv, nf);
      _announced = true;
    }
    return true; // eval-1 was the trunk passthrough -> re-eval so downstream sees the merged mesh
  }

  // a cook-loaded merge is ALREADY built (the disk-cached merged mesh is restored) -> skip the readback.
  bool cookLoad(datablock_constptr_t db) override {
    bool ok = MeshComputeInst::cookLoad(db);
    if (ok)
      _built = true;
    return ok;
  }

  const char* _cookSalt() const final;

  const MergeMeshData*   _d;
  mesh_outpluginst_ptr_t _output;
  mesh_inpluginst_ptr_t  _inA, _inB;
  bool _built     = false;
  bool _announced = false;
};

// cook salt ties to THIS TU's compile time — any kernel change auto-invalidates the disk cook-cache.
const char* MergeMeshModuleInst::_cookSalt() const {
  return "mergemesh " __DATE__ " " __TIME__;
}

static void _reshapeMergeIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "A");
  dflow::ModuleData::createInputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "B");
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
MergeMeshData::MergeMeshData() {
}
std::shared_ptr<MergeMeshData> MergeMeshData::createShared() {
  auto d = std::make_shared<MergeMeshData>();
  _reshapeMergeIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t MergeMeshData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<MergeMeshModuleInst>(this, g);
}
void MergeMeshData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return MergeMeshData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeMergeIOs(m); });
  clazz->directProperty("gid_a", &MergeMeshData::_gid_a);
  clazz->directProperty("gid_b", &MergeMeshData::_gid_b);
}

} // namespace ork::lev2::hypermesh
