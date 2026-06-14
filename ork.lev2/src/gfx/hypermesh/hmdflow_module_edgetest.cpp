////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"

ImplementReflectionX(ork::lev2::hypermesh::EdgeTestData, "hypermesh::EdgeTestData");

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////
// EdgeTest — a REGRESSION HARNESS for MeshEdges (NOT a modeling op). It enumerates the input mesh's
// unique edges on the GPU, then writes one OUTPUT vertex per edge slot encoding (va, vb, count) in XYZ;
// slots past the real edge count get a (-1,-1,-1) sentinel. dump_obj reads them back: filter sentinels,
// and the survivors are the edge table. A welded cube => 12 edges, all count=2; a raw (split-vert) box
// => 24 edges, all count=1 (the split topology shares no vertex index — correct, and a boundary test).
///////////////////////////////////////////////////////////////////////////////

static std::string _writeedges_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface wf_ct (descriptor_set 0) { buffer layout(std430) wctb { uint e_nc; uint e_nf; uint e_ne; uint e_s3; }; }
storage_interface wf_ed (descriptor_set 0) { buffer layout(std430) wedb { uint EDGE[]; }; }
storage_interface wf_et (descriptor_set 0) { buffer layout(std430) wetb { uint ETAG[]; }; }   // edge __tags (0 if none)
storage_interface wf_oP (descriptor_set 0) { buffer layout(std430) wopb { vec4 oP[]; }; }
storage_interface wf_oN (descriptor_set 0) { buffer layout(std430) wonb { vec4 oN[]; }; }
storage_interface wf_oB (descriptor_set 0) { buffer layout(std430) wobb { vec4 oB[]; }; }
storage_interface wf_ou (descriptor_set 0) { buffer layout(std430) woub { vec4 oUV[]; }; }
storage_interface wf_oc (descriptor_set 0) { buffer layout(std430) wocb { vec4 oC[]; }; }
compute_interface ifc { storage { wf_ct wf_ed wf_et wf_oP wf_oN wf_oB wf_ou wf_oc } inputs { layout(local_size_x = 64); } }
compute_shader cs_writeedges : ifc {
  uint i = gl_GlobalInvocationID.x;
  if (i >= e_nc) { return; }                          // e_nc = #edge SLOTS (= input corner count, upper bound)
  if (i < e_ne) {                                     // a real edge
    uint va = EDGE[i * 4u + 0u]; uint vb = EDGE[i * 4u + 1u]; uint f1 = EDGE[i * 4u + 3u];
    float count = (f1 == 0xFFFFFFFFu) ? 1.0 : 2.0;
    float sel   = float(ETAG[i] & 1u);                // group-0 selection bit, encoded into z (+10) for OBJ read-back
    oP[i] = vec4(float(va), float(vb), count + 10.0 * sel, 1.0);
  } else {
    oP[i] = vec4(-1.0, -1.0, -1.0, 1.0);              // sentinel for an unused slot
  }
  oN[i] = vec4(0.0, 0.0, 1.0, 0.0); oB[i] = vec4(1.0, 0.0, 0.0, 0.0);
  oUV[i] = vec4(0.0); oC[i] = vec4(1.0);
}
)S";
}

