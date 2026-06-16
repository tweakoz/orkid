////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"

ImplementReflectionX(ork::lev2::hypermesh::GpuComputeModuleData, "hypermesh::GpuComputeModuleData");

namespace ork::lev2::hypermesh {

// GpuComputeModule — a GENERIC per-vertex GPU compute op. Runs an arbitrary fxv2 compute kernel
// supplied as reflected DATA (`_shadertext` + `_kernel`), so new per-vertex GPU behaviors
// (ripple / twist / bend / noise-displace / ...) are authored as shader TEXT from the DSL with NO
// new C++ — the GpuCompute counterpart of terrain's ExprModule. 1 mesh-in, 1 mesh-out. The kernel
// binds (compute_interface storage order):
//   slot 0 = iP[]   (input POSITION, vec4)        slot 2 = EXPRP[] (8 vec4 runtime params)
//   slot 1 = oP[]   (produced POSITION, vec4)     slot 3 = control { uint p_nv; uint p_mode; ... }
// Topology + every OTHER channel PASSES THROUGH (alias); only POSITION is produced — chain
// recompute_tbn to refresh shading (the family convention; this op does not guess your shading).
// `_dispatch_mode` 0 = one invocation per VERTEX (groups = (nv+63)/64). `_time_slot` >= 0 marks
// the EXPRP slot whose .x/.y the module feeds from MeshEnv abstime/dt (the S.time bridge —
// declarative time, zero per-frame Python). The reflected `_shadertext`/`_kernel`/`_dispatch_mode`/
// `_time_slot` ARE the op identity — the base MeshComputeInst::cookComputeHash hashes the reflected
// state automatically (no per-inst hash needed; bump `_cookSalt` only when this C++ shell changes).
//
// Author's kernel must declare a matching storage layout, e.g.:
//   storage_interface gif_iP (descriptor_set 0) { buffer layout(std430) gipb { vec4 iP[]; }; }
//   storage_interface gif_oP (descriptor_set 0) { buffer layout(std430) gopb { vec4 oP[]; }; }
//   storage_interface gif_pm (descriptor_set 0) { buffer layout(std430) gpmb { vec4 EXPRP[]; }; }
//   storage_interface gif_ct (descriptor_set 0) { buffer layout(std430) gctb { uint p_nv; uint p_mode; uint p0; uint p1; }; }
//   compute_interface iface { storage { gif_iP gif_oP gif_pm gif_ct } inputs { layout(local_size_x = 64); } }
//   compute_shader cs_main : iface { uint v=gl_GlobalInvocationID.x; if(v>=p_nv){return;} oP[v]=iP[v]; }

struct GpuComputeModuleInst : public MeshComputeInst {
  GpuComputeModuleInst(const GpuComputeModuleData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}

  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _input  = typedInputNamed<MeshPlugTraits>("In");
    for (int k = 0; k < kMaxExprParams; k++)
      _exprplugs[k] = typedInputNamed<dflow::Vec4fPlugTraits>(FormatString("exprp%d", k).c_str());
  }

  // compile the authored kernel ONCE, after the input topology is host-ready (mirrors displace).
  bool onTopologyReady(Context* ctx) final {
    if (not _srcMesh(_input) or _cs) return false;
    if (_d->_shadertext.empty()) {
      if (not _warned) {
        printf("GpuCompute<%s>: empty shadertext — passing the mesh through (author a kernel via the DSL)\n",
               _dgmodule_data->_name.c_str());
        _warned = true;
      }
      return false;
    }
    auto sh = ctx->FXI()->shaderFromShaderText("hypermesh_gpucompute", _d->_shadertext);
    _cs     = ctx->FXI()->computeShader(sh, _d->_kernel.c_str());
    return true; // re-eval so the computed output is live this materialize
  }

