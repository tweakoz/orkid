////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"

ImplementReflectionX(ork::lev2::hypermesh::SelectData, "hypermesh::SelectData");

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////
// Select — SELECTION AS A TAG CHANNEL. mesh -> mesh passthrough that evaluates the SelExpr predicate
// (DSL-traced GLSL `_predicate` assigning `float _sel`, reading per-POLY locals fP/fN/fArea + the
// element's existing `_tags`) and applies the MaskOp TWO-TRIPLE transform to the uint32 `__tags`
// FACE channel: matched faces get the sel-triple, unmatched the unsel-triple. 32 boolean named groups
// per domain; the mask rides the mesh + recomputes each frame (animates with the mesh). Domain v1 = POLY.
///////////////////////////////////////////////////////////////////////////////

static std::string _select_text(const std::string& predicate) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface sif_P   (descriptor_set 0) { buffer layout(std430) pb  { vec4 Pd[];  }; }
storage_interface sif_vi  (descriptor_set 0) { buffer layout(std430) vib { uint VId[]; }; }
storage_interface sif_fo  (descriptor_set 0) { buffer layout(std430) fob { uint FOd[]; }; }
storage_interface sif_tag (descriptor_set 0) { buffer layout(std430) tgb { uint TAGd[]; }; }   // 32 groups/face (rmw)
storage_interface sif_ctl (descriptor_set 0) { buffer layout(std430) ctb {
  uint p_count; uint SA; uint SO; uint SX; uint UA; uint UO; uint UX; uint p_pad; }; }
compute_interface iface { storage { sif_P sif_vi sif_fo sif_tag sif_ctl }
                          inputs { layout(local_size_x = 64); } }
compute_shader cs_select : iface {
  uint _eid = gl_GlobalInvocationID.x;
  if (_eid >= p_count) { return; }
  uint a = FOd[_eid]; uint b = FOd[_eid + 1u]; uint n = b - a;
  vec3 fP = vec3(0.0);                                  // face centroid
  for (uint k = 0u; k < n; k++) { fP += Pd[VId[a + k]].xyz; }
  fP = fP / float(n);
  vec3 q0 = Pd[VId[a]].xyz; vec3 q1 = Pd[VId[a + 1u]].xyz; vec3 q2 = Pd[VId[a + 2u]].xyz;
  vec3 fN = normalize(cross(q1 - q0, q2 - q0));         // face normal (first-tri)
  float fArea = 0.0;
  for (uint k = 1u; k + 1u < n; k++) {
    vec3 e1 = Pd[VId[a + k]].xyz - q0; vec3 e2 = Pd[VId[a + k + 1u]].xyz - q0;
    fArea += 0.5 * length(cross(e1, e2));
  }
  uint _tags = TAGd[_eid];
  float _sel = 0.0;
  %PRED%
  uint selB = (_sel > 0.001) ? 0xFFFFFFFFu : 0u;
  uint hit  = ((_tags & SA) | SO) ^ SX;                 // matched-face triple
  uint miss = ((_tags & UA) | UO) ^ UX;                 // unmatched-face triple
  uint nt   = (selB & hit) | ((~selB) & miss);
  TAGd[_eid] = (_tags & 0xFFF00000u) | (nt & 0x000FFFFFu);  // gid [20:32) LOCKED (face domain only)
}
)S";
  _shadersub(t, "%PRED%", predicate);
  return t;
}

