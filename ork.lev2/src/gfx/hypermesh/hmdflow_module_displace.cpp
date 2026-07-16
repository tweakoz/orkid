////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include <ork/lev2/gfx/terrain/dflow/hfdflow.h> // the terrain BakeEnv THIS module stocks for its field subgraph

ImplementReflectionX(ork::lev2::hypermesh::DisplaceByFieldData, "hypermesh::DisplaceByFieldData");

namespace ork::lev2::hypermesh {

// DisplaceByFieldModule (HYPERECS E.1) — the first CROSS-FAMILY graph edge. "In" is a GpuMesh;
// "Field" is the family-neutral GpuComputeImage2D interchange plug, produced by TERRAIN modules
// (fbm / noise / the whole expression algebra) living in the SAME graph (they find their
// terrain::BakeEnv on the GraphInst next to the MeshEnv — TypeKeyedVars). One kernel:
//   cs_displace : per UNIQUE vertex, uv = clamp(P.xz/extent + 0.5) (the terrain planar
//                 convention, centered at the origin), BILINEAR-sample the field, then
//                 P' = P + dir * sample * amount   (dir = vertex normal | world +Y)
// Topology + N/B/uv/color/face attrs PASS THROUGH (alias); only POSITION is produced — chain
// smooth_normals/face_normals to refresh shading. amount/extent are runtime plugs (animate
// free); the field's resolution rides the params SSBO so nothing about the field is baked
// into the shader text. Terrain generators re-dispatch every eval (cheap at field dims; a
// field dirty gate is a later optimization, noted in the plan).
//
// ENV OWNERSHIP: the module that BRINGS another family into a graph carries that family's
// parameters and stocks its env — the generic mesh drivers know nothing about fields. This
// module's onLink ensures a terrain::BakeEnv on the GraphInst (TypeKeyedVars: coexists with
// MeshEnv) sized to its reflected `_field_dim`, BEFORE any terrain generator's onActivate
// allocates (link() for all modules runs before activate() for any — graph_inst.cpp).
// First-stocker wins; a second displace with a DIFFERENT field_dim warns loudly.

static std::string _displace_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface dif_iP (descriptor_set 0) { buffer layout(std430) dipb { vec4 iP[]; }; }
storage_interface dif_iN (descriptor_set 0) { buffer layout(std430) dinb { vec4 iN[]; }; }
storage_interface dif_oP (descriptor_set 0) { buffer layout(std430) dopb { vec4 oP[]; }; }
storage_interface dif_fd (descriptor_set 0) { buffer layout(std430) dfdb { float Fd[]; }; }
storage_interface dif_pm (descriptor_set 0) { buffer layout(std430) dpmb { vec4 PRM[]; }; }   // [0] = (amount, extent, _, _)
storage_interface dif_ct (descriptor_set 0) { buffer layout(std430) dctb {
  uint p_nv; uint p_fw; uint p_fh; uint p_mode; uint p_fch; uint p0; uint p1; uint p2; }; }
compute_interface iface { storage { dif_iP dif_iN dif_oP dif_fd dif_pm dif_ct }
                          inputs { layout(local_size_x = 64); } }
compute_shader cs_displace : iface {
  uint v = gl_GlobalInvocationID.x;
  if (v >= p_nv) { return; }
  vec3 P = iP[v].xyz;
  float amount = PRM[0].x;
  float extent = PRM[0].y;
  vec2 uv = clamp(P.xz / extent + vec2(0.5), vec2(0.0), vec2(1.0));
  float fx = uv.x * float(p_fw - 1u);
  float fy = uv.y * float(p_fh - 1u);
  uint x0 = uint(floor(fx)); uint y0 = uint(floor(fy));
  uint x1 = min(x0 + 1u, p_fw - 1u); uint y1 = min(y0 + 1u, p_fh - 1u);
  float tx = fx - float(x0); float ty = fy - float(y0);
  float h00 = Fd[(y0 * p_fw + x0) * p_fch]; float h10 = Fd[(y0 * p_fw + x1) * p_fch];
  float h01 = Fd[(y1 * p_fw + x0) * p_fch]; float h11 = Fd[(y1 * p_fw + x1) * p_fch];
  float h = mix(mix(h00, h10, tx), mix(h01, h11, tx), ty);
  vec3 dir = (p_mode == 1u) ? vec3(0.0, 1.0, 0.0) : normalize(iN[v].xyz);
  oP[v] = vec4(P + dir * (h * amount), 1.0);
}
)S";
}

