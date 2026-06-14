////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include <map>
#include <tuple>

ImplementReflectionX(ork::lev2::hypermesh::NormalsData, "hypermesh::NormalsData");

namespace ork::lev2::hypermesh {

// Like a modeling app's "shade flat / shade smooth": the op CHANGES TOPOLOGY so the result is correct
// regardless of how the input was shared:
//   smooth -> WELD coincident verts (merge by position), then per-vertex area-weighted average -> smooth.
//   flat   -> UNWELD (each face gets its own verts), then per-face Newell normal -> faceted.
// Either way the SAME GPU gather computes the result; the sharing is entirely in the (rebuilt) topology +
// vertex->face adjacency. cs_copy fills the output pos/uv/color from the per-output-vert SOURCE map; cs_norm
// gathers face normals (+ UV-aligned tangent) over the output topology each frame -> tracks deformation.

// two SEPARATE shader modules, each with its OWN interface holding ONLY the buffers it uses — a shared iface
// with per-shader subset usage trips shadlang's compute-only-SSBO fallback-binding trap.
static std::string _copy_text() {
  return R"S(
fxconfig fxcfg_default {}
storage_interface cif_iP (descriptor_set 0) { buffer layout(std430) cipb { vec4 inP[];  }; }
storage_interface cif_iN (descriptor_set 0) { buffer layout(std430) cinb { vec4 inN[];  }; }
storage_interface cif_iB (descriptor_set 0) { buffer layout(std430) cibb { vec4 inB[];  }; }
storage_interface cif_iu (descriptor_set 0) { buffer layout(std430) ciub { vec4 inUV[]; }; }
storage_interface cif_ic (descriptor_set 0) { buffer layout(std430) cicb { vec4 inC[];  }; }
storage_interface cif_oP (descriptor_set 0) { buffer layout(std430) copb { vec4 oP[];   }; }
storage_interface cif_oN (descriptor_set 0) { buffer layout(std430) conb { vec4 oN[];   }; }
storage_interface cif_oB (descriptor_set 0) { buffer layout(std430) cobb { vec4 oB[];   }; }
storage_interface cif_ou (descriptor_set 0) { buffer layout(std430) coub { vec4 oUV[];  }; }
storage_interface cif_oc (descriptor_set 0) { buffer layout(std430) cocb { vec4 oC[];   }; }
storage_interface cif_sr (descriptor_set 0) { buffer layout(std430) csrb { uint SRC[];  }; }
storage_interface cif_ct (descriptor_set 0) { buffer layout(std430) cctb { uint p_nv; uint p0; uint p1; uint p2; }; }
compute_interface ifc { storage { cif_iP cif_iN cif_iB cif_iu cif_ic cif_oP cif_oN cif_oB cif_ou cif_oc cif_sr cif_ct }
                        inputs { layout(local_size_x = 64); } }
compute_shader cs_copy : ifc {              // copy ALL channels from the source vert; cs_norm overwrites N/B
  uint o = gl_GlobalInvocationID.x;         // for the recomputed (selected) verts -> pass-through verts keep
  if (o >= p_nv) { return; }                // their original N/B.
  uint s = SRC[o];
  oP[o] = inP[s]; oN[o] = inN[s]; oB[o] = inB[s]; oUV[o] = inUV[s]; oC[o] = inC[s];
}
)S";
}
// cs_norm gather shader now lives in hmdflow_module.h (_gatherNormalsText) — SHARED with subdivide(smooth).

struct NormalsInst : public MeshComputeInst {
  NormalsInst(const NormalsData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _input  = typedInputNamed<MeshPlugTraits>("In");
  }
  static const MeshChannel kCh[5];

