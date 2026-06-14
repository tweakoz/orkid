////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"

ImplementReflectionX(ork::lev2::hypermesh::DeleteFacesData, "hypermesh::DeleteFacesData");

namespace ork::lev2::hypermesh {

// DeleteFaces — the first consumer of the shared count→scan→scatter (MeshScan). GPU-native, no readback.
//   cs_count   : per face -> output corner count (deleted face = 0, survivor = its size)
//   MeshScan   : prefix-sum the counts -> output face_offsets CSR (deleted faces become zero-length)
//   cs_scatter : per surviving face -> copy its corners into the packed output vidx at BASE[f]
// num_faces stays = input (the render's cs_tri skips zero-length faces); verts + all channels pass through.

static std::string _delete_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface df_vi (descriptor_set 0) { buffer layout(std430) dvib { uint VID[];  }; }   // in vidx
storage_interface df_fo (descriptor_set 0) { buffer layout(std430) dfob { uint FO[];   }; }   // in face_offsets
storage_interface df_tg (descriptor_set 0) { buffer layout(std430) dtgb { uint TG[];   }; }   // in __tags
storage_interface df_cn (descriptor_set 0) { buffer layout(std430) dcnb { uint CNT[];  }; }   // per-face out corner count
storage_interface df_bs (descriptor_set 0) { buffer layout(std430) dbsb { uint BASE[]; }; }   // scanned -> out face_offsets
storage_interface df_ov (descriptor_set 0) { buffer layout(std430) dovb { uint OVID[]; }; }   // out vidx (packed)
storage_interface df_ct (descriptor_set 0) { buffer layout(std430) dctb { uint p_nf; uint p_bit; uint c2; uint c3; }; }
compute_interface iface { storage { df_vi df_fo df_tg df_cn df_bs df_ov df_ct } inputs { layout(local_size_x = 64); } }
////////////////////////////////////////
compute_shader cs_count : iface {
  uint f = gl_GlobalInvocationID.x;
  if (f >= p_nf) { return; }
  uint sz = FO[f + 1u] - FO[f];
  CNT[f] = ((TG[f] & p_bit) != 0u) ? 0u : sz;       // deleted -> 0 corners; survivor -> its size
}
////////////////////////////////////////
compute_shader cs_scatter : iface {
  uint f = gl_GlobalInvocationID.x;
  if (f >= p_nf) { return; }
  if ((TG[f] & p_bit) != 0u) { return; }            // deleted -> emit nothing
  uint a = FO[f]; uint sz = FO[f + 1u] - a; uint dst = BASE[f];
  for (uint i = 0u; i < sz; i++) { OVID[dst + i] = VID[a + i]; }
}
)S";
}