// LINE (edge) domain: one thread per unique edge (from MeshEdges' EDGE table). Pre-computes the per-edge
// locals lP (midpoint), lLen (length), lDihedral (angle between the edge's two face normals; boundary=0),
// then evaluates the predicate and RMW's the EDGE-domain __tags. Early-out past the live edge count (EC.e_ne).
static std::string _select_line_text(const std::string& predicate) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface lif_P   (descriptor_set 0) { buffer layout(std430) lpb  { vec4 Pd[];  }; }
storage_interface lif_vi  (descriptor_set 0) { buffer layout(std430) lvib { uint VId[]; }; }
storage_interface lif_fo  (descriptor_set 0) { buffer layout(std430) lfob { uint FOd[]; }; }
storage_interface lif_ed  (descriptor_set 0) { buffer layout(std430) ledb { uint EDGE[]; }; }       // va,vb,f0,f1
storage_interface lif_tag (descriptor_set 0) { buffer layout(std430) ltgb { uint ETAGd[]; }; }      // edge __tags (rmw)
storage_interface lif_ec  (descriptor_set 0) { buffer layout(std430) lecb { uint e_nc; uint e_nf; uint e_ne; uint e_s3; }; }
storage_interface lif_ctl (descriptor_set 0) { buffer layout(std430) lctb {
  uint p_cap; uint SA; uint SO; uint SX; uint UA; uint UO; uint UX; uint p_pad; }; }
storage_interface lif_ft  (descriptor_set 0) { buffer layout(std430) lftb { uint FTAGd[]; }; }       // FACE __tags (read; for S.ftag)
compute_interface iface { storage { lif_P lif_vi lif_fo lif_ed lif_tag lif_ec lif_ctl lif_ft }
                          inputs { layout(local_size_x = 64); } }
compute_shader cs_select_line : iface {
  uint _eid = gl_GlobalInvocationID.x;
  if (_eid >= p_cap) { return; }
  if (_eid >= e_ne)  { return; }                       // past the live edge count (no readback)
  uint va = EDGE[_eid * 4u + 0u]; uint vb = EDGE[_eid * 4u + 1u];
  uint f0 = EDGE[_eid * 4u + 2u]; uint f1 = EDGE[_eid * 4u + 3u];
  vec3 pa = Pd[va].xyz; vec3 pb = Pd[vb].xyz;
  vec3 lP   = (pa + pb) * 0.5;
  float lLen = length(pb - pa);
  uint a0 = FOd[f0];                                    // f0 face normal (first-tri)
  vec3 n0 = normalize(cross(Pd[VId[a0 + 1u]].xyz - Pd[VId[a0]].xyz, Pd[VId[a0 + 2u]].xyz - Pd[VId[a0]].xyz));
  float lDihedral = 0.0;                                // boundary edge -> 0
  if (f1 != 0xFFFFFFFFu) {
    uint a1 = FOd[f1];
    vec3 n1 = normalize(cross(Pd[VId[a1 + 1u]].xyz - Pd[VId[a1]].xyz, Pd[VId[a1 + 2u]].xyz - Pd[VId[a1]].xyz));
    lDihedral = acos(clamp(dot(n0, n1), -1.0, 1.0));    // radians; 0 = coplanar
  }
  uint _tags = ETAGd[_eid];
  uint _ftags = FTAGd[f0];                              // adjacent FACE group tags (OR of the edge's two faces)
  if (f1 != 0xFFFFFFFFu) { _ftags = _ftags | FTAGd[f1]; }
  float _sel = 0.0;
  %PRED%
  uint selB = (_sel > 0.001) ? 0xFFFFFFFFu : 0u;
  uint hit  = ((_tags & SA) | SO) ^ SX;
  uint miss = ((_tags & UA) | UO) ^ UX;
  ETAGd[_eid] = (selB & hit) | ((~selB) & miss);
}
)S";
  _shadersub(t, "%PRED%", predicate);
  return t;
}

// POINT (vertex) domain: one thread per vertex. Pre-computes vP (position) + vN (normal), evaluates the
// predicate, RMW's the VERTEX-domain __tags. The simplest domain — verts are a stored domain (no enumeration).
static std::string _select_point_text(const std::string& predicate) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface pif_P   (descriptor_set 0) { buffer layout(std430) ppb  { vec4 Pd[]; }; }
storage_interface pif_N   (descriptor_set 0) { buffer layout(std430) pnb  { vec4 Nd[]; }; }
storage_interface pif_tag (descriptor_set 0) { buffer layout(std430) ptgb { uint VTAGd[]; }; }
storage_interface pif_ctl (descriptor_set 0) { buffer layout(std430) pctb {
  uint p_count; uint SA; uint SO; uint SX; uint UA; uint UO; uint UX; uint p_pad; }; }
