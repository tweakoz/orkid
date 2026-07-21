////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include <ork/kernel/string/ConstString.h>
#include <ork/reflect/properties/DirectTypedVector.hpp> // directVectorProperty on std::vector<float>
#include <ork/math/cvector3.h>
#include <ork/math/cmatrix4.h>
#include <cmath>
#include <vector>

ImplementReflectionX(ork::lev2::hypermesh::ParcelizeModuleData, "hypermesh::ParcelizeModuleData");
ImplementReflectionX(ork::lev2::hypermesh::BuildingSeedsModuleData, "hypermesh::BuildingSeedsModuleData");

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////
// Parcelize + BuildingSeeds (R-family) — derive frontage parcels along the spine
// and building seeds on them. Mirrors roads/mask_ref.py (parcelize / building_seeds)
// operation-for-operation. Deterministic (the shared counter-hash RNG; the SAME
// _mix64 chain as hfdflow_scatter.cpp:35-50 / route_ref.py). CPU at onTopologyReady.
//
// BuildingSeeds EMIT ADAPTER (owner Q2): the DEFAULT (_emit_adapter==0) is a
// scatter-sink-compatible InstanceSet ("same as tree scatter") so gates run TODAY;
// the FULL freeform-SoA per-item schema is built internally and documented in the
// header — a richer artifact (a new plug type carrying all SoA channels) is a small
// adapter swap at the seam marked below (the owner Q1-Q3 decision point).
///////////////////////////////////////////////////////////////////////////////

static constexpr uint64_t _U64MASK = ~0ull;
static inline uint64_t _mix64(uint64_t x) {
  x ^= x >> 30; x *= 0xBF58476D1CE4E5B9ull;
  x ^= x >> 27; x *= 0x94D049BB133111EBull;
  x ^= x >> 31; return x;
}
static inline uint64_t _rhash(uint64_t seed, uint64_t idx, uint64_t stream) {
  return _mix64(_mix64(_mix64((seed ^ 0x9E3779B97F4A7C15ull) & _U64MASK) ^ idx) ^ stream);
}
static inline double _ru01(uint64_t seed, uint64_t idx, uint64_t stream) {
  return double(_rhash(seed, idx, stream) >> 11) * (1.0 / 9007199254740992.0);
}

static ork::hyper::xfnodegraph_inst_ptr_t _srcXng(dflowgfx::xfng_inpluginst_ptr_t inp) {
  if (not inp) return nullptr;
  auto out = std::dynamic_pointer_cast<dflowgfx::xfng_outpluginst_t>(inp->_connectedOutput);
  return out ? out->_value : nullptr;
}
static dflowgfx::instanceset_inst_ptr_t _srcIset(dflowgfx::instset_inpluginst_ptr_t inp) {
  if (not inp) return nullptr;
  auto out = std::dynamic_pointer_cast<dflowgfx::instset_outpluginst_t>(inp->_connectedOutput);
  return out ? out->_value : nullptr;
}

static void _fillIset(Context* ctx, dflowgfx::instanceset_inst_ptr_t iset,
                      const std::vector<fmtx4>& mats, const std::vector<fvec4>& attrs) {
  auto fxi = ctx->FXI();
  const int N = int(mats.size());
  iset->_count = N;
  if (N == 0) { iset->markChanged(); return; }
  iset->_matrices = fxi->createStorageBuffer(size_t(N) * sizeof(fmtx4));
  iset->_attrs    = fxi->createStorageBuffer(size_t(N) * 4 * sizeof(float));
  { auto m = fxi->mapStorageBuffer(iset->_matrices, 0, size_t(N) * sizeof(fmtx4), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, mats.data(), size_t(N) * sizeof(fmtx4)); fxi->unmapStorageBuffer(m.get()); }
  { auto m = fxi->mapStorageBuffer(iset->_attrs, 0, size_t(N) * 16, BufferMapAccess::WRITE_ONLY);
    auto dst = (float*)m->_mappedaddr;
    for (int i = 0; i < N; i++) { dst[i*4+0]=attrs[i].x; dst[i*4+1]=attrs[i].y; dst[i*4+2]=attrs[i].z; dst[i*4+3]=attrs[i].w; }
    fxi->unmapStorageBuffer(m.get()); }
  iset->markChanged();
}