struct DeleteFacesInst : public MeshComputeInst {
  DeleteFacesInst(const DeleteFacesData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _input  = typedInputNamed<MeshPlugTraits>("In");
  }
  bool onTopologyReady(Context* ctx) final {
    if (not _srcMesh(_input) or _cs_count) return false;
    auto sh    = ctx->FXI()->shaderFromShaderText("hypermesh_delete", _delete_text());
    _cs_count  = ctx->FXI()->computeShader(sh, "cs_count");
    _cs_scat   = ctx->FXI()->computeShader(sh, "cs_scatter");
    _scan.init(ctx);
    return true;
  }
  void passthru(gpumesh_ptr_t in) {
    auto out = _output->_value;
    out->_channels = in->_channels; out->_faces = in->_faces; out->_vidx = in->_vidx;
    out->_face_offsets = in->_face_offsets; out->_header = in->_header; out->_capacity = in->_capacity;
    out->_num_verts = in->_num_verts; out->_num_corners = in->_num_corners; out->_num_faces = in->_num_faces;
  }
  void writeParams(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in) return;
    auto tg = in->face("__tags");
    _active = _cs_count and tg;                       // no selection channel -> nothing to delete -> passthrough
    if (not _active) { passthru(in); return; }
    auto env = _graphinst->_impl.getShared<MeshEnv>();
    auto fxi = ctx->FXI();
    int  nf  = in->_num_faces, nc = in->_num_corners;
    if (meshNextPow2(nf) != _capf) {
      _cnt = env->_pool->acquireChannel(4, nf);
      _ofo = env->_pool->acquireChannel(4, nf + 1);
      _capf = meshNextPow2(nf);
    }
    if (meshNextPow2(nc) != _capc) { _ovidx = env->_pool->acquireChannel(4, std::max(1, nc)); _capc = meshNextPow2(nc); }
    if (not _ct)  _ct  = fxi->createStorageBuffer(16);
    if (not _sct) _sct = fxi->createStorageBuffer(16);
    uint32_t ct[4] = {uint32_t(nf), 1u << (_d->_slot & 31), 0u, 0u};
    auto m = fxi->mapStorageBuffer(_ct, 0, sizeof(ct), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, ct, sizeof(ct)); fxi->unmapStorageBuffer(m.get());
    uint32_t sct[4] = {uint32_t(nf), 0u, 0u, 0u};     // p_n for the scan
    auto ms = fxi->mapStorageBuffer(_sct, 0, sizeof(sct), BufferMapAccess::WRITE_ONLY);
    std::memcpy(ms->_mappedaddr, sct, sizeof(sct)); fxi->unmapStorageBuffer(ms.get());
    auto out = _output->_value;
    out->_channels     = in->_channels;               // verts unchanged -> alias all vertex channels
    out->_faces        = in->_faces;                  // face attrs ride along (count unchanged)
    out->_vidx         = _ovidx;                      // packed surviving corners
    out->_face_offsets = _ofo;                        // the scan output IS the CSR (deleted -> zero-length)
    out->_header       = in->_header;
    out->_capacity     = in->_capacity;
    out->_num_verts    = in->_num_verts;
    out->_num_corners  = in->_num_corners;            // overestimate (valid corners = face_offsets[nf]); CSR-safe
    out->_num_faces    = in->_num_faces;
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    if (not _active) return;
    auto in  = _srcMesh(_input);
    auto env = inst->_impl.getShared<MeshEnv>();
    auto ci  = env->_ctx->CI();
    auto tg  = in->face("__tags");
    int  nf  = in->_num_faces;
    auto bind = [&](const FxComputeShader* cs) {
      ci->bindStorageBuffer(cs, 0, in->_vidx->_ssbo);
      ci->bindStorageBuffer(cs, 1, in->_face_offsets->_ssbo);
      ci->bindStorageBuffer(cs, 2, tg->_ssbo);
      ci->bindStorageBuffer(cs, 3, _cnt->_ssbo);
      ci->bindStorageBuffer(cs, 4, _ofo->_ssbo);
      ci->bindStorageBuffer(cs, 5, _ovidx->_ssbo);
      ci->bindStorageBuffer(cs, 6, _ct);
    };
    bind(_cs_count);   ci->dispatchCompute(_cs_count, (nf + 63) / 64, 1, 1);
    ci->storageBarrier();
    _scan.scan(env->_ctx, _cnt->_ssbo, _ofo->_ssbo, _sct);   // CNT -> face_offsets CSR
    ci->storageBarrier();
    bind(_cs_scat);    ci->dispatchCompute(_cs_scat, (nf + 63) / 64, 1, 1);
    ci->storageBarrier();
  }
  const DeleteFacesData* _d;
  mesh_outpluginst_ptr_t _output;
  mesh_inpluginst_ptr_t _input;
  MeshScan _scan;
  gpuchannel_ptr_t _cnt, _ofo, _ovidx;
  FxShaderStorageBuffer *_ct = nullptr, *_sct = nullptr;
  const FxComputeShader *_cs_count = nullptr, *_cs_scat = nullptr;
  int _capf = -1, _capc = -1;
  bool _active = false;
};

static void _reshapeDeleteIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
DeleteFacesData::DeleteFacesData() {
}
std::shared_ptr<DeleteFacesData> DeleteFacesData::createShared() {
  auto d = std::make_shared<DeleteFacesData>();
  _reshapeDeleteIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t DeleteFacesData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<DeleteFacesInst>(this, g);
}
void DeleteFacesData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return DeleteFacesData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeDeleteIOs(m); });
  clazz->directProperty("slot", &DeleteFacesData::_slot);
}

} // namespace ork::lev2::hypermesh