  void rebuild(Context* ctx, gpumesh_ptr_t in) {
    auto env = _graphinst->_impl.getShared<MeshEnv>();
    auto fxi = ctx->FXI();
    int nv = in->_num_verts, nf = in->_num_faces, nc = in->_num_corners;
    auto rdu = [&](FxShaderStorageBuffer* b, int n) {
      std::vector<uint32_t> v(n);
      auto m = fxi->mapStorageBuffer(b, 0, size_t(n) * 4, BufferMapAccess::READ_ONLY);
      std::memcpy(v.data(), m->_mappedaddr, size_t(n) * 4);
      fxi->unmapStorageBuffer(m.get());
      return v;
    };
    auto Pin = [&] {
      std::vector<float> v(size_t(nv) * 4);
      auto m = fxi->mapStorageBuffer(in->channel(MeshChannel::POSITION)->_ssbo, 0, size_t(nv) * 16, BufferMapAccess::READ_ONLY);
      std::memcpy(v.data(), m->_mappedaddr, size_t(nv) * 16);
      fxi->unmapStorageBuffer(m.get());
      return v;
    }();
    auto vidx = rdu(in->_vidx->_ssbo, nc);
    auto fo   = rdu(in->_face_offsets->_ssbo, nf + 1);

    // selection: -1 (whole mesh) -> all selected; else the __tags bit (missing __tags -> nothing selected).
    std::vector<char> selFace(nf, 1);
    if (_d->_slot >= 0) {
      uint32_t bit = 1u << (_d->_slot & 31);
      std::vector<uint32_t> tags(nf, 0);
      if (auto tg = in->face("__tags")) tags = rdu(tg->_ssbo, nf);
      for (int f = 0; f < nf; f++) selFace[f] = (tags[f] & bit) ? 1 : 0;
    }
    auto Pq = [&](uint32_t v) { return weldKey(Pin.data(), v); };  // shared position-weld key (hmdflow_module.h)
    // Each corner is assigned an output vert. UNSELECTED corners -> a pass-through vert keyed by the ORIGINAL
    // vert (keeps the input's sharing + original N/B, no adjacency). SELECTED corners -> recomputed: smooth
    // welds by position, flat splits per corner; adjacency lists ONLY selected faces -> crease at the seam.
    std::vector<uint32_t> ovidx(nc), src;
    std::vector<std::vector<uint32_t>> adjLists;
    std::map<uint32_t, uint32_t> passThrough;
    std::map<std::tuple<int, int, int>, uint32_t> selWeld;
    auto newVert = [&](uint32_t sv) -> uint32_t { uint32_t o = uint32_t(src.size()); src.push_back(sv); adjLists.emplace_back(); return o; };
    for (int f = 0; f < nf; f++) {
      for (uint32_t c = fo[f]; c < fo[f + 1]; c++) {
        uint32_t v = vidx[c], ov;
        if (selFace[f]) {
          if (_d->_smooth) {
            auto key = Pq(v); auto it = selWeld.find(key);
            if (it == selWeld.end()) { ov = newVert(v); selWeld[key] = ov; } else ov = it->second;
          } else {
            ov = newVert(v);
          }
          adjLists[ov].push_back(uint32_t(f));
        } else {
          auto it = passThrough.find(v);
          if (it == passThrough.end()) { ov = newVert(v); passThrough[v] = ov; } else ov = it->second;
        }
        ovidx[c] = ov;
      }
    }
    _new_nv = int(src.size()); _new_nc = nc; _new_nf = nf;
    std::vector<uint32_t> off(_new_nv + 1, 0);
    for (int o = 0; o < _new_nv; o++) off[o + 1] = off[o] + uint32_t(adjLists[o].size());
    std::vector<uint32_t> adj;
    adj.reserve(std::max<size_t>(1, off[_new_nv]));
    for (int o = 0; o < _new_nv; o++) for (auto af : adjLists[o]) adj.push_back(af);
    if (adj.empty()) adj.push_back(0u);

    _vidx = env->_pool->acquireChannel(4, nc);
    _fo   = env->_pool->acquireChannel(4, nf + 1);
    _src  = env->_pool->acquireChannel(4, std::max(1, _new_nv));
    _aoff = env->_pool->acquireChannel(4, _new_nv + 1);
    _adj  = env->_pool->acquireChannel(4, std::max<int>(1, int(adj.size())));
    auto up = [&](FxShaderStorageBuffer* b, const std::vector<uint32_t>& vv) {
      auto m = fxi->mapStorageBuffer(b, 0, std::max<size_t>(1, vv.size()) * 4, BufferMapAccess::WRITE_ONLY);
      if (not vv.empty()) std::memcpy(m->_mappedaddr, vv.data(), vv.size() * 4);
      fxi->unmapStorageBuffer(m.get());
    };
    up(_vidx->_ssbo, ovidx); up(_fo->_ssbo, fo); up(_src->_ssbo, src); up(_aoff->_ssbo, off); up(_adj->_ssbo, adj);
    for (auto c : kCh) _outch[c] = env->_pool->acquireChannel(c, _new_nv);
    if (not _header) _header = env->_pool->acquire(kMeshHeaderBytes, 1);
    if (not _ctl)    _ctl    = fxi->createStorageBuffer(16);
  }

