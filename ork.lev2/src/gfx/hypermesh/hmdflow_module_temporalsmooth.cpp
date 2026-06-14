////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include <cmath>

ImplementReflectionX(ork::lev2::hypermesh::TemporalSmoothData, "hypermesh::TemporalSmoothData");

namespace ork::lev2::hypermesh {

// TemporalSmooth (see hmdflow.h) — inter-frame EMA on POSITION + NORMAL. One per-vertex pass; the
// previous-frame buffers are Inst-owned (persist across recompute()). RE-PRIMES on nv / topology
// change so it degrades to a safe pass-through when vertex correspondence isn't stable.

static std::string _temporal_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface tsf_iP (descriptor_set 0) { buffer layout(std430) tipb { vec4 iP[]; }; }
storage_interface tsf_iN (descriptor_set 0) { buffer layout(std430) tinb { vec4 iN[]; }; }
storage_interface tsf_oP (descriptor_set 0) { buffer layout(std430) topb { vec4 oP[]; }; }
storage_interface tsf_oN (descriptor_set 0) { buffer layout(std430) tonb { vec4 oN[]; }; }
storage_interface tsf_pP (descriptor_set 0) { buffer layout(std430) tppb { vec4 pP[]; }; }
storage_interface tsf_pN (descriptor_set 0) { buffer layout(std430) tpnb { vec4 pN[]; }; }
storage_interface tsf_ct (descriptor_set 0) { buffer layout(std430) tctb {
  uint p_nv; uint p_primed; float p_alpha; float p_pad; }; }
compute_interface iface { storage { tsf_iP tsf_iN tsf_oP tsf_oN tsf_pP tsf_pN tsf_ct }
                          inputs { layout(local_size_x = 64); } }
compute_shader cs_temporal : iface {
  uint v = gl_GlobalInvocationID.x;
  if (v >= p_nv) { return; }
  vec3 pin = iP[v].xyz;
  vec3 nin = iN[v].xyz;
  float nl = length(nin);
  vec3  nn = (nl > 1.0e-6) ? (nin / nl) : vec3(0.0, 1.0, 0.0);
  if (p_primed == 0u) {              // first frame / re-prime: copy through, seed the prev buffers
    oP[v] = vec4(pin, 1.0); oN[v] = vec4(nn, 0.0);
    pP[v] = vec4(pin, 1.0); pN[v] = vec4(nn, 0.0);
    return;
  }
  vec3 po = mix(pP[v].xyz, pin, p_alpha);   // EMA: p_alpha = weight of the NEW frame (small = heavy smoothing)
  vec3 no = mix(pN[v].xyz, nn,  p_alpha);
  float ml = length(no); no = (ml > 1.0e-6) ? (no / ml) : nn;
  oP[v] = vec4(po, 1.0); oN[v] = vec4(no, 0.0);
  pP[v] = vec4(po, 1.0); pN[v] = vec4(no, 0.0);   // carry the smoothed result into next frame's prev
}
)S";
}

