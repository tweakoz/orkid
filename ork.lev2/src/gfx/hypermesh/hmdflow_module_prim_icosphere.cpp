////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include <unordered_map>

ImplementReflectionX(ork::lev2::hypermesh::IcoSphereData, "hypermesh::IcoSphereData");

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////
// IcoSphere (v2, INDEXED, all-tri, DYNAMIC subdivisions) — an icosahedron refined by midpoint
// subdivision with sphere RE-PROJECTION (a true icosphere): each level splits every triangle into 4
// (shared edge midpoints) and projects all verts onto the unit sphere. `subdivisions` is a RUNTIME
// int plug (0..%d; live-tweakable) — every level's unit-sphere DIRECTIONS + topology are precomputed
// once on the CPU + uploaded as constant buffers; per frame a shader writes P = dir*radius (radius
// runtime). V = 10*4^L + 2, F = 20*4^L. Smooth (position-direction) normals -> a clean round sphere.
///////////////////////////////////////////////////////////////////////////////

static constexpr int kIcoMaxLevel = 6;   // L6 = 20*4^6 = 81920 faces (renderable + CPU-rebuildable)

// per-subdivision-level precomputed icosphere data.
struct IcoLevel {
  int nv = 0, nc = 0, nf = 0;
  FxShaderStorageBuffer* dir = nullptr; // vec4[nv] unit directions (xyz) — constant
  gpuchannel_ptr_t vidx, fo;            // all-tri topology — constant (mesh aliases these)
};

// one verts shader for all levels: P = dir*radius, N = dir, spherical uv. COUNT bounds the level.
static std::string _icosphere_verts_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface sif_dir (descriptor_set 0) { buffer layout(std430) db { vec4 DIRd[]; }; }
storage_interface sif_P   (descriptor_set 0) { buffer layout(std430) pb  { vec4 Pd[];  }; }
storage_interface sif_N   (descriptor_set 0) { buffer layout(std430) nb  { vec4 Nd[];  }; }
storage_interface sif_B   (descriptor_set 0) { buffer layout(std430) bb2 { vec4 Bd[];  }; }
storage_interface sif_uv  (descriptor_set 0) { buffer layout(std430) ub  { vec4 UVd[]; }; }
storage_interface sif_clr (descriptor_set 0) { buffer layout(std430) cb  { vec4 Cd[];  }; }
storage_interface sif_par (descriptor_set 0) { buffer layout(std430) parb {
  float p_radius; float p_count; float p_pad0; float p_pad1; }; }
compute_interface iface { storage { sif_dir sif_P sif_N sif_B sif_uv sif_clr sif_par }
                          inputs { layout(local_size_x = 64); } }
compute_shader cs_verts : iface {
  uint v = gl_GlobalInvocationID.x;
  if (v >= uint(p_count + 0.5)) { return; }
  vec3 d  = DIRd[v].xyz;
  vec3 up = (abs(d.y) > 0.99) ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
  vec3 t  = normalize(cross(up, d));
  Pd[v]  = vec4(d * p_radius, 1.0);
  Nd[v]  = vec4(d, 0.0);
  Bd[v]  = vec4(t, 0.0);
  UVd[v] = vec4(atan(d.z, d.x) * 0.15915494 + 0.5, acos(clamp(d.y, -1.0, 1.0)) * 0.31830989, 0.0, 0.0);
  Cd[v]  = vec4(0.5 + 0.5 * d, 1.0);
}
)S";
}

static void _uploadF(Context* ctx, FxShaderStorageBuffer* b, const std::vector<float>& v) {
  if (v.empty() or not b) return;
  auto m = ctx->FXI()->mapStorageBuffer(b, 0, v.size() * 4, BufferMapAccess::WRITE_ONLY);
  std::memcpy(m->_mappedaddr, v.data(), v.size() * 4);
  ctx->FXI()->unmapStorageBuffer(m.get());
}
static void _uploadU(Context* ctx, FxShaderStorageBuffer* b, const std::vector<uint32_t>& v) {
  if (v.empty() or not b) return;
  auto m = ctx->FXI()->mapStorageBuffer(b, 0, v.size() * 4, BufferMapAccess::WRITE_ONLY);
  std::memcpy(m->_mappedaddr, v.data(), v.size() * 4);
  ctx->FXI()->unmapStorageBuffer(m.get());
}