// ---------------------------------------------------------------------------
struct ParcelizeModuleInst : public MeshComputeInst {
  ParcelizeModuleInst(const ParcelizeModuleData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}
  const char* _cookSalt() const final { return "roads parcelize.v1 evalv=1"; }
  void onLink(dflow::GraphInst*) final {
    _in  = typedInputNamed<dflowgfx::XfNodeGraphPlugTraits>("In");
    _out = typedOutputNamed<dflowgfx::InstanceSetPlugTraits>("Out");
  }
  bool onTopologyReady(Context* ctx) final {
    if (_built) return false;
    auto xng = _srcXng(_in);
    if (not(xng and not xng->_nodes.empty())) return false;
    const double extent = _d->_extent_m;
    // recover the layout cell size from the node width... instead snap via extent + a nominal grid.
    // (cell index only feeds the deterministic hash; any stable per-parcel key works — use a fine grid.)
    const int gdim = 4096;
    const double cell_m = extent / gdim;
    auto snap = [&](double wx, double wz) -> int {
      int i = int((wx + extent * 0.5) / cell_m), j = int((wz + extent * 0.5) / cell_m);
      i = std::min(std::max(i, 0), gdim - 1); j = std::min(std::max(j, 0), gdim - 1);
      return j * gdim + i;
    };
    std::vector<fmtx4> mats; std::vector<fvec4> attrs;
    uint64_t seed = uint64_t(uint32_t(_d->_seed));
    double side_off = 0.0; // width/2 + depth/2 (width read from the node attrs.x)
    uint32_t stream = 0;
    for (auto& nd : xng->_nodes) {
      if (nd._parent == 0xffffffffu) continue;
      const auto& pn = xng->_nodes[nd._parent];
      double ax = pn._xform[12], az = pn._xform[14], bx = nd._xform[12], bz = nd._xform[14];
      double ex = bx - ax, ez = bz - az, seglen = std::hypot(ex, ez);
      if (seglen < 1e-6) continue;
      double dx = ex / seglen, dz = ez / seglen;
      double nx = -dz, nz = dx;                          // left normal
      double width = nd._attrs[0] > 0 ? nd._attrs[0] : _d->_frontage_m * 0.5;
      side_off = width * 0.5 + _d->_depth_m * 0.5;
      double step = _d->_frontage_m + _d->_spacing_m;
      double elev_a = pn._xform[13], elev_b = nd._xform[13];
      for (double s = _d->_frontage_m * 0.5; s < seglen; s += step) {
        int keycell = snap(ax + dx * s, az + dz * s);
        double jit = (_ru01(seed, keycell, stream) - 0.5) * _d->_jitter * _d->_spacing_m;
        stream++;
        double sc = s + jit;
        if (sc < 0.0 or sc > seglen) continue;
        double mx = ax + dx * sc, mz = az + dz * sc;
        double my = elev_a + (sc / seglen) * (elev_b - elev_a);
        for (double sgn : {+1.0, -1.0}) {
          double off = sgn * side_off;
          double cx = mx + nx * off, cz = mz + nz * off;
          int cell = snap(cx, cz);
          // OBB frame: X=travel dir (frontage axis), Y=up, Z=inward normal (depth axis)
          fmtx4 m;
          m.setColumn(0, fvec4(float(dx), 0.0f, float(dz), 0.0f));
          m.setColumn(1, fvec4(0.0f, 1.0f, 0.0f, 0.0f));
          m.setColumn(2, fvec4(float(sgn * nx), 0.0f, float(sgn * nz), 0.0f));
          m.setColumn(3, fvec4(float(cx), float(my), float(cz), 1.0f));
          mats.push_back(m);
          attrs.push_back(fvec4(_d->_frontage_m, _d->_depth_m, float(sgn), float(cell)));
        }
      }
    }
    _fillIset(ctx, _out->_value, mats, attrs);
    _built = true;
    return true;
  }
  void compute(dflow::GraphInst*, ui::updatedata_ptr_t) final {}
  const ParcelizeModuleData* _d;
  dflowgfx::xfng_inpluginst_ptr_t _in;
  dflowgfx::instset_outpluginst_ptr_t _out;
  bool _built = false;
};