// the SOURCE field for an hfimg input = the CONNECTED output's value (mirrors _srcMesh / terrain _srcImg).
static dflowgfx::gpucomputeimage2d_inst_ptr_t _srcField(dflowgfx::hfimg_inpluginst_ptr_t inp) {
  auto out = std::dynamic_pointer_cast<dflowgfx::hfimg_outpluginst_t>(inp->_connectedOutput);
  return out ? out->_value : nullptr;
}

struct DisplaceByFieldInst : public MeshComputeInst {
  DisplaceByFieldInst(const DisplaceByFieldData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}

  void onLink(dflow::GraphInst* ginst) final {
    _output  = typedOutputNamed<MeshPlugTraits>("Out");
    _input   = typedInputNamed<MeshPlugTraits>("In");
    _field   = typedInputNamed<dflowgfx::HfImagePlugTraits>("Field");
    // stock the terrain BakeEnv for the field subgraph (see ENV OWNERSHIP above). Runs
    // before any module's onActivate, so the terrain generators alloc at OUR field_dim.
    auto fenv = ginst->_impl.getShared<terrain::BakeEnv>();
    if (not fenv) {
      auto menv             = ginst->_impl.getShared<MeshEnv>();
      OrkAssert(menv and menv->_ctx);
      fenv                  = std::make_shared<terrain::BakeEnv>();
      fenv->_ctx            = menv->_ctx;
      fenv->_w              = _d->_field_dim;
      fenv->_h              = _d->_field_dim;
      fenv->_extent_m       = float(_d->_field_dim); // 1 texel == 1 m (the terrain selftest convention;
                                                     //  heights are TRUE METERS — the displace mapping scale is the `extent` plug)
      ginst->_impl.setShared<terrain::BakeEnv>(fenv);
    } else if (fenv->_w != _d->_field_dim) {
      printf(
          "DisplaceByField<%s>: field_dim<%d> DIFFERS from the graph's already-stocked field env dim<%d> "
          "(first displace wins; one field resolution per graph)\n",
          _dgmodule_data->_name.c_str(), _d->_field_dim, fenv->_w);
    }
  }

  bool onTopologyReady(Context* ctx) final {
    if (not _srcMesh(_input) or _cs) return false;
    if (not _srcField(_field)) {
      if (not _warned) {
        printf("DisplaceByField<%s>: Field input UNCONNECTED — passing the mesh through "
               "(wire a terrain field, e.g. T.fbm, to displace)\n", _dgmodule_data->_name.c_str());
        _warned = true;
      }
      return false;
    }
    auto sh = ctx->FXI()->shaderFromShaderText("hypermesh_displace", _displace_text());
    _cs     = ctx->FXI()->computeShader(sh, "cs_displace");
    return true; // re-eval so the displaced output is live
  }

