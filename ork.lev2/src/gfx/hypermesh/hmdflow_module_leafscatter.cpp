////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include <cmath>
#include <cstring>

ImplementReflectionX(ork::lev2::hypermesh::LeafScatterModuleData, "hypermesh::LeafScatterModuleData");

namespace ork::lev2::hypermesh {

namespace dgfx = ork::lev2::dflowgfx;

// read the connected XfNodeGraph value (the _srcXfng pattern, shared with LSweep).
static ::ork::hyper::xfnodegraph_inst_ptr_t _srcXfng(dgfx::xfng_inpluginst_ptr_t inp) {
  auto out = std::dynamic_pointer_cast<dgfx::xfng_outpluginst_t>(inp->_connectedOutput);
  return out ? out->_value : nullptr;
}

// stateless hash -> uint32, deterministic per (seed, node, leaf) — for reproducible per-leaf jitter.
static inline uint32_t _lhash(int a, int b, int c) {
  uint32_t x = uint32_t(a) * 2654435761u + uint32_t(b) * 2246822519u + uint32_t(c) * 3266489917u + 374761393u;
  x ^= x >> 15; x *= 2246822519u; x ^= x >> 13; x *= 3266489917u; x ^= x >> 16;
  return x;
}

///////////////////////////////////////////////////////////////////////////////
// LeafScatterModuleInst — reads the XfNodeGraph skeleton (same input LSweep skins) and emits a
// broadleaf-card GpuMesh: at every node with generation (_attrs[1]) >= _min_gen, _per_node leaf cards
// placed by PHYLLOTAXIS (golden-angle roll around the node heading, drooped _pitch from it). CPU build.
///////////////////////////////////////////////////////////////////////////////
struct LeafScatterModuleInst : public MeshComputeInst {
  LeafScatterModuleInst(const LeafScatterModuleData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}

  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _input  = typedInputNamed<dgfx::XfNodeGraphPlugTraits>("In");
  }

  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto xng = _srcXfng(_input);
    if (not xng or xng->_nodes.empty())
      return; // producer not ready yet
    if (xng->_version == _lastXngV)
      return; // up to date — the leaves are static cards (wind is VS-side); rebuild only on skeleton change
    _lastXngV = xng->_version;
    auto env = inst->_impl.getShared<MeshEnv>();
    auto fxi = env->_ctx->FXI();

    const auto& nodes = xng->_nodes;
    const int   N     = int(nodes.size());
    const int   PER   = std::max(1, _d->_per_node);
    const int   QPL   = (_d->_style == 1) ? 2 : 1; // quads per leaf (cross = 2)

    // eligible nodes: generation (_attrs[1]) >= _min_gen — the outer twigs bear the canopy.
    std::vector<int> elig;
    for (int i = 0; i < N; i++)
      if (nodes[i]._attrs[1] >= _d->_min_gen)
        elig.push_back(i);

    const int nleaves  = int(elig.size()) * PER;
    const int nverts   = nleaves * 4 * QPL;
    const int nfaces   = nleaves * QPL;
    const int ncorners = nfaces * 4;
    if (nleaves == 0)
      return; // no canopy at this _min_gen — leave the mesh empty (nothing to draw)

    auto mesh = _output->_value;
    allocMesh(env, mesh, nverts, ncorners, nfaces,
              {MeshChannel::POSITION, MeshChannel::NORMAL, MeshChannel::BINORMAL,
               MeshChannel::UV0, MeshChannel::COLOR});
    mesh->_faces["material_id"] = env->_pool->acquireChannel(4, std::max(1, nfaces));
    auto matc = mesh->_faces["material_id"];

    std::vector<float> P(size_t(nverts) * 4), Nr(size_t(nverts) * 4), Bn(size_t(nverts) * 4),
        UV(size_t(nverts) * 4), Cl(size_t(nverts) * 4, 1.0f);
    std::vector<uint32_t> VI(size_t(ncorners), 0u);
    std::vector<uint32_t> FO(size_t(nfaces) + 1, 0u);
    std::vector<uint32_t> MAT(size_t(std::max(1, nfaces)), 0u);
    fvec3 bbmin(1e30f, 1e30f, 1e30f), bbmax(-1e30f, -1e30f, -1e30f);