// ---------------------------------------------------------------------------
// BuildingSeeds — one seed per parcel. FULL freeform-SoA schema (owner Q2):
//   xform          per-item placement (parcel center, facing toward the spine)
//   type_id        building archetype id (attrs.x)
//   variant_seed01 per-item variant hash 0..1 (attrs.y)
//   height_seed    [0,1] storey/height jitter (attrs.z)
//   style_seed     [0,1] style jitter (attrs.w)
//   frontage_m / depth_m / orient_rad — SoA EXTENSION (built here; carried by a
//     richer adapter, the owner-decision seam). Default adapter (_emit_adapter==0)
//     projects to the scatter-sink InstanceSet above (frontage/depth ride the
//     matrix scale-basis; consumers read known channels, ignore unknown).
struct BuildingSeedsModuleInst : public MeshComputeInst {
  BuildingSeedsModuleInst(const BuildingSeedsModuleData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}
  const char* _cookSalt() const final { return "roads buildingseeds.v1 evalv=1"; }
  void onLink(dflow::GraphInst*) final {
    _in  = typedInputNamed<dflowgfx::InstanceSetPlugTraits>("Parcels");
    _out = typedOutputNamed<dflowgfx::InstanceSetPlugTraits>("Out");
  }
  bool onTopologyReady(Context* ctx) final {
    if (_built) return false;
    auto parcels = _srcIset(_in);
    if (not(parcels and parcels->_matrices)) return false;
    const int P = parcels->_count;
    std::vector<fmtx4> pmats(std::max(1, P));
    std::vector<float> pattr(std::max(1, P) * 4, 0.0f);
    if (P > 0) {
      // mid-graph readback MUST map READ_ONLY (device-local readStorageBuffer aborts the CB)
      auto fxi = ctx->FXI();
      auto rd = [&](FxShaderStorageBuffer* b, size_t bytes, void* dst) {
        auto mp = fxi->mapStorageBuffer(b, 0, bytes, BufferMapAccess::READ_ONLY);
        std::memcpy(dst, mp->_mappedaddr, bytes);
        fxi->unmapStorageBuffer(mp.get());
      };
      rd(parcels->_matrices, size_t(P) * sizeof(fmtx4), pmats.data());
      if (parcels->_attrs)
        rd(parcels->_attrs, size_t(P) * 16, pattr.data());
    }
    std::vector<float> tw = _d->_type_weights;
    if (tw.empty()) tw.push_back(1.0f);
    double tw_sum = 0.0; for (float w : tw) tw_sum += w; if (tw_sum <= 0.0) tw_sum = 1.0;
    uint64_t seed = uint64_t(uint32_t(_d->_seed));
    std::vector<fmtx4> mats; std::vector<fvec4> attrs;
    for (int i = 0; i < P; i++) {
      const fmtx4& pm = pmats[i];
      fvec4 c3 = pm.column(3);
      fvec4 zc = pm.column(2);                            // inward normal (facing INTO parcel)
      uint32_t cell = uint32_t(pattr[i * 4 + 3]);
      // facing = toward the spine = -inward normal
      double fx = -double(zc.x), fz = -double(zc.z);
      double orient = std::atan2(fx, fz);                 // yaw about +Y
      double r = _ru01(seed, cell, 0x5EED0001ull + uint64_t(i)) * tw_sum;
      double acc = 0.0; int tid = 0;
      for (size_t t = 0; t < tw.size(); t++) { acc += tw[t]; if (r < acc) { tid = int(t); break; } }
      float variant01 = float((_rhash(seed, cell, 0x5EED0002ull + uint64_t(i)) >> 34) & 0x3FFFFFFFu) / float(0x40000000u);
      float height_seed = float(_ru01(seed, cell, 0x5EED0003ull + uint64_t(i)));
      float style_seed  = float(_ru01(seed, cell, 0x5EED0004ull + uint64_t(i)));
      // placement frame: face toward the spine (X=right, Z=facing), at the parcel center
      double cy = std::cos(orient), sy = std::sin(orient);
      fmtx4 m;
      m.setColumn(0, fvec4(float(cy), 0.0f, float(-sy), 0.0f));
      m.setColumn(1, fvec4(0.0f, 1.0f, 0.0f, 0.0f));
      m.setColumn(2, fvec4(float(sy), 0.0f, float(cy), 0.0f));
      m.setColumn(3, fvec4(c3.x, c3.y, c3.z, 1.0f));
      mats.push_back(m);
      attrs.push_back(fvec4(float(tid), variant01, height_seed, style_seed));
      // ---- SoA EXTENSION SEAM (owner Q2): frontage/depth/orient available here;
      //      a richer adapter emits them as named channels. Default adapter omits.
    }
    _fillIset(ctx, _out->_value, mats, attrs);
    _built = true;
    return true;
  }
  void compute(dflow::GraphInst*, ui::updatedata_ptr_t) final {}
  const BuildingSeedsModuleData* _d;
  dflowgfx::instset_inpluginst_ptr_t _in;
  dflowgfx::instset_outpluginst_ptr_t _out;
  bool _built = false;
};

