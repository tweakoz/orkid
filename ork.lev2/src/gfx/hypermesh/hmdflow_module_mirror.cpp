////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"

ImplementReflectionX(ork::lev2::hypermesh::MirrorData, "hypermesh::MirrorData");

namespace ork::lev2::hypermesh {

// Mirror — reflect across a principal plane + append the reversed-winding copy. GPU-native, no readback.
//   cs_markdup : per vert -> 1 if it needs a mirror copy (off-plane, or always when !weld)
//   MeshScan   : prefix-sum -> MBASE[v] = compacted mirror-vert index (welded seam verts get none)
//   cs_vert    : copy original; if dup, write the reflected copy at nv+MBASE[v]
//   cs_topo    : copy original faces; build mirror faces (reversed corners, each remapped to its mirror vert)
// Faces/corners are a deterministic 2x; verts <= 2x (the shared seam removed). num_verts over-allocated to
// 2*nv (welded seam leaves a few dead slots; a later compact repacks) -> zero readback.

static std::string _mirror_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface mf_iP (descriptor_set 0) { buffer layout(std430) mipb { vec4 iP[]; }; }
storage_interface mf_iN (descriptor_set 0) { buffer layout(std430) minb { vec4 iN[]; }; }
storage_interface mf_iB (descriptor_set 0) { buffer layout(std430) mibb { vec4 iB[]; }; }
storage_interface mf_iU (descriptor_set 0) { buffer layout(std430) miub { vec4 iU[]; }; }
storage_interface mf_iC (descriptor_set 0) { buffer layout(std430) micb { vec4 iC[]; }; }
storage_interface mf_oP (descriptor_set 0) { buffer layout(std430) mopb { vec4 oP[]; }; }
storage_interface mf_oN (descriptor_set 0) { buffer layout(std430) monb { vec4 oN[]; }; }
storage_interface mf_oB (descriptor_set 0) { buffer layout(std430) mobb { vec4 oB[]; }; }
storage_interface mf_oU (descriptor_set 0) { buffer layout(std430) moub { vec4 oU[]; }; }
storage_interface mf_oC (descriptor_set 0) { buffer layout(std430) mocb { vec4 oC[]; }; }
storage_interface mf_vi (descriptor_set 0) { buffer layout(std430) mvib { uint VID[];  }; }
storage_interface mf_fo (descriptor_set 0) { buffer layout(std430) mfob { uint FO[];   }; }
storage_interface mf_ov (descriptor_set 0) { buffer layout(std430) movb { uint OVID[]; }; }
storage_interface mf_of (descriptor_set 0) { buffer layout(std430) mofb { uint OFO[];  }; }
storage_interface mf_dp (descriptor_set 0) { buffer layout(std430) mdpb { uint DUP[];  }; }
storage_interface mf_mb (descriptor_set 0) { buffer layout(std430) mmbb { uint MBASE[]; }; }
storage_interface mf_ct (descriptor_set 0) { buffer layout(std430) mctb {
  uint p_nv; uint p_nf; uint p_nc; uint p_axis; uint p_epsbits; uint p_weld; uint c6; uint c7; }; }
compute_interface iface { storage { mf_iP mf_iN mf_iB mf_iU mf_iC mf_oP mf_oN mf_oB mf_oU mf_oC
                                    mf_vi mf_fo mf_ov mf_of mf_dp mf_mb mf_ct }
                          inputs { layout(local_size_x = 64); } }
////////////////////////////////////////
compute_shader cs_markdup : iface {
  uint v = gl_GlobalInvocationID.x;
  if (v >= p_nv) { return; }
  float eps = uintBitsToFloat(p_epsbits);
  float c   = (p_axis == 0u) ? iP[v].x : ((p_axis == 1u) ? iP[v].y : iP[v].z);
  DUP[v]    = (p_weld == 0u) ? 1u : ((abs(c) > eps) ? 1u : 0u);
}
////////////////////////////////////////
compute_shader cs_vert : iface {                    // copy original; reflect the off-plane copy
  uint v = gl_GlobalInvocationID.x;
  if (v >= p_nv) { return; }
  oP[v] = iP[v]; oN[v] = iN[v]; oB[v] = iB[v]; oU[v] = iU[v]; oC[v] = iC[v];
  if (DUP[v] == 0u) { return; }
  uint m = p_nv + MBASE[v];
  vec4 P = iP[v]; vec4 N = iN[v]; vec4 B = iB[v];
  if (p_axis == 0u)      { P.x = -P.x; N.x = -N.x; B.x = -B.x; }
  else if (p_axis == 1u) { P.y = -P.y; N.y = -N.y; B.y = -B.y; }
  else                   { P.z = -P.z; N.z = -N.z; B.z = -B.z; }
  oP[m] = P; oN[m] = N; oB[m] = B; oU[m] = iU[v]; oC[m] = iC[v];
}
////////////////////////////////////////
compute_shader cs_topo : iface {                    // original faces + reversed-winding mirror faces
  uint f = gl_GlobalInvocationID.x;
  if (f >= p_nf) { return; }
  uint a = FO[f]; uint sz = FO[f + 1u] - a;
  OFO[f]             = a;                            // original face: same corners
  OFO[p_nf + f]      = p_nc + a;                     // mirror face: shifted by total corners
  if (f == 0u) { OFO[2u * p_nf] = 2u * p_nc; }       // CSR final entry
  for (uint i = 0u; i < sz; i++) {
    uint u   = VID[a + i];
    OVID[a + i] = u;                                 // original corner
    uint mu  = (DUP[u] != 0u) ? (p_nv + MBASE[u]) : u;
    OVID[p_nc + a + (sz - 1u - i)] = mu;             // mirror corner, REVERSED order
  }
}
)S";
}