    const float DEG     = 0.01745329252f;
    const float rollRad = _d->_roll * DEG;
    int vbase = 0, fbase = 0, cc = 0;

    auto putVert = [&](const fvec3& p, const fvec3& n, const fvec3& w, float u, float vv, float flut, float lhash) {
      int v = vbase++;
      P[v * 4] = p.x;  P[v * 4 + 1] = p.y;  P[v * 4 + 2] = p.z;  P[v * 4 + 3] = 1;
      Nr[v * 4] = n.x; Nr[v * 4 + 1] = n.y; Nr[v * 4 + 2] = n.z; Nr[v * 4 + 3] = 0;
      Bn[v * 4] = w.x; Bn[v * 4 + 1] = w.y; Bn[v * 4 + 2] = w.z; Bn[v * 4 + 3] = 0; // width axis -> tangent
      UV[v * 4] = u;   UV[v * 4 + 1] = vv;  UV[v * 4 + 2] = 0;   UV[v * 4 + 3] = 0; // card uv (material textures it)
      Cl[v * 4] = flut;                 // COLOR.x = flutter weight (0 petiole .. 1 tip)
      Cl[v * 4 + 1] = lhash;            // COLOR.y = per-leaf hash (hue/phase variation)
      Cl[v * 4 + 2] = 0; Cl[v * 4 + 3] = 1;
      bbmin.x = std::min(bbmin.x, p.x); bbmin.y = std::min(bbmin.y, p.y); bbmin.z = std::min(bbmin.z, p.z);
      bbmax.x = std::max(bbmax.x, p.x); bbmax.y = std::max(bbmax.y, p.y); bbmax.z = std::max(bbmax.z, p.z);
    };

    for (int e = 0; e < int(elig.size()); e++) {
      const int   ni = elig[e];
      const float* m = nodes[ni]._xform; // column-major: X=right Y=heading Z=up T=pos
      fvec3 R(m[0], m[1], m[2]), H(m[4], m[5], m[6]), U(m[8], m[9], m[10]), Pn(m[12], m[13], m[14]);
      const float nodeBase = float(ni) * 2.399963f; // per-node golden-angle stagger (so nodes don't align)

      for (int j = 0; j < PER; j++) {
        uint32_t h  = _lhash(_d->_seed, ni, j);
        float    j0 = float(h & 0xffff) / 65535.0f;          // [0,1)
        float    j1 = float((h >> 16) & 0xffff) / 65535.0f;  // [0,1)
        float    az    = nodeBase + float(j) * rollRad + _d->_jitter * (j0 * 2.0f - 1.0f) * 0.4f;
        float    pitch = _d->_pitch * DEG * (1.0f + _d->_jitter * (j1 * 2.0f - 1.0f) * 0.5f);
        float    sz    = _d->_size * (1.0f + _d->_jitter * (j0 * 2.0f - 1.0f) * 0.4f);
        float    hw    = sz * _d->_aspect * 0.5f;
        float    lh    = float(h & 0xff) / 255.0f;           // per-leaf hash -> COLOR.y

        fvec3 outdir = R * std::cos(az) + U * std::sin(az);                 // radial (perp to heading)
        fvec3 L      = (H * std::cos(pitch) + outdir * std::sin(pitch));    // blade length axis (drooped)
        if (L.magnitude() < 1e-5f) continue;
        L = L.normalized();
        fvec3 W = L.crossWith(H);                                          // width axis (tangential)
        if (W.magnitude() < 1e-4f) W = L.crossWith(R);                     // degenerate guard (L ~ H)
        W = W.normalized();
        fvec3 Nn = W.crossWith(L).normalized();                            // leaf face normal

        // one quad (style 0) or a perpendicular cross (style 1). corners: petiole L/R -> tip R/L,
        // wound CCW from the front face. UV v = 0 at petiole, 1 at tip (= flutter weight).
        auto putQuad = [&](const fvec3& waxis, const fvec3& nrm) {
          int v0 = vbase;
          putVert(Pn + waxis * (-hw),          nrm, waxis, 0.0f, 0.0f, 0.0f, lh);
          putVert(Pn + waxis * (hw),           nrm, waxis, 1.0f, 0.0f, 0.0f, lh);
          putVert(Pn + L * sz + waxis * (hw),  nrm, waxis, 1.0f, 1.0f, 1.0f, lh);
          putVert(Pn + L * sz + waxis * (-hw), nrm, waxis, 0.0f, 1.0f, 1.0f, lh);
          FO[fbase] = uint32_t(cc);
          VI[cc++]  = uint32_t(v0);
          VI[cc++]  = uint32_t(v0 + 1);
          VI[cc++]  = uint32_t(v0 + 2);
          VI[cc++]  = uint32_t(v0 + 3);
          MAT[fbase] = 0u;
          fbase++;
        };
        putQuad(W, Nn);
        if (_d->_style == 1)
          putQuad(Nn, W); // the perpendicular blade of the cross (its normal is the first blade's width)
      }
    }
    FO[nfaces] = uint32_t(ncorners); // CSR terminal