compute_interface iface { storage { pif_P pif_N pif_tag pif_ctl } inputs { layout(local_size_x = 64); } }
compute_shader cs_select_point : iface {
  uint _eid = gl_GlobalInvocationID.x;
  if (_eid >= p_count) { return; }
  vec3 vP = Pd[_eid].xyz;
  vec3 vN = Nd[_eid].xyz;
  uint _tags = VTAGd[_eid];
  float _sel = 0.0;
  %PRED%
  uint selB = (_sel > 0.001) ? 0xFFFFFFFFu : 0u;
  uint hit  = ((_tags & SA) | SO) ^ SX;
  uint miss = ((_tags & UA) | UO) ^ UX;
  VTAGd[_eid] = (selB & hit) | ((~selB) & miss);
}
)S";
  _shadersub(t, "%PRED%", predicate);
  return t;
}

struct SelectInst : public MeshComputeInst {
  SelectInst(const SelectData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _input  = typedInputNamed<MeshPlugTraits>("In");
  }
  void onActivate(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<MeshEnv>();
    auto fxi = env->_ctx->FXI();
    _ctl = fxi->createStorageBuffer(32);   // count/cap + 6 masks + pad
    std::string pred = _d->_predicate.empty() ? std::string("_sel = 1.0;") : _d->_predicate;
    if (_d->_domain == 2) {                // LINE (edge) domain — enumerate edges via MeshEdges
      _meshedges.init(env->_ctx);
      auto sh = fxi->shaderFromShaderText("hypermesh_select_line", _select_line_text(pred));
      _cs     = fxi->computeShader(sh, "cs_select_line");
    } else if (_d->_domain == 1) {          // POINT (vertex) domain
      auto sh = fxi->shaderFromShaderText("hypermesh_select_point", _select_point_text(pred));
      _cs     = fxi->computeShader(sh, "cs_select_point");
    } else {                                // POLY (face) domain
      auto sh = fxi->shaderFromShaderText("hypermesh_select", _select_text(pred));
      _cs     = fxi->computeShader(sh, "cs_select");
    }
  }
  void writeCtl(Context* ctx, uint32_t count) {            // count/cap + the two MaskOp triples
    uint32_t ctl[8] = {count, _d->_sel_and, _d->_sel_or, _d->_sel_xor,
                       _d->_unsel_and, _d->_unsel_or, _d->_unsel_xor, 0u};
    auto m = ctx->FXI()->mapStorageBuffer(_ctl, 0, sizeof(ctl), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, ctl, sizeof(ctl));
    ctx->FXI()->unmapStorageBuffer(m.get());
  }
  void writeParams(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in) return;
    auto env = _graphinst->_impl.getShared<MeshEnv>();
    auto out = _output->_value;
    // mesh PASSTHROUGH (alias all channels/topology + the edge domain); the selection rides as an added
    // channel in the relevant domain (COPY each map; adding __tags below doesn't touch the input's).
    out->_channels     = in->_channels;
    out->_faces        = in->_faces;
    out->_edges        = in->_edges;
    out->_vidx         = in->_vidx;
    out->_face_offsets = in->_face_offsets;
    out->_header       = in->_header;
    out->_capacity     = in->_capacity;
    out->_num_verts    = in->_num_verts;
    out->_num_corners  = in->_num_corners;
    out->_num_faces    = in->_num_faces;
    out->_edge_table   = in->_edge_table;
    out->_edge_count   = in->_edge_count;
    out->_edge_cap     = in->_edge_cap;
    out->_version      = in->_version;                           // pure passthrough: forward the dirty/topology
    out->_topoVersion  = in->_topoVersion;                       // signal so consumers downstream of select still see it