// ---- reshape / factory / describe ----
static void _reshapeParcelizeIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<dflowgfx::XfNodeGraphPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createOutputPlug<dflowgfx::InstanceSetPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
ParcelizeModuleData::ParcelizeModuleData() {}
std::shared_ptr<ParcelizeModuleData> ParcelizeModuleData::createShared() {
  auto d = std::make_shared<ParcelizeModuleData>(); _reshapeParcelizeIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t ParcelizeModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<ParcelizeModuleInst>(this, g);
}
void ParcelizeModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return ParcelizeModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeParcelizeIOs(m); });
  clazz->annotateTyped<ConstString>("dsl.verb", "parcelize");
  clazz->annotateTyped<bool>("editor.palette", true);
  clazz->annotateTyped<int>("editor.palette.sort", 23);
  clazz->directProperty("frontage_m", &ParcelizeModuleData::_frontage_m);
  clazz->directProperty("depth_m", &ParcelizeModuleData::_depth_m);
  clazz->directProperty("spacing_m", &ParcelizeModuleData::_spacing_m);
  clazz->directProperty("jitter", &ParcelizeModuleData::_jitter);
  clazz->directProperty("extent_m", &ParcelizeModuleData::_extent_m);
  clazz->directProperty("seed", &ParcelizeModuleData::_seed);
}

static void _reshapeBuildingSeedsIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<dflowgfx::InstanceSetPlugTraits>(data, dflow::EPR_UNIFORM, "Parcels");
  dflow::ModuleData::createOutputPlug<dflowgfx::InstanceSetPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
BuildingSeedsModuleData::BuildingSeedsModuleData() {}
std::shared_ptr<BuildingSeedsModuleData> BuildingSeedsModuleData::createShared() {
  auto d = std::make_shared<BuildingSeedsModuleData>(); _reshapeBuildingSeedsIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t BuildingSeedsModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<BuildingSeedsModuleInst>(this, g);
}
void BuildingSeedsModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return BuildingSeedsModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeBuildingSeedsIOs(m); });
  clazz->annotateTyped<ConstString>("dsl.verb", "building_seeds");
  clazz->annotateTyped<bool>("editor.palette", true);
  clazz->annotateTyped<int>("editor.palette.sort", 24);
  clazz->directVectorProperty("type_weights", &BuildingSeedsModuleData::_type_weights);
  clazz->directProperty("emit_adapter", &BuildingSeedsModuleData::_emit_adapter);
  clazz->directProperty("seed", &BuildingSeedsModuleData::_seed);
}

} // namespace ork::lev2::hypermesh
