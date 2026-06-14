////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"

ImplementReflectionX(ork::lev2::hypermesh::CompactData, "hypermesh::CompactData");

namespace ork::lev2::hypermesh {

// Compact (vertices) — drop orphan verts + remap vidx, via the shared MeshScan.
//   cs_clearv  : zero a per-vert referenced-flag
//   cs_markv   : per face, flag the verts of every corner (zero-length faces have no corners -> their verts orphan)
//   MeshScan   : prefix-sum the flags -> VBASE; VBASE[v] = the kept vert's NEW index, VBASE[nv] = new count
//   cs_scatterv: kept verts copy their channels to the new index
//   cs_remap   : rewrite every corner's vidx through VBASE (faces/offsets unchanged)
// The new vert count (VBASE[nv]) is read back as a single uint in writeParams (last frame's synced buffer).

static std::string _compact_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface cf_iP (descriptor_set 0) { buffer layout(std430) cipb { vec4 iP[]; }; }
storage_interface cf_iN (descriptor_set 0) { buffer layout(std430) cinb { vec4 iN[]; }; }
storage_interface cf_iB (descriptor_set 0) { buffer layout(std430) cibb { vec4 iB[]; }; }
storage_interface cf_iU (descriptor_set 0) { buffer layout(std430) ciub { vec4 iU[]; }; }
storage_interface cf_iC (descriptor_set 0) { buffer layout(std430) cicb { vec4 iC[]; }; }
storage_interface cf_oP (descriptor_set 0) { buffer layout(std430) copb { vec4 oP[]; }; }
storage_interface cf_oN (descriptor_set 0) { buffer layout(std430) conb { vec4 oN[]; }; }
storage_interface cf_oB (descriptor_set 0) { buffer layout(std430) cobb { vec4 oB[]; }; }
storage_interface cf_oU (descriptor_set 0) { buffer layout(std430) coub { vec4 oU[]; }; }
storage_interface cf_oC (descriptor_set 0) { buffer layout(std430) cocb { vec4 oC[]; }; }
storage_interface cf_vi (descriptor_set 0) { buffer layout(std430) cvib { uint VID[];   }; }
storage_interface cf_fo (descriptor_set 0) { buffer layout(std430) cfob { uint FO[];    }; }
storage_interface cf_ov (descriptor_set 0) { buffer layout(std430) covb { uint OVID[];  }; }
storage_interface cf_vf (descriptor_set 0) { buffer layout(std430) cvfb { uint VFLAG[]; }; }
storage_interface cf_vb (descriptor_set 0) { buffer layout(std430) cvbb { uint VBASE[]; }; }
storage_interface cf_ct (descriptor_set 0) { buffer layout(std430) cctb { uint p_nv; uint p_nf; uint p_nc; uint c3; }; }
compute_interface iface { storage { cf_iP cf_iN cf_iB cf_iU cf_iC cf_oP cf_oN cf_oB cf_oU cf_oC
                                    cf_vi cf_fo cf_ov cf_vf cf_vb cf_ct }
                          inputs { layout(local_size_x = 64); } }
////////////////////////////////////////
compute_shader cs_clearv : iface {
  uint v = gl_GlobalInvocationID.x;
  if (v >= p_nv) { return; }
  VFLAG[v] = 0u;
}
////////////////////////////////////////
compute_shader cs_markv : iface {                   // verts touched by ANY face's corners are kept
  uint f = gl_GlobalInvocationID.x;
  if (f >= p_nf) { return; }
  for (uint c = FO[f]; c < FO[f + 1u]; c++) { VFLAG[VID[c]] = 1u; }
}
////////////////////////////////////////
compute_shader cs_scatterv : iface {
  uint v = gl_GlobalInvocationID.x;
  if (v >= p_nv) { return; }
  if (VFLAG[v] == 0u) { return; }
  uint d = VBASE[v];
  oP[d] = iP[v]; oN[d] = iN[v]; oB[d] = iB[v]; oU[d] = iU[v]; oC[d] = iC[v];
}
////////////////////////////////////////
compute_shader cs_remap : iface {                   // rewrite each corner's vidx through the old->new remap
  uint f = gl_GlobalInvocationID.x;
  if (f >= p_nf) { return; }
  for (uint c = FO[f]; c < FO[f + 1u]; c++) { OVID[c] = VBASE[VID[c]]; }
}
)S";
}

