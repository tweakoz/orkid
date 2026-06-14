////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"

ImplementReflectionX(ork::lev2::hypermesh::UvSphereData, "hypermesh::UvSphereData");

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////
// UvSphere (v2, INDEXED, MIXED quad/tri, DYNAMIC segments/rings) — lat/long sphere: QUAD body faces +
// TRIANGLE pole fans in one indexed mesh. `segments`/`rings` are RUNTIME int plugs (read each frame;
// runtime arrays + runtime params; buffers re-pool on pow2 change) so they're live-tweakable. radius
// runtime too. Counts: verts=(rings-1)*(segments+1)+2, faces=segments*rings, corners=segments*(4*rings-2).
///////////////////////////////////////////////////////////////////////////////

static std::string _uvsphere_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface sif_hdr (descriptor_set 0) { buffer layout(std430) hb {
  uint num_verts; uint num_corners; uint num_faces; uint flags; vec4 bbmin; vec4 bbmax; }; }
storage_interface sif_P   (descriptor_set 0) { buffer layout(std430) pb  { vec4 Pd[];  }; }
storage_interface sif_N   (descriptor_set 0) { buffer layout(std430) nb  { vec4 Nd[];  }; }
storage_interface sif_B   (descriptor_set 0) { buffer layout(std430) bb2 { vec4 Bd[];  }; }
storage_interface sif_uv  (descriptor_set 0) { buffer layout(std430) ub  { vec4 UVd[]; }; }
storage_interface sif_clr (descriptor_set 0) { buffer layout(std430) cb  { vec4 Cd[];  }; }
storage_interface sif_vi  (descriptor_set 0) { buffer layout(std430) vib { uint VId[]; }; }
storage_interface sif_fo  (descriptor_set 0) { buffer layout(std430) fob { uint FOd[]; }; }
storage_interface sif_par (descriptor_set 0) { buffer layout(std430) parb {
  float p_radius; float p_seg; float p_rings; float p_pad; }; }
compute_interface iface { storage { sif_hdr sif_P sif_N sif_B sif_uv sif_clr sif_vi sif_fo sif_par }
                          inputs { layout(local_size_x = 64); } }