    if (_d->_domain == 2) {                 // ---- LINE (edge) ----
      int nc = in->_num_corners;            // edge upper bound
      _meshedges.ensure(ctx, nc, in->_num_faces);
      auto in_etags = in->edge("__tags");
      if (in_etags) {                        // chained edge Select: RMW the same band (deterministic edge ids)
        out->_edges["__tags"] = in_etags; _etags = in_etags;
      } else {
        if (meshNextPow2(nc) != _etagcap) {
          _etags = env->_pool->acquireChannel(4, nc); _etagcap = meshNextPow2(nc); _ezeroed = false;
        }
        out->_edges["__tags"] = _etags;
      }
      out->_edge_table = _meshedges._edge;   // THIS select owns the enumeration -> publish it for consumers
      out->_edge_count = _meshedges._ectl;
      out->_edge_cap   = nc;
      writeCtl(ctx, uint32_t(nc));
      return;
    }
    if (_d->_domain == 1) {                 // ---- POINT (vertex) ----
      int nv = in->_num_verts;
      auto in_vtags = in->vattr("__tags");
      if (in_vtags) { out->_vattrs["__tags"] = in_vtags; _vtags = in_vtags; }
      else {
        if (meshNextPow2(nv) != _vtagcap) { _vtags = env->_pool->acquireChannel(4, nv); _vtagcap = meshNextPow2(nv); _vzeroed = false; }
        out->_vattrs["__tags"] = _vtags;
      }
      writeCtl(ctx, uint32_t(nv));
      return;
    }
    // ---- POLY (face) ----
    int nf = in->_num_faces;
    auto in_tags = in->face("__tags");
    if (in_tags) {
      out->_faces["__tags"] = in_tags;        // chained Select: RMW the same band (COW alias)
      _tags = in_tags;
    } else {
      if (meshNextPow2(nf) != _tagcap) {
        _tags   = env->_pool->acquireChannel(4, nf);
        _tagcap = meshNextPow2(nf);
        _zeroed = false;
      }
      out->_faces["__tags"] = _tags;
    }
    writeCtl(ctx, uint32_t(nf));
  }
  // zero a freshly-pooled tag band once (no upstream tags to inherit); RMW preserves it after.
  void zeroBandOnce(Context* ctx, gpuchannel_ptr_t band, int cap, bool& flag) {
    if (flag) return;
    std::vector<uint32_t> z(cap, 0u);
    auto m = ctx->FXI()->mapStorageBuffer(band->_ssbo, 0, size_t(cap) * 4, BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, z.data(), size_t(cap) * 4);
    ctx->FXI()->unmapStorageBuffer(m.get());
    flag = true;
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto in = _srcMesh(_input);
    if (not in) return;
    // input not ready yet (eval-1 runs before a topology-producing upstream's onTopologyReady -> it
    // early-returns from writeParams leaving null topology). Skip; we recompute every frame once valid.
    if (not in->_vidx or not in->_face_offsets or not in->channel(MeshChannel::POSITION)) return;
    auto env = inst->_impl.getShared<MeshEnv>();
    auto ci  = env->_ctx->CI();

    if (_d->_domain == 2) {                 // ---- LINE (edge) ----
      if (not _etags) return;
      if (not in->edge("__tags")) zeroBandOnce(env->_ctx, _etags, _etagcap, _ezeroed);
      _meshedges.build(env->_ctx, in);      // enumerate -> EDGE table + e_ne (GPU)
      ci->storageBarrier();
      ci->bindStorageBuffer(_cs, 0, in->channel(MeshChannel::POSITION)->_ssbo);
      ci->bindStorageBuffer(_cs, 1, in->_vidx->_ssbo);
      ci->bindStorageBuffer(_cs, 2, in->_face_offsets->_ssbo);
      ci->bindStorageBuffer(_cs, 3, _meshedges._edge);
      ci->bindStorageBuffer(_cs, 4, _etags->_ssbo);
      ci->bindStorageBuffer(_cs, 5, _meshedges._ectl);
      ci->bindStorageBuffer(_cs, 6, _ctl);
      auto ftags = in->face("__tags");                      // S.ftag reads adjacent FACE groups; fall back to a
      ci->bindStorageBuffer(_cs, 7, ftags ? ftags->_ssbo : in->_face_offsets->_ssbo);  // bounded dummy if untagged
      ci->dispatchCompute(_cs, (in->_num_corners + 63) / 64, 1, 1);
      ci->storageBarrier();
      return;
    }
    if (_d->_domain == 1) {                 // ---- POINT (vertex) ----
      if (not _vtags) return;
      if (not in->vattr("__tags")) zeroBandOnce(env->_ctx, _vtags, _vtagcap, _vzeroed);
      ci->bindStorageBuffer(_cs, 0, in->channel(MeshChannel::POSITION)->_ssbo);
      ci->bindStorageBuffer(_cs, 1, in->channel(MeshChannel::NORMAL)->_ssbo);
      ci->bindStorageBuffer(_cs, 2, _vtags->_ssbo);
      ci->bindStorageBuffer(_cs, 3, _ctl);
      ci->dispatchCompute(_cs, (in->_num_verts + 63) / 64, 1, 1);
      ci->storageBarrier();
      return;
    }
    // ---- POLY (face) ----
    if (not _tags) return;
    if (not in->face("__tags")) zeroBandOnce(env->_ctx, _tags, _tagcap, _zeroed);
    ci->bindStorageBuffer(_cs, 0, in->channel(MeshChannel::POSITION)->_ssbo);
    ci->bindStorageBuffer(_cs, 1, in->_vidx->_ssbo);
    ci->bindStorageBuffer(_cs, 2, in->_face_offsets->_ssbo);
    ci->bindStorageBuffer(_cs, 3, _tags->_ssbo);
    ci->bindStorageBuffer(_cs, 4, _ctl);
    ci->dispatchCompute(_cs, (in->_num_faces + 63) / 64, 1, 1);
    ci->storageBarrier();
  }
  const SelectData* _d;
  mesh_outpluginst_ptr_t _output;
  mesh_inpluginst_ptr_t _input;
  gpuchannel_ptr_t _tags, _etags, _vtags;
  MeshEdges _meshedges;
  int _tagcap = -1, _etagcap = -1, _vtagcap = -1;
  bool _zeroed = false, _ezeroed = false, _vzeroed = false;
  FxShaderStorageBuffer* _ctl = nullptr;
  const FxComputeShader* _cs  = nullptr;
};