struct CompactInst : public MeshComputeInst {
  CompactInst(const CompactData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _input  = typedInputNamed<MeshPlugTraits>("In");
  }
  static const MeshChannel kCh[5];
  bool onTopologyReady(Context* ctx) final {
    if (not _srcMesh(_input) or _cs_clearv) return false;
    auto sh     = ctx->FXI()->shaderFromShaderText("hypermesh_compact", _compact_text());
    _cs_clearv  = ctx->FXI()->computeShader(sh, "cs_clearv");
    _cs_markv   = ctx->FXI()->computeShader(sh, "cs_markv");
    _cs_scatter = ctx->FXI()->computeShader(sh, "cs_scatterv");
    _cs_remap   = ctx->FXI()->computeShader(sh, "cs_remap");
    _scan.init(ctx);
    return true;
  }
  void writeParams(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in) return;
    auto out = _output->_value;
    auto env = _graphinst->_impl.getShared<MeshEnv>();
    auto fxi = ctx->FXI();
    int  nv  = in->_num_verts, nf = in->_num_faces, nc = in->_num_corners;
    if (not _cs_clearv) {                              // pre-compile -> passthrough
      out->_channels = in->_channels; out->_faces = in->_faces; out->_vidx = in->_vidx;
      out->_face_offsets = in->_face_offsets; out->_header = in->_header; out->_capacity = in->_capacity;
      out->_num_verts = nv; out->_num_corners = nc; out->_num_faces = nf;
      return;
    }
    if (meshNextPow2(nv) != _capv) {
      for (auto c : kCh) _outch[c] = env->_pool->acquireChannel(c, nv);
      _vflag = env->_pool->acquireChannel(4, nv);
      _vbase = env->_pool->acquireChannel(4, nv + 1);
      uint32_t init = uint32_t(nv);                   // VBASE[nv] default (read before the first compute)
      auto mi = fxi->mapStorageBuffer(_vbase->_ssbo, size_t(nv) * 4, 4, BufferMapAccess::WRITE_ONLY);
      std::memcpy(mi->_mappedaddr, &init, 4); fxi->unmapStorageBuffer(mi.get());
      _capv = meshNextPow2(nv);
    }
    if (meshNextPow2(nc) != _capc) { _ovidx = env->_pool->acquireChannel(4, std::max(1, nc)); _capc = meshNextPow2(nc); }
    if (not _ct)  _ct  = fxi->createStorageBuffer(16);
    if (not _sct) _sct = fxi->createStorageBuffer(16);
    uint32_t newnv = nv;                              // read back the kept-vert count (VBASE[nv]) from last frame
    auto mr = fxi->mapStorageBuffer(_vbase->_ssbo, size_t(nv) * 4, 4, BufferMapAccess::READ_ONLY);
    std::memcpy(&newnv, mr->_mappedaddr, 4); fxi->unmapStorageBuffer(mr.get());
    uint32_t ct[4] = {uint32_t(nv), uint32_t(nf), uint32_t(nc), 0u};
    auto m = fxi->mapStorageBuffer(_ct, 0, sizeof(ct), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, ct, sizeof(ct)); fxi->unmapStorageBuffer(m.get());
    uint32_t sct[4] = {uint32_t(nv), 0u, 0u, 0u};     // scan p_n
    auto ms = fxi->mapStorageBuffer(_sct, 0, sizeof(sct), BufferMapAccess::WRITE_ONLY);
    std::memcpy(ms->_mappedaddr, sct, sizeof(sct)); fxi->unmapStorageBuffer(ms.get());
    out->_channels     = _outch;                      // packed to the kept verts
    out->_faces        = in->_faces;                  // faces unchanged -> attrs valid
    out->_vidx         = _ovidx;                      // remapped (same offsets)
    out->_face_offsets = in->_face_offsets;
    out->_header       = in->_header;
    out->_capacity     = in->_capacity;
    out->_num_verts    = int(newnv);                  // tight vert count
    out->_num_corners  = nc;
    out->_num_faces    = nf;
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto in = _srcMesh(_input);
    if (not in or not _cs_clearv) return;
    auto env = inst->_impl.getShared<MeshEnv>();
    auto ci  = env->_ctx->CI();
    auto& s  = in->_channels;
    int nv = in->_num_verts, nf = in->_num_faces;
    auto bind = [&](const FxComputeShader* cs) {
      ci->bindStorageBuffer(cs, 0, s[MeshChannel::POSITION]->_ssbo);
      ci->bindStorageBuffer(cs, 1, s[MeshChannel::NORMAL]->_ssbo);
      ci->bindStorageBuffer(cs, 2, s[MeshChannel::BINORMAL]->_ssbo);
      ci->bindStorageBuffer(cs, 3, s[MeshChannel::UV0]->_ssbo);
      ci->bindStorageBuffer(cs, 4, s[MeshChannel::COLOR]->_ssbo);
      ci->bindStorageBuffer(cs, 5, _outch[MeshChannel::POSITION]->_ssbo);
      ci->bindStorageBuffer(cs, 6, _outch[MeshChannel::NORMAL]->_ssbo);
      ci->bindStorageBuffer(cs, 7, _outch[MeshChannel::BINORMAL]->_ssbo);
      ci->bindStorageBuffer(cs, 8, _outch[MeshChannel::UV0]->_ssbo);
      ci->bindStorageBuffer(cs, 9, _outch[MeshChannel::COLOR]->_ssbo);
      ci->bindStorageBuffer(cs, 10, in->_vidx->_ssbo);
      ci->bindStorageBuffer(cs, 11, in->_face_offsets->_ssbo);
      ci->bindStorageBuffer(cs, 12, _ovidx->_ssbo);
      ci->bindStorageBuffer(cs, 13, _vflag->_ssbo);
      ci->bindStorageBuffer(cs, 14, _vbase->_ssbo);
      ci->bindStorageBuffer(cs, 15, _ct);
    };
    bind(_cs_clearv); ci->dispatchCompute(_cs_clearv, (nv + 63) / 64, 1, 1);
    ci->storageBarrier();
    bind(_cs_markv);  ci->dispatchCompute(_cs_markv,  (nf + 63) / 64, 1, 1);
    ci->storageBarrier();
    _scan.scan(env->_ctx, _vflag->_ssbo, _vbase->_ssbo, _sct);   // VFLAG -> VBASE (old->new remap; [nv]=count)
    ci->storageBarrier();
    bind(_cs_scatter); ci->dispatchCompute(_cs_scatter, (nv + 63) / 64, 1, 1);
    bind(_cs_remap);   ci->dispatchCompute(_cs_remap,   (nf + 63) / 64, 1, 1);
    ci->storageBarrier();
  }
  const CompactData* _d;
  mesh_outpluginst_ptr_t _output;
  mesh_inpluginst_ptr_t _input;
  MeshScan _scan;
  std::map<MeshChannel, gpuchannel_ptr_t> _outch;
  gpuchannel_ptr_t _ovidx, _vflag, _vbase;
  FxShaderStorageBuffer *_ct = nullptr, *_sct = nullptr;
  const FxComputeShader *_cs_clearv = nullptr, *_cs_markv = nullptr, *_cs_scatter = nullptr, *_cs_remap = nullptr;
  int _capv = -1, _capc = -1;
};
const MeshChannel CompactInst::kCh[5] = {
    MeshChannel::POSITION, MeshChannel::NORMAL, MeshChannel::BINORMAL, MeshChannel::UV0, MeshChannel::COLOR};

static void _reshapeCompactIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
CompactData::CompactData() {
}
std::shared_ptr<CompactData> CompactData::createShared() {
  auto d = std::make_shared<CompactData>();
  _reshapeCompactIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t CompactData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<CompactInst>(this, g);
}
void CompactData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return CompactData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeCompactIOs(m); });
}

} // namespace ork::lev2::hypermesh
