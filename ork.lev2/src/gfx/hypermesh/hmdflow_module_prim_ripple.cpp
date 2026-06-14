////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"

ImplementReflectionX(ork::lev2::hypermesh::RipplePrimitiveData, "hypermesh::RipplePrimitiveData");

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////
// RipplePrimitive (v2, INDEXED, DYNAMIC grid) — a sine-displaced shared-vertex grid: (grid+1)^2 verts
// + grid^2*2 tris. `grid` is a RUNTIME int plug (read each frame; the shader uses runtime arrays + a
// runtime `grid` param, buffers RE-POOL on pow2 change) so it's live-tweakable. amp/freq/extent are
// runtime floats. The TOPOLOGY (vidx + face_offsets) only depends on `grid`, so it re-runs only when
// `grid` changes; the vertex attrs re-run every frame (amp/freq live).
///////////////////////////////////////////////////////////////////////////////

static std::string _ripple_text() {
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
  float p_amp; float p_freq; float p_extent; float p_grid; }; }
compute_interface iface { storage { sif_hdr sif_P sif_N sif_B sif_uv sif_clr sif_vi sif_fo sif_par }
                          inputs { layout(local_size_x = 64); } }
////////////////////////////////////////
compute_shader cs_setup : iface {
  if (gl_GlobalInvocationID.x != 0u) { return; }
  uint G = uint(p_grid + 0.5);
  num_verts = (G + 1u) * (G + 1u); num_corners = G * G * 6u; num_faces = G * G * 2u; flags = 0u;
  float hx = 0.5 * p_extent;
  bbmin = vec4(-hx, -abs(p_amp) - 0.01, -hx, 1.0);
  bbmax = vec4( hx,  abs(p_amp) + 0.01,  hx, 1.0);
}
////////////////////////////////////////
// TOPOLOGY (one thread / corner): face_offsets[f]=f*3 (all-tri CSR) + vidx[corner] (the winding).
compute_shader cs_topo : iface {
  uint G = uint(p_grid + 0.5);
  uint ncorners = G * G * 6u; uint nfaces = G * G * 2u;
  uint corner = gl_GlobalInvocationID.x;
  if (corner >= ncorners) { return; }
  uint f = corner / 3u; uint k = corner % 3u;
  if (k == 0u) { FOd[f] = f * 3u; if (f == 0u) { FOd[nfaces] = ncorners; } }
  uint cell = f / 2u; uint tri = f % 2u;
  uint cx = cell % G; uint cz = cell / G;
  uint cc = tri * 3u + k;
  uint dx; uint dz;
  if      (cc == 0u) { dx = 0u; dz = 0u; }
  else if (cc == 1u) { dx = 1u; dz = 1u; }
  else if (cc == 2u) { dx = 1u; dz = 0u; }
  else if (cc == 3u) { dx = 0u; dz = 0u; }
  else if (cc == 4u) { dx = 0u; dz = 1u; }
  else               { dx = 1u; dz = 1u; }
  VId[corner] = (cz + dz) * (G + 1u) + (cx + dx);
}
////////////////////////////////////////
// VERTEX attrs (one thread / unique grid point) — sine ripple + analytic N/B.
compute_shader cs_verts : iface {
  uint G = uint(p_grid + 0.5);
  uint nverts = (G + 1u) * (G + 1u);
  uint v = gl_GlobalInvocationID.x;
  if (v >= nverts) { return; }
  uint gx = v % (G + 1u); uint gz = v / (G + 1u);
  float tx = float(gx) / float(G);
  float tz = float(gz) / float(G);
  float wx = (tx - 0.5) * p_extent;
  float wz = (tz - 0.5) * p_extent;
  float A = p_amp; float F = p_freq;
  float h   = A * sin(wx * F) * cos(wz * F);
  float dhx = A * F * cos(wx * F) * cos(wz * F);
  float dhz = -A * F * sin(wx * F) * sin(wz * F);
  Pd[v]  = vec4(wx, h, wz, 1.0);
  Nd[v]  = vec4(normalize(vec3(-dhx, 1.0, -dhz)), 0.0);
  Bd[v]  = vec4(normalize(vec3(1.0, dhx, 0.0)), 0.0);
  UVd[v] = vec4(tx, tz, 0.0, 0.0);
  Cd[v]  = vec4(0.55 + 0.35 * sin(wx), 0.6, 0.7 - 0.3 * cos(wz), 1.0);
}
)S";
}