  void writeParams(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in) return;
    auto out = _output->_value;
    auto env = _graphinst->_impl.getShared<MeshEnv>();
    auto fxi = ctx->FXI();
    int  nv  = in->_num_verts;
    // pre-compile (eval 1) or no kernel: pass the input straight through (alias all channels + topology).
    if (not _cs) {
      out->_channels = in->_channels; out->_faces = in->_faces; out->_vattrs = in->_vattrs;
      out->_vidx = in->_vidx; out->_face_offsets = in->_face_offsets; out->_header = in->_header;
      out->_capacity = in->_capacity; out->_num_verts = in->_num_verts;
      out->_num_corners = in->_num_corners; out->_num_faces = in->_num_faces;
      return;
    }
    if (meshNextPow2(nv) != _cap) {                       // (re)pool the produced POSITION channel
      _outP = env->_pool->acquireChannel(MeshChannel::POSITION, nv);
      _cap  = meshNextPow2(nv);
    }
    if (not _ct)    _ct    = fxi->createStorageBuffer(32);
    if (not _exprp) _exprp = fxi->createStorageBuffer(size_t(kMaxExprParams) * 16);
    // B.4: snapshot the 8 expression-param PLUGS into the EXPRP SSBO (the defined cross-thread handoff:
    // Python pokes plug values between frames; this pre-phase host write is the render-side snapshot).
    float ep[kMaxExprParams * 4];
    for (int k = 0; k < kMaxExprParams; k++) {
      fvec4 v = _exprplugs[k] ? *(_exprplugs[k]->_value) : fvec4(0, 0, 0, 0);
      ep[k * 4 + 0] = v.x; ep[k * 4 + 1] = v.y; ep[k * 4 + 2] = v.z; ep[k * 4 + 3] = v.w;
    }
    if (_d->_time_slot >= 0 and _d->_time_slot < kMaxExprParams) {
      ep[_d->_time_slot * 4 + 0] = float(env->_abstime);
      ep[_d->_time_slot * 4 + 1] = float(env->_dt);
    }
    auto mp = fxi->mapStorageBuffer(_exprp, 0, sizeof(ep), BufferMapAccess::WRITE_ONLY);
    std::memcpy(mp->_mappedaddr, ep, sizeof(ep));
    fxi->unmapStorageBuffer(mp.get());
    // control block: {p_nv, p_mode, ...}
    uint32_t ct[8] = {uint32_t(nv), uint32_t(_d->_dispatch_mode), 0, 0, 0, 0, 0, 0};
    auto cp = fxi->mapStorageBuffer(_ct, 0, sizeof(ct), BufferMapAccess::WRITE_ONLY);
    std::memcpy(cp->_mappedaddr, ct, sizeof(ct));
    fxi->unmapStorageBuffer(cp.get());
    // topology + every OTHER channel passes through; only POSITION is produced (the displace policy).
    _outch                        = in->_channels;
    _outch[MeshChannel::POSITION] = _outP;
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
  }

  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto in = _srcMesh(_input);
    if (not in or not _cs) return;
    auto env = inst->_impl.getShared<MeshEnv>();
    auto ci  = env->_ctx->CI();
    int nv   = in->_num_verts;
    ci->bindStorageBuffer(_cs, 0, in->channel(MeshChannel::POSITION)->_ssbo);  // iP
    ci->bindStorageBuffer(_cs, 1, _outP->_ssbo);                               // oP
    ci->bindStorageBuffer(_cs, 2, _exprp);                                     // EXPRP (8 vec4)
    ci->bindStorageBuffer(_cs, 3, _ct);                                        // control
    ci->dispatchCompute(_cs, (nv + 63) / 64, 1, 1);   // _dispatch_mode 0 = one thread / vertex
    ci->storageBarrier();
  }

  const char* _cookSalt() const final { return "gpucompute.v1"; } // bump if this C++ shell (bind order / control layout) changes

  const GpuComputeModuleData* _d;
  mesh_outpluginst_ptr_t _output;
  mesh_inpluginst_ptr_t _input;
  std::shared_ptr<dflow::inpluginst<dflow::Vec4fPlugTraits>> _exprplugs[kMaxExprParams] = {};
  std::map<MeshChannel, gpuchannel_ptr_t> _outch;
  gpuchannel_ptr_t _outP;
  FxShaderStorageBuffer *_ct = nullptr, *_exprp = nullptr;
  const FxComputeShader* _cs = nullptr;
  int _cap     = -1;
  bool _warned = false;
};

static void _reshapeGpuComputeIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  // the FIXED expression-param plug set (kMaxExprParams vec4s; unused idle at zero). createInputPlug
  // dedups by name, so the double reshapeIOs on deserialize stays idempotent.
  for (int k = 0; k < kMaxExprParams; k++) {
    auto nm = FormatString("exprp%d", k);
    dflow::ModuleData::createInputPlug<dflow::Vec4fPlugTraits>(data, dflow::EPR_UNIFORM, nm.c_str());
  }
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
GpuComputeModuleData::GpuComputeModuleData() {
}
std::shared_ptr<GpuComputeModuleData> GpuComputeModuleData::createShared() {
  auto d = std::make_shared<GpuComputeModuleData>();
  _reshapeGpuComputeIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t GpuComputeModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<GpuComputeModuleInst>(this, g);
}
void GpuComputeModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return GpuComputeModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeGpuComputeIOs(m); });
  // the authored shader text + entry-point ARE the portable, python-decoupled op identity -> reflect them
  // (also makes them part of the cook-cache content hash via the base MeshComputeInst::cookComputeHash).
  clazz->directProperty("shadertext", &GpuComputeModuleData::_shadertext);
  clazz->directProperty("kernel", &GpuComputeModuleData::_kernel);
  clazz->directProperty("dispatch_mode", &GpuComputeModuleData::_dispatch_mode);
  clazz->directProperty("time_slot", &GpuComputeModuleData::_time_slot);
}

} // namespace ork::lev2::hypermesh