  bool onTopologyReady(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in or _built) return false;
    auto shc  = ctx->FXI()->shaderFromShaderText("hypermesh_normals_copy", _copy_text());
    _cs_copy  = ctx->FXI()->computeShader(shc, "cs_copy");
    auto shn  = ctx->FXI()->shaderFromShaderText("hypermesh_normals_norm", _gatherNormalsText());
    _cs_norm  = ctx->FXI()->computeShader(shn, "cs_norm");
    rebuild(ctx, in);
    _built = true; _built_nf = in->_num_faces;
    ackSrcTopo(_input);
    return true;                                                  // produced a NEW mesh -> re-eval
  }
  void writeParams(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in or not _cs_copy) return;
    auto out = _output->_value;
    int nf   = in->_num_faces;
    bool topoChanged = false;
    _active  = false;
    if (_built and _topoPending) {                                // deferred last frame; the producer's compute() has since filled its
      rebuild(ctx, in); ackSrcTopo(_input); _built_nf = nf; _pending_nf = -1; _topoPending = false; _active = true; topoChanged = true;
    }                                                            // output buffers -> now safe to read them (weld needs real positions)
    else if (_built and srcTopoDirty(_input)) {                  // producer re-emitted topology THIS frame: its compute() hasn't run yet,
      ackSrcTopo(_input); _topoPending = true; _active = true;   // so its new buffers are UNINITIALIZED -> defer; keep last-built output
    }
    else if (_built and nf == _built_nf)        _active = true;
    else if (_built and nf == _pending_nf) { rebuild(ctx, in); ackSrcTopo(_input); _built_nf = nf; _pending_nf = -1; _active = true; topoChanged = true; }
    else if (_built)                       _pending_nf = nf;
    if (not _active) {                                            // PASSTHROUGH for the transition frame
      out->_channels = in->_channels; out->_vidx = in->_vidx; out->_face_offsets = in->_face_offsets;
      out->_header = in->_header; out->_capacity = in->_capacity;
      out->_num_verts = in->_num_verts; out->_num_corners = in->_num_corners; out->_num_faces = in->_num_faces;
      return;
    }
    out->_channels     = _outch;
    out->_faces        = in->_faces;                             // face attrs (__tags) ride along (same face count)
    out->_vidx         = _vidx;
    out->_face_offsets = _fo;
    out->_header       = _header;
    out->_capacity     = meshNextPow2(_new_nv);
    out->_num_verts    = _new_nv;
    out->_num_corners  = _new_nc;
    out->_num_faces    = _new_nf;
    uint32_t ctl[4] = {uint32_t(_new_nv), 0u, 0u, 0u};
    auto m = ctx->FXI()->mapStorageBuffer(_ctl, 0, sizeof(ctl), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, ctl, sizeof(ctl));
    ctx->FXI()->unmapStorageBuffer(m.get());
    uint32_t hdr[4] = {uint32_t(_new_nv), uint32_t(_new_nc), uint32_t(_new_nf), 0u};
    auto mh = ctx->FXI()->mapStorageBuffer(_header, 0, sizeof(hdr), BufferMapAccess::WRITE_ONLY);
    std::memcpy(mh->_mappedaddr, hdr, sizeof(hdr));
    ctx->FXI()->unmapStorageBuffer(mh.get());
    if (topoChanged) out->markTopoChanged(); else out->markChanged();   // propagate dirt downstream (render/wireframe)
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    if (not _active) return;
    auto in  = _srcMesh(_input);
    auto env = inst->_impl.getShared<MeshEnv>();
    auto ci  = env->_ctx->CI();
    auto& s  = in->_channels;
    // cs_copy: in P/N/B/uv/color + out P/N/B/uv/color + SRC + ctl
    ci->bindStorageBuffer(_cs_copy, 0, s[MeshChannel::POSITION]->_ssbo);
    ci->bindStorageBuffer(_cs_copy, 1, s[MeshChannel::NORMAL]->_ssbo);
    ci->bindStorageBuffer(_cs_copy, 2, s[MeshChannel::BINORMAL]->_ssbo);
    ci->bindStorageBuffer(_cs_copy, 3, s[MeshChannel::UV0]->_ssbo);
    ci->bindStorageBuffer(_cs_copy, 4, s[MeshChannel::COLOR]->_ssbo);
    ci->bindStorageBuffer(_cs_copy, 5, _outch[MeshChannel::POSITION]->_ssbo);
    ci->bindStorageBuffer(_cs_copy, 6, _outch[MeshChannel::NORMAL]->_ssbo);
    ci->bindStorageBuffer(_cs_copy, 7, _outch[MeshChannel::BINORMAL]->_ssbo);
    ci->bindStorageBuffer(_cs_copy, 8, _outch[MeshChannel::UV0]->_ssbo);
    ci->bindStorageBuffer(_cs_copy, 9, _outch[MeshChannel::COLOR]->_ssbo);
    ci->bindStorageBuffer(_cs_copy, 10, _src->_ssbo);
    ci->bindStorageBuffer(_cs_copy, 11, _ctl);
    ci->dispatchCompute(_cs_copy, (_new_nv + 63) / 64, 1, 1);
    ci->storageBarrier();
    // cs_norm: out pos/normal/binormal/uv + out topology + adjacency + ctl
    ci->bindStorageBuffer(_cs_norm, 0, _outch[MeshChannel::POSITION]->_ssbo);
    ci->bindStorageBuffer(_cs_norm, 1, _outch[MeshChannel::NORMAL]->_ssbo);
    ci->bindStorageBuffer(_cs_norm, 2, _outch[MeshChannel::BINORMAL]->_ssbo);
    ci->bindStorageBuffer(_cs_norm, 3, _outch[MeshChannel::UV0]->_ssbo);
    ci->bindStorageBuffer(_cs_norm, 4, _vidx->_ssbo);
    ci->bindStorageBuffer(_cs_norm, 5, _fo->_ssbo);
    ci->bindStorageBuffer(_cs_norm, 6, _aoff->_ssbo);
    ci->bindStorageBuffer(_cs_norm, 7, _adj->_ssbo);
    ci->bindStorageBuffer(_cs_norm, 8, _ctl);
    ci->dispatchCompute(_cs_norm, (_new_nv + 63) / 64, 1, 1);
    ci->storageBarrier();
  }
  const NormalsData* _d;
  mesh_outpluginst_ptr_t _output;
  mesh_inpluginst_ptr_t _input;
  std::map<MeshChannel, gpuchannel_ptr_t> _outch;
  gpuchannel_ptr_t _vidx, _fo, _src, _aoff, _adj;
  FxShaderStorageBuffer *_header = nullptr, *_ctl = nullptr;
  const FxComputeShader *_cs_copy = nullptr, *_cs_norm = nullptr;
  int _new_nv = 0, _new_nc = 0, _new_nf = 0;
  int _built_nf = -1, _pending_nf = -1;
  bool _topoPending = false;   // a producer topology change was seen but deferred 1 frame (its compute() must fill its buffers first)
  bool _built = false, _active = false;
};
const MeshChannel NormalsInst::kCh[5] = {
    MeshChannel::POSITION, MeshChannel::NORMAL, MeshChannel::BINORMAL, MeshChannel::UV0, MeshChannel::COLOR};

static void _reshapeNormalsIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
NormalsData::NormalsData() {
}
std::shared_ptr<NormalsData> NormalsData::createShared() {
  auto d = std::make_shared<NormalsData>();
  _reshapeNormalsIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t NormalsData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<NormalsInst>(this, g);
}
void NormalsData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return NormalsData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeNormalsIOs(m); });
  clazz->directProperty("smooth", &NormalsData::_smooth);
  clazz->directProperty("slot", &NormalsData::_slot);
}

} // namespace ork::lev2::hypermesh
