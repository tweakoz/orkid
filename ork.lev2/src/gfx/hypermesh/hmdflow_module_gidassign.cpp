////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"

ImplementReflectionX(ork::lev2::hypermesh::GidAssignData, "hypermesh::GidAssignData");

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////
// GidAssign (E.3) — see hmdflow.h. THE ONLY verb writing the LOCKED gid band
// (__tags [20:32), A1 tag contract): gid = the per-face material/semantic-
// class key the render bucketing partitions on. Every other tag write
// (Select MaskOps, BitOp bands, FaceTagger) is hard-masked to [0:20).
// Mesh passthrough; faces matched by selection bit `slot` ([0:20) from an
// upstream select()) get `gid`; slot < 0 = all faces. No upstream __tags
// channel: with slot < 0 the channel is CREATED (zeroed) so a fresh
// primitive can be gid-partitioned directly; with slot >= 0 a missing
// channel means "nothing selected" -> no-op passthrough.
///////////////////////////////////////////////////////////////////////////////

static const char* _gidassign_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface sif_tag (descriptor_set 0) { buffer layout(std430) tgb { uint TAGd[]; }; }   // rmw
storage_interface sif_ctl (descriptor_set 0) { buffer layout(std430) ctb {
  uint p_count; uint p_gid; uint p_slot; uint p_all; }; }
compute_interface iface { storage { sif_tag sif_ctl }
                          inputs { layout(local_size_x = 64); } }
compute_shader cs_gidassign : iface {
  uint e = gl_GlobalInvocationID.x;
  if (e >= p_count) { return; }
  uint t       = TAGd[e];
  uint matched = (p_all != 0u) ? 1u : ((t >> p_slot) & 1u);
  if (matched != 0u) {
    TAGd[e] = (t & 0x000FFFFFu) | ((p_gid & 0xFFFu) << 20u);
  }
}
)S";
}

struct GidAssignInst : public MeshComputeInst {
  GidAssignInst(const GidAssignData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _input  = typedInputNamed<MeshPlugTraits>("In");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<MeshEnv>();
    auto fxi = env->_ctx->FXI();
    _ctl    = fxi->createStorageBuffer(16); // count + gid + slot + all (all RUNTIME)
    auto sh = fxi->shaderFromShaderText("hypermesh_gidassign", _gidassign_text());
    _cs     = fxi->computeShader(sh, "cs_gidassign");
  }
  void writeParams(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in) return;
    auto env = _graphinst->_impl.getShared<MeshEnv>();
    auto out = _output->_value;
    // mesh PASSTHROUGH; the gid band is read-modify-written in place (COW alias).
    out->_channels     = in->_channels;
    out->_faces        = in->_faces;
    out->_vidx         = in->_vidx;
    out->_face_offsets = in->_face_offsets;
    out->_header       = in->_header;
    out->_capacity     = in->_capacity;
    out->_num_verts    = in->_num_verts;
    out->_num_corners  = in->_num_corners;
    out->_num_faces    = in->_num_faces;
    int nf      = in->_num_faces;
    auto intags = in->face("__tags");
    if (not intags and _d->_slot < 0) {
      // assign-to-ALL on an untagged mesh: CREATE the face channel (zeroed
      // once) so fresh primitives can be gid-partitioned without a select.
      // RE-EVAL IDEMPOTENCE (the topology-cascade re-runs writeParams): when
      // the pow2 class is unchanged, KEEP our previously-created _tags — the
      // old code re-read the (absent) input channel into the member and
      // published NULL, silently no-opping the whole assign on eval 2.
      if (meshNextPow2(nf) != _tagcap) {
        _tags   = env->_pool->acquireChannel(4, nf);
        _tagcap = meshNextPow2(nf);
        _zeroed = false;
      }
      out->_faces["__tags"] = _tags;
    } else {
      _tags = intags; // upstream-owned channel (RMW alias); null -> no-op passthrough
      if (_tags)
        out->_faces["__tags"] = _tags;
    }
    uint32_t ctl[4] = {
        uint32_t(nf), uint32_t(_d->_gid & 0xFFF),
        uint32_t(_d->_slot < 0 ? 0 : (_d->_slot & 31)),
        uint32_t(_d->_slot < 0 ? 1 : 0)};
    auto m = ctx->FXI()->mapStorageBuffer(_ctl, 0, sizeof(ctl), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, ctl, sizeof(ctl));
    ctx->FXI()->unmapStorageBuffer(m.get());
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto in = _srcMesh(_input);
    if (not in or not _tags) return; // slot-selected on an untagged mesh -> no-op
    auto env = inst->_impl.getShared<MeshEnv>();
    auto ci  = env->_ctx->CI();
    if (_tagcap > 0 and not _zeroed) { // OUR fresh channel only (slot<0 created it;
      // an upstream-aliased channel is already initialized): zero once before the
      // first assign (host write pre-submit — the phase submits at endDispatchPhase)
      std::vector<uint32_t> z(_tagcap, 0u);
      auto zm = env->_ctx->FXI()->mapStorageBuffer(_tags->_ssbo, 0, size_t(_tagcap) * 4, BufferMapAccess::WRITE_ONLY);
      std::memcpy(zm->_mappedaddr, z.data(), size_t(_tagcap) * 4);
      env->_ctx->FXI()->unmapStorageBuffer(zm.get());
      _zeroed = true;
    }
    ci->bindStorageBuffer(_cs, 0, _tags->_ssbo);
    ci->bindStorageBuffer(_cs, 1, _ctl);
    ci->dispatchCompute(_cs, (in->_num_faces + 63) / 64, 1, 1);
    ci->storageBarrier();
  }
  const GidAssignData* _d;
  mesh_outpluginst_ptr_t _output;
  mesh_inpluginst_ptr_t _input;
  gpuchannel_ptr_t _tags;
  FxShaderStorageBuffer* _ctl = nullptr;
  const FxComputeShader* _cs  = nullptr;
  int _tagcap  = -1;
  bool _zeroed = false;
};

static void _reshapeGidAssignIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
GidAssignData::GidAssignData() {
}
std::shared_ptr<GidAssignData> GidAssignData::createShared() {
  auto d = std::make_shared<GidAssignData>();
  _reshapeGidAssignIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t GidAssignData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<GidAssignInst>(this, g);
}
void GidAssignData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return GidAssignData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeGidAssignIOs(m); });
  clazz->directProperty("gid", &GidAssignData::_gid);
  clazz->directProperty("slot", &GidAssignData::_slot);
}

} // namespace ork::lev2::hypermesh
