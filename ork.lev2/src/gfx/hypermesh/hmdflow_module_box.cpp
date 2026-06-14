////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"

ImplementReflectionX(ork::lev2::hypermesh::BoxData, "hypermesh::BoxData");

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////
// BoxModule (v2, INDEXED) — a cube as 6 QUAD faces. Verts are SPLIT per face (24 = 6*4) so each
// face carries its own flat normal (no smoothing across the hard cube edges); vidx is the identity
// (each corner -> its own vert). face_offsets is the all-quad CSR [0,4,8,...,24]; the render fan-
// triangulates each quad. Demonstrates BOTH mixed-capable topology (quads, not tris) AND a per-FACE
// attribute: `material_id` (one uint / quad) lives in the FACE domain. float plug `size` = half-extent.
///////////////////////////////////////////////////////////////////////////////

static std::string _box_text(int vcap, int ccap, int fcap, float size) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface sif_hdr (descriptor_set 0) { buffer layout(std430) hb {
  uint num_verts; uint num_corners; uint num_faces; uint flags; vec4 bbmin; vec4 bbmax; }; }
storage_interface sif_P   (descriptor_set 0) { buffer layout(std430) pb  { vec4 Pd[%VCAP%];  }; }
storage_interface sif_N   (descriptor_set 0) { buffer layout(std430) nb  { vec4 Nd[%VCAP%];  }; }
storage_interface sif_B   (descriptor_set 0) { buffer layout(std430) bb2 { vec4 Bd[%VCAP%];  }; }
storage_interface sif_uv  (descriptor_set 0) { buffer layout(std430) ub  { vec4 UVd[%VCAP%]; }; }
storage_interface sif_clr (descriptor_set 0) { buffer layout(std430) cb  { vec4 Cd[%VCAP%];  }; }
storage_interface sif_vi  (descriptor_set 0) { buffer layout(std430) vib { uint VId[%CCAP%]; }; }   // corner->vert
storage_interface sif_fo  (descriptor_set 0) { buffer layout(std430) fob { uint FOd[%FCAP%]; }; }   // CSR offsets
storage_interface sif_mat (descriptor_set 0) { buffer layout(std430) mab { uint MAT[%FCAP%]; }; }   // FACE attr
compute_interface iface { storage { sif_hdr sif_P sif_N sif_B sif_uv sif_clr sif_vi sif_fo sif_mat }
                          inputs { layout(local_size_x = 64); } }
////////////////////////////////////////
// pass 0: header + per-FACE topology/attrs (thread 0; only 6 faces -> a tiny serial loop is fine).
compute_shader cs_setup : iface {
  if (gl_GlobalInvocationID.x != 0u) { return; }
  num_verts = 24u; num_corners = 24u; num_faces = 6u; flags = 0u;
  float S = %SIZE%;
  bbmin = vec4(-S, -S, -S, 1.0); bbmax = vec4(S, S, S, 1.0);
  for (uint f = 0u; f < 6u; f++) { FOd[f] = f * 4u; MAT[f] = f; }   // quad CSR + face material id
  FOd[6u] = 24u;                                                    // terminal offset
}
////////////////////////////////////////
// pass 1: verts (one thread / face-corner, 24) — per-face basis + identity vidx.
compute_shader cs_gen : iface {
  uint i = gl_GlobalInvocationID.x;
  if (i >= 24u) { return; }
  uint q = i / 4u; uint c = i % 4u;              // face 0..5, quad corner 0..3 (CCW)
  vec3 n; vec3 u; vec3 v;                          // per-face basis, cross(u,v)=n (outward)
  if      (q == 0u) { n = vec3( 1,0,0); u = vec3(0,1,0); v = vec3(0,0,1); }  // +x
  else if (q == 1u) { n = vec3(-1,0,0); u = vec3(0,0,1); v = vec3(0,1,0); }  // -x
  else if (q == 2u) { n = vec3(0, 1,0); u = vec3(0,0,1); v = vec3(1,0,0); }  // +y
  else if (q == 3u) { n = vec3(0,-1,0); u = vec3(1,0,0); v = vec3(0,0,1); }  // -y
  else if (q == 4u) { n = vec3(0,0, 1); u = vec3(1,0,0); v = vec3(0,1,0); }  // +z
  else              { n = vec3(0,0,-1); u = vec3(0,1,0); v = vec3(1,0,0); }  // -z
  float cu = (c == 1u || c == 2u) ? 1.0 : 0.0;
  float cv = (c == 2u || c == 3u) ? 1.0 : 0.0;
  float su = cu * 2.0 - 1.0; float sv = cv * 2.0 - 1.0;
  float S = %SIZE%;
  vec3 pos = (n + u * su + v * sv) * S;
  Pd[i]  = vec4(pos, 1.0);
  Nd[i]  = vec4(n, 0.0);
  Bd[i]  = vec4(u, 0.0);
  UVd[i] = vec4(cu, cv, 0.0, 0.0);
  Cd[i]  = vec4(0.5 + 0.5 * n, 1.0);             // tint by face normal (so the 6 faces read distinctly)
  VId[i] = i;                                     // identity: each corner is its own (split) vertex
}
)S";
  _shadersub(t, "%VCAP%", FormatString("%d", vcap));
  _shadersub(t, "%CCAP%", FormatString("%d", ccap));
  _shadersub(t, "%FCAP%", FormatString("%d", fcap));
  _shadersub(t, "%SIZE%", FormatString("%f", size));
  return t;
}

