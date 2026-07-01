////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include <cmath>
#include <cstring>

ImplementReflectionX(ork::lev2::hypermesh::LSweepModuleData, "hypermesh::LSweepModuleData");

namespace ork::lev2::hypermesh {

namespace dgfx = ork::lev2::dflowgfx;

// read the connected XfNodeGraph value (the _srcSdf pattern from displace_sdf).
static ::ork::hyper::xfnodegraph_inst_ptr_t _srcXfng(dgfx::xfng_inpluginst_ptr_t inp) {
  auto out = std::dynamic_pointer_cast<dgfx::xfng_outpluginst_t>(inp->_connectedOutput);
  return out ? out->_value : nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// LSweepModule (L-system family, M1 = G0b) — the SKINNER. Consumes an XfNodeGraph
// (ork::hyper) and emits a swept generalized-cylinder GpuMesh: one `sides`-gon ring
// per node (oriented by the node frame, scaled by attrs.x = radius), and `sides`
// quads per parent->child edge. v1 builds on the CPU from the XfNodeGraph CPU mirror
// and uploads the mesh channels (a GPU compute port is the follow-up).
///////////////////////////////////////////////////////////////////////////////

struct LSweepModuleInst : public MeshComputeInst {
  LSweepModuleInst(const LSweepModuleData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}

  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _input  = typedInputNamed<dgfx::XfNodeGraphPlugTraits>("In");
  }

  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto xng = _srcXfng(_input);
    if (not xng or xng->_nodes.empty())
      return; // producer not ready yet (or empty graph)
    if (xng->_version == _lastXngV)
      return; // up to date — reskin only when the producer re-emitted the skeleton (wind / edit)
    _lastXngV = xng->_version;
    auto env = inst->_impl.getShared<MeshEnv>();
    auto fxi = env->_ctx->FXI();

    const auto& nodes = xng->_nodes;
    const int N = int(nodes.size());
    const int S = std::max(3, _d->_sides);

    int nedges = 0;
    std::vector<int> childcount(size_t(N), 0);
    int rootidx = -1;
    for (int i = 0; i < N; i++) {
      uint32_t pa = nodes[i]._parent;
      if (pa != 0xffffffffu and int(pa) < N) {
        nedges++;
        childcount[pa]++;
      } else if (rootidx < 0) {
        rootidx = i; // turtle start = trunk base
      }
    }
    // end caps: every OPEN ring (the trunk base + each childless tip) is closed with a rounded
    // dome (`_cap_segments` converging sub-rings + an apex), or a flat n-gon when segments<=0.
    std::vector<std::pair<int, bool>> caps; // (node, isBase) — base winds reversed, like the wall
    if (rootidx >= 0)
      caps.emplace_back(rootidx, true);
    for (int i = 0; i < N; i++)
      if (childcount[i] == 0 and i != rootidx)
        caps.emplace_back(i, false);
    const int ncaps    = int(caps.size());
    const int D        = std::max(0, _d->_cap_segments);
    const float capRnd = _d->_cap_round;
    int capVerts, capFaces, capCorners;
    if (D <= 0) {
      capVerts = 0; capFaces = ncaps; capCorners = ncaps * S;
    } else {
      capVerts   = ncaps * (D * S + 1);       // D sub-rings of S + 1 apex per cap
      capFaces   = ncaps * S * (D + 1);        // D quad strips + 1 apex fan, S each
      capCorners = ncaps * (D * S * 4 + S * 3);
    }
    const int nverts   = N * S + capVerts;
    const int nfaces   = nedges * S + capFaces;
    const int ncorners = nedges * S * 4 + capCorners;

    auto mesh = _output->_value;
    // STABLE-vs-NON-STABLE: the producer's _topoVersion tells us whether the count/connectivity
    // changed. A POSITION-only update (wind: _version bumped, _topoVersion unchanged) reuses the
    // existing channels — NO realloc, and markChanged at the end so the renderer does NOT
    // re-triangulate. Only a real topology change reallocs + markTopoChanged (full re-triangulate).
    bool topoChanged = (xng->_topoVersion != _lastTopoV) or mesh->_channels.empty() or not mesh->_vidx;
    _lastTopoV = xng->_topoVersion;
    if (topoChanged) {
      allocMesh(env, mesh, nverts, ncorners, nfaces,
                {MeshChannel::POSITION, MeshChannel::NORMAL, MeshChannel::BINORMAL,
                 MeshChannel::UV0, MeshChannel::COLOR});
      mesh->_faces["material_id"] = env->_pool->acquireChannel(4, std::max(1, nfaces)); // FACE attr (uint material id)
    }
    auto matc = mesh->_faces["material_id"];

    // --- CPU build ---
    std::vector<float> P(size_t(nverts) * 4), Nr(size_t(nverts) * 4), Bn(size_t(nverts) * 4),
        UV(size_t(nverts) * 4), Cl(size_t(nverts) * 4, 1.0f);
    std::vector<uint32_t> VI(size_t(ncorners), 0u);
    std::vector<uint32_t> FO(size_t(nfaces) + 1, 0u);
    std::vector<uint32_t> MAT(size_t(std::max(1, nfaces)), 0u);
    fvec3 bbmin(1e30f, 1e30f, 1e30f), bbmax(-1e30f, -1e30f, -1e30f);

    for (int i = 0; i < N; i++) {
      const float* m = nodes[i]._xform;        // column-major: X=right, Y=heading, Z=up, T=pos
      fvec3 right(m[0], m[1], m[2]);
      fvec3 head(m[4], m[5], m[6]);
      fvec3 up(m[8], m[9], m[10]);
      fvec3 pos(m[12], m[13], m[14]);
      float rad = nodes[i]._attrs[0];
      for (int s = 0; s < S; s++) {
        float a   = 6.28318530718f * float(s) / float(S);
        fvec3 dir = right * std::cos(a) + up * std::sin(a); // radial (also the outward normal)
        fvec3 p   = pos + dir * rad;
        int v     = i * S + s;
        P[v * 4] = p.x; P[v * 4 + 1] = p.y; P[v * 4 + 2] = p.z; P[v * 4 + 3] = 1;
        Nr[v * 4] = dir.x; Nr[v * 4 + 1] = dir.y; Nr[v * 4 + 2] = dir.z; Nr[v * 4 + 3] = 0;
        Bn[v * 4] = head.x; Bn[v * 4 + 1] = head.y; Bn[v * 4 + 2] = head.z; Bn[v * 4 + 3] = 0;
        UV[v * 4] = float(s) / float(S); UV[v * 4 + 1] = nodes[i]._attrs[1]; // u = around, v = generation
        bbmin.x = std::min(bbmin.x, p.x); bbmin.y = std::min(bbmin.y, p.y); bbmin.z = std::min(bbmin.z, p.z);
        bbmax.x = std::max(bbmax.x, p.x); bbmax.y = std::max(bbmax.y, p.y); bbmax.z = std::max(bbmax.z, p.z);
      }
    }
    int fc = 0, cc = 0;
    for (int i = 0; i < N; i++) {
      uint32_t pa = nodes[i]._parent;
      if (pa == 0xffffffffu or int(pa) >= N)
        continue;
      for (int s = 0; s < S; s++) {
        int s2 = (s + 1) % S;
        // quad wound CCW from outside: parent.s, parent.s2, child.s2, child.s
        FO[fc]      = uint32_t(cc);
        VI[cc++]    = uint32_t(int(pa) * S + s);
        VI[cc++]    = uint32_t(int(pa) * S + s2);
        VI[cc++]    = uint32_t(i * S + s2);
        VI[cc++]    = uint32_t(i * S + s);
        MAT[fc]     = 0u;
        fc++;
      }
    }
    // --- end caps: rounded dome per open ring (flat n-gon when _cap_segments<=0) ---
    // a dome is D converging sub-rings (quarter-circle profile) + an apex; hemisphere normals
    // give it real rounded shading. Tip caps bulge +heading (natural winding), base caps
    // -heading (reversed) — matching the wall-quad front-face convention.
    int dv = N * S; // next appended dome vertex
    auto putVert = [&](const fvec3& p, const fvec3& n, const fvec3& b, float u, float vg) -> uint32_t {
      int v = dv++;
      P[v * 4] = p.x;  P[v * 4 + 1] = p.y;  P[v * 4 + 2] = p.z;  P[v * 4 + 3] = 1;
      Nr[v * 4] = n.x; Nr[v * 4 + 1] = n.y; Nr[v * 4 + 2] = n.z; Nr[v * 4 + 3] = 0;
      Bn[v * 4] = b.x; Bn[v * 4 + 1] = b.y; Bn[v * 4 + 2] = b.z; Bn[v * 4 + 3] = 0;
      UV[v * 4] = u;   UV[v * 4 + 1] = vg;  UV[v * 4 + 2] = 0;   UV[v * 4 + 3] = 0;
      bbmin.x = std::min(bbmin.x, p.x); bbmin.y = std::min(bbmin.y, p.y); bbmin.z = std::min(bbmin.z, p.z);
      bbmax.x = std::max(bbmax.x, p.x); bbmax.y = std::max(bbmax.y, p.y); bbmax.z = std::max(bbmax.z, p.z);
      return uint32_t(v);
    };
    for (auto& cap : caps) {
      int nd = cap.first; bool isBase = cap.second;
      if (D <= 0) { // flat n-gon over the existing ring verts
        FO[fc] = uint32_t(cc);
        for (int k = 0; k < S; k++) {
          int s    = isBase ? (S - 1 - k) : k;
          VI[cc++] = uint32_t(nd * S + s);
        }
        MAT[fc] = 0u; fc++;
        continue;
      }
      const float* m = nodes[nd]._xform;
      fvec3 R(m[0], m[1], m[2]), H(m[4], m[5], m[6]), U(m[8], m[9], m[10]), c(m[12], m[13], m[14]);
      float rad = nodes[nd]._attrs[0], vg = nodes[nd]._attrs[1];
      fvec3 O = isBase ? (H * -1.0f) : H; // the cap bulges this way
      std::vector<std::vector<uint32_t>> rings(size_t(D), std::vector<uint32_t>(size_t(S), 0u));
      for (int l = 0; l < D; l++) {
        float phi = (float(l + 1) / float(D + 1)) * 1.5707963f; // 0 (ring) .. pi/2 (apex)
        float cr = std::cos(phi), sr = std::sin(phi);
        for (int s = 0; s < S; s++) {
          float a      = 6.28318530718f * float(s) / float(S);
          fvec3 radial = R * std::cos(a) + U * std::sin(a);
          fvec3 tang   = R * (-std::sin(a)) + U * std::cos(a); // azimuthal tangent (binormal)
          fvec3 p      = c + radial * (rad * cr) + O * (rad * capRnd * sr);
          fvec3 n      = (radial * cr + O * sr).normalized();
          rings[l][s]  = putVert(p, n, tang, float(s) / float(S), vg);
        }
      }
      uint32_t apex = putVert(c + O * (rad * capRnd), O, R, 0.5f, vg);
      auto vat = [&](int level, int s) -> uint32_t {
        return (level < 0) ? uint32_t(nd * S + s) : rings[size_t(level)][size_t(s)];
      };
      for (int lp = -1; lp < D - 1; lp++) { // quad strips: main ring → sub-ring 0 → ... → sub-ring D-1
        for (int s = 0; s < S; s++) {
          int s2      = (s + 1) % S;
          uint32_t a0 = vat(lp, s), a1 = vat(lp, s2), b1 = vat(lp + 1, s2), b0 = vat(lp + 1, s);
          FO[fc] = uint32_t(cc);
          if (not isBase) { VI[cc++] = a0; VI[cc++] = a1; VI[cc++] = b1; VI[cc++] = b0; }
          else            { VI[cc++] = a1; VI[cc++] = a0; VI[cc++] = b0; VI[cc++] = b1; }
          MAT[fc] = 0u; fc++;
        }
      }
      for (int s = 0; s < S; s++) { // apex triangle fan
        int s2 = (s + 1) % S;
        FO[fc] = uint32_t(cc);
        if (not isBase) { VI[cc++] = rings[size_t(D-1)][size_t(s)];  VI[cc++] = rings[size_t(D-1)][size_t(s2)]; VI[cc++] = apex; }
        else            { VI[cc++] = rings[size_t(D-1)][size_t(s2)]; VI[cc++] = rings[size_t(D-1)][size_t(s)];  VI[cc++] = apex; }
        MAT[fc] = 0u; fc++;
      }
    }

    FO[nfaces] = uint32_t(ncorners); // CSR terminal

    // --- upload (DEVICE_LOCAL channels route through the staging copy on unmap) ---
    {
      auto mp  = fxi->mapStorageBuffer(mesh->_header, 0, 64, BufferMapAccess::WRITE_ONLY);
      auto hu  = (uint32_t*)mp->_mappedaddr;
      auto hf  = (float*)mp->_mappedaddr;
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
    upf(MeshChannel::POSITION, P);  // positions + normals move every frame under wind
    upf(MeshChannel::NORMAL, Nr);
    upf(MeshChannel::BINORMAL, Bn);
    if (topoChanged) { // UV (u=around / v=generation), color, and the topology arrays are static
      upf(MeshChannel::UV0, UV);
      upf(MeshChannel::COLOR, Cl);
      upu(mesh->_vidx->_ssbo, VI);
      upu(mesh->_face_offsets->_ssbo, FO);
      upu(matc->_ssbo, MAT);
    }

    if (topoChanged) mesh->markTopoChanged(); else mesh->markChanged();
    if (not _announced) {
      printf("LSweep<%s>: %d verts, %d faces (%d-gon) from %d nodes\n",
             _dgmodule_data->_name.c_str(), nverts, nfaces, S, N);
      _announced = true;
    }
  }

  const char* _cookSalt() const final;

  const LSweepModuleData* _d;
  mesh_outpluginst_ptr_t _output;
  dgfx::xfng_inpluginst_ptr_t _input;
  uint64_t _lastXngV  = ~0ull; // reskin only when the XfNodeGraph _version changes (animated rebuild)
  uint64_t _lastTopoV = ~0ull; // realloc + re-triangulate only when _topoVersion changes (non-stable topo)
  bool _announced     = false; // print the count line once, not every animated frame
};

// cook salt ties to THIS TU's compile time, so any LSweep kernel change (e.g. adding
// end caps) auto-invalidates the disk cook-cache on rebuild — no manual version bump.
const char* LSweepModuleInst::_cookSalt() const {
  return "lsweep " __DATE__ " " __TIME__;
}

static void _reshapeLSweepIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<dgfx::XfNodeGraphPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
LSweepModuleData::LSweepModuleData() {
}
std::shared_ptr<LSweepModuleData> LSweepModuleData::createShared() {
  auto d = std::make_shared<LSweepModuleData>();
  _reshapeLSweepIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t LSweepModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<LSweepModuleInst>(this, g);
}
void LSweepModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return LSweepModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeLSweepIOs(m); });
  clazz->directProperty("sides", &LSweepModuleData::_sides);
  clazz->directProperty("cap_segments", &LSweepModuleData::_cap_segments);
  clazz->directProperty("cap_round", &LSweepModuleData::_cap_round);
}

} // namespace ork::lev2::hypermesh