struct TemporalSmoothInst : public MeshComputeInst {
  TemporalSmoothInst(const TemporalSmoothData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}

  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _input  = typedInputNamed<MeshPlugTraits>("In");
  }

  bool onTopologyReady(Context* ctx) final {
    if (not _srcMesh(_input) or _cs)
      return false;
    auto fxi = ctx->FXI();
    _cs = fxi->computeShader(fxi->shaderFromShaderText("hm_temporal_smooth", _temporal_text()), "cs_temporal");
    return true;
  }

  void writeParams(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in or not _cs)
      return;
    auto out = _output->_value;
    auto env = _graphinst->_impl.getShared<MeshEnv>();
    auto fxi = ctx->FXI();
    int nv   = in->_num_verts;

    // (re)allocate output channels (pool) + persistent prev buffers (Inst-owned) on a size-class change
    bool capChanged = (meshNextPow2(nv) != _cap);
    if (capChanged) {
      _outP = env->_pool->acquireChannel(MeshChannel::POSITION, nv);
      _outN = env->_pool->acquireChannel(MeshChannel::NORMAL, nv);
      size_t bytes = size_t(meshNextPow2(std::max(1, nv))) * 16;
      _prevP = fxi->createStorageBuffer(bytes);   // persistent across frames (NOT a recycled pool channel)
      _prevN = fxi->createStorageBuffer(bytes);
      _cap   = meshNextPow2(nv);
    }
    if (not _ct)
      _ct = fxi->createStorageBuffer(16);

    // RE-PRIME (copy through, no blend) when vertex correspondence can't be trusted: first frame, an
    // nv change, OR an input topology re-emit (e.g. sdf_to_mesh re-meshes every frame -> verts are
    // different logical points). Otherwise blend with last frame's smoothed result.
    bool topoChanged = (in->_topoVersion != _prevTopoVer);
    bool prime       = (not _primed) or capChanged or (nv != _prevNV) or topoChanged;

    // alpha: mode 0 uses the alpha plug directly (per-frame); mode 1 derives it from dt + tau
    // (a = 1-exp(-dt/tau)) for a framerate-independent time constant. dt==0 (paused) -> a=0 = hold.
    float alpha;
    if (_d->_mode == 1) {
      float tau = *(_d->typedInputNamed<dflow::FloatPlugTraits>("tau")->_value);
      float dt  = float(env ? env->_dt : 0.0);
      alpha     = (tau > 1.0e-6f) ? (1.0f - std::exp(-dt / tau)) : 1.0f;
    } else {
      alpha = *(_d->typedInputNamed<dflow::FloatPlugTraits>("alpha")->_value);
    }
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    uint32_t P[4];
    P[0] = uint32_t(nv);
    P[1] = prime ? 0u : 1u;
    std::memcpy(&P[2], &alpha, 4);
    P[3] = 0u;
    auto cp = fxi->mapStorageBuffer(_ct, 0, sizeof(P), BufferMapAccess::WRITE_ONLY);
    std::memcpy(cp->_mappedaddr, P, sizeof(P));
    fxi->unmapStorageBuffer(cp.get());

    // POSITION + NORMAL produced; everything else (topology, uv/color, header) passes through
    _outch                        = in->_channels;
    _outch[MeshChannel::POSITION] = _outP;
    _outch[MeshChannel::NORMAL]   = _outN;
    out->_channels     = _outch;
    out->_faces        = in->_faces;
    out->_vattrs       = in->_vattrs;
    out->_vidx         = in->_vidx;
    out->_face_offsets = in->_face_offsets;
    out->_header       = in->_header;
    out->_capacity     = in->_capacity;
    out->_num_verts    = in->_num_verts;
    out->_num_corners  = in->_num_corners;
    out->_num_faces    = in->_num_faces;

    _primed     = true;
    _prevNV     = nv;
    _prevTopoVer = in->_topoVersion;
    if (topoChanged) out->markTopoChanged(); else out->markChanged();
  }

  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto in = _srcMesh(_input);
    if (not in or not _cs)
      return;
    auto env = inst->_impl.getShared<MeshEnv>();
    auto ci  = env->_ctx->CI();
    int nv   = in->_num_verts;
    int vg   = (nv + 63) / 64;
    ci->bindStorageBuffer(_cs, 0, in->channel(MeshChannel::POSITION)->_ssbo);
    ci->bindStorageBuffer(_cs, 1, in->channel(MeshChannel::NORMAL)->_ssbo);
    ci->bindStorageBuffer(_cs, 2, _outP->_ssbo);
    ci->bindStorageBuffer(_cs, 3, _outN->_ssbo);
    ci->bindStorageBuffer(_cs, 4, _prevP);
    ci->bindStorageBuffer(_cs, 5, _prevN);
    ci->bindStorageBuffer(_cs, 6, _ct);
    ci->dispatchCompute(_cs, vg, 1, 1);
    ci->storageBarrier();
  }

  const TemporalSmoothData* _d;
  mesh_outpluginst_ptr_t _output;
  mesh_inpluginst_ptr_t _input;
  std::map<MeshChannel, gpuchannel_ptr_t> _outch;
  gpuchannel_ptr_t _outP, _outN;
  FxShaderStorageBuffer* _ct    = nullptr;
  FxShaderStorageBuffer* _prevP = nullptr;   // PERSISTENT previous-frame position (Inst-owned)
  FxShaderStorageBuffer* _prevN = nullptr;   // PERSISTENT previous-frame normal
  const FxComputeShader* _cs    = nullptr;
  int _cap    = -1;
  int _prevNV = -1;
  uint64_t _prevTopoVer = ~0ull;
  bool _primed = false;
};

static void _reshapeTemporalSmoothIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "alpha")->setValue(0.4f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "tau")->setValue(0.1f);
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
TemporalSmoothData::TemporalSmoothData() {
}
std::shared_ptr<TemporalSmoothData> TemporalSmoothData::createShared() {
  auto d = std::make_shared<TemporalSmoothData>();
  _reshapeTemporalSmoothIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t TemporalSmoothData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<TemporalSmoothInst>(this, g);
}
void TemporalSmoothData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return TemporalSmoothData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeTemporalSmoothIOs(m); });
  clazz->directProperty("mode", &TemporalSmoothData::_mode);
}

} // namespace ork::lev2::hypermesh