struct IcoSphereInst : public MeshComputeInst {
  IcoSphereInst(const IcoSphereData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _radius = _floatPlug(this, _d, "radius");
    _subdiv = _intPlug(this, _d, "subdivisions");
  }
  static const MeshChannel kCh[5];

  void onActivate(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<MeshEnv>();
    auto fxi = env->_ctx->FXI();
    auto ctx = env->_ctx;
    // base icosahedron: 12 directions + 20 triangles.
    const double t = 1.6180339887498949;
    double bv[12][3] = {{-1,t,0},{1,t,0},{-1,-t,0},{1,-t,0},{0,-1,t},{0,1,t},
                        {0,-1,-t},{0,1,-t},{t,0,-1},{t,0,1},{-t,0,-1},{-t,0,1}};
    std::vector<fvec3> dir;
    for (int i = 0; i < 12; i++) dir.push_back(fvec3(bv[i][0], bv[i][1], bv[i][2]).normalized());
    std::vector<uint32_t> vidx = {
        0,11,5, 0,5,1, 0,1,7, 0,7,10, 0,10,11, 1,5,9, 5,11,4, 11,10,2, 10,7,6, 7,1,8,
        3,9,4, 3,4,2, 3,2,6, 3,6,8, 3,8,9, 4,9,5, 2,4,11, 6,2,10, 8,6,7, 9,8,1};

    for (int L = 0; L <= kIcoMaxLevel; L++) {
      auto& lv = _lv[L];
      lv.nv = int(dir.size());
      lv.nc = int(vidx.size());
      lv.nf = lv.nc / 3;
      std::vector<float> df(size_t(lv.nv) * 4);
      for (int i = 0; i < lv.nv; i++) { df[i*4+0]=dir[i].x; df[i*4+1]=dir[i].y; df[i*4+2]=dir[i].z; df[i*4+3]=1.0f; }
      std::vector<uint32_t> fo(lv.nf + 1);
      for (int f = 0; f <= lv.nf; f++) fo[f] = f * 3;
      lv.dir  = env->_pool->acquire(16, lv.nv);
      lv.vidx = env->_pool->acquireChannel(4, lv.nc);
      lv.fo   = env->_pool->acquireChannel(4, lv.nf + 1);
      _uploadF(ctx, lv.dir, df);
      _uploadU(ctx, lv.vidx->_ssbo, vidx);
      _uploadU(ctx, lv.fo->_ssbo, fo);
      if (L == kIcoMaxLevel) break;
      // subdivide -> level L+1: midpoint (shared, via edge map) + project to the sphere.
      std::unordered_map<uint64_t, uint32_t> emap;
      auto midId = [&](uint32_t a, uint32_t b) -> uint32_t {
        uint32_t lo = std::min(a, b), hi = std::max(a, b);
        uint64_t key = (uint64_t(lo) << 32) | hi;
        auto it = emap.find(key);
        if (it != emap.end()) return it->second;
        uint32_t id = uint32_t(dir.size());
        dir.push_back(((dir[a] + dir[b]) * 0.5f).normalized());  // project the midpoint onto the sphere
        emap[key] = id;
        return id;
      };
      std::vector<uint32_t> nvidx;
      for (size_t i = 0; i < vidx.size(); i += 3) {
        uint32_t c0 = vidx[i], c1 = vidx[i+1], c2 = vidx[i+2];
        uint32_t m01 = midId(c0, c1), m12 = midId(c1, c2), m20 = midId(c2, c0);
        uint32_t tris[12] = {c0,m01,m20, m01,c1,m12, m20,m12,c2, m01,m12,m20};
        for (int k = 0; k < 12; k++) nvidx.push_back(tris[k]);
      }
      vidx = nvidx;
    }
    _params   = fxi->createStorageBuffer(16);
    auto sh   = fxi->shaderFromShaderText("hypermesh_icosphere_verts", _icosphere_verts_text());
    _cs_verts = fxi->computeShader(sh, "cs_verts");
    _header   = env->_pool->acquire(kMeshHeaderBytes, 1);
  }
  // RUNTIME subdivisions: select the precomputed level, re-pool position channels on pow2 change,
  // alias the level's constant topology, host-write radius + count + the header.
  void writeParams(Context* ctx) final {
    int L = *(_d->typedInputNamed<dflow::IntPlugTraits>("subdivisions")->_value);
    L     = L < 0 ? 0 : (L > kIcoMaxLevel ? kIcoMaxLevel : L);
    _level = L;
    auto& lv  = _lv[L];
    auto out  = _output->_value;
    int vcap  = meshNextPow2(lv.nv);
    if (vcap != _vcap) {
      auto env = _graphinst->_impl.getShared<MeshEnv>();
      for (auto c : kCh) out->_channels[c] = env->_pool->acquireChannel(c, lv.nv);
      _vcap = vcap;
    }
    out->_vidx         = lv.vidx;     // precomputed constant topology (aliased)
    out->_face_offsets = lv.fo;
    out->_header       = _header;
    out->_capacity     = vcap;
    out->_num_verts    = lv.nv;
    out->_num_corners  = lv.nc;
    out->_num_faces    = lv.nf;
    float P4[4] = {*(_d->typedInputNamed<dflow::FloatPlugTraits>("radius")->_value), float(lv.nv), 0.0f, 0.0f};
    auto m = ctx->FXI()->mapStorageBuffer(_params, 0, sizeof(P4), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, P4, sizeof(P4));
    ctx->FXI()->unmapStorageBuffer(m.get());
    uint32_t hdr[4] = {uint32_t(lv.nv), uint32_t(lv.nc), uint32_t(lv.nf), 0u};
    auto mh = ctx->FXI()->mapStorageBuffer(_header, 0, sizeof(hdr), BufferMapAccess::WRITE_ONLY);
    std::memcpy(mh->_mappedaddr, hdr, sizeof(hdr));
    ctx->FXI()->unmapStorageBuffer(mh.get());
    tagFacesConst(ctx, out, _d->_mask);   // optional whole-mesh __tags (runtime; no-op if no mask)
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env  = inst->_impl.getShared<MeshEnv>();
    auto ci   = env->_ctx->CI();
    auto mesh = _output->_value;
    if (mesh->_channels.empty()) return;
    auto& lv  = _lv[_level];
    auto cs   = _cs_verts;
    ci->bindStorageBuffer(cs, 0, lv.dir);
    ci->bindStorageBuffer(cs, 1, mesh->channel(MeshChannel::POSITION)->_ssbo);
    ci->bindStorageBuffer(cs, 2, mesh->channel(MeshChannel::NORMAL)->_ssbo);
    ci->bindStorageBuffer(cs, 3, mesh->channel(MeshChannel::BINORMAL)->_ssbo);
    ci->bindStorageBuffer(cs, 4, mesh->channel(MeshChannel::UV0)->_ssbo);
    ci->bindStorageBuffer(cs, 5, mesh->channel(MeshChannel::COLOR)->_ssbo);
    ci->bindStorageBuffer(cs, 6, _params);
    ci->dispatchCompute(cs, (lv.nv + 63) / 64, 1, 1);
    ci->storageBarrier();
  }
  const IcoSphereData* _d;
  mesh_outpluginst_ptr_t _output;
  dflow::float_inp_pluginst_ptr_t _radius;
  dflow::int_inp_pluginst_ptr_t _subdiv;
  IcoLevel _lv[kIcoMaxLevel + 1];
  FxShaderStorageBuffer* _header   = nullptr;
  FxShaderStorageBuffer* _params   = nullptr;
  const FxComputeShader* _cs_verts = nullptr;
  int _level = 0, _vcap = -1;
};
const MeshChannel IcoSphereInst::kCh[5] = {
    MeshChannel::POSITION, MeshChannel::NORMAL, MeshChannel::BINORMAL, MeshChannel::UV0, MeshChannel::COLOR};

static void _reshapeIcoSphereIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "radius")->setValue(1.5f);
  dflow::ModuleData::createInputPlug<dflow::IntPlugTraits>(data, dflow::EPR_UNIFORM, "subdivisions")->setValue(2);
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
IcoSphereData::IcoSphereData() {
}
std::shared_ptr<IcoSphereData> IcoSphereData::createShared() {
  auto d = std::make_shared<IcoSphereData>();
  _reshapeIcoSphereIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t IcoSphereData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<IcoSphereInst>(this, g);
}
void IcoSphereData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return IcoSphereData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeIcoSphereIOs(m); });
  clazz->directVectorProperty("mask", &IcoSphereData::_mask);
}

} // namespace ork::lev2::hypermesh
