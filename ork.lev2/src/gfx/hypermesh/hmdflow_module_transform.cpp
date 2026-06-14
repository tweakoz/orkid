////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"

ImplementReflectionX(ork::lev2::hypermesh::TransformData, "hypermesh::TransformData");

namespace ork::lev2::hypermesh {

// TransformModule — GPU-NATIVE affine transform of a vertex selection. NO CPU readback of topology: the
// topology passes through unchanged and three compute passes do everything on the GPU each frame —
//   cs_clear : zero a per-vert affected-flag buffer
//   cs_mark  : per FACE, if its live __tags bit is set, flag its corners' verts (race-to-same-value)
//   cs_xform : per VERT, if affected (or whole-mesh), P' = M·P and N/B by the inverse-transpose
// The matrix (rows) + its normal matrix + the slot/counts are all PARAMS (two small SSBOs written each
// frame in writeParams) so the transform ANIMATES with zero recompile. P' = M·P uses the orkid/GLM column
// convention: P'.x = dot(row0, vec4(P,1)) — so we upload matrix.row(0..2).

static std::string _xform_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface tif_iP (descriptor_set 0) { buffer layout(std430) tipb { vec4 iP[]; }; }
storage_interface tif_iN (descriptor_set 0) { buffer layout(std430) tinb { vec4 iN[]; }; }
storage_interface tif_iB (descriptor_set 0) { buffer layout(std430) tibb { vec4 iB[]; }; }
storage_interface tif_oP (descriptor_set 0) { buffer layout(std430) topb { vec4 oP[]; }; }
storage_interface tif_oN (descriptor_set 0) { buffer layout(std430) tonb { vec4 oN[]; }; }
storage_interface tif_oB (descriptor_set 0) { buffer layout(std430) tobb { vec4 oB[]; }; }
storage_interface tif_vi (descriptor_set 0) { buffer layout(std430) tvib { uint VID[]; }; }
storage_interface tif_fo (descriptor_set 0) { buffer layout(std430) tfob { uint FO[];  }; }
storage_interface tif_tg (descriptor_set 0) { buffer layout(std430) ttgb { uint TG[];  }; }
storage_interface tif_vf (descriptor_set 0) { buffer layout(std430) tvfb { uint VF[];  }; }
storage_interface tif_pm (descriptor_set 0) { buffer layout(std430) tpmb { vec4 PM[];  }; }   // M rows 0-3, N-mat rows 4-6
storage_interface tif_ct (descriptor_set 0) { buffer layout(std430) tctb { uint p_nv; uint p_nf; uint p_bit; uint p_mask; }; }
compute_interface iface { storage { tif_iP tif_iN tif_iB tif_oP tif_oN tif_oB tif_vi tif_fo tif_tg tif_vf tif_pm tif_ct }
                          inputs { layout(local_size_x = 64); } }
////////////////////////////////////////
compute_shader cs_clear : iface {
  uint v = gl_GlobalInvocationID.x;
  if (v >= p_nv) { return; }
  VF[v] = 0u;
}
////////////////////////////////////////
compute_shader cs_mark : iface {                    // per face -> flag the verts of selected faces
  uint f = gl_GlobalInvocationID.x;
  if (f >= p_nf) { return; }
  if (p_mask == 0u) { return; }                     // whole-mesh: cs_xform treats everything as affected
  if ((TG[f] & p_bit) == 0u) { return; }
  for (uint c = FO[f]; c < FO[f + 1u]; c++) { VF[VID[c]] = 1u; }
}
////////////////////////////////////////
compute_shader cs_xform : iface {
  uint v = gl_GlobalInvocationID.x;
  if (v >= p_nv) { return; }
  bool aff = (p_mask == 0u) || (VF[v] != 0u);
  if (aff) {
    vec4 P = vec4(iP[v].xyz, 1.0);            // full homogeneous transform incl. the projective row + W divide,
    float w = dot(PM[3], P);                  // so a PROJECTION matrix (row3 != [0,0,0,1]) works, not just affine
    float iw = (abs(w) > 1.0e-8) ? (1.0 / w) : 1.0;
    oP[v] = vec4(dot(PM[0], P) * iw, dot(PM[1], P) * iw, dot(PM[2], P) * iw, 1.0);
    vec3 N = iN[v].xyz;                        // directional part via inverse-transpose (rows 4-6)
    oN[v] = vec4(normalize(vec3(dot(PM[4].xyz, N), dot(PM[5].xyz, N), dot(PM[6].xyz, N))), 0.0);
    vec3 B = iB[v].xyz;
    oB[v] = vec4(normalize(vec3(dot(PM[4].xyz, B), dot(PM[5].xyz, B), dot(PM[6].xyz, B))), 0.0);
  } else {
    oP[v] = iP[v]; oN[v] = iN[v]; oB[v] = iB[v];
  }
}
)S";
}

struct TransformInst : public MeshComputeInst {
  TransformInst(const TransformData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _input  = typedInputNamed<MeshPlugTraits>("In");
  }

  bool onTopologyReady(Context* ctx) final {
    if (not _srcMesh(_input) or _cs_xform) return false;
    auto sh   = ctx->FXI()->shaderFromShaderText("hypermesh_transform", _xform_text());
    _cs_clear = ctx->FXI()->computeShader(sh, "cs_clear");
    _cs_mark  = ctx->FXI()->computeShader(sh, "cs_mark");
    _cs_xform = ctx->FXI()->computeShader(sh, "cs_xform");
    return true;                                    // re-eval so the transformed output is live
  }