struct BoxModuleInst : public MeshComputeInst {
  BoxModuleInst(const BoxData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _size   = _floatPlug(this, _d, "size");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto env  = inst->_impl.getShared<MeshEnv>();
    auto fxi  = env->_ctx->FXI();
    int vcap  = meshNextPow2(24);
    int ccap  = meshNextPow2(24);
    int fcap  = meshNextPow2(6 + 1);
    auto mesh = _output->_value;
    allocMesh(env, mesh, 24, 24, 6,
              {MeshChannel::POSITION, MeshChannel::NORMAL, MeshChannel::BINORMAL,
               MeshChannel::UV0, MeshChannel::COLOR});
    mesh->_faces["material_id"] = env->_pool->acquireChannel(4, 6); // FACE-domain attr (uint / quad)
    auto sh   = fxi->shaderFromShaderText("hypermesh_box", _box_text(vcap, ccap, fcap, _size->value()));
    _cs_setup = fxi->computeShader(sh, "cs_setup");
    _cs_gen   = fxi->computeShader(sh, "cs_gen");
  }
  void writeParams(Context* ctx) final {
    tagFacesConst(ctx, _output->_value, _d->_mask);  // optional whole-mesh __tags (runtime; no-op if none)
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env  = inst->_impl.getShared<MeshEnv>();
    auto ci   = env->_ctx->CI();
    auto mesh = _output->_value;
    auto bind = [&](const FxComputeShader* cs) {
      ci->bindStorageBuffer(cs, 0, mesh->_header);
      ci->bindStorageBuffer(cs, 1, mesh->channel(MeshChannel::POSITION)->_ssbo);
      ci->bindStorageBuffer(cs, 2, mesh->channel(MeshChannel::NORMAL)->_ssbo);
      ci->bindStorageBuffer(cs, 3, mesh->channel(MeshChannel::BINORMAL)->_ssbo);
      ci->bindStorageBuffer(cs, 4, mesh->channel(MeshChannel::UV0)->_ssbo);
      ci->bindStorageBuffer(cs, 5, mesh->channel(MeshChannel::COLOR)->_ssbo);
      ci->bindStorageBuffer(cs, 6, mesh->_vidx->_ssbo);
      ci->bindStorageBuffer(cs, 7, mesh->_face_offsets->_ssbo);
      ci->bindStorageBuffer(cs, 8, mesh->face("material_id")->_ssbo);
    };
    bind(_cs_setup);
    ci->dispatchCompute(_cs_setup, 1, 1, 1);
    ci->storageBarrier();
    bind(_cs_gen);
    ci->dispatchCompute(_cs_gen, 1, 1, 1);  // 24 verts < 64
    ci->storageBarrier();
  }
  const BoxData* _d;
  mesh_outpluginst_ptr_t _output;
  dflow::float_inp_pluginst_ptr_t _size;
  const FxComputeShader* _cs_setup = nullptr;
  const FxComputeShader* _cs_gen   = nullptr;
};

static void _reshapeBoxIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "size")->setValue(1.0f);
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
BoxData::BoxData() {
}
std::shared_ptr<BoxData> BoxData::createShared() {
  auto d = std::make_shared<BoxData>();
  _reshapeBoxIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t BoxData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<BoxModuleInst>(this, g);
}
void BoxData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return BoxData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeBoxIOs(m); });
  clazz->directVectorProperty("mask", &BoxData::_mask);
}

} // namespace ork::lev2::hypermesh