    // --- upload (header + channels) — same staging-copy path as LSweep ---
    {
      auto mp = fxi->mapStorageBuffer(mesh->_header, 0, 64, BufferMapAccess::WRITE_ONLY);
      auto hu = (uint32_t*)mp->_mappedaddr;
      auto hf = (float*)mp->_mappedaddr;
      hu[0] = uint32_t(nverts); hu[1] = uint32_t(ncorners); hu[2] = uint32_t(nfaces); hu[3] = 0u;
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
    upu(matc->_ssbo, MAT);

    mesh->markTopoChanged();
    if (not _announced) {
      printf("LeafScatter<%s>: %d leaves (%d verts, %d faces) on %d/%d nodes (gen>=%.1f)\n",
             _dgmodule_data->_name.c_str(), nleaves, nverts, nfaces, int(elig.size()), N, _d->_min_gen);
      _announced = true;
    }
  }

  const char* _cookSalt() const final;

  const LeafScatterModuleData* _d;
  mesh_outpluginst_ptr_t       _output;
  dgfx::xfng_inpluginst_ptr_t  _input;
  uint64_t _lastXngV = ~0ull;
  bool     _announced = false;
};

// cook salt ties to THIS TU's compile time — any kernel change auto-invalidates the disk cook-cache.
const char* LeafScatterModuleInst::_cookSalt() const {
  return "leafscatter " __DATE__ " " __TIME__;
}

static void _reshapeLeafIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<dgfx::XfNodeGraphPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
LeafScatterModuleData::LeafScatterModuleData() {
}
std::shared_ptr<LeafScatterModuleData> LeafScatterModuleData::createShared() {
  auto d = std::make_shared<LeafScatterModuleData>();
  _reshapeLeafIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t LeafScatterModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<LeafScatterModuleInst>(this, g);
}
void LeafScatterModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return LeafScatterModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeLeafIOs(m); });
  clazz->directProperty("style", &LeafScatterModuleData::_style);
  clazz->directProperty("per_node", &LeafScatterModuleData::_per_node);
  clazz->directProperty("min_gen", &LeafScatterModuleData::_min_gen);
  clazz->directProperty("size", &LeafScatterModuleData::_size);
  clazz->directProperty("aspect", &LeafScatterModuleData::_aspect);
  clazz->directProperty("roll", &LeafScatterModuleData::_roll);
  clazz->directProperty("pitch", &LeafScatterModuleData::_pitch);
  clazz->directProperty("jitter", &LeafScatterModuleData::_jitter);
  clazz->directProperty("seed", &LeafScatterModuleData::_seed);
}

} // namespace ork::lev2::hypermesh
