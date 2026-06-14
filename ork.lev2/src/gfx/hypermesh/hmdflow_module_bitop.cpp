////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"

ImplementReflectionX(ork::lev2::hypermesh::BitOpData, "hypermesh::BitOpData");

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////
// BitOp — __tags BIT-BANKING. mesh -> mesh passthrough that rewrites a width-bit destination band of
// the uint32 `__tags` FACE channel from one/two source bands (per element). The band offsets + op +
// width are ALL RUNTIME (ctl SSBO, re-read every writeParams) — the shell is compiled ONCE and never
// recompiles when the banking parameters change (tweak/animate them live). Requires an upstream Select
// (an existing __tags channel); a no-op passthrough otherwise.
///////////////////////////////////////////////////////////////////////////////

static const char* _bitop_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface sif_tag (descriptor_set 0) { buffer layout(std430) tgb { uint TAGd[]; }; }   // rmw
storage_interface sif_ctl (descriptor_set 0) { buffer layout(std430) ctb {
  uint p_count; uint p_dst; uint p_a; uint p_b; uint p_op; uint p_width; uint p_pad0; uint p_pad1; }; }
compute_interface iface { storage { sif_tag sif_ctl }
                          inputs { layout(local_size_x = 64); } }
compute_shader cs_bitop : iface {
  uint e = gl_GlobalInvocationID.x;
  if (e >= p_count) { return; }
  uint t  = TAGd[e];
  uint wm = (p_width >= 32u) ? 0xFFFFFFFFu : ((1u << p_width) - 1u);   // band mask (runtime width)
  uint sa = (t >> p_a) & wm;
  uint sb = (t >> p_b) & wm;
  uint res;
  if      (p_op == 0u) { res = sa; }              // COPY a
  else if (p_op == 1u) { res = (~sa) & wm; }      // NOT  a
  else if (p_op == 2u) { res = sa & sb; }         // AND
  else if (p_op == 3u) { res = sa | sb; }         // OR
  else                 { res = sa ^ sb; }         // XOR
  uint dm = (wm << p_dst) & 0x000FFFFFu;          // destination band, clamped to the free region (gid [20:32) LOCKED)
  TAGd[e] = (t & (~dm)) | (((res & wm) << p_dst) & dm);
}
)S";
}

struct BitOpInst : public MeshComputeInst {
  BitOpInst(const BitOpData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _input  = typedInputNamed<MeshPlugTraits>("In");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<MeshEnv>();
    auto fxi = env->_ctx->FXI();
    _ctl = fxi->createStorageBuffer(32);   // count + dst/a/b/op/width + pad (all RUNTIME)
    auto sh = fxi->shaderFromShaderText("hypermesh_bitop", _bitop_text());
    _cs     = fxi->computeShader(sh, "cs_bitop");
  }
  void writeParams(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in) return;
    auto out = _output->_value;
    // mesh PASSTHROUGH; the __tags band is read-modify-written in place (COW alias the input's channel).
    out->_channels     = in->_channels;
    out->_faces        = in->_faces;
    out->_vidx         = in->_vidx;
    out->_face_offsets = in->_face_offsets;
    out->_header       = in->_header;
    out->_capacity     = in->_capacity;
    out->_num_verts    = in->_num_verts;
    out->_num_corners  = in->_num_corners;
    out->_num_faces    = in->_num_faces;
    _tags = in->face("__tags");   // requires an upstream Select; else null -> compute() no-ops
    // gid contract: the destination band must stay inside the free region [0:20) — bits [20:32)
    // are the LOCKED gid, writable only by assign_gid. Fail loud, never silently truncate.
    if ((_d->_dst & 31) + _d->_width > 20) {
      printf("hypermesh::BitOp FATAL: dst=%d width=%d crosses into the gid bits [20:32) "
             "(dst+width must be <= 20; gid is writable only via assign_gid)\n", _d->_dst, _d->_width);
      OrkAssert(false);
    }
    uint32_t ctl[8] = {uint32_t(in->_num_faces), uint32_t(_d->_dst & 31), uint32_t(_d->_a & 31),
                       uint32_t(_d->_b & 31), uint32_t(_d->_op), uint32_t(_d->_width), 0u, 0u};
    auto m = ctx->FXI()->mapStorageBuffer(_ctl, 0, sizeof(ctl), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, ctl, sizeof(ctl));
    ctx->FXI()->unmapStorageBuffer(m.get());
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto in = _srcMesh(_input);
    if (not in or not _tags) return;   // no upstream __tags -> passthrough no-op
    auto env = inst->_impl.getShared<MeshEnv>();
    auto ci  = env->_ctx->CI();
    ci->bindStorageBuffer(_cs, 0, _tags->_ssbo);
    ci->bindStorageBuffer(_cs, 1, _ctl);
    ci->dispatchCompute(_cs, (in->_num_faces + 63) / 64, 1, 1);
    ci->storageBarrier();
  }
  const BitOpData* _d;
  mesh_outpluginst_ptr_t _output;
  mesh_inpluginst_ptr_t _input;
  gpuchannel_ptr_t _tags;
  FxShaderStorageBuffer* _ctl = nullptr;
  const FxComputeShader* _cs  = nullptr;
};

static void _reshapeBitOpIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
BitOpData::BitOpData() {
}
std::shared_ptr<BitOpData> BitOpData::createShared() {
  auto d = std::make_shared<BitOpData>();
  _reshapeBitOpIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t BitOpData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<BitOpInst>(this, g);
}
void BitOpData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return BitOpData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeBitOpIOs(m); });
  clazz->directProperty("dst", &BitOpData::_dst);
  clazz->directProperty("a", &BitOpData::_a);
  clazz->directProperty("b", &BitOpData::_b);
  clazz->directProperty("op", &BitOpData::_op);
  clazz->directProperty("width", &BitOpData::_width);
}

} // namespace ork::lev2::hypermesh
