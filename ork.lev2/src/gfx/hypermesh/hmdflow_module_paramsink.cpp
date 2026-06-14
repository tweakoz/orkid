////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"

ImplementReflectionX(ork::lev2::hypermesh::MaterialParamSinkData, "hypermesh::MaterialParamSinkData");

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////
// MaterialParamSink (E.6/2.12) — see hmdflow.h. The INST is a pure mesh
// passthrough (GidAssign-style COW alias, zero GPU work); the sink's actual
// effect happens drawable-side: hm_drawable reads the pokeable "value" DATA
// plug per-frame and bindParam()s the resolved materials by _param_name.
// Keeping the read on the DATA plug (not an inst copy) means a poke is live
// even on a STATIC graph whose writeParams never re-runs.
///////////////////////////////////////////////////////////////////////////////

struct MaterialParamSinkInst : public MeshComputeInst {
  MaterialParamSinkInst(const MaterialParamSinkData* d, dflow::GraphInst* g)
      : MeshComputeInst(d, g)
      , _d(d) {
  }
  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _input  = typedInputNamed<MeshPlugTraits>("In");
  }
  void writeParams(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in)
      return;
    auto out           = _output->_value;
    out->_channels     = in->_channels;
    out->_faces        = in->_faces;
    out->_vidx         = in->_vidx;
    out->_face_offsets = in->_face_offsets;
    out->_header       = in->_header;
    out->_capacity     = in->_capacity;
    out->_num_verts    = in->_num_verts;
    out->_num_corners  = in->_num_corners;
    out->_num_faces    = in->_num_faces;
  }
  void compute(dflow::GraphInst*, ui::updatedata_ptr_t) final {
  }
  const MaterialParamSinkData* _d;
  mesh_outpluginst_ptr_t _output;
  mesh_inpluginst_ptr_t _input;
};

static void _reshapeParamSinkIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "value")->setValue(0.0f);
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
MaterialParamSinkData::MaterialParamSinkData() {
}
std::shared_ptr<MaterialParamSinkData> MaterialParamSinkData::createShared() {
  auto d = std::make_shared<MaterialParamSinkData>();
  _reshapeParamSinkIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t MaterialParamSinkData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<MaterialParamSinkInst>(this, g);
}
void MaterialParamSinkData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return MaterialParamSinkData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeParamSinkIOs(m); });
  clazz->directProperty("param_name", &MaterialParamSinkData::_param_name);
}

} // namespace ork::lev2::hypermesh