struct EdgeTestInst : public MeshComputeInst {
  EdgeTestInst(const EdgeTestData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _input  = typedInputNamed<MeshPlugTraits>("In");
  }
  bool onTopologyReady(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in or _built) return false;
    auto env = _graphinst->_impl.getShared<MeshEnv>();
    auto fxi = ctx->FXI();
    _nc = in->_num_corners;                            // edge slots = corner upper bound (CPU-side, no readback)
    int nf = std::max(1, _nc / 3);
    auto out = _output->_value;
    allocMesh(env, out, _nc, _nc, nf,
              {MeshChannel::POSITION, MeshChannel::NORMAL, MeshChannel::BINORMAL, MeshChannel::UV0, MeshChannel::COLOR});
    // fixed output topology (identity vidx + triangle CSR) so dump_obj emits every slot as a vertex.
    std::vector<uint32_t> vid(_nc), fo(nf + 1);
    for (int i = 0; i < _nc; i++) vid[i] = uint32_t(i);
    for (int f = 0; f <= nf; f++) fo[f] = uint32_t(std::min(f * 3, _nc));
    auto up = [&](FxShaderStorageBuffer* b, const std::vector<uint32_t>& v) {
      auto m = fxi->mapStorageBuffer(b, 0, v.size() * 4, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, v.data(), v.size() * 4); fxi->unmapStorageBuffer(m.get());
    };
    up(out->_vidx->_ssbo, vid); up(out->_face_offsets->_ssbo, fo);
    _ztags = fxi->createStorageBuffer(size_t(_nc) * 4);   // zero edge-tag fallback (input had no LINE select)
    std::vector<uint32_t> zt(_nc, 0u);
    { auto m = fxi->mapStorageBuffer(_ztags, 0, size_t(_nc) * 4, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, zt.data(), size_t(_nc) * 4); fxi->unmapStorageBuffer(m.get()); }
    _edges.init(ctx);
    _cs_write = fxi->computeShader(fxi->shaderFromShaderText("hmedge_writeedges", _writeedges_text()), "cs_writeedges");
    _built = true;
    return true;
  }
  void writeParams(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in or not _built) return;
    _edges.ensure(ctx, in->_num_corners, in->_num_faces);
    auto out = _output->_value;
    out->_num_verts = _nc; out->_num_corners = _nc; out->_num_faces = std::max(1, _nc / 3);
    uint32_t hdr[4] = {uint32_t(_nc), uint32_t(_nc), uint32_t(std::max(1, _nc / 3)), 0u};
    auto mh = ctx->FXI()->mapStorageBuffer(out->_header, 0, sizeof(hdr), BufferMapAccess::WRITE_ONLY);
    std::memcpy(mh->_mappedaddr, hdr, sizeof(hdr)); ctx->FXI()->unmapStorageBuffer(mh.get());
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto in  = _srcMesh(_input);
    if (not in or not _built) return;
    auto env = inst->_impl.getShared<MeshEnv>();
    auto ci  = env->_ctx->CI();
    auto out = _output->_value;
    _edges.build(env->_ctx, in);
    auto etag = in->edge("__tags");                          // upstream edge selection (if any)
    ci->bindStorageBuffer(_cs_write, 0, _edges._ectl);
    ci->bindStorageBuffer(_cs_write, 1, _edges._edge);
    ci->bindStorageBuffer(_cs_write, 2, etag ? etag->_ssbo : _ztags);
    ci->bindStorageBuffer(_cs_write, 3, out->channel(MeshChannel::POSITION)->_ssbo);
    ci->bindStorageBuffer(_cs_write, 4, out->channel(MeshChannel::NORMAL)->_ssbo);
    ci->bindStorageBuffer(_cs_write, 5, out->channel(MeshChannel::BINORMAL)->_ssbo);
    ci->bindStorageBuffer(_cs_write, 6, out->channel(MeshChannel::UV0)->_ssbo);
    ci->bindStorageBuffer(_cs_write, 7, out->channel(MeshChannel::COLOR)->_ssbo);
    ci->dispatchCompute(_cs_write, (_nc + 63) / 64, 1, 1);
    ci->storageBarrier();
  }
  const EdgeTestData* _d;
  mesh_outpluginst_ptr_t _output;
  mesh_inpluginst_ptr_t _input;
  MeshEdges _edges;
  FxShaderStorageBuffer* _ztags = nullptr;
  const FxComputeShader* _cs_write = nullptr;
  int _nc = 0;
  bool _built = false;
};

static void _reshapeEdgeTestIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
EdgeTestData::EdgeTestData() {}
std::shared_ptr<EdgeTestData> EdgeTestData::createShared() {
  auto d = std::make_shared<EdgeTestData>();
  _reshapeEdgeTestIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t EdgeTestData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<EdgeTestInst>(this, g);
}
void EdgeTestData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return EdgeTestData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeEdgeTestIOs(m); });
}

} // namespace ork::lev2::hypermesh
