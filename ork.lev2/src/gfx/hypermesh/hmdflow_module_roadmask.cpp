////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include <ork/kernel/string/ConstString.h>
#include <ork/lev2/gfx/terrain/dflow/hfdflow.h> // BakeEnv (output field dim == terrain bake dim)
#include <cmath>
#include <vector>

ImplementReflectionX(ork::lev2::hypermesh::RoadbedMaskModuleData, "hypermesh::RoadbedMaskModuleData");
ImplementReflectionX(ork::lev2::hypermesh::KeepoutMaskModuleData, "hypermesh::KeepoutMaskModuleData");

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////
// RoadbedMask + KeepoutMask (R-family) — rasterize the XfNodeGraph spine into
// terrain-currency HfImage fields (spec §0 BACKWARD coupling). Mirrors the
// pure-python reference roads/mask_ref.py operation-for-operation. CPU raster at
// onTopologyReady (spine must be built upstream first — the driver's topology
// cascade re-evals between nodes, so RoadbedMask reads RouteSpine's FINAL spine).
// Output dim = the terrain BakeEnv dim so stock MaskBlend flattens the heightfield
// with ZERO new terrain C++ (Q5). The road GEOMETRY is declared in METERS, so the
// road is dim-independent regardless of the output resolution.
///////////////////////////////////////////////////////////////////////////////

static ork::hyper::xfnodegraph_inst_ptr_t _srcXng(dflowgfx::xfng_inpluginst_ptr_t inp) {
  if (not inp) return nullptr;
  auto out = std::dynamic_pointer_cast<dflowgfx::xfng_outpluginst_t>(inp->_connectedOutput);
  return out ? out->_value : nullptr;
}
static dflowgfx::gpucomputeimage2d_inst_ptr_t _srcImg(dflowgfx::hfimg_inpluginst_ptr_t inp) {
  if (not inp) return nullptr;
  auto out = std::dynamic_pointer_cast<dflowgfx::hfimg_outpluginst_t>(inp->_connectedOutput);
  return out ? out->_value : nullptr;
}

// closest point q=a+t*(b-a), t in [0,1], to p — returns (dist,t). Mirrors mask_ref._nearest_on_segment.
static void _nearestOnSeg(double px, double pz, double ax, double az, double bx, double bz,
                          double& outd, double& outt) {
  double dx = bx - ax, dz = bz - az, L2 = dx * dx + dz * dz;
  if (L2 < 1e-12) { outd = std::hypot(px - ax, pz - az); outt = 0.0; return; }
  double t = ((px - ax) * dx + (pz - az) * dz) / L2;
  if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0;
  double qx = ax + t * dx, qz = az + t * dz;
  outd = std::hypot(px - qx, pz - qz); outt = t;
}
static double _signedLateral(double px, double pz, double ax, double az, double bx, double bz) {
  double dx = bx - ax, dz = bz - az, m = std::hypot(dx, dz);
  if (m < 1e-12) return 0.0;
  return ((px - ax) * (-dz) + (pz - az) * (dx)) / m; // left-normal of travel dir
}

// (re)allocate an HfImage output plug value at dim×dim R32F.
static void _allocField(Context* ctx, dflowgfx::gpucomputeimage2d_inst_ptr_t img, int dim) {
  img->_w = dim; img->_h = dim; img->_channels = 1;
  img->_ssbo = ctx->FXI()->createStorageBuffer(size_t(dim) * dim * sizeof(float));
}
static void _uploadField(Context* ctx, dflowgfx::gpucomputeimage2d_inst_ptr_t img, const std::vector<float>& v) {
  auto fxi = ctx->FXI();
  auto mp  = fxi->mapStorageBuffer(img->_ssbo, 0, v.size() * sizeof(float), BufferMapAccess::WRITE_ONLY);
  std::memcpy(mp->_mappedaddr, v.data(), v.size() * sizeof(float));
  fxi->unmapStorageBuffer(mp.get());
}

struct RoadbedMaskModuleInst : public MeshComputeInst {
  RoadbedMaskModuleInst(const RoadbedMaskModuleData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}
  const char* _cookSalt() const final { return "roads roadbedmask.v1 evalv=1"; }

  void onLink(dflow::GraphInst*) final {
    _in       = typedInputNamed<dflowgfx::XfNodeGraphPlugTraits>("In");
    _outBed   = typedOutputNamed<dflowgfx::HfImagePlugTraits>("Out");      // PRIMARY = roadbed coverage
    _outElev  = typedOutputNamed<dflowgfx::HfImagePlugTraits>("RoadElev");
    _outU     = typedOutputNamed<dflowgfx::HfImagePlugTraits>("UvU");
    _outV     = typedOutputNamed<dflowgfx::HfImagePlugTraits>("UvV");
  }