static void _reshapeSelectIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
SelectData::SelectData() {
}
std::shared_ptr<SelectData> SelectData::createShared() {
  auto d = std::make_shared<SelectData>();
  _reshapeSelectIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t SelectData::createInstance(dflow::GraphInst* g) const {
  // serialized-predicate ABI gate: an old asset whose GLSL targets a different shader-shell contract
  // must FAIL LOUDLY here, not mis-compile (see kPredicateABIVersion in hmdflow.h).
  OrkAssert(_predicate_abi == kPredicateABIVersion);
  return std::make_shared<SelectInst>(this, g);
}
void SelectData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return SelectData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeSelectIOs(m); });
  // AUTHORED STATE (serialize exactly what changes the compute). The predicate is the POST-TRACE GLSL —
  // the portable artifact (see kPredicateABIVersion). Without these a deserialized Select selected
  // NOTHING and every downstream op silently ran on an empty set.
  clazz->directProperty("predicate_abi", &SelectData::_predicate_abi);
  clazz->directProperty("predicate", &SelectData::_predicate);
  clazz->directProperty("domain", &SelectData::_domain);
  clazz->directProperty("sel_and", &SelectData::_sel_and);
  clazz->directProperty("sel_or", &SelectData::_sel_or);
  clazz->directProperty("sel_xor", &SelectData::_sel_xor);
  clazz->directProperty("unsel_and", &SelectData::_unsel_and);
  clazz->directProperty("unsel_or", &SelectData::_unsel_or);
  clazz->directProperty("unsel_xor", &SelectData::_unsel_xor);
}

} // namespace ork::lev2::hypermesh