struct MirrorInst : public MeshComputeInst {
  MirrorInst(const MirrorData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _input  = typedInputNamed<MeshPlugTraits>("In");
  }
  static const MeshChannel kCh[5];
  bool onTopologyReady(Context* ctx) final {
    if (not _srcMesh(_input) or _cs_vert) return false;
    auto sh    = ctx->FXI()->shaderFromShaderText("hypermesh_mirror", _mirror_text());
    _cs_mark   = ctx->FXI()->computeShader(sh, "cs_markdup");
    _cs_vert   = ctx->FXI()->computeShader(sh, "cs_vert");
    _cs_topo   = ctx->FXI()->computeShader(sh, "cs_topo");
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
    if (not _cs_vert) {                               // pre-compile -> passthrough
      out->_channels = in->_channels; out->_faces = in->_faces; out->_vidx = in->_vidx;
      out->_face_offsets = in->_face_offsets; out->_header = in->_header; out->_capacity = in->_capacity;
      out->_num_verts = nv; out->_num_corners = nc; out->_num_faces = nf;
      return;
    }
    if (meshNextPow2(2 * nv) != _capv) {              // 2x worst case (weld leaves dead slots)
      for (auto c : kCh) _outch[c] = env->_pool->acquireChannel(c, 2 * nv);
      _dup   = env->_pool->acquireChannel(4, nv);
      _mbase = env->_pool->acquireChannel(4, nv + 1);
      _capv  = meshNextPow2(2 * nv);
    }
    if (meshNextPow2(2 * nf) != _capf) {
      _ofo  = env->_pool->acquireChannel(4, 2 * nf + 1);
      _capf = meshNextPow2(2 * nf);
    }
    if (meshNextPow2(2 * nc) != _capc) { _ovidx = env->_pool->acquireChannel(4, std::max(1, 2 * nc)); _capc = meshNextPow2(2 * nc); }
    if (not _ct)  _ct  = fxi->createStorageBuffer(32);
    if (not _sct) _sct = fxi->createStorageBuffer(16);
    uint32_t eb; std::memcpy(&eb, &_d->_eps, 4);
    uint32_t ct[8] = {uint32_t(nv), uint32_t(nf), uint32_t(nc), uint32_t(_d->_axis & 3),
                      eb, _d->_weld ? 1u : 0u, 0u, 0u};
    auto m = fxi->mapStorageBuffer(_ct, 0, sizeof(ct), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, ct, sizeof(ct)); fxi->unmapStorageBuffer(m.get());
    uint32_t sct[4] = {uint32_t(nv), 0u, 0u, 0u};
    auto ms = fxi->mapStorageBuffer(_sct, 0, sizeof(sct), BufferMapAccess::WRITE_ONLY);
    std::memcpy(ms->_mappedaddr, sct, sizeof(sct)); fxi->unmapStorageBuffer(ms.get());
    if (not _header) _header = env->_pool->acquire(kMeshHeaderBytes, 1);
    out->_channels     = _outch;
    out->_faces        = in->_faces;                  // FIXME(follow-up): mirror faces need __tags propagated (FaceTagger)
    out->_vidx         = _ovidx;
    out->_face_offsets = _ofo;
    out->_header       = _header;
    out->_capacity     = meshNextPow2(2 * nv);
    out->_num_verts    = 2 * nv;                      // overestimate (welded seam leaves dead slots); CSR-safe
    out->_num_corners  = 2 * nc;
    out->_num_faces    = 2 * nf;
    uint32_t hdr[4] = {uint32_t(2 * nv), uint32_t(2 * nc), uint32_t(2 * nf), 0u};
    auto mh = fxi->mapStorageBuffer(_header, 0, sizeof(hdr), BufferMapAccess::WRITE_ONLY);
    std::memcpy(mh->_mappedaddr, hdr, sizeof(hdr)); fxi->unmapStorageBuffer(mh.get());
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto in = _srcMesh(_input);
    if (not in or not _cs_vert) return;
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
      ci->bindStorageBuffer(cs, 13, _ofo->_ssbo);
      ci->bindStorageBuffer(cs, 14, _dup->_ssbo);
      ci->bindStorageBuffer(cs, 15, _mbase->_ssbo);
      ci->bindStorageBuffer(cs, 16, _ct);
    };
    bind(_cs_mark); ci->dispatchCompute(_cs_mark, (nv + 63) / 64, 1, 1);
    ci->storageBarrier();
    _scan.scan(env->_ctx, _dup->_ssbo, _mbase->_ssbo, _sct);  // DUP -> MBASE (compacted mirror indices)
    ci->storageBarrier();
    bind(_cs_vert); ci->dispatchCompute(_cs_vert, (nv + 63) / 64, 1, 1);
    bind(_cs_topo); ci->dispatchCompute(_cs_topo, (nf + 63) / 64, 1, 1);
    ci->storageBarrier();
  }
  const MirrorData* _d;
  mesh_outpluginst_ptr_t _output;
  mesh_inpluginst_ptr_t _input;
  MeshScan _scan;
  std::map<MeshChannel, gpuchannel_ptr_t> _outch;
  gpuchannel_ptr_t _ovidx, _ofo, _dup, _mbase;
  FxShaderStorageBuffer *_ct = nullptr, *_sct = nullptr, *_header = nullptr;
  const FxComputeShader *_cs_mark = nullptr, *_cs_vert = nullptr, *_cs_topo = nullptr;
  int _capv = -1, _capf = -1, _capc = -1;
};
const MeshChannel MirrorInst::kCh[5] = {
    MeshChannel::POSITION, MeshChannel::NORMAL, MeshChannel::BINORMAL, MeshChannel::UV0, MeshChannel::COLOR};

static void _reshapeMirrorIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
MirrorData::MirrorData() {
}
std::shared_ptr<MirrorData> MirrorData::createShared() {
  auto d = std::make_shared<MirrorData>();
  _reshapeMirrorIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t MirrorData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<MirrorInst>(this, g);
}
void MirrorData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return MirrorData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeMirrorIOs(m); });
  clazz->directProperty("axis", &MirrorData::_axis);
  clazz->directProperty("weld", &MirrorData::_weld);
  clazz->directProperty("eps", &MirrorData::_eps);
}

} // namespace ork::lev2::hypermesh