////////////////////////////////////////
compute_shader cs_setup : iface {
  if (gl_GlobalInvocationID.x != 0u) { return; }
  uint S = uint(p_seg + 0.5); uint R = uint(p_rings + 0.5);
  uint nbv = (R - 1u) * (S + 1u);
  num_verts = nbv + 2u; num_corners = S * (4u * R - 2u); num_faces = S * R; flags = 0u;
  float rad = p_radius;
  bbmin = vec4(-rad, -rad, -rad, 1.0); bbmax = vec4(rad, rad, rad, 1.0);
}
////////////////////////////////////////
// vertex attrs (one thread / vertex): interior-ring grid verts + the 2 poles. normal = position dir.
compute_shader cs_verts : iface {
  uint S = uint(p_seg + 0.5); uint R = uint(p_rings + 0.5);
  uint nbv = (R - 1u) * (S + 1u);
  uint v = gl_GlobalInvocationID.x;
  if (v >= nbv + 2u) { return; }
  float rad = p_radius;
  if (v < nbv) {
    uint i = v / (S + 1u) + 1u;        // ring 1..rings-1
    uint j = v % (S + 1u);             // 0..segments
    float fu = float(j) / float(S);
    float fv = float(i) / float(R);
    float theta = fv * 3.14159265;
    float phi   = fu * 6.28318531;
    float st = sin(theta); float ct = cos(theta); float sp = sin(phi); float cp = cos(phi);
    vec3 nrm = vec3(st * cp, ct, st * sp);
    Pd[v]  = vec4(nrm * rad, 1.0);
    Nd[v]  = vec4(nrm, 0.0);
    Bd[v]  = vec4(normalize(vec3(-sp, 0.0, cp)), 0.0);
    UVd[v] = vec4(fu, fv, 0.0, 0.0);
    Cd[v]  = vec4(0.5 + 0.5 * nrm, 1.0);
  } else if (v == nbv) {               // north pole
    Pd[v] = vec4(0.0, rad, 0.0, 1.0); Nd[v] = vec4(0.0, 1.0, 0.0, 0.0);
    Bd[v] = vec4(1.0, 0.0, 0.0, 0.0); UVd[v] = vec4(0.5, 0.0, 0.0, 0.0); Cd[v] = vec4(0.5,1.0,0.5,1.0);
  } else {                             // south pole
    Pd[v] = vec4(0.0, -rad, 0.0, 1.0); Nd[v] = vec4(0.0, -1.0, 0.0, 0.0);
    Bd[v] = vec4(1.0, 0.0, 0.0, 0.0); UVd[v] = vec4(0.5, 1.0, 0.0, 0.0); Cd[v] = vec4(0.5,0.5,1.0,1.0);
  }
}
////////////////////////////////////////
// topology (one thread / face): north tri-fan, body quads, south tri-fan -> MIXED CSR.
compute_shader cs_topo : iface {
  uint S = uint(p_seg + 0.5); uint R = uint(p_rings + 0.5);
  uint nbv = (R - 1u) * (S + 1u);
  uint nfaces = S * R; uint ncorners = S * (4u * R - 2u);
  uint f = gl_GlobalInvocationID.x;
  if (f >= nfaces) { return; }
  uint NP = nbv; uint SP = nbv + 1u;
  uint body = (R - 2u) * S;
  if (f < S) {                                  // north triangle
    uint j = f; uint o = f * 3u;
    VId[o + 0u] = NP; VId[o + 1u] = (j + 1u); VId[o + 2u] = j;
    FOd[f] = o;
  } else if (f < S + body) {                    // body quad
    uint bf = f - S; uint i = bf / S + 1u; uint j = bf % S;
    uint o = S * 3u + bf * 4u;
    uint i0 = (i - 1u) * (S + 1u); uint i1 = i * (S + 1u);
    VId[o + 0u] = i0 + j; VId[o + 1u] = i0 + j + 1u; VId[o + 2u] = i1 + j + 1u; VId[o + 3u] = i1 + j;
    FOd[f] = o;
  } else {                                      // south triangle
    uint sf = f - S - body; uint j = sf;
    uint o = S * 3u + body * 4u + sf * 3u;
    uint r = (R - 2u) * (S + 1u);
    VId[o + 0u] = SP; VId[o + 1u] = r + j; VId[o + 2u] = r + j + 1u;
    FOd[f] = o;
  }
  if (f == 0u) { FOd[nfaces] = ncorners; }      // terminal CSR offset
}
)S";
}

