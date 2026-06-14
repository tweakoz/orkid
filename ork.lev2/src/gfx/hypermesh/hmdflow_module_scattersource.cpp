////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include <ork/lev2/gfx/meshutil/geometry.h>
#include <ork/file/path.h>
#include <filesystem>

ImplementReflectionX(ork::lev2::hypermesh::ScatterSourceData, "hypermesh::ScatterSourceData");

// the family-neutral InstanceSet plug (HYPERECS E.2) — template instantiations + reflection.
// Lives here (the first producing family's TU), named NEUTRALLY: any family may carry the type.
namespace ork::lev2::dflowgfx {
instanceset_inst_ptr_t InstanceSetPlugTraits::data_to_inst(instanceset_data_ptr_t inp) {
  return std::make_shared<InstanceSetInst>(inp);
}
} // namespace ork::lev2::dflowgfx

namespace dflow = ::ork::dataflow;
namespace dgfx  = ork::lev2::dflowgfx;

template <> //
void dgfx::instset_outplugdata_t::describeX(class_t* clazz) {
}
template <> //
void dgfx::instset_inplugdata_t::describeX(class_t* clazz) {
}
template <> //
dflow::inpluginst_ptr_t dgfx::instset_inplugdata_t::createInstance(ModuleInst* minst) const {
  return std::make_shared<dgfx::instset_inpluginst_t>(this, minst);
}
template <> //
dflow::outpluginst_ptr_t dgfx::instset_outplugdata_t::createInstance(ModuleInst* minst) const {
  return std::make_shared<dgfx::instset_outpluginst_t>(this, minst);
}
ImplementTemplateReflectionX(dgfx::instset_outplugdata_t, "dflowgfx::instsetoutplug");
ImplementTemplateReflectionX(dgfx::instset_inplugdata_t, "dflowgfx::instsetinplug");

namespace ork::lev2::hypermesh {

// ScatterSourceModule — see hmdflow.h. Reads the baked ScatterSet .ogeo ONCE at
// onActivate and fills the InstanceSet's matrices + attrs SSBOs (a STATIC set, v1 —
// a dynamic/streamed source later just refills + markChanged()s; consumers key on
// _version). No compute (nothing per-frame).

struct ScatterSourceInst : public MeshComputeInst {
  ScatterSourceInst(const ScatterSourceData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}

  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<dflowgfx::InstanceSetPlugTraits>("Out");
  }

  std::string _resolvePath() const {
    if (not _d->_ogeo_path.empty())
      return _d->_ogeo_path;
    OrkAssert(not _d->_scatter_asset.empty() and not _d->_sink.empty());
    return file::Path::expandPathString(
        "<assetcache>/terrain/" + _d->_scatter_asset + "/" + _d->_sink + ".ogeo");
  }

  void onActivate(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<MeshEnv>();
    auto fxi = env->_ctx->FXI();
    auto path = _resolvePath();
    if (not std::filesystem::exists(path)) {
      printf(
          "ScatterSource<%s>: ScatterSet MISSING <%s> — the HeightField asset must materialize "
          "(and place its sinks) BEFORE this graph activates (declaration order = dependency "
          "order). Refusing to emit an empty set silently.\n",
          _dgmodule_data->_name.c_str(), path.c_str());
      OrkAssert(false);
    }
    auto geo = meshutil::Geometry::readChunkfile(file::Path(path.c_str()));
    OrkAssert(geo);
    auto chX = geo->_point.channelAs<fmtx4>("xform");
    auto chT = geo->_point.channelAs<int>("type_id");
    auto chS = geo->_point.channelAs<int>("variant_seed");
    OrkAssert(chX and chT);
    const int total = int(chX->_data.size());

    // filter by type (one typed hypermesh node per type; -1 = the whole set)
    std::vector<int> sel;
    sel.reserve(total);
    for (int i = 0; i < total; i++)
      if (_d->_type_id < 0 or chT->_data[i] == _d->_type_id)
        sel.push_back(i);
    const int N = int(sel.size());

    auto iset = _output->_value; // InstanceSetInst (created via data_to_inst)
    iset->_count = N;
    if (N == 0) {
      printf("ScatterSource<%s>: type_id<%d> selects ZERO of %d instances in <%s>\n",
             _dgmodule_data->_name.c_str(), _d->_type_id, total, path.c_str());
      iset->markChanged();
      return; // a legitimately-empty type: 0 instances draw (count rides the indirect args)
    }
    iset->_matrices = fxi->createStorageBuffer(size_t(N) * sizeof(fmtx4));
    iset->_attrs    = fxi->createStorageBuffer(size_t(N) * 4 * sizeof(float));
    {
      auto m = fxi->mapStorageBuffer(iset->_matrices, 0, size_t(N) * sizeof(fmtx4), BufferMapAccess::WRITE_ONLY);
      auto dst = (fmtx4*)m->_mappedaddr;
      for (int i = 0; i < N; i++)
        dst[i] = chX->_data[sel[i]];
      fxi->unmapStorageBuffer(m.get());
    }
    {
      auto m = fxi->mapStorageBuffer(iset->_attrs, 0, size_t(N) * 16, BufferMapAccess::WRITE_ONLY);
      auto dst = (float*)m->_mappedaddr;
      for (int i = 0; i < N; i++) {
        const int s   = sel[i];
        dst[i * 4 + 0] = float(chT->_data[s]);                                        // x = type_id (raw)
        dst[i * 4 + 1] = chS ? float(chS->_data[s] % 1000) / 1000.0f : 0.0f;          // y = variant seed 0..1
        dst[i * 4 + 2] = 0.0f;
        dst[i * 4 + 3] = 1.0f;
      }
      fxi->unmapStorageBuffer(m.get());
    }
    iset->markChanged();
    printf("ScatterSource<%s>: %d/%d instances (type_id %d) from <%s>\n",
           _dgmodule_data->_name.c_str(), N, total, _d->_type_id, path.c_str());
  }

  void compute(dflow::GraphInst*, ui::updatedata_ptr_t) final {} // static set: nothing per-frame

  const ScatterSourceData* _d;
  dflowgfx::instset_outpluginst_ptr_t _output;
};

static void _reshapeScatterSourceIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createOutputPlug<dflowgfx::InstanceSetPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
ScatterSourceData::ScatterSourceData() {
}
std::shared_ptr<ScatterSourceData> ScatterSourceData::createShared() {
  auto d = std::make_shared<ScatterSourceData>();
  _reshapeScatterSourceIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t ScatterSourceData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<ScatterSourceInst>(this, g);
}
void ScatterSourceData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return ScatterSourceData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeScatterSourceIOs(m); });
  clazz->directProperty("scatter_asset", &ScatterSourceData::_scatter_asset); // portable: asset + sink
  clazz->directProperty("sink", &ScatterSourceData::_sink);
  clazz->directProperty("ogeo_path", &ScatterSourceData::_ogeo_path);         // direct override (tools)
  clazz->directProperty("type_id", &ScatterSourceData::_type_id);
}

} // namespace ork::lev2::hypermesh
