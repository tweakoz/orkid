////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"

ImplementReflectionX(ork::lev2::hypermesh::SortTestData, "hypermesh::SortTestData");

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////
// SortTest — a REGRESSION HARNESS for the MeshSort primitive (NOT a modeling op). It fills N
// (key,payload) pairs with a collision-heavy hash (8-bit keys over N>>256 -> ~N/256 collisions each,
// so STABILITY is exercised hard), runs MeshSort, and writes the sorted (key,payload) into vertex
// XY so dump_obj reads the result back through the normal staging path. A correct dump has: X (key)
// non-decreasing; within every equal-key run, Y (payload = original index) strictly ascending (proves
// stability/determinism); and {Y} == {0..N-1} (proves the scatter is a permutation — nothing lost or
// duplicated). It runs entirely through the real compute-dispatch path on DEVICE_LOCAL buffers.
///////////////////////////////////////////////////////////////////////////////

static std::string _sorttest_text(int cap, int n) {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface sif_hdr (descriptor_set 0) { buffer layout(std430) hb {
  uint num_verts; uint num_corners; uint num_faces; uint flags; vec4 bbmin; vec4 bbmax; }; }
storage_interface sif_P   (descriptor_set 0) { buffer layout(std430) pb  { vec4 Pd[%CAP%];  }; }
storage_interface sif_N   (descriptor_set 0) { buffer layout(std430) nb  { vec4 Nd[%CAP%];  }; }
storage_interface sif_B   (descriptor_set 0) { buffer layout(std430) bb2 { vec4 Bd[%CAP%];  }; }
storage_interface sif_uv  (descriptor_set 0) { buffer layout(std430) ub  { vec4 UVd[%CAP%]; }; }
storage_interface sif_clr (descriptor_set 0) { buffer layout(std430) cb  { vec4 Cd[%CAP%];  }; }
storage_interface sif_vi  (descriptor_set 0) { buffer layout(std430) vib { uint VId[%CAP%]; }; }
storage_interface sif_fo  (descriptor_set 0) { buffer layout(std430) fob { uint FOd[%CAP%]; }; }
storage_interface sif_key (descriptor_set 0) { buffer layout(std430) kb  { uint KEY[%CAP%]; }; }
storage_interface sif_pay (descriptor_set 0) { buffer layout(std430) yb  { uint PAY[%CAP%]; }; }
compute_interface iface { storage { sif_hdr sif_P sif_N sif_B sif_uv sif_clr sif_vi sif_fo sif_key sif_pay }
                          inputs { layout(local_size_x = 64); } }
////////////////////////////////////////
compute_shader cs_setup : iface {                       // header + triangle CSR (thread 0)
  if (gl_GlobalInvocationID.x != 0u) { return; }
  num_verts = %N%u; num_corners = %N%u; num_faces = (%N%u / 3u); flags = 0u;
  bbmin = vec4(0.0, 0.0, 0.0, 1.0); bbmax = vec4(256.0, 256.0, 1.0, 1.0);
  uint nf = %N%u / 3u;
  for (uint f = 0u; f < nf; f++) { FOd[f] = f * 3u; }
  FOd[nf] = nf * 3u;
}
////////////////////////////////////////
compute_shader cs_fill : iface {                        // KEY = collision-heavy 8-bit hash; PAY = original index
  uint i = gl_GlobalInvocationID.x;
  if (i >= %N%u) { return; }
  uint h = (i * 2654435761u) ^ (i >> 3u);
  KEY[i] = h & 255u;
  PAY[i] = i;
}
////////////////////////////////////////
compute_shader cs_writepos : iface {                    // sorted (KEY,PAY) -> vertex XY (read back via dump_obj)
  uint i = gl_GlobalInvocationID.x;
  if (i >= %N%u) { return; }
  Pd[i]  = vec4(float(KEY[i]), float(PAY[i]), 0.0, 1.0);
  Nd[i]  = vec4(0.0, 0.0, 1.0, 0.0);
  Bd[i]  = vec4(1.0, 0.0, 0.0, 0.0);
  UVd[i] = vec4(0.0);
  Cd[i]  = vec4(1.0);
  VId[i] = i;
}
)S";
  _shadersub(t, "%CAP%", FormatString("%d", cap));
  _shadersub(t, "%N%", FormatString("%d", n));
  return t;
}

struct SortTestInst : public MeshComputeInst {
  SortTestInst(const SortTestData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final { _output = typedOutputNamed<MeshPlugTraits>("Out"); }
  void onActivate(dflow::GraphInst* inst) final {
    auto env  = inst->_impl.getShared<MeshEnv>();
    auto fxi  = env->_ctx->FXI();
    _n        = _d->_n;
    int cap   = meshNextPow2(_n);
    auto mesh = _output->_value;
    allocMesh(env, mesh, _n, _n, _n / 3,
              {MeshChannel::POSITION, MeshChannel::NORMAL, MeshChannel::BINORMAL,
               MeshChannel::UV0, MeshChannel::COLOR});
    _key = fxi->createStorageBuffer(size_t(cap) * 4, StorageBufferUsage::DEFAULT, BufferResidency::DEVICE);
    _pay = fxi->createStorageBuffer(size_t(cap) * 4, StorageBufferUsage::DEFAULT, BufferResidency::DEVICE);
    _sort.init(env->_ctx);
    auto sh     = fxi->shaderFromShaderText("hypermesh_sorttest", _sorttest_text(cap, _n));
    _cs_setup   = fxi->computeShader(sh, "cs_setup");
    _cs_fill    = fxi->computeShader(sh, "cs_fill");
    _cs_writepos = fxi->computeShader(sh, "cs_writepos");
  }
  void writeParams(Context* ctx) final { _sort.ensure(ctx, _n); }   // PRE: host-reset p_n/pass + scratch
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
      ci->bindStorageBuffer(cs, 8, _key);
      ci->bindStorageBuffer(cs, 9, _pay);
    };
    bind(_cs_setup);   ci->dispatchCompute(_cs_setup, 1, 1, 1);                ci->storageBarrier();
    bind(_cs_fill);    ci->dispatchCompute(_cs_fill, (_n + 63) / 64, 1, 1);    ci->storageBarrier();
    _sort.sort(env->_ctx, _key, _pay);                                        ci->storageBarrier();
    bind(_cs_writepos); ci->dispatchCompute(_cs_writepos, (_n + 63) / 64, 1, 1); ci->storageBarrier();
  }
  const SortTestData* _d;
  mesh_outpluginst_ptr_t _output;
  MeshSort _sort;
  FxShaderStorageBuffer *_key = nullptr, *_pay = nullptr;
  const FxComputeShader *_cs_setup = nullptr, *_cs_fill = nullptr, *_cs_writepos = nullptr;
  int _n = 252;
};

static void _reshapeSortTestIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
SortTestData::SortTestData() {}
std::shared_ptr<SortTestData> SortTestData::createShared() {
  auto d = std::make_shared<SortTestData>();
  _reshapeSortTestIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t SortTestData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<SortTestInst>(this, g);
}
void SortTestData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return SortTestData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeSortTestIOs(m); });
  clazz->directProperty("n", &SortTestData::_n);
}

} // namespace ork::lev2::hypermesh