struct UvSphereInst : public MeshComputeInst {
  UvSphereInst(const UvSphereData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _radius = _floatPlug(this, _d, "radius");
    _segs   = _intPlug(this, _d, "segments");
    _rings  = _intPlug(this, _d, "rings");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto env  = inst->_impl.getShared<MeshEnv>();
    auto fxi  = env->_ctx->FXI();
    _params   = fxi->createStorageBuffer(16);
    auto sh   = fxi->shaderFromShaderText("hypermesh_uvsphere", _uvsphere_text());
    _cs_setup = fxi->computeShader(sh, "cs_setup");
    _cs_verts = fxi->computeShader(sh, "cs_verts");
    _cs_topo  = fxi->computeShader(sh, "cs_topo");
  }
  // RUNTIME segments/rings: read each frame; re-pool on pow2 change; set counts; write params.
  void writeParams(Context* ctx) final {
    int seg   = *(_d->typedInputNamed<dflow::IntPlugTraits>("segments")->_value);
    int rings = *(_d->typedInputNamed<dflow::IntPlugTraits>("rings")->_value);
    seg   = seg < 3 ? 3 : (seg > 512 ? 512 : seg);
    rings = rings < 2 ? 2 : (rings > 512 ? 512 : rings);
    _seg = seg; _rings_n = rings;
    int nbv   = (rings - 1) * (seg + 1);
    _nverts   = nbv + 2;
    _nfaces   = seg * rings;
    _ncorners = seg * (4 * rings - 2);
    auto out  = _output->_value;
    int vcap = meshNextPow2(_nverts), ccap = meshNextPow2(_ncorners), fcap = meshNextPow2(_nfaces + 1);
    if (vcap != _vcap or ccap != _ccap or fcap != _fcap) {
      auto env = _graphinst->_impl.getShared<MeshEnv>();
      allocMesh(env, out, _nverts, _ncorners, _nfaces,
                {MeshChannel::POSITION, MeshChannel::NORMAL, MeshChannel::BINORMAL,
                 MeshChannel::UV0, MeshChannel::COLOR});
      _vcap = vcap; _ccap = ccap; _fcap = fcap;
    } else {
      out->_num_verts = _nverts; out->_num_corners = _ncorners; out->_num_faces = _nfaces;
    }
    float P4[4] = {*(_d->typedInputNamed<dflow::FloatPlugTraits>("radius")->_value),
                   float(seg), float(rings), 0.0f};
    auto m = ctx->FXI()->mapStorageBuffer(_params, 0, sizeof(P4), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, P4, sizeof(P4));
    ctx->FXI()->unmapStorageBuffer(m.get());
    tagFacesConst(ctx, out, _d->_mask);   // optional whole-mesh __tags (runtime; no-op if no mask)
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env  = inst->_impl.getShared<MeshEnv>();
    auto ci   = env->_ctx->CI();
    auto mesh = _output->_value;
    if (not mesh->_vidx) return;
    auto bind = [&](const FxComputeShader* cs) {
      ci->bindStorageBuffer(cs, 0, mesh->_header);
      ci->bindStorageBuffer(cs, 1, mesh->channel(MeshChannel::POSITION)->_ssbo);
      ci->bindStorageBuffer(cs, 2, mesh->channel(MeshChannel::NORMAL)->_ssbo);
      ci->bindStorageBuffer(cs, 3, mesh->channel(MeshChannel::BINORMAL)->_ssbo);
      ci->bindStorageBuffer(cs, 4, mesh->channel(MeshChannel::UV0)->_ssbo);
      ci->bindStorageBuffer(cs, 5, mesh->channel(MeshChannel::COLOR)->_ssbo);
      ci->bindStorageBuffer(cs, 6, mesh->_vidx->_ssbo);
      ci->bindStorageBuffer(cs, 7, mesh->_face_offsets->_ssbo);
      ci->bindStorageBuffer(cs, 8, _params);
    };
    bind(_cs_setup);
    ci->dispatchCompute(_cs_setup, 1, 1, 1);
    ci->storageBarrier();
    if (_seg != _topo_seg or _rings_n != _topo_rings) {  // topology depends only on seg/rings -> on change
      bind(_cs_topo);
      ci->dispatchCompute(_cs_topo, (_nfaces + 63) / 64, 1, 1);
      ci->storageBarrier();
      _topo_seg = _seg; _topo_rings = _rings_n;
    }
    bind(_cs_verts);
    ci->dispatchCompute(_cs_verts, (_nverts + 63) / 64, 1, 1);
    ci->storageBarrier();
  }
  const UvSphereData* _d;
  mesh_outpluginst_ptr_t _output;
  dflow::float_inp_pluginst_ptr_t _radius;
  dflow::int_inp_pluginst_ptr_t _segs, _rings;
  int _seg = 0, _rings_n = 0, _nverts = 0, _nfaces = 0, _ncorners = 0;
  int _vcap = -1, _ccap = -1, _fcap = -1, _topo_seg = -1, _topo_rings = -1;
  FxShaderStorageBuffer* _params   = nullptr;
  const FxComputeShader* _cs_setup = nullptr;
  const FxComputeShader* _cs_verts = nullptr;
  const FxComputeShader* _cs_topo  = nullptr;
};

static void _reshapeUvSphereIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "radius")->setValue(1.5f);
  dflow::ModuleData::createInputPlug<dflow::IntPlugTraits>(data, dflow::EPR_UNIFORM, "segments")->setValue(32);
  dflow::ModuleData::createInputPlug<dflow::IntPlugTraits>(data, dflow::EPR_UNIFORM, "rings")->setValue(16);
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
UvSphereData::UvSphereData() {
}
std::shared_ptr<UvSphereData> UvSphereData::createShared() {
  auto d = std::make_shared<UvSphereData>();
  _reshapeUvSphereIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t UvSphereData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<UvSphereInst>(this, g);
}
void UvSphereData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return UvSphereData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeUvSphereIOs(m); });
  clazz->directVectorProperty("mask", &UvSphereData::_mask);
}

} // namespace ork::lev2::hypermesh