  void writeParams(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in) return;
    auto out = _output->_value;
    auto env = _graphinst->_impl.getShared<MeshEnv>();
    auto fxi = ctx->FXI();
    int  nv  = in->_num_verts;
    // pre-compile (eval 1) or fieldless: pass the input straight through.
    auto fld = _srcField(_field);
    if (not _cs or not fld) {
      out->_channels = in->_channels; out->_faces = in->_faces; out->_vattrs = in->_vattrs;
      out->_vidx = in->_vidx; out->_face_offsets = in->_face_offsets; out->_header = in->_header;
      out->_capacity = in->_capacity; out->_num_verts = in->_num_verts;
      out->_num_corners = in->_num_corners; out->_num_faces = in->_num_faces;
      return;
    }
    if (meshNextPow2(nv) != _cap) { // (re)pool the produced POSITION channel
      _outP = env->_pool->acquireChannel(MeshChannel::POSITION, nv);
      _cap  = meshNextPow2(nv);
    }
    if (not _pm) _pm = fxi->createStorageBuffer(16);
    if (not _ct) _ct = fxi->createStorageBuffer(32);
    // runtime params re-read from the DATA plugs (the pokeable channel — m.inputs.amount
    // set from Python/editor lands next frame; the inset/extrude idiom).
    float pm[4] = {*(_d->typedInputNamed<dflow::FloatPlugTraits>("amount")->_value),
                   *(_d->typedInputNamed<dflow::FloatPlugTraits>("extent")->_value), 0.0f, 0.0f};
    auto mp = fxi->mapStorageBuffer(_pm, 0, sizeof(pm), BufferMapAccess::WRITE_ONLY);
    std::memcpy(mp->_mappedaddr, pm, sizeof(pm));
    fxi->unmapStorageBuffer(mp.get());
    int fch = (fld->_channels < 1) ? 1 : fld->_channels;
    uint32_t ct[8] = {uint32_t(nv), uint32_t(fld->_w), uint32_t(fld->_h),
                      uint32_t(_d->_mode), uint32_t(fch), 0, 0, 0};
    auto cp = fxi->mapStorageBuffer(_ct, 0, sizeof(ct), BufferMapAccess::WRITE_ONLY);
    std::memcpy(cp->_mappedaddr, ct, sizeof(ct));
    fxi->unmapStorageBuffer(cp.get());
    // topology + every other channel passes through; only POSITION is produced.
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
    auto in  = _srcMesh(_input);
    auto fld = _srcField(_field);
    if (not in or not fld or not _cs) return;
    auto env = inst->_impl.getShared<MeshEnv>();
    auto ci  = env->_ctx->CI();
    int nv   = in->_num_verts;
    ci->bindStorageBuffer(_cs, 0, in->channel(MeshChannel::POSITION)->_ssbo);
    ci->bindStorageBuffer(_cs, 1, in->channel(MeshChannel::NORMAL)->_ssbo);
    ci->bindStorageBuffer(_cs, 2, _outP->_ssbo);
    ci->bindStorageBuffer(_cs, 3, fld->_ssbo);
    ci->bindStorageBuffer(_cs, 4, _pm);
    ci->bindStorageBuffer(_cs, 5, _ct);
    ci->dispatchCompute(_cs, (nv + 63) / 64, 1, 1);
    ci->storageBarrier();
  }

  const DisplaceByFieldData* _d;
  mesh_outpluginst_ptr_t _output;
  mesh_inpluginst_ptr_t _input;
  dflowgfx::hfimg_inpluginst_ptr_t _field;
  std::map<MeshChannel, gpuchannel_ptr_t> _outch;
  gpuchannel_ptr_t _outP;
  FxShaderStorageBuffer *_pm = nullptr, *_ct = nullptr;
  const FxComputeShader* _cs = nullptr;
  int _cap     = -1;
  bool _warned = false;
};

static void _reshapeDisplaceIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createInputPlug<dflowgfx::HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Field");
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "amount")->setValue(1.0f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "extent")->setValue(8.0f);
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
DisplaceByFieldData::DisplaceByFieldData() {
}
std::shared_ptr<DisplaceByFieldData> DisplaceByFieldData::createShared() {
  auto d = std::make_shared<DisplaceByFieldData>();
  _reshapeDisplaceIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t DisplaceByFieldData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<DisplaceByFieldInst>(this, g);
}
void DisplaceByFieldData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return DisplaceByFieldData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeDisplaceIOs(m); });
  clazz->directProperty("mode", &DisplaceByFieldData::_mode);           // 0 = along vertex normal, 1 = world +Y
  clazz->directProperty("field_dim", &DisplaceByFieldData::_field_dim); // field subgraph bake resolution (module-carried)
}

} // namespace ork::lev2::hypermesh