struct RipplePrimitiveInst : public MeshComputeInst {
  RipplePrimitiveInst(const RipplePrimitiveData* d, dflow::GraphInst* g)
      : MeshComputeInst(d, g)
      , _d(d) {
  }
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _grid   = _intPlug(this, _d, "grid");
    _amp    = _floatPlug(this, _d, "amp");
    _freq   = _floatPlug(this, _d, "freq");
    _extent = _floatPlug(this, _d, "extent");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto env  = inst->_impl.getShared<MeshEnv>();
    auto fxi  = env->_ctx->FXI();
    _params   = fxi->createStorageBuffer(16);
    auto sh   = fxi->shaderFromShaderText("hypermesh_ripple", _ripple_text());
    _cs_setup = fxi->computeShader(sh, "cs_setup");
    _cs_topo  = fxi->computeShader(sh, "cs_topo");
    _cs_verts = fxi->computeShader(sh, "cs_verts");
  }
  // RUNTIME grid: read each frame; re-pool on pow2 change; set counts; write amp/freq/extent/grid.
  void writeParams(Context* ctx) final {
    int grid  = *(_d->typedInputNamed<dflow::IntPlugTraits>("grid")->_value);
    grid      = grid < 1 ? 1 : (grid > 512 ? 512 : grid);
    _grid_n   = grid;
    _nverts   = (grid + 1) * (grid + 1);
    _nfaces   = grid * grid * 2;
    _ncorners = _nfaces * 3;
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
    auto df = [&](const char* nm) { return *(_d->typedInputNamed<dflow::FloatPlugTraits>(nm)->_value); };
    float P4[4] = {df("amp"), df("freq"), df("extent"), float(grid)};
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
    if (_grid_n != _topo_grid) {                 // topology depends only on `grid` -> re-run on change
      bind(_cs_topo);
      ci->dispatchCompute(_cs_topo, (_ncorners + 63) / 64, 1, 1);
      ci->storageBarrier();
      _topo_grid = _grid_n;
    }
    bind(_cs_verts);
    ci->dispatchCompute(_cs_verts, (_nverts + 63) / 64, 1, 1);
    ci->storageBarrier();
  }

  const RipplePrimitiveData* _d;
  mesh_outpluginst_ptr_t _output;
  dflow::int_inp_pluginst_ptr_t _grid;
  dflow::float_inp_pluginst_ptr_t _amp, _freq, _extent;
  int _grid_n = 0, _nverts = 0, _nfaces = 0, _ncorners = 0;
  int _vcap = -1, _ccap = -1, _fcap = -1, _topo_grid = -1;
  FxShaderStorageBuffer* _params   = nullptr;
  const FxComputeShader* _cs_setup = nullptr;
  const FxComputeShader* _cs_topo  = nullptr;
  const FxComputeShader* _cs_verts = nullptr;
};

static void _reshapeRippleIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<dflow::IntPlugTraits>(data, dflow::EPR_UNIFORM, "grid")->setValue(96);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "amp")->setValue(1.2f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "freq")->setValue(2.2f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "extent")->setValue(8.0f);
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
RipplePrimitiveData::RipplePrimitiveData() {
}
std::shared_ptr<RipplePrimitiveData> RipplePrimitiveData::createShared() {
  auto d = std::make_shared<RipplePrimitiveData>();
  _reshapeRippleIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t RipplePrimitiveData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<RipplePrimitiveInst>(this, g);
}
void RipplePrimitiveData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return RipplePrimitiveData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeRippleIOs(m); });
  clazz->directProperty("kind", &RipplePrimitiveData::_kind);
  clazz->directVectorProperty("mask", &RipplePrimitiveData::_mask);
}

} // namespace ork::lev2::hypermesh
