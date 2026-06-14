////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"

ImplementReflectionX(ork::lev2::hypermesh::VertTestData, "hypermesh::VertTestData");

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////
// VertTest — a REGRESSION HARNESS for POINT (vertex) SELECTION (NOT a modeling op). Reads the input
// mesh's VERTEX-domain __tags (written by a POINT Select) and emits one output vertex per input vertex
// encoding (vertid, group-0 selbit, 0) in XYZ. dump_obj reads them back: count the verts with y==1 to
// get the selected-vertex count. (No upstream POINT select -> __tags absent -> all selbit 0.)
///////////////////////////////////////////////////////////////////////////////

static std::string _writeverts_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface vf_ct (descriptor_set 0) { buffer layout(std430) vctb {
  uint v_nv; uint v_nc; uint v_nf; uint v_fl; vec4 v_bbmin; vec4 v_bbmax; }; }   // = mesh header
storage_interface vf_vt (descriptor_set 0) { buffer layout(std430) vvtb { uint VTAG[]; }; }
storage_interface vf_oP (descriptor_set 0) { buffer layout(std430) vopb { vec4 oP[]; }; }
storage_interface vf_oN (descriptor_set 0) { buffer layout(std430) vonb { vec4 oN[]; }; }
storage_interface vf_oB (descriptor_set 0) { buffer layout(std430) vobb { vec4 oB[]; }; }
storage_interface vf_ou (descriptor_set 0) { buffer layout(std430) voub { vec4 oUV[]; }; }
storage_interface vf_oc (descriptor_set 0) { buffer layout(std430) vocb { vec4 oC[]; }; }
compute_interface ifc { storage { vf_ct vf_vt vf_oP vf_oN vf_oB vf_ou vf_oc } inputs { layout(local_size_x = 64); } }
compute_shader cs_writeverts : ifc {
  uint i = gl_GlobalInvocationID.x;
  if (i >= v_nv) { return; }
  float sel = float(VTAG[i] & 1u);
  oP[i] = vec4(float(i), sel, 0.0, 1.0);                 // (vertid, selbit, 0) — read back via dump_obj
  oN[i] = vec4(0.0, 0.0, 1.0, 0.0); oB[i] = vec4(1.0, 0.0, 0.0, 0.0);
  oUV[i] = vec4(0.0); oC[i] = vec4(1.0);
}
)S";
}

struct VertTestInst : public MeshComputeInst {
  VertTestInst(const VertTestData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _input  = typedInputNamed<MeshPlugTraits>("In");
  }
  bool onTopologyReady(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in or _built) return false;
    auto env = _graphinst->_impl.getShared<MeshEnv>();
    auto fxi = ctx->FXI();
    _nv = in->_num_verts;
    int nf = std::max(1, _nv / 3);
    auto out = _output->_value;
    allocMesh(env, out, _nv, _nv, nf,
              {MeshChannel::POSITION, MeshChannel::NORMAL, MeshChannel::BINORMAL, MeshChannel::UV0, MeshChannel::COLOR});
    std::vector<uint32_t> vid(_nv), fo(nf + 1);
    for (int i = 0; i < _nv; i++) vid[i] = uint32_t(i);
    for (int f = 0; f <= nf; f++) fo[f] = uint32_t(std::min(f * 3, _nv));
    auto up = [&](FxShaderStorageBuffer* b, const std::vector<uint32_t>& v) {
      auto m = fxi->mapStorageBuffer(b, 0, v.size() * 4, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, v.data(), v.size() * 4); fxi->unmapStorageBuffer(m.get());
    };
    up(out->_vidx->_ssbo, vid); up(out->_face_offsets->_ssbo, fo);
    _zvtags = fxi->createStorageBuffer(size_t(_nv) * 4);   // zero vertex-tag fallback (input had no POINT select)
    std::vector<uint32_t> zt(_nv, 0u);
    { auto m = fxi->mapStorageBuffer(_zvtags, 0, size_t(_nv) * 4, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, zt.data(), size_t(_nv) * 4); fxi->unmapStorageBuffer(m.get()); }
    _cs = fxi->computeShader(fxi->shaderFromShaderText("hmvert_writeverts", _writeverts_text()), "cs_writeverts");
    _built = true;
    return true;
  }
  void writeParams(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in or not _built) return;
    auto out = _output->_value;
    out->_num_verts = _nv; out->_num_corners = _nv; out->_num_faces = std::max(1, _nv / 3);
    uint32_t hdr[4] = {uint32_t(_nv), uint32_t(_nv), uint32_t(std::max(1, _nv / 3)), 0u};
    auto mh = ctx->FXI()->mapStorageBuffer(out->_header, 0, sizeof(hdr), BufferMapAccess::WRITE_ONLY);
    std::memcpy(mh->_mappedaddr, hdr, sizeof(hdr)); ctx->FXI()->unmapStorageBuffer(mh.get());
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto in  = _srcMesh(_input);
    if (not in or not _built) return;
    if (not in->channel(MeshChannel::POSITION)) return;
    auto env = inst->_impl.getShared<MeshEnv>();
    auto ci  = env->_ctx->CI();
    auto out = _output->_value;
    auto vtag = in->vattr("__tags");                       // upstream POINT selection (if any)
    ci->bindStorageBuffer(_cs, 0, out->_header);
    ci->bindStorageBuffer(_cs, 1, vtag ? vtag->_ssbo : _zvtags);
    ci->bindStorageBuffer(_cs, 2, out->channel(MeshChannel::POSITION)->_ssbo);
    ci->bindStorageBuffer(_cs, 3, out->channel(MeshChannel::NORMAL)->_ssbo);
    ci->bindStorageBuffer(_cs, 4, out->channel(MeshChannel::BINORMAL)->_ssbo);
    ci->bindStorageBuffer(_cs, 5, out->channel(MeshChannel::UV0)->_ssbo);
    ci->bindStorageBuffer(_cs, 6, out->channel(MeshChannel::COLOR)->_ssbo);
    ci->dispatchCompute(_cs, (_nv + 63) / 64, 1, 1);
    ci->storageBarrier();
  }
  const VertTestData* _d;
  mesh_outpluginst_ptr_t _output;
  mesh_inpluginst_ptr_t _input;
  FxShaderStorageBuffer* _zvtags = nullptr;
  const FxComputeShader* _cs = nullptr;
  int _nv = 0;
  bool _built = false;
};

static void _reshapeVertTestIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
VertTestData::VertTestData() {}
std::shared_ptr<VertTestData> VertTestData::createShared() {
  auto d = std::make_shared<VertTestData>();
  _reshapeVertTestIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t VertTestData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<VertTestInst>(this, g);
}
void VertTestData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return VertTestData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeVertTestIOs(m); });
}

} // namespace ork::lev2::hypermesh