  int _outDim(dflow::GraphInst* g) const {
    auto env = g->_impl.getShared<terrain::BakeEnv>();
    return (env and env->_w > 0) ? env->_w : _d->_out_dim; // match the terrain flatten dim
  }

  bool onTopologyReady(Context* ctx) final {
    if (_built) return false;
    auto xng = _srcXng(_in);
    if (not(xng and not xng->_nodes.empty())) return false; // spine not built yet
    const int dim = _outDim(_graphinst);
    const double extent = _d->_extent_m;
    const double hw = _d->_width_m * 0.5, band = hw + _d->_shoulder_m;
    const int N = dim * dim;

    // segments = (parent, node) world XZ + road_elev(_attrs.z) + arclen(_attrs.y)
    struct Seg { double ax, az, bx, bz, aelev, belev, aarc, barc; };
    std::vector<Seg> segs;
    for (auto& nd : xng->_nodes) {
      if (nd._parent == 0xffffffffu) continue;
      const auto& pn = xng->_nodes[nd._parent];
      segs.push_back(Seg{pn._xform[12], pn._xform[14], nd._xform[12], nd._xform[14],
                         pn._attrs[2], nd._attrs[2], pn._attrs[1], nd._attrs[1]});
    }
    std::vector<float> bed(N, 0.0f), elev(N, 0.0f), uu(N, 0.0f), uv(N, 0.0f);
    if (not segs.empty()) {
      const double cell_m = extent / dim;
      for (int j = 0; j < dim; j++) {
        for (int i = 0; i < dim; i++) {
          double px = (i + 0.5) * cell_m - extent * 0.5;
          double pz = (j + 0.5) * cell_m - extent * 0.5;
          double bestd = 1e300, bestt = 0.0; int bestk = -1;
          for (size_t k = 0; k < segs.size(); k++) {
            double d, t; _nearestOnSeg(px, pz, segs[k].ax, segs[k].az, segs[k].bx, segs[k].bz, d, t);
            if (d < bestd) { bestd = d; bestt = t; bestk = int(k); }
          }
          if (bestk < 0 or bestd > band) continue;
          const auto& s = segs[bestk];
          int c = j * dim + i;
          bed[c]  = (bestd <= hw) ? 1.0f : float(std::max(0.0, 1.0 - (bestd - hw) / std::max(1e-6, double(_d->_shoulder_m))));
          elev[c] = float(s.aelev + bestt * (s.belev - s.aelev));
          double lat = _signedLateral(px, pz, s.ax, s.az, s.bx, s.bz);
          uu[c] = float(std::min(1.0, std::max(0.0, 0.5 + 0.5 * (lat / std::max(1e-6, hw)))));
          uv[c] = float((s.aarc + bestt * (s.barc - s.aarc)) / std::max(1e-6, double(_d->_v_meters_per_tile)));
        }
      }
    }
    _allocField(ctx, _outBed->_value, dim);  _uploadField(ctx, _outBed->_value, bed);
    _allocField(ctx, _outElev->_value, dim); _uploadField(ctx, _outElev->_value, elev);
    _allocField(ctx, _outU->_value, dim);    _uploadField(ctx, _outU->_value, uu);
    _allocField(ctx, _outV->_value, dim);    _uploadField(ctx, _outV->_value, uv);
    _built = true;
    return true;
  }
  void compute(dflow::GraphInst*, ui::updatedata_ptr_t) final {}

  const RoadbedMaskModuleData* _d;
  dflowgfx::xfng_inpluginst_ptr_t _in;
  dflowgfx::hfimg_outpluginst_ptr_t _outBed, _outElev, _outU, _outV;
  bool _built = false;
};

struct KeepoutMaskModuleInst : public MeshComputeInst {
  KeepoutMaskModuleInst(const KeepoutMaskModuleData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}
  const char* _cookSalt() const final { return "roads keepoutmask.v1 evalv=1"; }

  void onLink(dflow::GraphInst*) final {
    _inBed = typedInputNamed<dflowgfx::HfImagePlugTraits>("Roadbed");
    _out   = typedOutputNamed<dflowgfx::HfImagePlugTraits>("Out"); // PRIMARY = keepout mask
  }

