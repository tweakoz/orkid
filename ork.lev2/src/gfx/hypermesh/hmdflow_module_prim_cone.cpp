////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"

ImplementReflectionX(ork::lev2::hypermesh::ConeData, "hypermesh::ConeData");

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////
// Cone (v2, INDEXED, MIXED tri + ngon, DYNAMIC sides) — `sides` triangle side faces (apex fan) + ONE
// n-gon base cap. The base RIM is split (side ring w/ slope normals + base ring w/ down normal) for a
// sharp rim. `sides` is a RUNTIME int plug (>=3) read each frame: the shader uses runtime-sized arrays
// + a runtime `sides` param, and the buffers RE-POOL when the pow2 size-class changes — so `sides` can
// be tweaked/animated live (topology regenerates each frame). radius/height runtime too.
//   verts = 1 + 2*sides, faces = sides tris + 1 n-gon, corners = sides*4.
///////////////////////////////////////////////////////////////////////////////

static std::string _cone_text() {
  // runtime arrays ([]) + runtime `sides` (p_sides) -> one compiled shader drives any side count.
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
  float p_radius; float p_height; float p_sides; float p_pad; }; }
compute_interface iface { storage { sif_hdr sif_P sif_N sif_B sif_uv sif_clr sif_vi sif_fo sif_par }
                          inputs { layout(local_size_x = 64); } }
////////////////////////////////////////
compute_shader cs_setup : iface {
  if (gl_GlobalInvocationID.x != 0u) { return; }
  uint S = uint(p_sides + 0.5);
  num_verts = 1u + 2u * S; num_corners = 4u * S; num_faces = S + 1u; flags = 0u;
  float R = p_radius; float H = p_height;
  bbmin = vec4(-R, 0.0, -R, 1.0); bbmax = vec4(R, H, R, 1.0);
}
////////////////////////////////////////
// vertex attrs: apex (0), side ring (1..S, slope normals), base ring (S+1..2S, down normal).
compute_shader cs_verts : iface {
  uint v = gl_GlobalInvocationID.x;
  uint S = uint(p_sides + 0.5); uint nv = 1u + 2u * S;
  if (v >= nv) { return; }
  float R = p_radius; float H = p_height;
  if (v == 0u) {                                  // apex
    Pd[0]  = vec4(0.0, H, 0.0, 1.0); Nd[0] = vec4(0.0, 1.0, 0.0, 0.0);
    Bd[0]  = vec4(1.0, 0.0, 0.0, 0.0); UVd[0] = vec4(0.5, 1.0, 0.0, 0.0); Cd[0] = vec4(0.9, 0.9, 0.9, 1.0);
  } else if (v <= S) {                            // side ring
    uint i  = v - 1u;
    float th = 6.28318531 * float(i) / float(S);
    float c = cos(th); float s = sin(th);
    Pd[v]  = vec4(R * c, 0.0, R * s, 1.0);
    Nd[v]  = vec4(normalize(vec3(H * c, R, H * s)), 0.0);     // cone slope normal (radial + up)
    Bd[v]  = vec4(normalize(vec3(-s, 0.0, c)), 0.0);
    UVd[v] = vec4(float(i) / float(S), 0.0, 0.0, 0.0);
    Cd[v]  = vec4(0.5 + 0.5 * c, 0.6, 0.5 + 0.5 * s, 1.0);
  } else {                                        // base ring (same positions, down normal)
    uint i  = v - 1u - S;
    float th = 6.28318531 * float(i) / float(S);
    float c = cos(th); float s = sin(th);
    Pd[v]  = vec4(R * c, 0.0, R * s, 1.0);
    Nd[v]  = vec4(0.0, -1.0, 0.0, 0.0);
    Bd[v]  = vec4(1.0, 0.0, 0.0, 0.0);
    UVd[v] = vec4(0.5 + 0.5 * c, 0.5 + 0.5 * s, 0.0, 0.0);
    Cd[v]  = vec4(0.4, 0.45, 0.5, 1.0);
  }
}
////////////////////////////////////////
// topology: faces 0..S-1 = side tris, face S = base n-gon. Winding outward; base n-gon faces -y.
compute_shader cs_topo : iface {
  uint f = gl_GlobalInvocationID.x;
  uint S = uint(p_sides + 0.5);
  if (f >= S + 1u) { return; }
  if (f < S) {                                    // side triangle
    uint o = f * 3u;
    VId[o + 0u] = 0u;
    VId[o + 1u] = 1u + ((f + 1u) % S);
    VId[o + 2u] = 1u + f;
    FOd[f] = o;
  } else {                                        // base n-gon (thread f == S)
    FOd[S] = S * 3u;
    for (uint i = 0u; i < S; i++) VId[S * 3u + i] = 1u + S + i;
    FOd[S + 1u] = S * 4u;                          // terminal CSR offset
  }
}
)S";
}