  void writeParams(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in) return;
    auto out = _output->_value;
    auto env = _graphinst->_impl.getShared<MeshEnv>();
    auto fxi = ctx->FXI();
    int  nv  = in->_num_verts;
    if (not _cs_xform) {                            // pre-compile (eval 1): pass the input straight through
      out->_channels = in->_channels; out->_faces = in->_faces; out->_vidx = in->_vidx;
      out->_face_offsets = in->_face_offsets; out->_header = in->_header; out->_capacity = in->_capacity;
      out->_num_verts = in->_num_verts; out->_num_corners = in->_num_corners; out->_num_faces = in->_num_faces;
      return;
    }
    if (meshNextPow2(nv) != _cap) {                 // (re)pool the produced P/N/B + the vert-flag buffer
      for (auto c : kCh) _outch[c] = env->_pool->acquireChannel(c, nv);
      _flags = env->_pool->acquire(4, nv);
      _cap   = meshNextPow2(nv);
    }
    if (not _pm) _pm = fxi->createStorageBuffer(7 * 16);   // M rows 0-3 (full, incl projective) + N-mat rows 4-6
    if (not _ct) _ct = fxi->createStorageBuffer(16);
    // full matrix rows (P' = M·P, W-divide in shader -> projections work); normal matrix = inverse-transpose.
    fmtx4 nrm = _d->_matrix.inverse().transposed();
    fvec4 pm[7] = {_d->_matrix.row(0), _d->_matrix.row(1), _d->_matrix.row(2), _d->_matrix.row(3),
                   nrm.row(0),          nrm.row(1),          nrm.row(2)};
    auto mp = fxi->mapStorageBuffer(_pm, 0, sizeof(pm), BufferMapAccess::WRITE_ONLY);
    std::memcpy(mp->_mappedaddr, pm, sizeof(pm));
    fxi->unmapStorageBuffer(mp.get());
    uint32_t ct[4] = {uint32_t(nv), uint32_t(in->_num_faces),
                      (_d->_slot >= 0) ? (1u << (_d->_slot & 31)) : 0u, (_d->_slot >= 0) ? 1u : 0u};
    auto cp = fxi->mapStorageBuffer(_ct, 0, sizeof(ct), BufferMapAccess::WRITE_ONLY);
    std::memcpy(cp->_mappedaddr, ct, sizeof(ct));
    fxi->unmapStorageBuffer(cp.get());
    // topology + uv/color + face attrs pass through; only P/N/B are produced.
    _outch[MeshChannel::UV0]   = in->channel(MeshChannel::UV0);
    _outch[MeshChannel::COLOR] = in->channel(MeshChannel::COLOR);
    out->_channels     = _outch;
    out->_faces        = in->_faces;
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
    if (not in or not _cs_xform) return;
    auto env = inst->_impl.getShared<MeshEnv>();
    auto ci  = env->_ctx->CI();
    auto& s  = in->_channels;
    auto tg  = in->face("__tags");
    FxShaderStorageBuffer* tgb = tg ? tg->_ssbo : in->_vidx->_ssbo;   // dummy when no __tags (p_mask=0 -> unread)
    int nv = in->_num_verts, nf = in->_num_faces;
    auto bind = [&](const FxComputeShader* cs) {
      ci->bindStorageBuffer(cs, 0, s[MeshChannel::POSITION]->_ssbo);
      ci->bindStorageBuffer(cs, 1, s[MeshChannel::NORMAL]->_ssbo);
      ci->bindStorageBuffer(cs, 2, s[MeshChannel::BINORMAL]->_ssbo);
      ci->bindStorageBuffer(cs, 3, _outch[MeshChannel::POSITION]->_ssbo);
      ci->bindStorageBuffer(cs, 4, _outch[MeshChannel::NORMAL]->_ssbo);
      ci->bindStorageBuffer(cs, 5, _outch[MeshChannel::BINORMAL]->_ssbo);
      ci->bindStorageBuffer(cs, 6, in->_vidx->_ssbo);
      ci->bindStorageBuffer(cs, 7, in->_face_offsets->_ssbo);
      ci->bindStorageBuffer(cs, 8, tgb);
      ci->bindStorageBuffer(cs, 9, _flags);
      ci->bindStorageBuffer(cs, 10, _pm);
      ci->bindStorageBuffer(cs, 11, _ct);
    };
    bind(_cs_clear); ci->dispatchCompute(_cs_clear, (nv + 63) / 64, 1, 1);
    ci->storageBarrier();
    bind(_cs_mark);  ci->dispatchCompute(_cs_mark,  (nf + 63) / 64, 1, 1);
    ci->storageBarrier();
    bind(_cs_xform); ci->dispatchCompute(_cs_xform, (nv + 63) / 64, 1, 1);
    ci->storageBarrier();
  }

  const TransformData* _d;
  mesh_outpluginst_ptr_t _output;
  mesh_inpluginst_ptr_t _input;
  static const MeshChannel kCh[3];
  std::map<MeshChannel, gpuchannel_ptr_t> _outch;
  FxShaderStorageBuffer *_flags = nullptr, *_pm = nullptr, *_ct = nullptr;
  const FxComputeShader *_cs_clear = nullptr, *_cs_mark = nullptr, *_cs_xform = nullptr;
  int _cap = -1;
};
const MeshChannel TransformInst::kCh[3] = {MeshChannel::POSITION, MeshChannel::NORMAL, MeshChannel::BINORMAL};

static void _reshapeTransformIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
TransformData::TransformData() {
}
std::shared_ptr<TransformData> TransformData::createShared() {
  auto d = std::make_shared<TransformData>();
  _reshapeTransformIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t TransformData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<TransformInst>(this, g);
}
void TransformData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return TransformData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeTransformIOs(m); });
  clazz->directProperty("slot", &TransformData::_slot);
  clazz->directProperty("matrix", &TransformData::_matrix);   // the affine — was silently identity on reload
}

} // namespace ork::lev2::hypermesh