  bool onTopologyReady(Context* ctx) final {
    if (_built) return false;
    auto bed = _srcImg(_inBed);
    if (not(bed and bed->_ssbo)) return false; // roadbed not produced yet
    const int dim = bed->_w;
    const int N = dim * dim;
    std::vector<float> src(N);
    { // mid-graph readback MUST map READ_ONLY (device-local readStorageBuffer aborts the CB)
      auto fxi = ctx->FXI();
      auto mp  = fxi->mapStorageBuffer(bed->_ssbo, 0, size_t(N) * sizeof(float), BufferMapAccess::READ_ONLY);
      std::memcpy(src.data(), mp->_mappedaddr, size_t(N) * sizeof(float));
      fxi->unmapStorageBuffer(mp.get());
    }
    // dilate(>0.5) by keepout_radius (world meters -> texels). Mirrors mask_ref.dilate.
    const double cell_m = _d->_extent_m / dim;
    int r = std::max(0, int(std::lround(_d->_keepout_radius_m / std::max(1e-6, cell_m))));
    std::vector<float> ko(N, 0.0f);
    std::vector<uint8_t> on(N, 0);
    for (int c = 0; c < N; c++) on[c] = (src[c] > 0.5f) ? 1 : 0;
    for (int j = 0; j < dim; j++) {
      for (int i = 0; i < dim; i++) {
        if (on[j * dim + i]) { ko[j * dim + i] = 1.0f; continue; }
        bool hit = false;
        for (int dj = -r; dj <= r and not hit; dj++) {
          for (int di = -r; di <= r; di++) {
            int ni = i + di, nj = j + dj;
            if (ni >= 0 and nj >= 0 and ni < dim and nj < dim and on[nj * dim + ni]) { hit = true; break; }
          }
        }
        if (hit) ko[j * dim + i] = 1.0f;
      }
    }
    _allocField(ctx, _out->_value, dim);
    _uploadField(ctx, _out->_value, ko);
    _built = true;
    return true;
  }
  void compute(dflow::GraphInst*, ui::updatedata_ptr_t) final {}

  const KeepoutMaskModuleData* _d;
  dflowgfx::hfimg_inpluginst_ptr_t _inBed;
  dflowgfx::hfimg_outpluginst_ptr_t _out;
  bool _built = false;
};

// ---- reshape / factory / describe ----
static void _reshapeRoadbedIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<dflowgfx::XfNodeGraphPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  // PRIMARY output is named "Out" (the codebase convention) = roadbed coverage. Named-only
  // outputs (no "Out") trip ModuleInst::outputNamed's assert in the materializeLive terminal
  // probe — every producer carries an "Out".
  dflow::ModuleData::createOutputPlug<dflowgfx::HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
  dflow::ModuleData::createOutputPlug<dflowgfx::HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "RoadElev");
  dflow::ModuleData::createOutputPlug<dflowgfx::HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "UvU");
  dflow::ModuleData::createOutputPlug<dflowgfx::HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "UvV");
}
RoadbedMaskModuleData::RoadbedMaskModuleData() {}
std::shared_ptr<RoadbedMaskModuleData> RoadbedMaskModuleData::createShared() {
  auto d = std::make_shared<RoadbedMaskModuleData>(); _reshapeRoadbedIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t RoadbedMaskModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<RoadbedMaskModuleInst>(this, g);
}
void RoadbedMaskModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return RoadbedMaskModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeRoadbedIOs(m); });
  clazz->annotateTyped<ConstString>("dsl.verb", "roadbed_mask");
  clazz->annotateTyped<bool>("editor.palette", true);
  clazz->annotateTyped<int>("editor.palette.sort", 21);
  clazz->directProperty("width_m", &RoadbedMaskModuleData::_width_m);
  clazz->directProperty("shoulder_m", &RoadbedMaskModuleData::_shoulder_m);
  clazz->directProperty("v_meters_per_tile", &RoadbedMaskModuleData::_v_meters_per_tile);
  clazz->directProperty("extent_m", &RoadbedMaskModuleData::_extent_m);
  clazz->directProperty("out_dim", &RoadbedMaskModuleData::_out_dim);
}

static void _reshapeKeepoutIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<dflowgfx::HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Roadbed");
  dflow::ModuleData::createOutputPlug<dflowgfx::HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out"); // named "Out" (see RoadbedMask note)
}
KeepoutMaskModuleData::KeepoutMaskModuleData() {}
std::shared_ptr<KeepoutMaskModuleData> KeepoutMaskModuleData::createShared() {
  auto d = std::make_shared<KeepoutMaskModuleData>(); _reshapeKeepoutIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t KeepoutMaskModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<KeepoutMaskModuleInst>(this, g);
}
void KeepoutMaskModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return KeepoutMaskModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeKeepoutIOs(m); });
  clazz->annotateTyped<ConstString>("dsl.verb", "keepout_mask");
  clazz->annotateTyped<bool>("editor.palette", true);
  clazz->annotateTyped<int>("editor.palette.sort", 22);
  clazz->directProperty("keepout_radius_m", &KeepoutMaskModuleData::_keepout_radius_m);
  clazz->directProperty("extent_m", &KeepoutMaskModuleData::_extent_m);
}

} // namespace ork::lev2::hypermesh