struct ConeInst : public MeshComputeInst {
  ConeInst(const ConeData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _radius = _floatPlug(this, _d, "radius");
    _height = _floatPlug(this, _d, "height");
    _sides  = _intPlug(this, _d, "sides");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto env  = inst->_impl.getShared<MeshEnv>();
    auto fxi  = env->_ctx->FXI();
    _params   = fxi->createStorageBuffer(16);
    auto sh   = fxi->shaderFromShaderText("hypermesh_cone", _cone_text()); // runtime arrays -> compiled once
    _cs_setup = fxi->computeShader(sh, "cs_setup");
    _cs_verts = fxi->computeShader(sh, "cs_verts");
    _cs_topo  = fxi->computeShader(sh, "cs_topo");
  }
  // RUNTIME sides: read the plug each frame; re-pool the mesh buffers when the size-class changes
  // (so `sides` is live-tweakable), set counts, host-write radius/height/sides params.
  void writeParams(Context* ctx) final {
    int sides = *(_d->typedInputNamed<dflow::IntPlugTraits>("sides")->_value);
    sides     = sides < 3 ? 3 : (sides > 1024 ? 1024 : sides);
    _nverts   = 1 + 2 * sides;
    _nfaces   = sides + 1;
    _ncorners = sides * 4;
    auto out  = _output->_value;
    int vcap = meshNextPow2(_nverts), ccap = meshNextPow2(_ncorners), fcap = meshNextPow2(_nfaces + 1);
    if (vcap != _vcap or ccap != _ccap or fcap != _fcap) {       // size-class changed -> re-pool
      auto env = _graphinst->_impl.getShared<MeshEnv>();
      allocMesh(env, out, _nverts, _ncorners, _nfaces,
                {MeshChannel::POSITION, MeshChannel::NORMAL, MeshChannel::BINORMAL,
                 MeshChannel::UV0, MeshChannel::COLOR});
      _vcap = vcap; _ccap = ccap; _fcap = fcap;
    } else {                                                      // same buffers; just update counts
      out->_num_verts = _nverts; out->_num_corners = _ncorners; out->_num_faces = _nfaces;
    }
    if (_params) {
      float P4[4] = {*(_d->typedInputNamed<dflow::FloatPlugTraits>("radius")->_value),
                     *(_d->typedInputNamed<dflow::FloatPlugTraits>("height")->_value), float(sides), 0.0f};
      auto m = ctx->FXI()->mapStorageBuffer(_params, 0, sizeof(P4), BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, P4, sizeof(P4));
      ctx->FXI()->unmapStorageBuffer(m.get());
    }
    tagFacesConst(ctx, out, _d->_mask);   // optional whole-mesh __tags (runtime; no-op if no mask)
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env  = inst->_impl.getShared<MeshEnv>();
    auto ci   = env->_ctx->CI();
    auto mesh = _output->_value;
    if (not mesh->_vidx) return; // writeParams hasn't allocated yet
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
    bind(_cs_topo);                                              // topology regenerates each frame (sides runtime)
    ci->dispatchCompute(_cs_topo, (_nfaces + 63) / 64, 1, 1);
    ci->storageBarrier();
    bind(_cs_verts);
    ci->dispatchCompute(_cs_verts, (_nverts + 63) / 64, 1, 1);
    ci->storageBarrier();
  }
  const ConeData* _d;
  mesh_outpluginst_ptr_t _output;
  dflow::float_inp_pluginst_ptr_t _radius, _height;
  dflow::int_inp_pluginst_ptr_t _sides;
  int _nverts = 0, _nfaces = 0, _ncorners = 0, _vcap = -1, _ccap = -1, _fcap = -1;
  FxShaderStorageBuffer* _params   = nullptr;
  const FxComputeShader* _cs_setup = nullptr;
  const FxComputeShader* _cs_verts = nullptr;
  const FxComputeShader* _cs_topo  = nullptr;
};

static void _reshapeConeIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "radius")->setValue(1.5f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "height")->setValue(2.5f);
  dflow::ModuleData::createInputPlug<dflow::IntPlugTraits>(data, dflow::EPR_UNIFORM, "sides")->setValue(24);
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
ConeData::ConeData() {
}
std::shared_ptr<ConeData> ConeData::createShared() {
  auto d = std::make_shared<ConeData>();
  _reshapeConeIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t ConeData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<ConeInst>(this, g);
}
void ConeData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return ConeData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeConeIOs(m); });
  clazz->directVectorProperty("mask", &ConeData::_mask);
}

} // namespace ork::lev2::hypermesh
